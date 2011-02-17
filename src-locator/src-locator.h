#ifndef __DEFENS_SOURCE_LOCATOR_H
#define __DEFENS_SOURCE_LOCATOR_H

#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/Constants.h"
#include "llvm/BasicBlock.h"
#include "llvm/Pass.h"
#include "llvm/Support/CFG.h"
#include "llvm/Analysis/DebugInfo.h"

#include <set>
#include <map>
#include <fstream>
#include <ext/hash_map>

#include "../../llvm/idm/util.h"

using namespace llvm;
using namespace std;
using namespace __gnu_cxx;

namespace __gnu_cxx {
	template<> struct hash<pair<string, unsigned> > {
		size_t operator()(const pair<string, unsigned> &a) const {
			return hash<const char *>()(a.first.c_str()) + a.second;
		}
	};
}

namespace defens {

	struct SourceLocator: public ModulePass {

		static char ID;

		SourceLocator(): ModulePass(&ID) {}

		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);
		void print(raw_ostream &O, const Module *M) const;

		Instruction *get_first_instruction(const string &file_name, unsigned line_no) const;
		Instruction *get_last_instruction(const string &file_name, unsigned line_no) const;
		Instruction *get_first_cloned_instruction(const string &file_name, unsigned line_no) const;
		Instruction *get_last_cloned_instruction(const string &file_name, unsigned line_no) const;
		bool get_location(const Instruction *ins, string &file_name, unsigned &line_no) const;

	private:
		typedef hash_map<pair<string, unsigned>, vector<Instruction *> > MapLocToIns;
		typedef DenseMap<const Instruction *, pair<string, unsigned> > MapInsToLoc;

		bool is_cloned_bb(const BasicBlock *bb) const;

		MapLocToIns loc_to_ins;
		MapInsToLoc ins_to_loc;
	};
}

#endif
