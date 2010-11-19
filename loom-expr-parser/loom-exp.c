#include "loom-exp.h"
#include <libdwarf.h>
#include <dwarf.h>
#include <string.h>
#include "assert.h"
#include "defs.h"
#include "expression.h"
#include "gdbtypes.h"
#include "find-symbol.h"

inline enum Loom_Symbol_Op atom_to_sym_op(Dwarf_Small atom) {
	switch (atom) {
		case DW_OP_addr: return SYM_ADDR;
		case DW_OP_reg0: return SYM_EAX;
		case DW_OP_reg1: return SYM_ECX;
		case DW_OP_reg2: return SYM_EDX;
		case DW_OP_reg3: return SYM_EBX;
		case DW_OP_reg4: return SYM_ESP;
		case DW_OP_reg5: return SYM_EBP;
		case DW_OP_reg6: return SYM_ESI;
		case DW_OP_reg7: return SYM_EDI;
		case DW_OP_reg11: return SYM_ST0;
		case DW_OP_reg12: return SYM_ST1;
		case DW_OP_reg13: return SYM_ST2;
		case DW_OP_reg14: return SYM_ST3;
		case DW_OP_reg15: return SYM_ST4;
		case DW_OP_reg16: return SYM_ST5;
		case DW_OP_breg3: return SYM_BASE_EBX;
		case DW_OP_breg5: return SYM_BASE_EBP;
		case DW_OP_fbreg: return SYM_BASE_EBP;
		default: assert(0 && "Unknown atom");
	}
}

/* Remember to dealloc <die> afterwards. */
struct Loom_Expression *construct_loom_symbol_from_dwarf(Dwarf_Debug dbg, Dwarf_Loc loc, struct symbol *sym) {
	struct Loom_Expression *loom_sym = (struct Loom_Expression *)malloc(sizeof(struct Loom_Expression));
	// TODO: We assume only <lr_number> is used. 
	loom_sym->op = LOOM_SYMBOL;
	loom_sym->sym_op = atom_to_sym_op(loc.lr_atom);
	loom_sym->number = (Dwarf_Signed)loc.lr_number; // <lr_number> is an unsigned 64-bit integer.
	loom_sym->type = sym->type;
	loom_sym->type_size = sym->type->length;
	loom_sym->lch = loom_sym->rch = NULL;
	return loom_sym;
}

struct Loom_Expression *construct_loom_const(struct expression *exp, int val) {
	struct Loom_Expression *loom_sym = (struct Loom_Expression *)malloc(sizeof(struct Loom_Expression));
	loom_sym->op = LOOM_SYMBOL;
	loom_sym->sym_op = SYM_NUMBER;
	loom_sym->number = val;
	loom_sym->type = builtin_type(exp->gdbarch)->builtin_int;
	loom_sym->type_size = loom_sym->type->length;
	loom_sym->lch = loom_sym->rch = NULL;
	return loom_sym;
}

void dump_loom_node(struct Loom_Expression *node) {
	fprintf(stderr, "%s", loom_expression_op_name(node->op));
	if (node->op == LOOM_SYMBOL) {
		fprintf(stderr, " (sym op = %s, number = %d)", loom_symbol_op_name(node->sym_op), node->number);
	} else if (node->op == LOOM_ADD) {
		fprintf(stderr, " (offset weight = %d)", node->weight);
	}
	fprintf(stderr, " (type = %s, length = %d)", (node->type ? node->type->main_type->name : "<no type>"), node->type_size);
	fprintf(stderr, "\n");
}

void dump_loom_subexp(struct Loom_Expression *exp, int indent) {
	int i;
	for (i = 0; i < indent; i++)
		fprintf(stderr, " ");
	dump_loom_node(exp);
	if (exp->lch)
		dump_loom_subexp(exp->lch, indent + 2);
	if (exp->rch)
		dump_loom_subexp(exp->rch, indent + 2);
}

void dump_loom_expression(struct Loom_Expression *exp) {
	dump_loom_subexp(exp, 0);
}

struct Loom_Expression *construct_loom_subexp(Dwarf_Debug dbg, struct expression *exp, Dwarf_Addr pc, int *pos) {
	int p = *pos;
	(*pos)++;
	enum exp_opcode opcode = exp->elts[p].opcode;
	if (opcode == OP_LONG) {
		(*pos) += 3;
		return construct_loom_const(exp, exp->elts[p + 2].longconst);
	}
	if (opcode == OP_VAR_VALUE) {
		(*pos) += 3;
		struct symbol *sym = exp->elts[p + 2].symbol;
		Dwarf_Die die;
		Dwarf_Addr cu_base_pc;
		if (!find_symbol_in_module(dbg, pc, sym->ginfo.name, &die, &cu_base_pc)) {
			assert(0 && "Cannot find the symbol in the module");
			return NULL;
		}
		Dwarf_Loc loc;
		if (!find_location(dbg, die, pc - cu_base_pc, &loc)) {
			dwarf_dealloc(dbg, die, DW_DLA_DIE);
			assert(0 && "Cannot find the location");
			return NULL;
		}
		// TODO: deal with DW_OP_fbreg
		struct Loom_Expression *node = construct_loom_symbol_from_dwarf(dbg, loc, sym);
		dwarf_dealloc(dbg, die, DW_DLA_DIE);
		return node;
#if 0
		struct type *target_type = sym->type->main_type->target_type;
		if (target_type)
			fprintf(stderr, "length of target_type = %d\n", target_type->length);
		struct field *fields = sym->type->main_type->fields;
		if (fields) {
			short n_fields = sym->type->main_type->nfields;
			short i;
			for (i = 0; i < n_fields; i++)
				fprintf(stderr, "field %d: name = %s, length = %d\n", i, fields[i].name, fields[i].type->length);
		}
#endif
	}
	if (opcode == BINOP_ADD) {
		fprintf(stderr, "ADD\n");
		struct Loom_Expression *lch = construct_loom_subexp(dbg, exp, pc, pos);
		struct Loom_Expression *rch = construct_loom_subexp(dbg, exp, pc, pos);
		struct Loom_Expression *node = (struct Loom_Expression *)malloc(sizeof(struct Loom_Expression));
		node->op = LOOM_ADD;
		if (!is_integral_type(rch->type)) {
			struct Loom_Expression *tmp = lch;
			lch = rch;
			rch = tmp;
		}
		assert(is_integral_type(rch->type) && "At least one of the two types should be integral");
		if (is_integral_type(lch->type)) {
			node->weight = 1;
			if (TYPE_LENGTH(lch->type) > TYPE_LENGTH(rch->type))
				node->type = lch->type;
			else
				node->type = rch->type;
		} else {
			struct type *target_type = lch->type->main_type->target_type;
			assert(target_type && "<lch> is not of a pointer type");
			node->weight = target_type->length;
			node->type = lch->type;
		}
		node->type_size = node->type->length;
		node->lch = lch;
		node->rch = rch;
		return node;
	}
	if (opcode == BINOP_SUBSCRIPT) {
		fprintf(stderr, "SUBSCRIPT\n");
		struct Loom_Expression *base = construct_loom_subexp(dbg, exp, pc, pos);
		struct Loom_Expression *sub = construct_loom_subexp(dbg, exp, pc, pos);
		assert(base && "<base> is NULL");
		assert(sub && "<sub> is NULL");
		
		struct Loom_Expression *sum = (struct Loom_Expression *)malloc(sizeof(struct Loom_Expression));
		sum->op = LOOM_ADD;
		struct type *target_type = base->type->main_type->target_type;
		assert(target_type && "<base> is not of a pointer type");
		sum->weight = target_type->length;
		sum->type = base->type;
		sum->type_size = sum->type->length;
		sum->lch = base;
		sum->rch = sub;

		struct Loom_Expression *deref = (struct Loom_Expression *)malloc(sizeof(struct Loom_Expression));
		deref->op = LOOM_DEREF;
		deref->type = target_type;
		deref->type_size = deref->type->length;
		deref->lch = sum;
		deref->rch = NULL;

		return deref;
	}
	if (opcode == STRUCTOP_STRUCT) {
		int tem = longest_to_int(exp->elts[p + 1].longconst);
		(*pos) += 3 + BYTES_TO_EXP_ELEM(tem + 1);
		fprintf(stderr, "STRUCTOP_STRUCT: tem = %d, bytes = %d\n", tem, BYTES_TO_EXP_ELEM(tem + 1));

		struct Loom_Expression *lch = construct_loom_subexp(dbg, exp, pc, pos);

		char *field_name = &exp->elts[p + 2].string;
		struct field *fields = lch->type->main_type->fields;
		assert(fields && "Not a field type");
		short n_fields = lch->type->main_type->nfields;
		short i;
		struct field *f = NULL;
		for (i = 0; i < n_fields; i++) {
			if (strcmp(fields[i].name, field_name) == 0) {
				f = &fields[i];
				break;
			}
		}
		assert(f && "Field name not found");
		struct Loom_Expression *rch = construct_loom_const(exp, f->loc.bitpos / 8); // TODO: 64-bit

		struct Loom_Expression *node = (struct Loom_Expression *)malloc(sizeof(struct Loom_Expression));
		node->op = LOOM_OFFSET;
		node->weight = 1;
		node->type = f->type;
		node->type_size = node->type->length;
		node->lch = lch;
		node->rch = rch;

		return node;
	}
	if (opcode == STRUCTOP_PTR) {
		int tem = longest_to_int(exp->elts[p + 1].longconst);
		(*pos) += 3 + BYTES_TO_EXP_ELEM(tem + 1);

		struct Loom_Expression *lch = construct_loom_subexp(dbg, exp, pc, pos);
		char *field_name = &exp->elts[p + 2].string;
		struct type *target_type = lch->type->main_type->target_type;
		assert(target_type && "<lch> is not of a pointer type");
		struct field *fields = target_type->main_type->fields;
		assert(fields && "<target_type> is not a field type");
		short n_fields = target_type->main_type->nfields;
		short i;
		struct field *f = NULL;
		for (i = 0; i < n_fields; i++) {
			if (strcmp(fields[i].name, field_name) == 0) {
				f = &fields[i];
				break;
			}
		}
		assert(f && "Field name not found");

		struct Loom_Expression *rch = construct_loom_const(exp, f->loc.bitpos / 8); // TODO: 64-bit

		struct Loom_Expression *node = (struct Loom_Expression *)malloc(sizeof(struct Loom_Expression));
		node->op = LOOM_ADD;
		node->weight = 1;
		node->type = f->type;
		node->type_size = node->type->length;
		node->lch = lch;
		node->rch = rch;

		return node;
	}
	if (opcode == UNOP_IND) {
		fprintf(stderr, "UNOP_IND\n");
		struct Loom_Expression *lch = construct_loom_subexp(dbg, exp, pc, pos);
		struct Loom_Expression *node = (struct Loom_Expression *)malloc(sizeof(struct Loom_Expression));
		node->op = LOOM_DEREF;
		node->type = lch->type->main_type->target_type;
		node->type_size = node->type->length;
		assert(node->type && "<lch> is not of a pointer type");
		node->lch = lch;
		node->rch = NULL;
		return node;
	}
	assert(0 && "Not implemented");
	return NULL;
}

struct Loom_Expression *construct_loom_expression(Dwarf_Debug dbg, struct expression *exp, Dwarf_Addr pc) {
	int pos = 0;
	return construct_loom_subexp(dbg, exp, pc, &pos);
}

void destruct_loom_expression(struct Loom_Expression *exp) {
	if (exp->lch)
		destruct_loom_expression(exp->lch);
	if (exp->rch)
		destruct_loom_expression(exp->rch);
	free(exp);
}

