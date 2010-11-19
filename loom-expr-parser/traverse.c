#include "traverse.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

/* <depth> is die's depth */
void traverse_children(Dwarf_Debug dbg, Dwarf_Die die, DIE_HANDLER handler, int depth) {
	Dwarf_Die child, sib;
	int ret;
	Dwarf_Error err;

	// Get the first child.
	ret = dwarf_child(die, &child, &err);
	if (ret == DW_DLV_NO_ENTRY)
		return;
	assert(ret == DW_DLV_OK);
	traverse_die_and_its_children(dbg, child, handler, depth + 1);
	// Get all other children. 
	while (dwarf_siblingof(dbg, child, &sib, &err) == DW_DLV_OK) {
		dwarf_dealloc(dbg, child, DW_DLA_DIE);
		traverse_die_and_its_children(dbg, sib, handler, depth + 1);
		child = sib;
	}
	dwarf_dealloc(dbg, child, DW_DLA_DIE);
}

void traverse_die_and_its_children(Dwarf_Debug dbg, Dwarf_Die die, DIE_HANDLER handler, int depth) {
	handler(dbg, die, depth);
	traverse_children(dbg, die, handler, depth);
}

void traverse_cu(Dwarf_Debug dbg, Dwarf_Die cu, DIE_HANDLER handler) {
	traverse_die_and_its_children(dbg, cu, handler, 0);
}

void traverse_module(Dwarf_Debug dbg, DIE_HANDLER handler) {
	Dwarf_Off next_cu_offset;
	int ret;
	Dwarf_Error err;
	while ((ret = dwarf_next_cu_header(dbg, NULL, NULL, NULL, NULL, &next_cu_offset, &err)) == DW_DLV_OK) {
		Dwarf_Die last_cu = NULL, current_cu;
		while ((ret = dwarf_siblingof(dbg, last_cu, &current_cu, &err)) == DW_DLV_OK) {
			dwarf_dealloc(dbg, last_cu, DW_DLA_DIE);
			traverse_cu(dbg, current_cu, handler);
			last_cu = current_cu;
		}
		dwarf_dealloc(dbg, last_cu, DW_DLA_DIE);
	}
}

