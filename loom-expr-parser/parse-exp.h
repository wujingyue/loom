#ifndef __PARSE_EXP_H
#define __PARSE_EXP_H

#ifdef __cplusplus
extern "C" {
#endif
struct expression;

struct expression *parse_exp_at_pc(char *str, unsigned long long pc);
void print_exp(struct expression *exp);
void exp_parser_init(char *filename);
#ifdef __cplusplus
}
#endif

#endif

