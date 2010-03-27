#ifndef L2O_H
#define L2O_H

#include <stdbool.h>
#include <stdio.h>


typedef struct buffer
{
  char *buf;  /* First element of buffer. */
  char *ptr;  /* Current element being examined. */
  char *eob;  /* One past the last element. */
  bool eof;   /* True if input file has all been read. */
} buf_t;


typedef struct local_variables loc_t;

struct local_variables
{
  char *var;
  size_t var_len;
  loc_t *next;
  size_t vector_len;    /* Planned future support for vectors. */
  int type;
};


typedef struct function_variables
{
  char *args[8];
  size_t arg_len[8];
  size_t nargs;
  size_t nret;
  size_t stores;
  loc_t *loc_vars;
  size_t nloc;  
} vars_t;


/* Insructions. */
enum instructions
{
    ADD,
    FADD,
    SUB,
    FSUB,
    MUL,
    FMUL,
    UDIV,
    SDIV,
    FDIV,
    UREM,
    SREM,
    FREM,
    AND,
    OR,
    XOR,
    SEL,
    ICMP
};


/* Better support for types in the future. */
enum types
{
  I1,
  I8,
  UI8,
  I16,
  UI16,
  I32,
  UI32,
  I64,
  UI64,
  SFLT,
  DFLT
};


/* Conditions for the icmp command. */
enum icmp_cond
{
  EQ,
  NE,
  UGT,
  UGE,
  ULT,
  ULE,
  SGT,
  SGE,
  SLT,
  SLE
};


void error (char *msg);
static loc_t * declare (char *var, vars_t *vars);
static loc_t * declare_check (char *var, vars_t *vars);
static void instr_binop (buf_t *ibuf, buf_t *obuf, vars_t *vars, loc_t *dest);
static void instr_icmp (buf_t *ibuf, buf_t *obuf, vars_t *vars);
static void instr_select (buf_t *ibuf, buf_t *obuf, vars_t *vars);
static int match_arg (char *var, vars_t *vars);
static char * parse_arg (char *argument, char **arg_src, vars_t *vars);
static int parse_icmp_cond (char *cond);
static int parse_op (char *op);
static int parse_type (char *type, bool unsign);
static inline void print_arg (buf_t *ibuf, buf_t *obuf, vars_t *vars,
                              char **arg_src, int type);
static void print_cast (int type, buf_t *obuf);
static void print_cond (int cond, buf_t *obuf);
static void print_op (int instruction, buf_t *obuf);
static void print_opcode (buf_t *ibuf, buf_t *obuf);
static void prototype (buf_t *ibuf, buf_t *obuf, vars_t *vars);
static void regop (buf_t *ibuf, buf_t *obuf, vars_t *vars);
static void store (buf_t *ibuf, buf_t *obuf, vars_t *vars);
static void translate (buf_t *ibuf, buf_t *obuf, FILE *ofl);
static void usage (int exit_code);

#endif
