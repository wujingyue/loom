#include "line-to-addr.h"
#include <assert.h>
#include <string.h>

int suffix_compare(const char *a, const char *b) {
	int i = (int)strlen(a) - 1, j = (int)strlen(b) - 1;
	while (i >= 0 && j >= 0) {
		if (a[i] != b[j])
			return (int)a[i] - (int)b[j];
		i--;
		j--;
	}
	return 0;
}

int line_to_addr_in_cu(Dwarf_Debug dbg, Dwarf_Die cu, const char *s, Dwarf_Unsigned l, Dwarf_Addr *addr) {
	Dwarf_Error err;
	int ret;

	Dwarf_Signed n_srcs;
	char **srcs;
	ret = dwarf_srcfiles(cu, &srcs, &n_srcs, &err);
	if (ret == DW_DLV_NO_ENTRY)
		return DW_DLV_NO_ENTRY;
	assert(ret == DW_DLV_OK);

	int found_src = 0;
	Dwarf_Signed i;
	for (i = 0; i < n_srcs; i++) {
		if (suffix_compare(srcs[i], s) == 0)
			found_src = 1;
		dwarf_dealloc(dbg, srcs[i], DW_DLA_STRING);
	}
	dwarf_dealloc(dbg, srcs, DW_DLA_LIST);

	if (!found_src)
		return DW_DLV_NO_ENTRY;

	Dwarf_Signed n_lines;
	Dwarf_Line *lines;
	ret = dwarf_srclines(cu, &lines, &n_lines, &err);
	if (ret == DW_DLV_NO_ENTRY)
		return DW_DLV_NO_ENTRY;
	assert(ret == DW_DLV_OK);

	int found = 0;
	for (i = 0; i < n_lines; i++) {
		char *src;
		Dwarf_Unsigned line;

		ret = dwarf_linesrc(lines[i], &src, &err);
		if (ret == DW_DLV_NO_ENTRY)
			continue;
		assert(ret == DW_DLV_OK);
		int cmp = suffix_compare(src, s);
		dwarf_dealloc(dbg, src, DW_DLA_STRING);
		if (cmp != 0)
			continue;

		ret = dwarf_lineno(lines[i], &line, &err);
		if (ret == DW_DLV_NO_ENTRY)
			continue;
		assert(ret == DW_DLV_OK);
		if (line != l)
			continue;

		ret = dwarf_lineaddr(lines[i], addr, &err);
		if (ret == DW_DLV_OK) {
			found = 1;
			break;
		}
	}

	for (i = 0; i < n_lines; i++)
		dwarf_dealloc(dbg, lines[i], DW_DLA_LINE);
	dwarf_dealloc(dbg, lines, DW_DLA_LIST);

	return (found ? DW_DLV_OK : DW_DLV_NO_ENTRY);
}

int line_to_addr_in_module(Dwarf_Debug dbg, const char *src, Dwarf_Unsigned line, Dwarf_Addr *addr) {
	Dwarf_Off next_cu_offset;
	Dwarf_Error err;
	int ret;

	int found = 0;
	while ((ret = dwarf_next_cu_header(dbg, NULL, NULL, NULL, NULL, &next_cu_offset, &err)) == DW_DLV_OK) {
		Dwarf_Die last_cu = NULL, current_cu;
		while ((ret = dwarf_siblingof(dbg, last_cu, &current_cu, &err)) == DW_DLV_OK) {
			dwarf_dealloc(dbg, last_cu, DW_DLA_DIE);
			ret = line_to_addr_in_cu(dbg, current_cu, src, line, addr);
			if (ret == DW_DLV_OK)
				found = 1;
			last_cu = current_cu;
		}
		dwarf_dealloc(dbg, last_cu, DW_DLA_DIE);
	}
	return (found ? DW_DLV_OK : DW_DLV_NO_ENTRY);
}

