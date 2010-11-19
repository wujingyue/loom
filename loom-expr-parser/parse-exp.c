#include <stdlib.h>
#include "parse-exp.h"
#include "defs.h"
#include "expression.h"
#include "block.h"
#include "top.h"
#include "symfile.h"
#include "exec.h"
#include "inferior.h"
#include "exceptions.h"
#include "c-lang.h"
#include "language.h"
#include "dwarf2loc.h"
#include "dwarf2expr.h"
#include "dwarf2.h"
#include "call-cmds.h"
#include "gdbcore.h"

struct expression *parse_exp_at_pc(char *str, unsigned long long pc) {
	struct block *blk;
	struct blockvector *bv = blockvector_for_pc(pc, &blk);
	if (!bv)
		return NULL;
	return parse_exp_1(&str, blk, 0);
}

void print_exp(struct expression *exp) {
	dump_prefix_expression(exp, gdb_stdout);
}

/* Copied from pingdb */
void exp_parser_init(char *filename) {

	if (getcwd(gdb_dirbuf, sizeof(gdb_dirbuf)) == NULL) {
		perror("[PIN_GDB] Failed to get the current working directory:");
		return;
	}
	// GDB Global
	current_directory = gdb_dirbuf;

	gdb_stdout = stdio_fileopen (stdout);
	gdb_stderr = stdio_fileopen (stderr);
	gdb_stdlog = gdb_stderr;	/* for moment */
	gdb_stdtarg = gdb_stderr;	/* for moment */
	gdb_stdin = stdio_fileopen (stdin);
	gdb_stdtargerr = gdb_stderr;	/* for moment */
	gdb_stdtargin = gdb_stdin;	/* for moment */


	debug_file_directory = (char *) malloc(2);
	gdb_sysroot = (char *) malloc(2);
	debug_file_directory[0] = gdb_sysroot[0] = '.';
	debug_file_directory[1] = gdb_sysroot[1] = '.';

	if (*gdb_sysroot) {
		char *canon_sysroot = lrealpath(gdb_sysroot);
		if (canon_sysroot) {
			xfree(gdb_sysroot);
			gdb_sysroot = canon_sysroot;
		}
	}


	if (*debug_file_directory) {
		char *canon_debug = lrealpath(debug_file_directory);
		if (canon_debug) {
			xfree(debug_file_directory);
			debug_file_directory = canon_debug;
		}
	}

	/* copied from gdb_init() */
	initialize_targets();
	initialize_utils();
	initialize_all_files();
	initialize_current_architecture();

	if (catch_command_errors(exec_file_attach, filename, 1, RETURN_MASK_ALL)) {
		if (!catch_command_errors(symbol_file_add_main, filename, 1, RETURN_MASK_ALL)) {
			fprintf(stderr, "cannot read symbol tables.\n");
		}
	} else {
		fprintf(stderr, "cannot open the file.\n");
	}
}

