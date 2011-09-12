#include <set>
#include <map>
#include <fstream>
using namespace std;

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"
#include "llvm/Analysis/Dominators.h"
#include "common/util.h"
#include "common/IDAssigner.h"
#include "loom/loom.h"
using namespace llvm;

const static string SUFFIX = ".loom";

namespace defens {
	enum COLOR {
		WHITE = 0,
		GRAY,
		BLACK
	};

	struct BackEdge {
		BasicBlock *first, *second;
		unsigned idx_successor;
		BackEdge(BasicBlock *ff, BasicBlock *ss, unsigned ii):
			first(ff), second(ss), idx_successor(ii) {}
	};

	struct CloneFunc: public ModulePass {
		static char ID;

		const Type *int_type, *int_arr_type;
		// Constant *is_func_patched;
		Constant *loom_is_func_patched;
		FunctionType *stub_fty, *init_fty, *switch_fty, *no_arg_fty;

		map<BasicBlock *, COLOR> bb_color;

		CloneFunc(): ModulePass(&ID) {}

		virtual void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.addRequired<IDAssigner>();
			AU.addRequired<DominatorTree>();
			ModulePass::getAnalysisUsage(AU);
		}

		void DFS(BasicBlock *x, vector<BackEdge> &back_edges) {
			bb_color[x] = GRAY;
			TerminatorInst *ti = x->getTerminator();
			assert(ti);
			for (unsigned i = 0; i < ti->getNumSuccessors(); ++i) {
				BasicBlock *y = ti->getSuccessor(i);
				if (bb_color[y] == WHITE)
					DFS(y, back_edges);
				else if (bb_color[y] == GRAY) {
					back_edges.push_back(BackEdge(x, y, i));
				}
			}
			bb_color[x] = BLACK;
		}

		void print_ins(Instruction *ins) {
			errs() << ins->getParent()->getParent()->getNameStr();
			errs() << "." << ins->getParent()->getNameStr();
			ins->dump();
		}

		void create_funcs_and_gvs(Module &M) {
			int_type = IntegerType::get(M.getContext(), 32);
			int_arr_type = ArrayType::get(int_type, MAX_N_FUNCS);

			vector<const Type *> params;
			params.push_back(int_type);
			stub_fty = FunctionType::get(Type::getVoidTy(M.getContext()), params, false);
			switch_fty = FunctionType::get(int_type, params, false);
			init_fty = FunctionType::get(Type::getVoidTy(M.getContext()), params, false);
			no_arg_fty = FunctionType::get(Type::getVoidTy(M.getContext()), vector<const Type *>(), false);

			// is_func_patched = M.getOrInsertGlobal("is_func_patched", int_arr_type);
			loom_is_func_patched = M.getOrInsertFunction(LOOM_IS_FUNC_PATCHED, switch_fty);
		}

		void statistic(Module &M) {
			/* Get the number of functions. */
			int n_funcs = 0, n_funcs_including_decl = 0;
			forallfunc(M, fi) {
				n_funcs_including_decl++;
				if (!fi->isDeclaration()) {
					n_funcs++;
				}
			}
			fprintf(stderr, "# of defined functions = %d\n", n_funcs);
			fprintf(stderr, "# of functions = %d\n", n_funcs_including_decl);
		}

		void check_for_dominance(Function *fi) {
			// Check for dominance
			DominatorTree &DT1 = getAnalysis<DominatorTree>(*fi);
			for (Function::iterator bi = fi->begin(); bi != fi->end(); ++bi) {
				for (BasicBlock::iterator ii = bi->begin(); ii != bi->end(); ++ii) {
					for (Value::use_iterator ui = ii->use_begin(); ui != ii->use_end(); ++ui) {
						Instruction *user = dyn_cast<Instruction>(*ui);
						if (!user)
							continue;
						if (isa<PHINode>(user))
							continue;
						if (isa<InvokeInst>(ii))
							continue;
						if (!DT1.dominates(ii, user)) {
							print_ins(ii);
							print_ins(user);
							assert(false && "STILL not dominate");
						}
					}
				}
			}
		}

		void check_for_phi_nodes(Function *fi) {
#if 1
			// Check the validity of PHI nodes
			for (Function::iterator bi = fi->begin(); bi != fi->end(); ++bi) {
				vector<BasicBlock *> preds(pred_begin(bi), pred_end(bi));
				sort(preds.begin(), preds.end());
				for (BasicBlock::iterator ii = bi->begin(); ii != bi->end(); ++ii) {
					PHINode *phi = dyn_cast<PHINode>(ii);
					if (!phi)
						break;
					vector<BasicBlock *> incoming;
					for (unsigned j = 0; j < phi->getNumIncomingValues(); ++j)
						incoming.push_back(phi->getIncomingBlock(j));
					sort(incoming.begin(), incoming.end());
					bool valid = true;
					assert(preds.size() == phi->getNumIncomingValues());
					for (unsigned j = 0; j < preds.size(); ++j) {
						if (preds[j] != incoming[j])
							valid = false;
					}
					if (!valid) {
						errs() << "Predecessors\n";
						for (vector<BasicBlock *>::iterator j = preds.begin();
								j != preds.end(); ++j)
							errs() << (*j)->getNameStr() << " ";
						errs() << "\n";
						errs() << "Incoming blocks:\n";
						for (vector<BasicBlock *>::iterator j = incoming.begin();
								j != incoming.end(); ++j)
							errs() << (*j)->getNameStr() << " ";
						errs() << "\n";
					}
					for (unsigned j = 0; j < preds.size(); ++j)
						assert(preds[j] == incoming[j]);
				}
			}
#endif
		}

		bool is_tight_loop(BasicBlock *x, BasicBlock *y) {
			if (x == y) {
				for (BasicBlock::iterator ii = x->begin(); ii != x->end(); ++ii) {
					if (is_call(ii) && !is_intrinsic_call(ii)) {
						return false;
					}
				}
				return true;
			}
			bool y_to_x = false;
			for (succ_iterator it = succ_begin(y); it != succ_end(y); ++it) {
				if (*it == x) {
					y_to_x = true;
					break;
				}
			}
			if (!y_to_x)
				return false;
			for (BasicBlock::iterator ii = x->begin(); ii != x->end(); ++ii) {
				if (is_call(ii) && !is_intrinsic_call(ii)) {
					return false;
				}
			}
			for (BasicBlock::iterator ii = y->begin(); ii != y->end(); ++ii) {
				if (is_call(ii) && !is_intrinsic_call(ii)) {
					return false;
				}
			}
			return true;
		}

		void process(Function *fi) {
			IDAssigner &IDA = getAnalysis<IDAssigner>();

			int func_id = IDA.getFunctionID(fi);
			dbgs() << "Function: " << fi->getNameStr() << "\n";
			// Find all back edges.
			bb_color.clear();
			for (Function::iterator bi = fi->begin(); bi != fi->end(); bi++)
				bb_color[bi] = WHITE;
			vector<BackEdge> back_edges;
			DFS(&fi->getEntryBlock(), back_edges);

			// OPT 5: Do not instrument back edges on tight loops
#ifdef OPT5
			for (int i = (int)back_edges.size() - 1; i >= 0; i--) {
				BasicBlock *x = back_edges[i].first, *y = back_edges[i].second;
				if (is_tight_loop(x, y)) {
					errs() << "Ignored the back edge: " << back_edges[i].first->getNameStr();
					errs() << " -> " << back_edges[i].second->getNameStr() << "\n";
					back_edges.erase(back_edges.begin() + i);
				}
			}
#endif

			// Clone
			DenseMap<const Value *, Value *> clone_mapping;
			for (Function::arg_iterator ai = fi->arg_begin(); ai != fi->arg_end(); ++ai)
				clone_mapping[ai] = ai;
			// Put basic blocks to a local list first.
			// Otherwise we may clone the cloned basic blocks.
			vector<BasicBlock *> bbs;
			set<BasicBlock *> set_old_bbs;
			for (Function::iterator bi = fi->begin(); bi != fi->end(); ++bi) {
				bbs.push_back(bi);
				set_old_bbs.insert(bi);
			}
			// Clone basic blocks.
			for (size_t i = 0; i < bbs.size(); ++i) {
				BasicBlock *new_bb = CloneBasicBlock(bbs[i], clone_mapping, SUFFIX.c_str(), fi, NULL);
				clone_mapping[bbs[i]] = new_bb;
			}
			// Construct reverse mapping
			DenseMap<Value *, Value *> reverse_clone_mapping;
			for (size_t i = 0; i < bbs.size(); ++i) {
				for (BasicBlock::iterator ii = bbs[i]->begin(); ii != bbs[i]->end(); ++ii) {
					if (clone_mapping.count(ii))
						reverse_clone_mapping[clone_mapping[ii]] = ii;
				}
			}
			// Remap instructions to avoid duplicated definitions
			for (size_t i = 0; i < bbs.size(); ++i) {
				BasicBlock *new_bb = dyn_cast<BasicBlock>(clone_mapping[bbs[i]]);
				for (BasicBlock::iterator ii = new_bb->begin(); ii != new_bb->end(); ++ii) {
					RemapInstruction(ii, clone_mapping);
				}
			}

#if 1
			// Add back-edge basic blocks for the new version. 
			// New version first, because the modification of the new version
			// doesn't affect the old version.
			for (size_t i = 0; i < back_edges.size(); ++i) {
				BasicBlock *x = back_edges[i].first, *y = back_edges[i].second;
				x = dyn_cast<BasicBlock>(clone_mapping[x]); y = dyn_cast<BasicBlock>(clone_mapping[y]);
				unsigned idx = back_edges[i].idx_successor;
				// cerr << x->getNameStr() << " -> " << y->getNameStr() << endl;
				// Create the back-edge basic block
				// BasicBlock *bb = BasicBlock::Create(getGlobalContext(), "BE_" + x->getNameStr() + "_" + y->getNameStr(), fi);
				BasicBlock *bb = BasicBlock::Create(fi->getContext(), "", fi);
				bb->setName("new_backedge_" + bb->getNameStr());
				// Set its sucessor to y
				BranchInst::Create(y, bb);
				// Modify new_x's successor
				TerminatorInst *ti = x->getTerminator();
				assert(ti);
				ti->setSuccessor(idx, bb);
				// Modify PHI nodes of y;
				// For y, replace the incoming edge from x with from the back-edge BB
				for (BasicBlock::iterator ii = y->begin(); PHINode *phi = dyn_cast<PHINode>(ii); ++ii) {
					// Only replace the first one, because there may be multiple edges
					for (unsigned j = 0; j < phi->getNumIncomingValues(); ++j) {
						if (phi->getIncomingBlock(j) == x) {
							phi->setIncomingBlock(j, bb);
							break;
						}
					}
				}
			}

			// Add back-edge basic blocks for old version. 
			for (size_t i = 0; i < back_edges.size(); ++i) {
				BasicBlock *x = back_edges[i].first, *y = back_edges[i].second;
				unsigned idx = back_edges[i].idx_successor;
				// cerr << x->getNameStr() << " -> " << y->getNameStr() << endl;
				// BasicBlock *new_x = dyn_cast<BasicBlock>(clone_mapping[x]);
				BasicBlock *new_y = dyn_cast<BasicBlock>(clone_mapping[y]);
				// Create the back-edge basic block
				// BasicBlock *bb = BasicBlock::Create(getGlobalContext(), "BE_" + x->getNameStr() + "_" + y->getNameStr(), fi);
				BasicBlock *bb = BasicBlock::Create(fi->getContext(), "", fi);
				bb->setName("backedge_" + bb->getNameStr());
#if 0
				vector<Value *> indices;
				indices.push_back(ConstantInt::get(int_type, 0));
				indices.push_back(ConstantInt::get(int_type, func_id));
				Value *p = GetElementPtrInst::Create(is_func_patched, indices.begin(), indices.end(), "", bb);
				Value *patched = new LoadInst(p, "", bb);
#endif
				Value *patched = CallInst::Create(loom_is_func_patched, ConstantInt::get(int_type, func_id), "", bb);
				Value *jump_to_new = new ICmpInst(*bb, ICmpInst::ICMP_NE, patched, ConstantInt::get(int_type, 0));
				// Set its sucessor to y
				BranchInst::Create(new_y, y, jump_to_new, bb);
				// Modify x's successor
				TerminatorInst *ti = x->getTerminator();
				assert(ti);
				ti->setSuccessor(idx, bb);
				// Modify PHI nodes of y and new_y;
				// For y, replace the incoming edge from x with from the back-edge BB
				for (BasicBlock::iterator ii = y->begin(); PHINode *phi = dyn_cast<PHINode>(ii); ++ii) {
					PHINode *new_phi = dyn_cast<PHINode>(clone_mapping[phi]);
					// Only replace the first one, because there may be multiple edges
					for (unsigned j = 0; j < phi->getNumIncomingValues(); ++j) {
						if (phi->getIncomingBlock(j) == x) {
							phi->setIncomingBlock(j, bb);
							new_phi->addIncoming(phi->getIncomingValue(j), bb);
							break;
						}
					}
				}
			}
#endif

			// Add a switch at the function entry for thread functions including the main function. 
			BasicBlock *old_entry = &fi->getEntryBlock();
			BasicBlock *switch_bb = BasicBlock::Create(fi->getContext(), "__entry", fi, old_entry);
#if 0
			vector<Value *> indices;
			indices.push_back(ConstantInt::get(int_type, 0));
			indices.push_back(ConstantInt::get(int_type, func_id));
			Value *p = GetElementPtrInst::Create(is_func_patched, indices.begin(), indices.end(), "", switch_bb);
			Value *patched = new LoadInst(p, "", switch_bb);
#endif
			Value *patched = CallInst::Create(loom_is_func_patched, ConstantInt::get(int_type, func_id), "", switch_bb);
			Value *jump_to_new = new ICmpInst(*switch_bb, ICmpInst::ICMP_NE, patched, ConstantInt::get(int_type, 0));
			BranchInst::Create(dyn_cast<BasicBlock>(clone_mapping[old_entry]), old_entry, jump_to_new, switch_bb);

			// Resolve the conflict caused by redefinition.
			DominatorTree &DT = getAnalysis<DominatorTree>(*fi);
			map<Instruction *, vector<Instruction *> > to_resolve;
			for (Function::iterator bi = fi->begin(); bi != fi->end(); ++bi) {
				for (BasicBlock::iterator ii = bi->begin(); ii != bi->end(); ++ii) {
					for (Value::use_iterator ui = ii->use_begin(); ui != ii->use_end(); ++ui) {
						Instruction *user = dyn_cast<Instruction>(*ui);
						if (!user)
							continue;
						// Keep in mind that a PHI node doesn't follow the dominance rule,
						// even if it's valid. 
						if (!DT.dominates(ii, user)) {
							to_resolve[ii].push_back(user);
						}
					}
				}
			}
			for (map<Instruction *, vector<Instruction *> >::iterator it = to_resolve.begin();
					it != to_resolve.end(); ++it) {
				Instruction *ii = it->first;
				// Skip old basic blocks. An old instruction always dominates its users.
				// because there's no switch from the new path to the old path. 
				// InvokeInst does not dominate its usages.
				if (set_old_bbs.count(ii->getParent()))
					continue;
				// Create the SSAUpdater for ii
				SmallVector<PHINode *, 8> inserted_phis;
				SSAUpdater su(&inserted_phis);
				su.Initialize(ii);
				Instruction *old_ii = dyn_cast<Instruction>(reverse_clone_mapping[ii]);
				assert(old_ii != NULL);
				su.AddAvailableValue(old_ii->getParent(), old_ii);
				su.AddAvailableValue(ii->getParent(), ii);

				vector<Instruction *> &users = it->second;
				for (size_t k = 0; k < users.size(); ++k) {
					Instruction *user = users[k];
					// There may be more than one operands of the same value.
					for (unsigned i = 0; i < user->getNumOperands(); ++i) {
						if (user->getOperand(i) == ii)
							su.RewriteUse(user->getOperandUse(i));
					}
				}
			}

			// check_for_dominance(fi);
			check_for_phi_nodes(fi);
		}

		virtual bool runOnModule(Module &M) {
			create_funcs_and_gvs(M);
			statistic(M);
			// Clone basic blocks inside each function.
			forallfunc(M, fi) {
				if (fi->isDeclaration())
					continue;
				process(fi);
			}

			return true;
		}
	};

	char CloneFunc::ID = 0;
}

namespace {
	static RegisterPass<defens::CloneFunc>
		X("clone-func", "Clone a slow copy of all the functions");
}
