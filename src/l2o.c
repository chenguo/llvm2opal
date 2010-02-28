#include <ctype.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "args.h"
#include "l2o.h"
#include "bufops.h"

#define SKIP_LINE() {\
  char *tmp = strchr (ibuf->ptr, '\n');\
  if (!tmp)\
    break;\
  else if (++tmp < ibuf->eob)\
    ibuf->ptr = tmp;\
  continue;\
} 

bool opcode;

enum {HELP_OPT};

static struct option const longopts[] =
{
  {"opcode", no_argument, NULL, 'o'}, 
  {"help", no_argument, NULL, HELP_OPT},
  {NULL, 0, NULL, 0}
};

int
main (int argc, char **argv)
{
  opcode = false;

  while (1)
    {
      int this_optind = optind ? optind : 1;
      char opt = getopt_long (argc, argv, "o", longopts, NULL);
      if (opt == -1)
        break;

      switch (opt)
        {
        case 'o': opcode = true;
          break;

        case HELP_OPT: usage (EXIT_SUCCESS);
          break;

        default: usage (EXIT_FAILURE);
        }
    }

  /* Parse infile, outfile; open file descriptors. */
  FILE *ifl, *ofl;
  if ((ifl = fopen (argv[optind++], "r")) == NULL)
    error ("Cannot open input file.");

  if (optind < argc)
    {
      ofl = fopen (argv[optind], "w");
      if (ofl == NULL)
        error ("Cannot open output file.");
    }
  else
    ofl = stdout;

  /* Declare and initialize input and output buffers. */
  buf_t ibuf, obuf;
  init_buf (&ibuf);
  init_buf (&obuf);

  /* Read input from input file. */
  fillbuf (&ibuf, ifl, false);

  /* IMPORTANT NOTE: It is assumed that the buffer is big enough to hold
     any single function found in the source file. */
  do
    {
      /* While another function is wholly in buffer. */
      while (strchr (ibuf.ptr, '}'))
        translate (&ibuf, &obuf, ofl);
      fillbuf (&ibuf, ifl, true);
    }
  while (!ibuf.eof || strchr (ibuf.ptr, '}'));

  close (ifl);
  close (ofl);

 return 0;
}


/* Mark VAR as declared. */
static loc_t *
declare (char *var, vars_t *vars)
{
  loc_t *iter = vars->loc_vars;

  /* Traverse linked list of variables. */
  int i = 0;
  while (iter->next)
    iter = iter->next;

  iter->next = malloc (sizeof *iter->next);
  iter = iter->next;
  iter->var = var;
  iter->var_len = wordlen (var);
  vars->nloc++;

  return iter;
}


/* Check if a local variable has been declared. If so, return true. If not,
   mark it as declared, and return false. */
static loc_t *
declare_check (char *var, vars_t *vars)
{
  size_t var_len = wordlen (var);
  int i;
  loc_t *iter = vars->loc_vars;
  iter = iter->next;
  for (i = 0; i < vars->nloc; i++)
    {
 /*     fprintf (stderr, "comparing ");
      writeword (var);
      fprintf (stderr, " and ");
      writeword (iter->var);
      fprintf (stderr, "\n");
   */   if (var_len == iter->var_len
          && xstrcmp (var, iter->var, var_len))
        return iter;
      iter = iter->next;
    }
  return NULL;
}      


void
error (char *msg)
{
  fprintf (stderr, "%s\n", msg);
  exit (1);
}


/* Handles all binary operations. */
static void
instr_binop (buf_t *ibuf, buf_t *obuf, vars_t *vars, loc_t *dest)
{
  int type = -1;
  bool unsign = false;
  int instruction = parse_op (ibuf->ptr, &unsign);

  /* Increment to instruction's type. */
  while (ibuf->ptr != ibuf->eob && type == -1)
    {
      skip_word (ibuf);
      skip_space (ibuf);
      type = parse_type (ibuf->ptr, unsign);
    }
  if (ibuf->ptr == ibuf->eob)
    error ("Unexpected end of input.");
  dest->type = type;

  /* All remaining tokens are the arguments of the intruction. */
  putword ("( ", obuf, strlen ("( "));

  skip_word (ibuf);
  skip_space (ibuf);
  print_arg (ibuf, obuf, vars, args, type);

  print_op (instruction, obuf);

  skip_word (ibuf);
  skip_space (ibuf);
  print_arg (ibuf, obuf, vars, argsi, type);

  putword (" );\n", obuf, strlen (" );\n"));    
}


/* Translates the icmp instruction. */
static void
instr_icmp (buf_t *ibuf, buf_t *obuf, vars_t *vars)
{
  /* Skip to the condition. */
  skip_word (ibuf);
  skip_space (ibuf);

  /* Parse the condition. */
  int cond = parse_icmp_cond (ibuf->ptr);
  if (cond < 0)
    error ("Unrecognized icmp condition.");
  bool unsign = (*ibuf->ptr == 'u');

  skip_word (ibuf);
  skip_space (ibuf);

  /* Parse the type. */
  int type = parse_type (ibuf->ptr, unsign);
  if (type < 0)
    error ("Unrecognized icmp type.");

  /* All remaining tokens are the arguments of the intruction. */
  putword ("( ", obuf, strlen ("( "));

  skip_word (ibuf);
  skip_space (ibuf);
  print_arg (ibuf, obuf, vars, args, type);

  print_cond (cond, obuf);

  skip_word (ibuf);
  skip_space (ibuf);
  print_arg (ibuf, obuf, vars, args, type);

  putword (" );\n", obuf, strlen (" );\n"));    
}


/* Translates the select instruction. */
static void
instr_select (buf_t *ibuf, buf_t *obuf, vars_t *vars)
{
  /* TODO: support vector select. */
  skip_word (ibuf);
  skip_space (ibuf);

  /* Assume i1, skip parsing selty. */
  skip_word (ibuf);
  skip_space (ibuf);

  /* Print select condition. */
  putword ("( ", obuf, strlen ("( "));
  char *condition = parse_arg (ibuf->ptr, args, vars);
  putword (condition, obuf, wordlen (condition)); 
  putword (" )? ", obuf, strlen (" )? "));

  /* Parse and print true value. */
  skip_word (ibuf);
  skip_space (ibuf);
  /* Sign doesn't matter in this case. */
  int type = parse_type (ibuf->ptr, true);
  skip_word (ibuf);
  skip_space (ibuf);
  print_arg (ibuf, obuf, vars, args, type);

  putword (" : ", obuf, strlen (" : "));

  /* Parse and print false value. */
  skip_word (ibuf);
  skip_space (ibuf);
  type = parse_type (ibuf->ptr, true);
  skip_word (ibuf);
  skip_space (ibuf);
  print_arg (ibuf, obuf, vars, args, type);

  putword (";\n", obuf, strlen (";\n"));
}

/* Attempts to match input to an argument. If match is found, returns
   number of that argument. If no match is found, 0 is returned. */
static int
match_arg (char *var, vars_t *vars)
{
  int i;
  size_t var_len = wordlen (var);
  //  fprintf (stderr, "checking %c%c%c\n", *var, *(var+1), *(var+2));

  for (i = 0; i < vars->nargs; i++)
    {
      if (var_len == vars->arg_len[i]
          && xstrcmp (var, vars->args[i], var_len))
        return i + 1; 
    }
  return 0;
}

/* Parse instruction argument. */
static char *
parse_arg (char *argument, char **arg_src, vars_t *vars)
{
  char *ret = NULL;
  int arg = match_arg (argument, vars);
  switch (arg)
    {
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 8:
        ret = arg_src [arg - 1];
        break;

      default:
        /* Check for constant of local variable. */
        if (*argument == '%' && declare_check (argument, vars))
          ret = argument + 1;
        else if (isnum (argument))
          ret = argument;
        else if (xstrcmp (argument, "true", wordlen ("true")))
          ret = arg_true;
        else if (xstrcmp (argument, "false", wordlen ("false")))
          ret = arg_false;
        else{
          writeword (argument);
          error ("Invalid instruction argument.");
          break;}
    }
  return ret;
}

/* Parse the icmp condition. */
static int
parse_icmp_cond (char *cond)
{
  int ret = -1;
  if (xstrcmp (cond, "eq", wordlen ("eq")))
    ret = EQ;
  else if (xstrcmp (cond, "ne", wordlen ("ne")))
    ret = NE;
  else if (xstrcmp (cond, "ugt", wordlen ("ugt")))
    ret = UGT;
  else if (xstrcmp (cond, "uge", wordlen ("uge")))
    ret = UGE;
  else if (xstrcmp (cond, "ult", wordlen ("ult")))
    ret = ULT;
  else if (xstrcmp (cond, "ule", wordlen ("ule")))
    ret = ULE;
  else if (xstrcmp (cond, "sgt", wordlen ("sgt")))
    ret = SGT;
  else if (xstrcmp (cond, "sge", wordlen ("sge")))
    ret = SGE;
  else if (xstrcmp (cond, "slt", wordlen ("slt")))
    ret = SLT;
  else if (xstrcmp (cond, "sle", wordlen ("sle")))
    ret = SLE;
  return ret;
}


/* Parse the instruction. */
static int
parse_op (char *op, bool *unsign)
{
  /* By default unsign is false. */
  if (xstrcmp (op, "add", wordlen ("add")))
    return ADD;
  else if (xstrcmp (op, "sub", wordlen ("sub")))
    return SUB;
  else if (xstrcmp (op, "mul", wordlen ("mul")))
    return MUL;
  else if (xstrcmp (op, "udiv", wordlen ("udiv")))
    {
      *unsign = true;
      return UDIV;
    }
  else if (xstrcmp (op, "sdiv", wordlen ("sdiv")))
    return SDIV;
  else if (xstrcmp (op, "urem", wordlen ("urem")))
    {
      *unsign = true;
      return UREM;
    }
  else if (xstrcmp (op, "srem", wordlen ("srem")))
    return SREM;
  else if (xstrcmp (op, "and", wordlen ("and")))
    return AND;
  else if (xstrcmp (op, "or", wordlen ("or")))
    return OR;
  else if (xstrcmp (op, "xor", wordlen ("xor")))
    return XOR;
  else
    return -1;
}


/* Parse the type. */
static int
parse_type (char *type, bool unsign)
{
  int ret = -1;
  if (*type == 'i')
    {
      if (xstrcmp (type, "i8", wordlen ("i8")))
        ret = (unsign)? UI8 : I8;
      else if (xstrcmp (type, "i16", wordlen ("i16")))
        ret = (unsign)? UI16 : I16;
      else if (xstrcmp (type, "i32", wordlen ("i32")))
        ret = (unsign)? UI32 : I32;
      else if (xstrcmp (type, "i64", wordlen ("i64")))
        ret = (unsign)? UI64 : I64;
      else
        error ("Unrecognized integer format.");
    }
  else if (xstrcmp (type, "float", wordlen ("float")))
    ret = SFLT;
  else if (xstrcmp (type, "double", wordlen ("double")))
    ret = DFLT;
  return ret;
}


/* Print argument. */
static inline void
print_arg (buf_t *ibuf, buf_t *obuf, vars_t *vars, char **arg_src, int type)
{
  if (type != I32)
    print_cast (type, obuf);
  char *argument = parse_arg (ibuf->ptr, arg_src, vars);
  putword (argument, obuf, wordlen (argument));
}


/* Place the appropriate typecast in the buffer. */
static void
print_cast (int type, buf_t *obuf)
{
  if (type == I8)
    putword ("(int8_t) ", obuf, strlen ("(int8_t) "));
  else if (type == UI8)
    putword ("(uint8_t) ", obuf, strlen ("(uint8_t) "));
  else if (type == I16)
    putword ("(int16_t) ", obuf, strlen ("(int16_t) "));
  else if (type == UI16)
    putword ("(uint16_t) ", obuf, strlen ("(uint16_t) "));
  else if (type == UI32)
    putword ("(uint32_t) ", obuf, strlen ("(uint32_t) "));
  else if (type == I64)
    putword ("(int64_t) ", obuf, strlen ("(uint64_t) "));
  else if (type == UI64)
    putword ("(uint64_t) ", obuf, strlen ("(uint64_t) "));
  else
    error ("Invalid type.");
}


/* Print icmp comparison operator. */
static void
print_cond (int cond, buf_t *obuf)
{
  if (cond == EQ)
    putword (" == ", obuf, strlen (" == "));
  else if (cond == NE)
    putword (" != ", obuf, strlen (" != "));
  else if (cond == UGT || cond == SGT)
    putword (" > ", obuf, strlen (" > "));
  else if (cond == UGE || cond == SGE)
    putword (" >= ", obuf, strlen (" >= "));
  else if (cond == ULT || cond == SLT)
    putword (" < ", obuf, strlen (" < "));
  else if (cond == ULE || cond == SLE)
    putword (" <= ", obuf, strlen (" <= "));
  else
    error ("Invalid comparison type.");
}


/* Print the operator corresponding to the instruction. */
static void
print_op (int instruction, buf_t *obuf)
{
  if (instruction == ADD)
    putword (" + ", obuf, strlen (" + "));
  else if (instruction == SUB)
    putword (" - ", obuf, strlen (" + "));
  else if (instruction == MUL)
    putword (" * ", obuf, strlen (" * "));
  else if (instruction == UDIV || instruction == SDIV)
    putword (" / ", obuf, strlen (" / "));
  else if (instruction == SREM || instruction == UREM)
    putword (" % ", obuf, strlen (" % "));
  else if (instruction == AND)
    putword (" & ", obuf, strlen (" & "));
  else if (instruction == OR)
    putword (" | ", obuf, strlen (" | "));
  else if (instruction == XOR)
    putword (" ^ ", obuf, strlen (" ^ "));
  else
    error ("Invalid intruction operator.");
}

/* Print the opcode. */
static void
print_opcode (buf_t *ibuf, buf_t *obuf)
{
  /* Opcode string is of form OPCODE 0xFFF. */
  putword (ibuf->ptr, obuf, wordlen (ibuf->ptr));
  putword (" ", obuf, wordlen (ibuf->ptr));
  skip_word (ibuf);
  skip_space (ibuf);
  putword (ibuf->ptr, obuf, wordlen (ibuf->ptr));
  putword ("\n", obuf, strlen ("\n"));
}

/* Translate prototpye and record arguments. */
static void
prototype (buf_t *ibuf, buf_t *obuf, vars_t *vars)
{
  putword ("DX_RETURNT ", obuf, strlen ("DX_RETURNT "));
  char *func_name = strchr (ibuf->ptr, '@');
  if (!func_name)
    error ("LLVM function name not found.");
  putword (func_name + 1, obuf, wordlen (func_name + 1));
  putword (" (DX_PLIST) {\n", obuf, strlen (" (DX_PLIST) {\n"));

  /* Parse prototype for arugments. Assume all arguments are of type i32. */
  /* Locate beginning of arguments. */
  ibuf->ptr = strchr (func_name, '(');
  if (!ibuf->ptr)
    error ("LLVM function arguments not found.");
  char *args_end  = strchr (ibuf->ptr, ')');
  if (!args_end)
    error ("LLVM function arguments list end not found");

  char *arg = strstr (ibuf->ptr, "i32");

  /* While there are still arguments. */
  while (arg && arg < args_end)
    {
      ibuf->ptr = arg+3;
      skip_space (ibuf);
      /* If argument is not a destination register, add to args list. */
      if (*ibuf->ptr != '*')
        {
          if (vars->nargs >= 10)
            error ("Too many input registers.");
          vars->args[vars->nargs] = ibuf->ptr;
          vars->arg_len[vars->nargs++] = wordlen (ibuf->ptr);
        }
      else
        {
          if (vars->nret >= 2)
            error ("Too many output registers.");
          vars->nret++;
        }

      /* Find next argument. */
      arg = strstr (ibuf->ptr, "i32");
    }
  //printf ("num of inputs %u, num of outputs %u\n", vars->nargs, vars->nret);
}


/* Translates a register operation. */
static void
regop (buf_t *ibuf, buf_t *obuf, vars_t *vars)
{
  loc_t *result;

  /* ibuf->ptr points at the % */
  putword ("  ", obuf, 2);
  result = declare_check (ibuf->ptr, vars);
  if (!result)
    {
      result = declare (ibuf->ptr, vars);
      putword ("IREGISTER ", obuf, strlen ("IREGISTER "));
    }
  /* Skip the % for printing. */
  ibuf->ptr++;
  putword (ibuf->ptr, obuf, wordlen (ibuf->ptr));
  putword (" = ", obuf, strlen (" = "));

  /* Skip to function: increment to = sign, then look at first
     token past the = sign, which should be an instruction. */
  ibuf->ptr = strchr (ibuf->ptr, '=');
  if (!ibuf->ptr)
    error ("Invalid statement.");
  ibuf->ptr++;
  skip_space (ibuf);

  if (xstrcmp (ibuf->ptr, "select", wordlen ("select")))
    instr_select (ibuf, obuf, vars);
  else if (xstrcmp (ibuf->ptr, "icmp", wordlen ("icmp")))
    instr_icmp (ibuf, obuf, vars);
  else
    instr_binop (ibuf, obuf, vars, result);
}


/* Translates a store operation. */
static void
store (buf_t *ibuf, buf_t *obuf, vars_t *vars)
{
  if (vars->stores + 1 > vars->nret)
    error ("Insufficient write destinations.");

  if (vars->stores == 0)
      putword ("  WRITE_DEST ( ", obuf, strlen ("  WRITE_DEST ( "));
  else if (vars->stores == 1)
    putword ("  WRITE_DEST2 ( ", obuf, strlen ("  WRITE_DEST2 ( "));
  else
    error ("Invalid number of stores.");

  char *arg = strchr (ibuf->ptr, '%');
  int arg_no = match_arg (arg, vars);

  if (arg_no > 0)
    {
      putword ("SOURCE", obuf, strlen ("SOURCE"));
      char num = arg_no + 48;
      putword (&num, obuf, 1);
    }
  else
    putword (arg + 1, obuf, wordlen (arg + 1));
  putword (" );\n", obuf, strlen (" );\n"));
  
  vars->stores++;
}


/* Translates one function in LLVM immediate code to opal magic code. */
static void
translate (buf_t *ibuf, buf_t *obuf, FILE *ofl)
{
  /* Array of pointers to arguments. */
  vars_t vars;
  vars.nargs = 0;
  vars.nret = 0;
  vars.stores = 0;
  vars.loc_vars = malloc (sizeof (loc_t));
  vars.nloc = 0;

  /* Translate function line by line. */
  bool func_end = false;
  while (!func_end)
    {
      /* Reset output buf. */
      obuf->ptr = obuf->buf;

      /* Skip white space leading up to line. */
      skip_space (ibuf);

      /* Check for content of line. */
      if (xstrcmp (ibuf->ptr, "opcode", strlen ("opcode")))
        {
          if (!opcode)
           SKIP_LINE();
          print_opcode (ibuf, obuf);
        }
      else if (xstrcmp (ibuf->ptr, "define", strlen ("define")))
        {
          prototype (ibuf, obuf, &vars);
          if (opcode)
            SKIP_LINE();
        }
      else if (xstrcmp (ibuf->ptr, "%", strlen ("%")))
        regop (ibuf, obuf, &vars);
      else if (xstrcmp (ibuf->ptr, "store", strlen ("store")))
        store (ibuf, obuf, &vars);
      else if (xstrcmp (ibuf->ptr, "}", strlen ("}")))
        {
          func_end = true;
          ibuf->ptr += 2;
          if (opcode)
            putword ("\n", obuf, strlen ("\n"));
          else
            putword ("}\n\n", obuf, strlen ("}\n\n"));
        }
      else
        SKIP_LINE();

      fwrite (obuf->buf, obuf->ptr - obuf->buf, 1, ofl);
      char *tmp = strchr (ibuf->ptr, '\n');
      if (!tmp)
        break;
      else if (++tmp < ibuf->eob)
        ibuf->ptr = tmp;
    }
}


static void
usage (int exit_code)
{
  fprintf (stderr, "Correct usage for llvm2opal:\n\
$ llvm2opal [OPTION]... infile outfile\n\n\
OPTIONS:\n\
  -o, --opcode            Print opcode instead of function prototype.\n\n\
FILES:\n\
  infile: input file containing LLVM immediate code\n\
  outfile: desired output file containing opal magic code, STDOUT if not\n\
    specified\n\n");
  exit (exit_code);
}
