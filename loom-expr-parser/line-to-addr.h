#ifndef __LINE_TO_ADDR_H
#define __LINE_TO_ADDR_H

#include <dwarf.h>
#include <libdwarf.h>

#ifdef __cplusplus
extern "C" {
#endif

int line_to_addr_in_module(Dwarf_Debug dbg, const char *src, Dwarf_Unsigned line, Dwarf_Addr *addr);

#ifdef __cplusplus
}
#endif

#endif

