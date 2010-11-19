#include <libdwarf.h>
#include <dwarf.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include "find-symbol.h"

void shrink_range(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Addr *lowpc, Dwarf_Addr *highpc) {
	Dwarf_Half tag;
	Dwarf_Error err;
	int ret = dwarf_tag(die, &tag, &err);
	assert(ret == DW_DLV_OK);
	if (tag == DW_TAG_subprogram) {
		Dwarf_Addr low_pc, high_pc;
		ret = dwarf_lowpc(die, &low_pc, &err);
		assert(ret == DW_DLV_OK || ret == DW_DLV_NO_ENTRY);
		ret = dwarf_highpc(die, &high_pc, &err);
		assert(ret == DW_DLV_OK || ret == DW_DLV_NO_ENTRY);
		if (ret == DW_DLV_OK) {
			*lowpc = low_pc;
			*highpc = high_pc;
		}
	}
}

/* lowpc <= pc < highpc */
int find_symbol_in_die(Dwarf_Debug dbg, Dwarf_Die die,
		Dwarf_Addr lowpc, Dwarf_Addr highpc,
		Dwarf_Addr pc, const char *sym_name,
		Dwarf_Off *offset, Dwarf_Off *innermost_range) {
	Dwarf_Half tag;
	Dwarf_Error err;
	int ret = dwarf_tag(die, &tag, &err);
	assert(ret == DW_DLV_OK);

	char *name;
	int updated = 0;
	if (tag == DW_TAG_variable || tag == DW_TAG_formal_parameter) {
		ret = dwarf_diename(die, &name, &err);
		// Skip anonymous variables or parameters.
		if (ret == DW_DLV_NO_ENTRY)
			return 0;
		assert(ret == DW_DLV_OK);
		if (strcmp(name, sym_name) == 0 && highpc - lowpc <= *innermost_range) {
			ret = dwarf_dieoffset(die, offset, &err);
			assert(ret == DW_DLV_OK);
			*innermost_range = highpc - lowpc;
			updated = 1;
		}
		dwarf_dealloc(dbg, name, DW_DLA_STRING);
	}
	return updated;
}

/* lowpc <= pc < highpc */
int find_symbol_in_children(Dwarf_Debug dbg, Dwarf_Die die,
		Dwarf_Addr lowpc, Dwarf_Addr highpc,
		Dwarf_Addr pc, const char *sym_name,
		Dwarf_Off *offset, Dwarf_Off *innermost_range) {
	Dwarf_Die child, sib;
	int ret;
	Dwarf_Error err;

	// Get the first child.
	ret = dwarf_child(die, &child, &err);
	if (ret == DW_DLV_NO_ENTRY)
		return 0;
	assert(ret == DW_DLV_OK);

	int found = 0;
	/* Note that there is no logic short-circuit for |= */
	found |= find_symbol_in_die_and_its_children(dbg, child, lowpc, highpc, pc, sym_name, offset, innermost_range);
	// Get all other children. 
	while (dwarf_siblingof(dbg, child, &sib, &err) == DW_DLV_OK) {
		dwarf_dealloc(dbg, child, DW_DLA_DIE);
		found |= find_symbol_in_die_and_its_children(dbg, sib, lowpc, highpc, pc, sym_name, offset, innermost_range);
		child = sib;
	}
	dwarf_dealloc(dbg, child, DW_DLA_DIE);
	return found;
}

int find_symbol_in_die_and_its_children(Dwarf_Debug dbg, Dwarf_Die die,
		Dwarf_Addr lowpc, Dwarf_Addr highpc,
		Dwarf_Addr pc, const char *sym_name,
		Dwarf_Off *offset, Dwarf_Off *innermost_range) {
	shrink_range(dbg, die, &lowpc, &highpc);
	if (pc < lowpc || pc >= highpc)
		return 0;
	int found = 0;
	found |= find_symbol_in_die(dbg, die, lowpc, highpc, pc, sym_name, offset, innermost_range);
	found |= find_symbol_in_children(dbg, die, lowpc, highpc, pc, sym_name, offset, innermost_range);
	return found;
}

int find_symbol_in_cu(Dwarf_Debug dbg, Dwarf_Die die,
		Dwarf_Addr pc, const char *sym_name,
		Dwarf_Off *offset, Dwarf_Off *innermost_range) {
	return find_symbol_in_die_and_its_children(dbg, die, 0, -1ULL, pc, sym_name, offset, innermost_range);
}

void get_cu_base_pc(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Addr *base_pc) {
	int ret;
	Dwarf_Error err;
	Dwarf_Attribute attr;
	ret = dwarf_attr(die, DW_AT_entry_pc, &attr, &err);
	if (ret == DW_DLV_OK) {
		ret = dwarf_formaddr(attr, base_pc, &err);
		assert(ret == DW_DLV_OK);
	} else {
		assert(ret == DW_DLV_NO_ENTRY);
		ret = dwarf_lowpc(die, base_pc, &err);
		assert(ret == DW_DLV_OK);
	}
}

int find_symbol_in_module(Dwarf_Debug dbg, Dwarf_Addr pc, const char *sym_name, Dwarf_Die *sym_die, Dwarf_Addr *cu_base_pc) {
	Dwarf_Off next_cu_offset;
	Dwarf_Error err;
	Dwarf_Off sym_die_offset = (Dwarf_Off)(-1);
	Dwarf_Off innermost_range = -1ULL;
	int ret;

	while ((ret = dwarf_next_cu_header(dbg, NULL, NULL, NULL, NULL, &next_cu_offset, &err)) == DW_DLV_OK) {
		Dwarf_Die last_cu = NULL, current_cu;
		while ((ret = dwarf_siblingof(dbg, last_cu, &current_cu, &err)) == DW_DLV_OK) {
			dwarf_dealloc(dbg, last_cu, DW_DLA_DIE);
			if (find_symbol_in_cu(dbg, current_cu, pc, sym_name, &sym_die_offset, &innermost_range))
				get_cu_base_pc(dbg, current_cu, cu_base_pc);
			last_cu = current_cu;
		}
		dwarf_dealloc(dbg, last_cu, DW_DLA_DIE);
	}

	if (sym_die_offset == (Dwarf_Off)(-1))
		return 0;

	printf("symbol offset = %lld\n", sym_die_offset);
	ret = dwarf_offdie(dbg, sym_die_offset, sym_die, &err);
	assert(ret == DW_DLV_OK);

	Dwarf_Off sym_die_cu_offset;
	ret = dwarf_die_CU_offset(*sym_die, &sym_die_cu_offset, &err);
	assert(ret == DW_DLV_OK);
	printf("symbol CU offset = %lld\n", sym_die_cu_offset);

	return 1;
}

static void dump_symbol(Dwarf_Debug dbg, Dwarf_Die die) {
	int ret;
	Dwarf_Error err;

	char *name;
	ret = dwarf_diename(die, &name, &err);
	assert(ret == DW_DLV_OK);

	Dwarf_Off cu_offset;
	ret = dwarf_die_CU_offset(die, &cu_offset, &err);

	fprintf(stderr, "name = %s, cu offset = %lld\n", name, cu_offset);
	dwarf_dealloc(dbg, name, DW_DLA_STRING);

}

int find_location(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Addr cu_pc, Dwarf_Loc *loc) {
	int ret;
	Dwarf_Error err;

	dump_symbol(dbg, die);

	Dwarf_Attribute attr;
	ret = dwarf_attr(die, DW_AT_location, &attr, &err);
	assert(ret == DW_DLV_OK);

	Dwarf_Locdesc **locs;
	Dwarf_Signed n_locs;
	ret = dwarf_loclist_n(attr, &locs, &n_locs, &err);
	assert(ret == DW_DLV_OK);
	dwarf_dealloc(dbg, attr, DW_DLA_ATTR);

	// fprintf(stderr, "cu_pc = %llx\n", cu_pc);
	Dwarf_Signed i = 0;
	for (i = 0; i < n_locs; i++) {
		// fprintf(stderr, "[%llx %llx]\n", locs[i]->ld_lopc, locs[i]->ld_hipc);
		if (locs[i]->ld_lopc <= cu_pc && cu_pc < locs[i]->ld_hipc)
			break;
	}
	if (i >= n_locs)
		return 0;
	assert(locs[i]->ld_cents == 1 && "Locdesc has multiple operations");
	Dwarf_Loc op = locs[i]->ld_s[0];

	for (i = 0; i < n_locs; i++) {
		dwarf_dealloc(dbg, locs[i]->ld_s, DW_DLA_LOC_BLOCK);
		dwarf_dealloc(dbg, locs[i], DW_DLA_LOCDESC);
	}
	dwarf_dealloc(dbg, locs, DW_DLA_LIST);

	*loc = op;
	return 1;
}


