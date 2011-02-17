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
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"

#include <set>
#include <map>
#include <fstream>
#include <ext/hash_map>

#include "../../llvm/idm/util.h"
#include "src-locator.h"

using namespace llvm;
using namespace std;
using namespace __gnu_cxx;

namespace defens {

	const static string SUFFIX = ".loom";

	void SourceLocator::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		ModulePass::getAnalysisUsage(AU);
	}

	bool SourceLocator::runOnModule(Module &M) {
		loc_to_ins.clear();
		ins_to_loc.clear();
		int num_dbgs = 0, num_insts = 0;
		forallinst(M, ii) {
			MDNode *dbg = ii->getMetadata("dbg");
			if (dbg) {
				DILocation loc(dbg);
				unsigned line_no = loc.getLineNumber();
				string file_name = loc.getFilename();
				loc_to_ins[make_pair(file_name, line_no)].push_back(ii);
				ins_to_loc[ii] = make_pair(file_name, line_no);
				num_dbgs++;
			}
			num_insts++;
		}
		cerr << "# of instructions with location info = " << num_dbgs << endl;
		cerr << "# of instructions = " << num_insts << endl;
		return false;
	}

	Instruction *SourceLocator::get_first_instruction(const string &file_name, unsigned line_no) const {
		MapLocToIns::const_iterator it = loc_to_ins.find(make_pair(file_name, line_no));
		if (it == loc_to_ins.end())
			return NULL;
		if (it->second.empty())
			return NULL;
		return it->second.front();
	}

	Instruction *SourceLocator::get_first_cloned_instruction(const string &file_name, unsigned line_no) const {
		MapLocToIns::const_iterator it = loc_to_ins.find(make_pair(file_name, line_no));
		if (it == loc_to_ins.end())
			return NULL;
		if (it->second.empty())
			return NULL;
		for (size_t i = 0; i < it->second.size(); ++i) {
			if (is_cloned_bb(it->second[i]->getParent()))
				return it->second[i];
		}
		return NULL;
	}

	Instruction *SourceLocator::get_last_instruction(const string &file_name, unsigned line_no) const {
		MapLocToIns::const_iterator it = loc_to_ins.find(make_pair(file_name, line_no));
		if (it == loc_to_ins.end())
			return NULL;
		if (it->second.empty())
			return NULL;
		return it->second.back();
	}

	Instruction *SourceLocator::get_last_cloned_instruction(const string &file_name, unsigned line_no) const {
		MapLocToIns::const_iterator it = loc_to_ins.find(make_pair(file_name, line_no));
		if (it == loc_to_ins.end())
			return NULL;
		if (it->second.empty())
			return NULL;
		for (int i = (int)it->second.size() - 1; i >= 0; i--) {
			if (is_cloned_bb(it->second[i]->getParent()))
				return it->second[i];
		}
		return NULL;
	}

	bool SourceLocator::is_cloned_bb(const BasicBlock *bb) const {
		const string &name = bb->getNameStr();
		return name.length() >= SUFFIX.length() && name.substr(name.length() - SUFFIX.length()) == SUFFIX;
	}

	bool SourceLocator::get_location(const Instruction *ins, string &file_name, unsigned &line_no) const {
		MapInsToLoc::const_iterator it = ins_to_loc.find(ins);
		if (it == ins_to_loc.end())
			return false;
		file_name = it->second.first;
		line_no = it->second.second;
		return true;
	}

	void SourceLocator::print(raw_ostream &O, const Module *M) const {
		MapLocToIns::const_iterator it = loc_to_ins.begin();
		for (; it != loc_to_ins.end(); ++it) {
			O << it->first.first << ':' << it->first.second << ' ' << it->second.size() << "\n";
		}
	}

	char SourceLocator::ID = 0;
}

namespace {
	static RegisterPass<defens::SourceLocator>
		X("src-locator", "From line number to instruction", false, true);
}

