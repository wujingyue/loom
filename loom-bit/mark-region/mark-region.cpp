#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/Constants.h"
#include "llvm/BasicBlock.h"
#include "llvm/Pass.h"
#include "llvm/Support/CFG.h"
#include "llvm/Transforms/Utils/BasicInliner.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"

#include <set>
#include <map>
#include <fstream>
#include <sstream>

#include "../../../llvm/idm/util.h"
#include "../../../llvm/idm/id.h"
#include "../loom.h"

using namespace llvm;
using namespace std;

const static string SUFFIX = ".loom";

namespace defens {


	struct MarkRegion: public ModulePass {

		static char ID;

		DenseMap<BasicBlock *, vector<Function *> > called_func;
		DenseMap<Function *, vector<Instruction *> > call_sites;
		DenseMap<Function *, vector<Instruction *> > exits;
		DenseSet<BasicBlock *> fixed, uncrossable, visited_bb, postdomed;
		DenseSet<Function *> cuts;
		DenseSet<Instruction *> visited_ins, sinks; /* sinks always empty */

		const Type *intty;
		FunctionType *stub_fty, *stub_ret_fty, *init_fty, *switch_fty, *no_arg_fty;
		int nodes_explored;

		vector<Instruction *> frontier, postier;

		Function *mysql_handle_slave_sql, *mysql_handle_slave_io;

		Constant *loom_check, *loom_before_call, *loom_after_call;
		Constant *loom_enter_thread_func, *loom_exit_thread_func;

		MarkRegion(): ModulePass(&ID) {}

		virtual void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.setPreservesAll();
			AU.addRequired<ObjectID>();
			ModulePass::getAnalysisUsage(AU);
		}

		void print_ins(Instruction *ins) {
			cerr << ins->getParent()->getParent()->getNameStr();
			cerr << "." << ins->getParent()->getNameStr();
			ins->dump();
		}

		bool is_exit_block(BasicBlock *bb) {
			if (succ_begin(bb) != succ_end(bb))
				return false;
			for (BasicBlock::iterator ii = bb->begin(); ii != bb->end(); ii++) {
				if (dyn_cast<ReturnInst>(ii))
					return true;
			}
			return false;
		}

		void construct_call_graph(Module &M) {
			/* Which functions are used as function pointers? */
			set<Function *> fps;
			forallinst(M, ii) {
				if (is_intrinsic_call(ii))
					continue;
				unsigned start_idx = 0;
				if (is_call(ii))
					start_idx = 1;
				unsigned n = ii->getNumOperands();
				for (unsigned i = start_idx; i < n; ++i) {
					Value *v = ii->getOperand(i);
					if (isa<Function>(v)) {
						Function *f = dyn_cast<Function>(v);
						if (f->isDeclaration())
							continue;
						fps.insert(f);
					}
				}
			}
#if 0
			cerr << "# of functions used as function pointers = " << fps.size() << endl;
			for (set<Function *>::iterator it = fps.begin(); it != fps.end(); ++it)
				cerr << (*it)->getNameStr() << endl;
#endif
			/* Every BB's called functions */
			called_func.clear();
			call_sites.clear();
			vector<int> dist_n_cands;
			forallinst(M, ii) {
				if (is_call(ii) && !is_intrinsic_call(ii)) {
					CallSite cs(ii);
					Function *callee = cs.getCalledFunction();
					if (!callee) {
						Value *v = cs.getCalledValue();
						assert(v);
						assert(v->getType()->isPointerTy());
						const Type *t = dyn_cast<PointerType>(v->getType())->getElementType();
						assert(t->isFunctionTy());
#if 0
						cerr << "Function Pointer detected\n";
						cerr << "Candidates:\n";
#endif
						int n_cands = 0;
						for (set<Function *>::iterator it = fps.begin(); it != fps.end(); ++it) {
							Function *fi = *it;
							if (fi->isDeclaration())
								continue;
							if (fi->getFunctionType() == t) {
								// cerr << fi->getNameStr() << ' ';
								called_func[ii->getParent()].push_back(fi);
								call_sites[fi].push_back(ii);
								n_cands++;
							}
						}
						dist_n_cands.push_back(n_cands);
						// cerr << endl;
					} else if (!callee->isDeclaration()) {
						called_func[ii->getParent()].push_back(callee);
						call_sites[callee].push_back(ii);
					}
				}
			}
#if 0
			cerr << "Distribution of # of candidates:\n";
			sort(dist_n_cands.begin(), dist_n_cands.end());
			for (size_t i = 0; i < dist_n_cands.size(); ++i) {
				cerr << dist_n_cands[i] << ' ';
			}
			cerr << endl;
#endif
			for (DenseMap<BasicBlock *, vector<Function *> >::iterator it = called_func.begin();
					it != called_func.end(); ++it) {
				sort(it->second.begin(), it->second.end());
				it->second.resize(unique(it->second.begin(), it->second.end()) - it->second.begin());
			}
			for (DenseMap<Function *, vector<Instruction *> >::iterator it = call_sites.begin();
					it != call_sites.end(); ++it) {
				sort(it->second.begin(), it->second.end());
				it->second.resize(unique(it->second.begin(), it->second.end()) - it->second.begin());
			}
			exits.clear();
			forallbb(M, bi) {
				if (is_exit_block(bi))
					exits[bi->getParent()].push_back(&bi->back());
			}
			fprintf(stderr, "Finished generating the call graph.\n");
		}

		bool get_uncrossable(BasicBlock *bb, bool &updated) {
			if (fixed.count(bb) || visited_bb.count(bb))
				return uncrossable.count(bb);
			visited_bb.insert(bb);
			bool old_value = uncrossable.count(bb);
			if (called_func.find(bb) != called_func.end()) {
				vector<Function *> &called_funcs = called_func[bb];
				for (vector<Function *>::iterator it = called_funcs.begin(); it != called_funcs.end(); it++) {
					Function *callee = *it;
					BasicBlock *y = &callee->getEntryBlock();
					if (get_uncrossable(y, updated)) {
						if (fixed.count(y)) {
							fixed.insert(bb);
						}
						assert(old_value == true);
						return true;
					}
				}
			}
			if (succ_begin(bb) == succ_end(bb)) {
				if (old_value != false) {
					uncrossable.erase(bb);
					updated = true;
					fixed.insert(bb);
				}
				return false;
			}
			bool succ_all_fixed = true;
			for (succ_iterator it = succ_begin(bb); it != succ_end(bb); it++) {
				BasicBlock *y = *it;
				if (!get_uncrossable(y, updated)) {
					if (old_value != false) {
						updated = true;
						uncrossable.erase(bb);
						fixed.insert(bb);
					}
					return false;
				}
				/* uncrossable == true */
				if (!fixed.count(y))
					succ_all_fixed = false;
			}
			assert(old_value == true);
			if (succ_all_fixed) {
				fixed.insert(bb);
			}
			return true;
		}

		/*
		 * Input: uncrossable and fixed
		 * Output: cuts
		 */
		void construct_partial_postdom(Module &M) {
			bool updated;
			do {
				fprintf(stderr, "iteration\n");
				updated = false;
				visited_bb.clear();
				forallbb(M, bi) {
					if (!fixed.count(bi) && !visited_bb.count(bi))
						get_uncrossable(bi, updated);
				}
			} while (updated);

#if 0
			fprintf(stderr, "uncrossable.size() == %u\n", uncrossable.size());
			forallbb(M, bi) {
				if (uncrossable.count(bi)) {
					fprintf(stderr, "Basic block %s.%s is not crossable\n", bi->getParent()->getNameStr().c_str(), bi->getNameStr().c_str());
				}
			}
#endif
			cuts.clear();
			forallfunc(M, fi) {
				if (!fi->isDeclaration()) {
					if (uncrossable.count(&fi->getEntryBlock()))
						cuts.insert(fi);
				}
			}

#if 0
			forallfunc(M, fi) {
				if (cuts.count(fi))
					fprintf(stderr, "Function %s is not crossable.\n", fi->getNameStr().c_str());
			}
#endif
		}

		Instruction *traverse_next_inst(Instruction *x, const DenseSet<Instruction *> &cut, bool follow_return) {
			BasicBlock *bb = x->getParent();
			BasicBlock::iterator y = x;
			y++;
			if (y != bb->end()) {
				if (!visited_ins.count(y)) {
					Instruction *res = DFS_ins(y, cut, follow_return);
					if (res != NULL)
						return res;
				}
			} else {
				// Reached the end of the basic block. Traverse its successors. 
				for (succ_iterator it = succ_begin(bb); it != succ_end(bb); it++) {
					y = (*it)->begin();
					if (!visited_ins.count(y)) {
						Instruction *res = DFS_ins(y, cut, follow_return);
						if (res != NULL)
							return res;
					}
				}
			}
			return NULL;
		}

		Instruction *DFS_ins(Instruction *x, const DenseSet<Instruction *> &cut, bool follow_return) {
			nodes_explored++;
			if (nodes_explored % 10000 == 0)
				fprintf(stderr, "# of nodes explored = %d\n", nodes_explored);
			if (x == x->getParent()->begin())
				fprintf(stderr, "Traversing Basic Block %s.%s\n",
						x->getParent()->getParent()->getNameStr().c_str(),
						x->getParent()->getNameStr().c_str());
			assert(x);
			if (cut.count(x))
				return NULL;
			visited_ins.insert(x);
			if (sinks.count(x))
				return x;
			if (is_call(x) && !is_intrinsic_call(x)) {
				CallSite cs(x);
				Function *callee = cs.getCalledFunction();
				if (callee && !callee->isDeclaration()) {
					/* Explicit function calls */
					// fprintf(stderr, "Traversing function %s...\n", callee->getName().c_str());
					Instruction *y = callee->getEntryBlock().begin();
					if (!visited_ins.count(y)) {
						Instruction *res = DFS_ins(y, cut, false);
						if (res)
							return res;
					}
					if (cuts.count(callee))
						return NULL;
				} else if (callee && callee->getName() == "pthread_create") {
					/* Thread creation */
					CallSite::arg_iterator ai = cs.arg_begin();
					while (ai != cs.arg_end() && !isa<Function>(ai)) {
						if (isa<BitCastInst>(ai))
							cerr << "bitcast\n";
						ai++;
					}
					if (ai == cs.arg_end()) {
						fprintf(stderr, "[WARNING] pthread_create without a constant function.\n");
						x->dump();
						string str_ins;
						raw_string_ostream strout(str_ins);
						x->print(strout);
						if (str_ins.find("h_func") != string::npos) {
							fprintf(stderr, "[WARNING] Suspect it is a function pointer in mysql server.\n"
									"The calling function can only be handle_slave_io or handle_slave_sql.\n");
							if (mysql_handle_slave_io) {
								Instruction *y = mysql_handle_slave_io->getEntryBlock().begin();
								if (!visited_ins.count(y)) {
									Instruction *res = DFS_ins(y, cut, false);
									if (res)
										return res;
								}
							}
							if (mysql_handle_slave_sql) {
								Instruction *y = mysql_handle_slave_sql->getEntryBlock().begin();
								if (!visited_ins.count(y)) {
									Instruction *res = DFS_ins(y, cut, false);
									if (res)
										return res;
								}
							}
						}
					} else {
						Function *target = dyn_cast<Function>(*ai);
						fprintf(stderr, "pthread_create with a constant function:\n");
						if (target && !target->isDeclaration()) {
							cerr << target->getNameStr() << endl;
							Instruction *y = target->getEntryBlock().begin();
							if (!visited_ins.count(y)) {
								Instruction *res = DFS_ins(y, cut, false);
								if (res)
									return res;
							}
						}
					}
				}
			} /* is a function call */
			if (follow_return && dyn_cast<ReturnInst>(x)) {
				/*
				 * If the flag follow_return is set and it is a return instruction, 
				 * traverse the next instruction of each call site.
				 */
				vector<Instruction *> css = call_sites[x->getParent()->getParent()];
				for (size_t i = 0; i < css.size(); i++) {
					Instruction *res = traverse_next_inst(css[i], cut, follow_return);
					if (res)
						return res;
				}
			}
			Instruction *res = traverse_next_inst(x, cut, follow_return);
			if (res)
				return res;
			return NULL;
		}

		int read_region(Module &M, ObjectID &IDM) {
			ifstream fin("/tmp/mark-region.in");
			if (!fin.is_open())
				return -1;

			string line;
			int ins_id;
			getline(fin, line);
			istringstream strin(line);
			frontier.clear();
			while (strin >> ins_id) {
				Instruction *ins = IDM.getInstruction(ins_id);
				assert(ins);
				frontier.push_back(ins);
			}
			getline(fin, line);
			strin.clear();
			strin.str(line);
			postier.clear();
			while (strin >> ins_id) {
				Instruction *ins = IDM.getInstruction(ins_id);
				assert(ins);
				postier.push_back(ins);
			}

			return 0;
		}

		void get_injected_functions(Module &M) {
			intty = IntegerType::get(getGlobalContext(), 32);
			vector<const Type *> params;
			params.push_back(intty);
			stub_fty = FunctionType::get(Type::getVoidTy(getGlobalContext()), params, false);
			stub_ret_fty = FunctionType::get(intty, params, false);
			switch_fty = FunctionType::get(intty, params, false);
			init_fty = FunctionType::get(Type::getVoidTy(getGlobalContext()), params, false);
			no_arg_fty = FunctionType::get(Type::getVoidTy(getGlobalContext()), vector<const Type *>(), false);

			loom_enter_thread_func = M.getOrInsertFunction(LOOM_ENTER_THREAD_FUNC, stub_fty);
			loom_exit_thread_func = M.getOrInsertFunction(LOOM_EXIT_THREAD_FUNC, stub_fty);
			loom_before_call = M.getOrInsertFunction(LOOM_BEFORE_CALL, stub_ret_fty);
			loom_after_call = M.getOrInsertFunction(LOOM_AFTER_CALL, stub_fty);
			loom_check = M.getOrInsertFunction(LOOM_CHECK, stub_fty);
		}

		virtual bool runOnModule(Module &M) {

			get_injected_functions(M);

			ObjectID &IDM = getAnalysis<ObjectID>();

			if (read_region(M, IDM) == -1) {
				cerr << "Cannot find the input file\n";
				return false;
			}

			construct_call_graph(M);
			uncrossable.clear();
			forallbb(M, bi) {
				uncrossable.insert(bi);
			}
			fixed.clear();
			for (size_t i = 0; i < postier.size(); ++i) {
				BasicBlock *bb = postier[i]->getParent();
				fixed.insert(bb);
			}
			construct_partial_postdom(M);

			DenseSet<Instruction *> cut;
			for (size_t i = 0; i < postier.size(); ++i)
				cut.insert(postier[i]);

			nodes_explored = 0;
			visited_ins.clear();
			for (size_t i = 0; i < frontier.size(); ++i) {
				Instruction *start = frontier[i];
				DFS_ins(start, cut, true);
			}

			set<int> checks_to_be_disabled;
			for (DenseSet<Instruction *>::iterator it = visited_ins.begin();
					it != visited_ins.end(); ++it) {
				if (is_call(*it)) {
					CallSite cs(*it);
					Function *func = cs.getCalledFunction();
					if (func == loom_check ||
							func == loom_before_call ||
							func == loom_after_call) {
						int check_id = -1;
						for (unsigned i = 0; i < (*it)->getNumOperands(); ++i) {
							Value *v = (*it)->getOperand(i);
							if (v->getType() == intty) {
								assert(isa<ConstantInt>(v));
								check_id = dyn_cast<ConstantInt>(v)->getValue().getLimitedValue();
								break;
							}
						}
						if (check_id < 0)
							print_ins(*it);
						assert(check_id >= 0);
						checks_to_be_disabled.insert(check_id);
					}
				}
				cerr << "Instruction " << IDM.getInstructionID(*it) << endl;
			}
			ofstream fout("/tmp/mark-region.out");
			for (set<int>::iterator it = checks_to_be_disabled.begin();
					it != checks_to_be_disabled.end(); ++it) {
				cerr << "Check " << *it << endl;
				fout << *it << endl;
			}
			fout.close();
			fout.open("/tmp/funcs-to-be-patched.out");
			for (size_t i = 0; i < frontier.size(); ++i) {
				Function *f = frontier[i]->getParent()->getParent();
				assert(f);
				cerr << "Function: " << f->getNameStr() << endl;
				fout << IDM.getFunctionID(f) << endl;
			}
			for (size_t i = 0; i < postier.size(); ++i) {
				Function *f = postier[i]->getParent()->getParent();
				assert(f);
				cerr << "Function: " << f->getNameStr() << endl;
				fout << IDM.getFunctionID(f) << endl;
			}
			fout.close();
			return false;
		}
	};

	char MarkRegion::ID = 0;
}

namespace {
	static RegisterPass<defens::MarkRegion>
		X("mark-region", "An analysis to get the checks within a region");
}
