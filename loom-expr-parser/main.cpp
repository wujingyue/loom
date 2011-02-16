#include <iostream>
#include <cassert>
#include <sstream>
#include <set>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <libdwarf.h>
#include <dwarf.h>

#include <sys/stat.h>
#include <fcntl.h>

#include "traverse.h"
#include "find-symbol.h"
#include "parse-exp.h"
#include "line-to-addr.h"
#include "loom-exp.h"

using namespace std;

void print_indent(int cnt) {
	cout << string(cnt, ' ');
}

void print_die(Dwarf_Debug dbg, Dwarf_Die die, int depth) {
	int ret;
	char *name;
	Dwarf_Error err;

	print_indent(depth);

	// Name
	ret = dwarf_diename(die, &name, &err);
	if (ret == DW_DLV_NO_ENTRY)
		cout << "<no name>" << endl;
	else {
		assert(ret == DW_DLV_OK);
		cout << name << endl;
		dwarf_dealloc(dbg, name, DW_DLA_STRING);
	}

	// Tag
	Dwarf_Half tag;
	ret = dwarf_tag(die, &tag, &err);
	assert(ret == DW_DLV_OK);
	if (tag == DW_TAG_subprogram) {
		Dwarf_Addr low_pc, high_pc;
		ret = dwarf_lowpc(die, &low_pc, &err);
		assert(ret == DW_DLV_OK || ret == DW_DLV_NO_ENTRY);
		ret = dwarf_highpc(die, &high_pc, &err);
		assert(ret == DW_DLV_OK || ret == DW_DLV_NO_ENTRY);
		if (ret == DW_DLV_OK) {
			print_indent(depth);
			printf("[%llx %llx)\n", low_pc, high_pc);
		}
	}
}

set<int> used_ops;

void check_location(Dwarf_Debug dbg, Dwarf_Die die, int depth) {
	// Found the symbol.
	Dwarf_Half tag;
	int ret;	
	Dwarf_Error err;

	// print_die(dbg, die, depth);

	ret = dwarf_tag(die, &tag, &err);	
	assert(ret == DW_DLV_OK);
	// Only check variables and formal parameters.
	if (tag != DW_TAG_variable && tag != DW_TAG_formal_parameter)
		return;

	char *name;
	ret = dwarf_diename(die, &name, &err);
	// Skip variables or parameters that do not have names. 
	if (ret == DW_DLV_NO_ENTRY)
		return;
	assert(ret == DW_DLV_OK);
	dwarf_dealloc(dbg, name, DW_DLA_STRING);

	Dwarf_Attribute attr;
	ret = dwarf_attr(die, DW_AT_declaration, &attr, &err);
	if (ret == DW_DLV_OK) {
		Dwarf_Bool is_decl;
		ret = dwarf_formflag(attr, &is_decl, &err);
		assert(ret == DW_DLV_OK);
		assert(is_decl);
		dwarf_dealloc(dbg, attr, DW_DLA_ATTR);
		return;
	}

	ret = dwarf_attr(die, DW_AT_location, &attr, &err);
	if (ret != DW_DLV_OK) {
		
		char *name;
		ret = dwarf_diename(die, &name, &err);
		assert(ret == DW_DLV_OK);
		dwarf_dealloc(dbg, name, DW_DLA_STRING);

		Dwarf_Off cu_offset;
		int ret = dwarf_die_CU_offset(die, &cu_offset, &err);
		assert(ret == DW_DLV_OK);
		
		// TODO: constant variables
		// fprintf(stderr, "[Warning] The symbol <%s> at cu_offset <%lld> does not have a DW_AT_location.\n", name, cu_offset);
		dwarf_dealloc(dbg, name, DW_DLA_STRING);
		return;
	}
	assert(ret == DW_DLV_OK);

	Dwarf_Locdesc **locs;
	Dwarf_Signed n_locs;
	ret = dwarf_loclist_n(attr, &locs, &n_locs, &err);
	assert(ret == DW_DLV_OK);

	for (Dwarf_Signed i = 0; i < n_locs; i++) {
		Dwarf_Half n_ops = locs[i]->ld_cents;
		if (n_ops != 1) {
			char *name;
			ret = dwarf_diename(die, &name, &err);
			assert(ret == DW_DLV_OK);
			dwarf_dealloc(dbg, name, DW_DLA_STRING);

			Dwarf_Off cu_offset;
			int ret = dwarf_die_CU_offset(die, &cu_offset, &err);
			assert(ret == DW_DLV_OK);

			fprintf(stderr, "[Warning] The symbol <%s> at cu_offset <%lld> has more than one ops.\n", name, cu_offset);
			dwarf_dealloc(dbg, name, DW_DLA_STRING);
			return;
		}
		assert(n_ops == 1);
		Dwarf_Loc op = locs[i]->ld_s[0];
		used_ops.insert(op.lr_atom);
	}

	for (Dwarf_Signed i = 0; i < n_locs; i++) {
		dwarf_dealloc(dbg, locs[i]->ld_s, DW_DLA_LOC_BLOCK);
		dwarf_dealloc(dbg, locs[i], DW_DLA_LOCDESC);
	}
	dwarf_dealloc(dbg, attr, DW_DLA_ATTR);
}

void print_usage_symbol(int argc, char *argv[]) {
	fprintf(stderr, "Usage: %s <executable> <pc|file:line> <symbol>\n", argv[0]);
}

void print_usage_exp(int argc, char *argv[]) {
	fprintf(stderr, "Usage: %s <executable> <pc|file:line> <expression>\n", argv[0]);
}

Dwarf_Addr hex_str_to_int(char *input) {
	Dwarf_Addr res;
	sscanf(input, "%llx", &res);
	return res;
}

Dwarf_Addr parse_loc(Dwarf_Debug dbg, char *str) {
	fprintf(stderr, "%s = ", str);
	Dwarf_Addr ans;
	if (strstr(str, ":") == NULL) {
		ans = hex_str_to_int(str);
	} else {
		char *src = strtok(str, ":");
		int line = atoi(strtok(NULL, ":"));
		int ret = line_to_addr_in_module(dbg, src, line, &ans);
		assert(ret == DW_DLV_OK && "The specified location does not exist.");
	}
	fprintf(stderr, "%llx\n", ans);
	return ans;
}

int test_symbol(int argc, char *argv[]) {
	if (argc < 4) {
		print_usage_exp(argc, argv);
		return -1;
	}

	int fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		perror("open");
		exit(-1);
	}

	Dwarf_Error err;
	Dwarf_Debug dbg;
	int ret;
	
	ret = dwarf_init(fd, DW_DLC_READ, NULL, NULL, &dbg, &err);
	assert(ret == DW_DLV_OK);

	// FDE
	Dwarf_Fde *fde_list;
	Dwarf_Cie *cie_list; // not used
	Dwarf_Signed fde_cnt;
	Dwarf_Signed cie_cnt; // not used
	ret = dwarf_get_fde_list(dbg, &cie_list, &cie_cnt, &fde_list, &fde_cnt, &err);
	assert(ret == DW_DLV_OK);

	Dwarf_Addr pc = parse_loc(dbg, argv[2]);
	Dwarf_Fde fde;
	ret = dwarf_get_fde_at_pc(fde_list, pc, &fde, NULL, NULL, &err);
	assert(ret == DW_DLV_OK || ret == DW_DLV_NO_ENTRY);
	if (ret == DW_DLV_OK) {
		Dwarf_Regtable reg_table;
		ret = dwarf_get_fde_info_for_all_regs(fde, pc, &reg_table, NULL, &err);
		assert(ret == DW_DLV_OK);
		cerr << "DW_REG_TABLE_SIZE = " << DW_REG_TABLE_SIZE << endl;
		for (int i = 0; i < DW_REG_TABLE_SIZE; i++) {
			assert(reg_table.rules[i].dw_value_type == DW_EXPR_OFFSET);
			cerr << i << ": ";
			if (reg_table.rules[i].dw_offset_relevant) {
				cerr << reg_table.rules[i].dw_regnum << " + " << hex << reg_table.rules[i].dw_offset << dec << endl;
			} else {
				cerr << "(" << reg_table.rules[i].dw_regnum << ")" << endl;
			}
		}
	} else {
		fprintf(stderr, "Cannot find the FDE at the PC %llx\n", pc);
	}

	dwarf_dealloc(dbg, cie_list, DW_DLA_LIST);
	dwarf_dealloc(dbg, fde_list, DW_DLA_LIST);

	// traverse_module(dbg, print_die);
	traverse_module(dbg, check_location);

	for (set<int>::iterator it = used_ops.begin(); it != used_ops.end(); ++it) {
		fprintf(stderr, "used_ops: %x\n", *it);
	}

	Dwarf_Die sym_die;
	Dwarf_Addr cu_base_pc;
	ret = find_symbol_in_module(dbg, pc, argv[3], &sym_die, &cu_base_pc);
	if (ret) {
		// Found the symbol.
		Dwarf_Loc loc;
		int found = find_location(dbg, sym_die, pc - cu_base_pc, &loc);
		assert(found && "Cannot find the location");
		fprintf(stderr, "lr_atom = %d\nlr_number = %llx\n", (int)loc.lr_atom, loc.lr_number);
		dwarf_dealloc(dbg, sym_die, DW_DLA_DIE);
	}

	ret = dwarf_finish(dbg, &err);
	assert(ret == DW_DLV_OK);
	return 0;
}

int test_exp(int argc, char *argv[]) {
	if (argc < 4) {
		print_usage_symbol(argc, argv);
		return -1;
	}

	int fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		perror("open");
		exit(-1);
	}

	Dwarf_Error err;
	Dwarf_Debug dbg;
	int ret;
	
	ret = dwarf_init(fd, DW_DLC_READ, NULL, NULL, &dbg, &err);
	assert(ret == DW_DLV_OK);

	exp_parser_init(argv[1]);
	fprintf(stderr, "exp = %s\n", argv[3]);

	Dwarf_Addr pc = parse_loc(dbg, argv[2]);
	struct expression *exp = parse_exp_at_pc(argv[3], pc);
	assert(exp);

	print_exp(exp);

	struct Loom_Expression *expout = construct_loom_expression(dbg, exp, pc);
	dump_loom_expression(expout);

	ret = dwarf_finish(dbg, &err);
	assert(ret == DW_DLV_OK);

	return 0;
}

int main(int argc, char *argv[]) {
	// return test_symbol(argc, argv);
	return test_exp(argc, argv);
}

