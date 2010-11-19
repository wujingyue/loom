#ifndef __FIXES_H
#define __FIXES_H

#include "sync_objs.h"

#include <string>
#include <vector>

enum Fix_Type {
	CRITICAL = 0,
	ORDER = 1,
	ATOMIC = 2,
	BARRIER = 3,
	EXISTING_CRITICAL = 4,
	UNKNOWN = 5
};

/**
 * There are at most two operations for each fix type. 
 * CRITICAL: lock, unlock
 * ORDER: wait, signal
 * ATOMIC: enter, exit
 * BARRIER: wait
 */
enum Operation_Type {
	START = 0,
	END = 1,
	MASTER = 2,
};

struct Operation {
	Operation_Type type;
	std::string file_name, obj, context;
	int line_no, pos;
};

struct Fix {
	Fix() : type(UNKNOWN) {}
	Fix_Type type;
	std::vector<Operation> ops;
};

extern Fix fixes[MAX_N_FIXES];
extern int n_fixes;

void init_fixes();
int read_fix(const std::string &file_name, Fix &fix);
int add_fix(int fix_id, const Fix &fix);
int del_fix(int fix_id);
int preload_fix(int fix_id, const std::string &file_name, char *err_msg);

#endif
