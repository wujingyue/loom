#ifndef __TRAVERSE_H
#define __TRAVERSE_H

#include <libdwarf.h>
#include <dwarf.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*DIE_HANDLER)(Dwarf_Debug, Dwarf_Die, int);

void traverse_module(Dwarf_Debug dbg, DIE_HANDLER handler);
void traverse_cu(Dwarf_Debug dbg, Dwarf_Die cu, DIE_HANDLER handler);
void traverse_die_and_its_children(Dwarf_Debug dbg, Dwarf_Die die, DIE_HANDLER handler, int depth);
void traverse_children(Dwarf_Debug dbg, Dwarf_Die die, DIE_HANDLER handler, int depth);

#ifdef __cplusplus
}
#endif

#endif

