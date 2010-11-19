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
#include "llvm/InlineAsm.h"

#include <set>
#include <map>
#include <fstream>

#include "../../include/util.h"
#include "../../idm/id.h"
#include "../loom.h"

using namespace llvm;
using namespace std;

const static string SUFFIX = ".loom";

namespace defens {

	bool is_in_libm(const Function *f) {
		string name = f->getNameStr();
		if (name == "cos") return true;
		if (name == "sin") return true;
		if (name == "tan") return true;
		if (name == "acos") return true;
		if (name == "asin") return true;
		if (name == "atan") return true;
		if (name == "atan2") return true;
		if (name == "cosh") return true;
		if (name == "sinh") return true;
		if (name == "tanh") return true;
		if (name == "exp") return true;
		if (name == "frexp") return true;
		if (name == "ldexp") return true;
		if (name == "log") return true;
		if (name == "log10") return true;
		if (name == "modf") return true;
		if (name == "pow") return true;
		if (name == "sqrt") return true;
		if (name == "ceil") return true;
		if (name == "fabs") return true;
		if (name == "floor") return true;
		if (name == "fmod") return true;
		return false;
	}

	bool is_non_blocking_sync(const Function *f) {
		string name = f->getNameStr();
		if (name == "pthread_mutex_unlock") return true;
		if (name == "pthread_rwlock_unlock") return true;
		if (name == "pthread_getspecific") return true;
		return false;
	}

	bool is_string_func(const Function *f) {
		string name = f->getNameStr();
		if (name == "stpcpy") return true;
		if (name == "strtol") return true;
		if (name == "strlen") return true;
		return false;
	}

	bool is_memory_func(const Function *f) {
		string name = f->getNameStr();
		if (name == "memcmp") return true;
		if (name == "malloc") return true;
		return false;
	}

	bool is_in_white_list(const Function *f) {
		if (is_in_libm(f))
			return true;
		if (is_non_blocking_sync(f))
			return true;
		if (is_string_func(f))
			return true;
		if (is_memory_func(f))
			return true;
		string name = f->getNameStr();
		// if (name == "pthread_mutex_lock") return true;
		if (name == "drand48") return true;
		if (name == "__errno_location") return true;
		return false;
	}

	bool is_blocking_external_func(const Function *f) {
		string name = f->getNameStr();
		if (name == "connect") return true;
		if (name == "open") return true;
		if (name == "accept") return true;
		if (name == "fork") return true;
		if (name == "abort") return true;
		if (name == "select") return true;

		if (name == "fwrite") return true;
		if (name == "fprintf") return true;
		if (name == "write") return true;
		if (name == "printf") return true;
		if (name == "puts") return true;
		if (name == "fputc") return true;
		if (name == "fputs") return true;
		if (name == "sendto") return true;

		if (name == "read") return true;
		if (name == "recvfrom") return true;
		if (name == "pthread_join") return true;
		if (name == "pthread_mutex_lock") return true;
		if (name == "pthread_cond_wait") return true;
		if (name == "pthread_cond_timedwait") return true;

		return false;
	}

	struct InjectHook: public ModulePass {

		static char ID;

		const Type *int_ty, *void_ty, *long_ty, *snapshot_ty, *plong_ty;

		Constant *loom_is_func_patched;
		Constant *loom_setup;
		Constant *loom_dispatcher;
		Constant *loom_enter_thread_func, *loom_exit_thread_func;
		Constant *loom_before_call, *loom_after_call;
		Constant *loom_check;
		Constant *loom_enter_func;
		FunctionType *stub_fty, *stub_ret_fty, *dispatcher_fty, *init_fty, *switch_fty, *no_arg_fty;

		vector<Constant *> hook_functions;
		vector<Function *> thread_funcs;

		int n_checks;

		InjectHook(): ModulePass(&ID) {}

		virtual void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.addRequired<ObjectID>();
			ModulePass::getAnalysisUsage(AU);
		}

		void print_ins(Instruction *ins) {
			cerr << ins->getParent()->getParent()->getNameStr();
			cerr << "." << ins->getParent()->getNameStr();
			ins->dump();
		}

		void create_stub_functions(Module &M) {
			int_ty = IntegerType::get(getGlobalContext(), 32);
#if __WORDSIZE == 64
			long_ty = IntegerType::get(getGlobalContext(), 64);
#else
			long_ty = IntegerType::get(getGlobalContext(), 32);
#endif
			void_ty = Type::getVoidTy(getGlobalContext());
			snapshot_ty = ArrayType::get(long_ty, N_REGISTERS);
			plong_ty = PointerType::getUnqual(long_ty);

			vector<const Type *> stub_params;
			stub_params.push_back(int_ty);
			stub_fty = FunctionType::get(void_ty, stub_params, false);
			stub_ret_fty = FunctionType::get(int_ty, stub_params, false);
			switch_fty = FunctionType::get(int_ty, stub_params, false);
			init_fty = FunctionType::get(void_ty, stub_params, false);
			no_arg_fty = FunctionType::get(void_ty, vector<const Type *>(), false);
			vector<const Type *> dispatcher_params;
			dispatcher_params.push_back(int_ty);
#ifdef REGISTER_SNAPSHOT
			dispatcher_params.push_back(plong_ty);
#endif
			dispatcher_fty = FunctionType::get(void_ty, dispatcher_params, false);

			loom_setup = M.getOrInsertFunction(LOOM_SETUP, no_arg_fty);
			loom_dispatcher = M.getOrInsertFunction(LOOM_DISPATCHER, dispatcher_fty);
			loom_enter_thread_func = M.getOrInsertFunction(LOOM_ENTER_THREAD_FUNC, stub_fty);
			loom_exit_thread_func = M.getOrInsertFunction(LOOM_EXIT_THREAD_FUNC, stub_fty);
			loom_before_call = M.getOrInsertFunction(LOOM_BEFORE_CALL, stub_ret_fty);
			loom_after_call = M.getOrInsertFunction(LOOM_AFTER_CALL, stub_fty);
			loom_is_func_patched = M.getFunction(LOOM_IS_FUNC_PATCHED);
			loom_check = M.getOrInsertFunction(LOOM_CHECK, stub_fty);
			loom_enter_func = M.getOrInsertFunction(LOOM_ENTER_FUNC, stub_fty);

#if 0
			dyn_cast<Function>(loom_setup)->setDoesNotThrow();
			dyn_cast<Function>(loom_dispatcher)->setDoesNotThrow();
			dyn_cast<Function>(loom_enter_thread_func)->setDoesNotThrow();
			dyn_cast<Function>(loom_exit_thread_func)->setDoesNotThrow();
			dyn_cast<Function>(loom_before_call)->setDoesNotThrow();
			dyn_cast<Function>(loom_after_call)->setDoesNotThrow();
			dyn_cast<Function>(loom_is_func_patched)->setDoesNotThrow();
			dyn_cast<Function>(loom_check)->setDoesNotThrow();
			dyn_cast<Function>(loom_enter_func)->setDoesNotThrow();
#endif

			hook_functions.clear();
			hook_functions.push_back(loom_setup);
			hook_functions.push_back(loom_dispatcher);
			hook_functions.push_back(loom_enter_thread_func);
			hook_functions.push_back(loom_exit_thread_func);
			hook_functions.push_back(loom_before_call);
			hook_functions.push_back(loom_after_call);
			if (loom_is_func_patched)
				hook_functions.push_back(loom_is_func_patched);
			hook_functions.push_back(loom_check);
			hook_functions.push_back(loom_enter_func);
		}

		bool is_our_hook_function(Constant *f) {
			return find(hook_functions.begin(), hook_functions.end(), f) != hook_functions.end();
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

#if 0
			/* Count micro basic blocks. */
			MicroBasicBlockBuilder &MBB = getAnalysis<MicroBasicBlockBuilder>();
			int n_mbbs = 0;
			forallbb(M, bi) {
				for (mbb_iterator mi = MBB.begin(bi); mi != MBB.end(bi); mi++) {
					n_mbbs++;
				}
			}
			fprintf(stderr, "# of mbbs = %d\n", n_mbbs);
#endif
			/* Count instructions. */
			int n_insts = 0;
			forallbb(M, bi)
				n_insts += bi->size();
			cerr << "# of instructions = " << n_insts << endl;
		}

		void insert_loom_check(Function *fi) {
#if 0
			// Add loom_check on function entries
			CallInst::Create(loom_check, ConstantInt::get(int_ty, n_checks), "", fi->getEntryBlock().getFirstNonPHI());
			n_checks++;
#endif
			// Add loom_check on back edges
			for (Function::iterator bi = fi->begin(); bi != fi->end(); ++bi) {
				if (bi->getNameStr().compare(0, 9, "backedge_") == 0 ||
						bi->getNameStr().compare(0, 13, "new_backedge_") == 0) {
					BasicBlock::iterator ii = bi->getFirstNonPHI();
					CallInst::Create(loom_check, ConstantInt::get(int_ty, n_checks), "", ii);
					n_checks++;
				}
			}
		}

		void insert_loom_enter_exit_thread(Function *fi) {
			/* Add loom_exit_thread_func before pthread_exit. */
			for (Function::iterator bi = fi->begin(); bi != fi->end(); ++bi) {
				for (BasicBlock::iterator ii = bi->begin(); ii != bi->end(); ++ii) {
					if (is_call(ii) && !is_intrinsic_call(ii)) {
						CallSite cs(ii);
						if (cs.doesNotReturn()) {
							Function *callee = cs.getCalledFunction();
							assert(callee);
							if (callee->isDeclaration() && callee->getNameStr() == "pthread_exit")
								CallInst::Create(loom_exit_thread_func, ConstantInt::get(int_ty, -1), "", ii);
							break;
						}
					}
				}
			}
			/* If it is a thread function, add loom_enter_thread_func at the entry, 
			 * and loom_exit_thread_func at the function exits. */
			if (find(thread_funcs.begin(), thread_funcs.end(), fi) != thread_funcs.end()) {
				CallInst::Create(loom_enter_thread_func, ConstantInt::get(int_ty, -1), "", fi->getEntryBlock().getFirstNonPHI());
				for (Function::iterator bi = fi->begin(); bi != fi->end(); ++bi) {
					if (succ_begin(bi) == succ_end(bi)) {
						TerminatorInst *ti = bi->getTerminator();
						assert(ti);
						CallInst::Create(loom_exit_thread_func, ConstantInt::get(int_ty, -1), "", ti);
					}
				}
			}
		}

		void insert_before_after_call(Function *fi) {
			// Add loom_before/after_call around each blocking external function call
			for (Function::iterator bi = fi->begin(); bi != fi->end(); ++bi) {
				for (BasicBlock::iterator ii = bi->begin(); ii != bi->end(); ++ii) {
					if (is_call(ii) && !is_intrinsic_call(ii)) {
						CallSite cs(ii);
						Function *callee = cs.getCalledFunction();
						/* Skip function pointers. Assume they are our functions or non-blocking external functions. */
						if (!callee)
							continue;
						assert(callee);
						/* Skip non-external functions */
						if (!callee->isDeclaration())
							continue;
						/* Skip our own functions */
						if (is_our_hook_function(callee))
							continue;
						/* Skip functions that do not return */
						if (callee->doesNotReturn())
							continue;
						/* Skip function fork(). Handled by registering pthread_atfork */
						if (callee->getNameStr() == "fork")
							continue;
						/*
						 * TODO: handle InvokeInst
						 * Add loom_after_call at the entry of each basic block
						 * ? multiple loom_after_call at the entry of the exception block
						 */
						if (isa<InvokeInst>(ii))
							continue;
#if 1
						/* OPT 4: Skip functions in the white list */
						if (is_in_white_list(callee))
							continue;
#endif
						/* Skip external function calls that are already instrumented */
						BasicBlock::iterator next = ii; ++next;
						if (is_call(next)) {
							CallSite cs(next);
							if (cs.getCalledFunction() == loom_after_call)
								continue;
						}
						Value *ret = CallInst::Create(loom_before_call, ConstantInt::get(int_ty, n_checks), "", ii);
						Instruction *after_call = dyn_cast<Instruction>(
								CallInst::Create(loom_after_call, ConstantInt::get(int_ty, n_checks), "", next));
						Value *skip = new ICmpInst(after_call, ICmpInst::ICMP_NE, ret, ConstantInt::get(int_ty, 0));
						n_checks++;
						BasicBlock *ext_bb = bi->splitBasicBlock(after_call);
						ext_bb->setName("after_call_" + ext_bb->getNameStr());
						BasicBlock *rest_bb = ext_bb->splitBasicBlock(next);
						/* 
						 * Remember to append .loom to the names of basic blocks on slow paths;
						 * otherwise insert_paddings_on_slow_paths will skip such basic blocks.
						 */
						rest_bb->setName("after_extern_func_" + rest_bb->getNameStr());
						if (bi->getNameStr().find(SUFFIX) != string::npos)
							rest_bb->setName(rest_bb->getNameStr() + SUFFIX);

						TerminatorInst *ti = bi->getTerminator();
						assert(ti);
						ti->eraseFromParent();
						// BranchInst::Create(ext_bb, bi);
						BranchInst::Create(rest_bb, ext_bb, skip, bi);

						break;
					}
				}
			}
		}

		void find_thread_functions(Module &M) {
			/* Which functions are used as function pointers? */
			set<Function *> fps;
			forallinst(M, ii) {
				if (!is_call(ii)) {
					unsigned n = ii->getNumOperands();
					for (unsigned i = 0; i < n; ++i) {
						Value *v = ii->getOperand(i);
						if (isa<Function>(v)) {
							Function *f = dyn_cast<Function>(v);
							if (!f->isDeclaration())
								fps.insert(f);
						}
					}
				} else if (!is_intrinsic_call(ii)) {
					CallSite cs(ii);
					for (CallSite::arg_iterator ai = cs.arg_begin(); ai != cs.arg_end(); ++ai) {
						Value *v = *ai;
						if (isa<Function>(v)) {
							Function *f = dyn_cast<Function>(v);
							if (!f->isDeclaration())
								fps.insert(f);
						}
					}
				}
			}
			cerr << "# of functions used as function pointers = " << fps.size() << endl;
			for (set<Function *>::iterator it = fps.begin(); it != fps.end(); ++it)
				cerr << (*it)->getNameStr() << endl;

#if 0
			/* Specially handle the function pointer h_func in mysql server. */
			Function *mysql_handle_slave_io = NULL, *mysql_handle_slave_sql = NULL;
			forallfunc(M, fi) {
				if (fi->getName().find("handle_slave_io") != string::npos) {
					mysql_handle_slave_io = fi;
				}
				if (fi->getName().find("handle_slave_sql") != string::npos) {
					mysql_handle_slave_sql = fi;
				}
			}
			// Add the two thread functions pointed by the h_func function pointer. 
			if (mysql_handle_slave_sql) {
				thread_funcs.push_back(mysql_handle_slave_sql);
			}
			if (mysql_handle_slave_io) {
				thread_funcs.push_back(mysql_handle_slave_io);
			}
#endif
			forallinst(M, ii) {
				if (is_call(ii) && !is_intrinsic_call(ii)) {
					CallSite cs(ii);
					Function *callee = cs.getCalledFunction();
					if (callee && (callee->getName() == "pthread_create")) {
						CallSite::arg_iterator ai;
						// Search for functions or function pointers
						for (ai = cs.arg_begin(); ai != cs.arg_end(); ai++) {
							if (isa<Function>(*ai)) {
								// Real functions
								Function *target = dyn_cast<Function>(*ai);
								thread_funcs.push_back(target);
							} else {
								Value *v = *ai;
								if (v->getType()->isPointerTy()) {
									const Type *t = dyn_cast<PointerType>(v->getType())->getElementType();
									if (t->isFunctionTy()) {
										/* Function pointers */
										for (set<Function *>::iterator it = fps.begin(); it != fps.end(); ++it) {
											Function *fi = *it;
											if (fi->isDeclaration())
												continue;
											if (fi->getFunctionType() == t)
												thread_funcs.push_back(fi);
										}
									}
								}
							}
						}
						if (ai != cs.arg_end()) {
							Function *target = dyn_cast<Function>(*ai);
							if (target && !target->isDeclaration())
								thread_funcs.push_back(target);
						}
					}
				}
			}
			sort(thread_funcs.begin(), thread_funcs.end());
			thread_funcs.resize(unique(thread_funcs.begin(), thread_funcs.end()) - thread_funcs.begin());

			cerr << "Thread functions:\n";
			for (size_t i = 0; i < thread_funcs.size(); ++i)
				cerr << thread_funcs[i]->getNameStr() << endl;
		}

		void insert_loom_setup(Module &M) {
			/* Instrument the program with loom_setup in the main function. */
			forallfunc(M, fi) {
				if (fi->getName() != "main")
					continue;
				Instruction *pos = fi->getEntryBlock().getFirstNonPHI();
				CallInst::Create(loom_setup, "", pos);
			}
		}

		void insert_paddings_on_slow_paths(Module &M) {

			for (Module::iterator fi = M.begin(); fi != M.end(); ++fi) {
				// Skip empty functions because they do not have a function body. 
				if (fi->empty())
					continue;


#ifdef REGISTER_SNAPSHOT
				// TODO: support X86_64
				Instruction *entry_inst = fi->getEntryBlock().getFirstNonPHI();
				Value *snapshot = new AllocaInst(snapshot_ty, "", entry_inst);
				FunctionType *snapshot_func_ty = FunctionType::get(void_ty, vector<const Type *>(1, plong_ty), false);
				vector<InlineAsm *> copy_registers;
				copy_registers.push_back(InlineAsm::get(snapshot_func_ty, "movl %eax, $0", "=*m", false));
				copy_registers.push_back(InlineAsm::get(snapshot_func_ty, "movl %ebx, $0", "=*m", false));
				copy_registers.push_back(InlineAsm::get(snapshot_func_ty, "movl %ecx, $0", "=*m", false));
				copy_registers.push_back(InlineAsm::get(snapshot_func_ty, "movl %ebp, $0", "=*m", false));
				assert(copy_registers.size() == N_REGISTERS);
#endif

				for (Function::iterator bi = fi->begin(); bi != fi->end(); ++bi) {
					string name = bi->getNameStr();
					if (name.compare(0, 9, "backedge_") == 0)
						continue;
					if (name.compare(0, 13, "new_backedge_") == 0)
						continue;
					if (name.compare(0, 7, "__entry") == 0)
						continue;
					/* OPT 1: Modify here if you want to inject loom_dispatcher to fast path as well */
#ifdef OPT1
					if (name.find(SUFFIX) != string::npos) {
#else
					if (true) {
#endif // OPT1
						BasicBlock::iterator ii = bi->getFirstNonPHI();
						for (; ii != bi->end(); ++ii) {
							if (is_intrinsic_call(ii))
								continue;
							if (is_call(ii)) {
								CallSite cs(ii);
								Function *callee = cs.getCalledFunction();
								// Do not add loom_dispatcher before our hook functions since it is unnecessary.
								if (is_our_hook_function(callee))
									continue;
							}
#ifdef REGISTER_SNAPSHOT
							// Take a snapshot
							for (int i = 0; i < N_REGISTERS; i++) {
								vector<Value *> indices;
								indices.push_back(ConstantInt::get(int_ty, 0));
								indices.push_back(ConstantInt::get(int_ty, i));
								Value *register_pos = GetElementPtrInst::CreateInBounds(snapshot, indices.begin(), indices.end(), "", ii);
								CallInst::Create(copy_registers[i], register_pos, "", ii);
							}
#endif
							// Use 0 as a temporary argument
							vector<Value *> args;
							args.push_back(ConstantInt::get(int_ty, 0));
#ifdef REGISTER_SNAPSHOT
							vector<Value *> indices_first(2, ConstantInt::get(int_ty, 0));
							Value *pos_first = GetElementPtrInst::CreateInBounds(snapshot, indices_first.begin(), indices_first.end(), "", ii); 
							args.push_back(pos_first);
#endif
							CallInst::Create(loom_dispatcher, args.begin(), args.end(), "", ii);
							if (is_call(ii)) {
								CallSite cs(ii);
								Function *callee = cs.getCalledFunction();
								if (callee && callee->doesNotReturn())
									break;
							}
						}
					}
				}
			}

			ObjectID &IDM = getAnalysis<ObjectID>();
			IDM.runOnModule(M);
			/*
			 * Set the argument value as the instruction ID
			 * We don't do it in the previous phase because that phase inserts instruction,
			 * thus changes the instruction ID mapping
			 */
			forallbb(M, bi) {
				BasicBlock::iterator ii = bi->getFirstNonPHI();
				for (; ii != bi->end(); ++ii) {
					if (is_call(ii)) {
						CallSite cs(ii);
						Function *callee = cs.getCalledFunction();
						if (callee && callee == loom_dispatcher) {
							int ins_id = IDM.getInstructionID(ii);
							for (unsigned i = 0; i < ii->getNumOperands(); ++i) {
								if (ii->getOperand(i)->getType() == int_ty)
									ii->setOperand(i, ConstantInt::get(int_ty, ins_id));
							}
						}
					}
				}
			}
		}

		virtual bool runOnModule(Module &M) {

			create_stub_functions(M);
			statistic(M);
			find_thread_functions(M);

			n_checks = 0;
			forallfunc(M, fi) {
				if (fi->isDeclaration())
					continue;
				insert_loom_check(fi);
				insert_loom_enter_exit_thread(fi);
				insert_before_after_call(fi);
			}
			insert_loom_setup(M);
			insert_paddings_on_slow_paths(M);

			cerr << "# of checks = " << n_checks << endl;

			return true;
		}
	};

	char InjectHook::ID = 0;
}

namespace {
	static RegisterPass<defens::InjectHook>
		X("inject-hook", "Inject hook functions");
}
