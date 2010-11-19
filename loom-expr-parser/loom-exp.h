#ifndef __LOOM_EXP_H
#define __LOOM_EXP_H

#include <libdwarf.h>
#include <dwarf.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif
enum Loom_Expression_Op {
	LOOM_ADD,
	LOOM_DEREF,
	LOOM_SYMBOL,
	LOOM_OFFSET
};

inline const char *loom_expression_op_name(enum Loom_Expression_Op op) {
	if (op == LOOM_ADD)
		return "ADD";
	if (op == LOOM_DEREF)
		return "DEREF";
	if (op == LOOM_SYMBOL)
		return "SYMBOL";
	if (op == LOOM_OFFSET)
		return "OFFSET";
	assert(0 && "Unknown operator");
}

enum Loom_Symbol_Op {
	SYM_NUMBER, // immediate number
	SYM_ADDR,
	SYM_EAX,
	SYM_EBX,
	SYM_ECX,
	SYM_EDX,
	SYM_ESP,
	SYM_EBP,
	SYM_ESI,
	SYM_EDI,
	SYM_ST0,
	SYM_ST1,
	SYM_ST2,
	SYM_ST3,
	SYM_ST4,
	SYM_ST5,
	SYM_BASE_EBX,
	SYM_BASE_EBP
};

inline const char *loom_symbol_op_name(enum Loom_Symbol_Op op) {
	switch (op) {
		case SYM_NUMBER: return "NUMBER";
		case SYM_ADDR: return "ADDR";
		case SYM_BASE_EBP: return "BASE_EBP";
		case SYM_EAX: return "EAX";
		case SYM_EBX: return "EBX";
		case SYM_ECX: return "ECX";
		case SYM_EDX: return "EDX";
		default: assert(0 && "Unknown symbol operator");
	}
}

struct Loom_Expression {
	enum Loom_Expression_Op op; // Operator. Currently <+>, <deref>, <offset> and <symbol>
	enum Loom_Symbol_Op sym_op; // Only used for <symbol>
	int number; // Only used for <symbol>
	int weight; // offset weight. Used for <+> and <offset> only.
	struct type *type; // Type of the result
	int type_size; // Size of <type>
	struct Loom_Expression *lch; // Left child
	struct Loom_Expression *rch; // Right child
};

struct expression;
struct symbol;

struct Loom_Expression *construct_loom_symbol_from_dwarf(Dwarf_Debug dbg, Dwarf_Loc loc, struct symbol *sym);
struct Loom_Expression *construct_loom_expression(Dwarf_Debug dbg, struct expression *exp, Dwarf_Addr pc);
void destruct_loom_expression(struct Loom_Expression *exp);
void dump_loom_expression(struct Loom_Expression *exp);

#ifdef __cplusplus
}
#endif

#endif

