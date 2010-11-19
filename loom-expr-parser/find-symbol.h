#ifndef __FIND_SYMBOL_H
#define __FIND_SYMBOL_H

#include <libdwarf.h>
#include <dwarf.h>

#ifdef __cplusplus
extern "C" {
#endif

void shrink_range(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Addr *lowpc, Dwarf_Addr *highpc);

/* lowpc <= pc < highpc */
int find_symbol_in_die(Dwarf_Debug dbg, Dwarf_Die die,
		Dwarf_Addr lowpc, Dwarf_Addr highpc,
		Dwarf_Addr pc, const char *sym_name,
		Dwarf_Off *offset, Dwarf_Off *innermost_range);

int find_symbol_in_die_and_its_children(Dwarf_Debug dbg, Dwarf_Die die,
		Dwarf_Addr lowpc, Dwarf_Addr highpc,
		Dwarf_Addr pc, const char *sym_name,
		Dwarf_Off *offset, Dwarf_Off *innermost_range);

/* lowpc <= pc < highpc */
int find_symbol_in_children(Dwarf_Debug dbg, Dwarf_Die die,
		Dwarf_Addr lowpc, Dwarf_Addr highpc,
		Dwarf_Addr pc, const char *sym_name,
		Dwarf_Off *offset, Dwarf_Off *innermost_range);

int find_symbol_in_cu(Dwarf_Debug dbg, Dwarf_Die die,
		Dwarf_Addr pc, const char *sym_name,
		Dwarf_Off *offset, Dwarf_Off *innermost_range);

/* Return boolean */
int find_symbol_in_module(Dwarf_Debug dbg,
		Dwarf_Addr pc, const char *sym_name,
		Dwarf_Die *sym_die, Dwarf_Addr *cu_base_pc);

int find_location(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Addr cu_pc, Dwarf_Loc *loc);

#ifdef __cplusplus
}
#endif

#endif

