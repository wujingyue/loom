#define _REENTRANT

#include "loom/loom.h"
#include "loom/fixes.h"
#include "loom/sync_objs.h"

#include <fstream>
#include <iostream>
#include <sstream>
using namespace std;

Fix fixes[MAX_N_FIXES];
int n_fixes;

void init_fixes() {
	for (int i = 0; i < MAX_N_FIXES; ++i)
		fixes[i].type = UNKNOWN;
	n_fixes = 0;
}

int read_fix(const string &file_name, Fix &fix)
{
	ifstream fin(file_name.c_str());
	if (!fin) {
		cerr << "Error: the constraint file does not exist.\n";
		return -1;
	}

	int type, n_ops;
	fin >> type >> n_ops;
	switch (type) {
		case 0: fix.type = CRITICAL; break;
		case 1: fix.type = ORDER; break;
		case 2: fix.type = ATOMIC; break;
		case 3: fix.type = BARRIER; break;
		case 4: fix.type = EXISTING_CRITICAL; break;
		default:
				cerr << "Error: unknown type " << type << "\n";
				return -1;
	}
	for (int j = 0; j < n_ops; j++) {
		Operation op;
		string pos;
		fin >> type >> pos;
		switch (type) {
			case 0: op.type = START; break;
			case 1: op.type = END; break;
			default: op.type = MASTER;
		}
		size_t p = pos.find(':');
		if (p == string::npos) {
			cerr << "Error: wrong format for position\n";
			return -1;
		}
		op.file_name = pos.substr(0, p);
		istringstream strin(pos.substr(p + 1));
		if (!(strin >> op.line_no)) {
			cerr << "Error: wrong format for line number\n";
			return -1;
		}
		// TODO: translation
		op.pos = op.line_no;
		string str_rest;
		getline(fin, str_rest);
		strin.clear();
		strin.str(str_rest);
		string option;
		while (strin >> option) {
			if (option == "-o" || option == "-O") {
				strin >> op.obj;
			} else if (option == "-c" || option == "-C") {
				strin >> op.context;
			}
		}
		fix.ops.push_back(op);
	}
	return 0;
}

int add_fix(int fix_id, const Fix &fix) {
	if (fix.type != CRITICAL && fix.type != ATOMIC && fix.type != ORDER) {
		fprintf(stderr, "only critical region and atomic region implemented\n");
		return -1;
	}
	if (fixes[fix_id].type != UNKNOWN) {
		fprintf(stderr, "Fix %d is in use\n", fix_id);
		return -1;
	}
	fixes[fix_id] = fix;
	if (fix_id >= n_fixes)
		n_fixes = fix_id + 1;
	for (size_t i = 0; i < fix.ops.size(); ++i) {
		fprintf(stderr, "add_fix: %d %d\n", fix.ops[i].type, fix.ops[i].pos);
		if (fix.ops[i].type == START) {
			int ins_id = fix.ops[i].pos;
			if (fix.type == CRITICAL)
				callback[ins_id] = enter_critical_region;
			else if (fix.type == ATOMIC)
				callback[ins_id] = enter_atomic_region;
			else
				callback[ins_id] = semaphore_down;
			arguments[ins_id] = (argument_t)fix_id;
		} else if (fix.ops[i].type == END) {
			int ins_id = fix.ops[i].pos;
			if (fix.type == CRITICAL)
				callback[ins_id] = exit_critical_region;
			else if (fix.type == ATOMIC)
				callback[ins_id] = exit_atomic_region;
			else
				callback[ins_id] = semaphore_up;
			arguments[ins_id] = (argument_t)fix_id;
		}
	}
	return 0;
}

int del_fix(int fix_id) {
	fprintf(stderr, "del_fix(%d)\n", fix_id);
	Fix &fix = fixes[fix_id];
	fix.type = UNKNOWN;
	for (size_t i = 0; i < fix.ops.size(); ++i) {
		int ins_id = fix.ops[i].pos;
		callback[ins_id] = NULL;
	}
	return 0;
}

