/* Separate lexical analyzer for GNU C++.
   Copyright (C) 1987, 1989, 1992, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.
   Hacked by Michael Tiemann (tiemann@cygnus.com)

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */


/* This file is the lexical analyzer for GNU C++.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "input.h"
#include "tree.h"
#include "cp-tree.h"
#include "cpplib.h"
#include "lex.h"
#include "flags.h"
#include "c-pragma.h"
#include "toplev.h"
#include "output.h"
#include "tm_p.h"
#include "timevar.h"
#include "diagnostic.h"

#ifdef MULTIBYTE_CHARS
#include "mbchar.h"
#include <locale.h>
#endif

static int interface_strcmp PARAMS ((const char *));
static void init_cp_pragma PARAMS ((void));

static tree parse_strconst_pragma PARAMS ((const char *, int));
static void handle_pragma_vtable PARAMS ((cpp_reader *));
static void handle_pragma_unit PARAMS ((cpp_reader *));
static void handle_pragma_interface PARAMS ((cpp_reader *));
static void handle_pragma_implementation PARAMS ((cpp_reader *));
static void handle_pragma_java_exceptions PARAMS ((cpp_reader *));

static int is_global PARAMS ((tree));
static void init_operators PARAMS ((void));
static void copy_lang_type PARAMS ((tree));

/* A constraint that can be tested at compile time.  */
#define CONSTRAINT(name, expr) extern int constraint_##name [(expr) ? 1 : -1]

/* Functions and data structures for #pragma interface.

   `#pragma implementation' means that the main file being compiled
   is considered to implement (provide) the classes that appear in
   its main body.  I.e., if this is file "foo.cc", and class `bar'
   is defined in "foo.cc", then we say that "foo.cc implements bar".

   All main input files "implement" themselves automagically.

   `#pragma interface' means that unless this file (of the form "foo.h"
   is not presently being included by file "foo.cc", the
   CLASSTYPE_INTERFACE_ONLY bit gets set.  The effect is that none
   of the vtables nor any of the inline functions defined in foo.h
   will ever be output.

   There are cases when we want to link files such as "defs.h" and
   "main.cc".  In this case, we give "defs.h" a `#pragma interface',
   and "main.cc" has `#pragma implementation "defs.h"'.  */

struct impl_files
{
  const char *filename;
  struct impl_files *next;
};

static struct impl_files *impl_file_chain;


/* Return something to represent absolute declarators containing a *.
   TARGET is the absolute declarator that the * contains.
   CV_QUALIFIERS is a list of modifiers such as const or volatile
   to apply to the pointer type, represented as identifiers.

   We return an INDIRECT_REF whose "contents" are TARGET
   and whose type is the modifier list.  */

tree
make_pointer_declarator (cv_qualifiers, target)
     tree cv_qualifiers, target;
{
  if (target && TREE_CODE (target) == IDENTIFIER_NODE
      && ANON_AGGRNAME_P (target))
    error ("type name expected before `*'");
  target = build_nt (INDIRECT_REF, target);
  TREE_TYPE (target) = cv_qualifiers;
  return target;
}

/* Return something to represent absolute declarators containing a &.
   TARGET is the absolute declarator that the & contains.
   CV_QUALIFIERS is a list of modifiers such as const or volatile
   to apply to the reference type, represented as identifiers.

   We return an ADDR_EXPR whose "contents" are TARGET
   and whose type is the modifier list.  */

tree
make_reference_declarator (cv_qualifiers, target)
     tree cv_qualifiers, target;
{
  target = build_nt (ADDR_EXPR, target);
  TREE_TYPE (target) = cv_qualifiers;
  return target;
}

tree
make_call_declarator (target, parms, cv_qualifiers, exception_specification)
     tree target, parms, cv_qualifiers, exception_specification;
{
  target = build_nt (CALL_EXPR, target,
		     tree_cons (parms, cv_qualifiers, NULL_TREE),
		     /* The third operand is really RTL.  We
			shouldn't put anything there.  */
		     NULL_TREE);
  CALL_DECLARATOR_EXCEPTION_SPEC (target) = exception_specification;
  return target;
}

void
set_quals_and_spec (call_declarator, cv_qualifiers, exception_specification)
     tree call_declarator, cv_qualifiers, exception_specification;
{
  CALL_DECLARATOR_QUALS (call_declarator) = cv_qualifiers;
  CALL_DECLARATOR_EXCEPTION_SPEC (call_declarator) = exception_specification;
}

int interface_only;		/* whether or not current file is only for
				   interface definitions.  */
int interface_unknown;		/* whether or not we know this class
				   to behave according to #pragma interface.  */


/* Initialization before switch parsing.  */
void
cxx_init_options ()
{
  c_common_init_options (clk_cplusplus);

  /* Default exceptions on.  */
  flag_exceptions = 1;
  /* By default wrap lines at 80 characters.  Is getenv ("COLUMNS")
     preferable?  */
  diagnostic_line_cutoff (global_dc) = 80;
  /* By default, emit location information once for every
     diagnostic message.  */
  diagnostic_prefixing_rule (global_dc) = DIAGNOSTICS_SHOW_PREFIX_ONCE;
}

void
cxx_finish ()
{
  c_common_finish ();
}

/* A mapping from tree codes to operator name information.  */
operator_name_info_t operator_name_info[(int) LAST_CPLUS_TREE_CODE];
/* Similar, but for assignment operators.  */
operator_name_info_t assignment_operator_name_info[(int) LAST_CPLUS_TREE_CODE];

/* Initialize data structures that keep track of operator names.  */

#define DEF_OPERATOR(NAME, C, M, AR, AP) \
 CONSTRAINT (C, sizeof "operator " + sizeof NAME <= 256);
#include "operators.def"
#undef DEF_OPERATOR

static void
init_operators ()
{
  tree identifier;
  char buffer[256];
  struct operator_name_info_t *oni;

#define DEF_OPERATOR(NAME, CODE, MANGLING, ARITY, ASSN_P)		    \
  sprintf (buffer, ISALPHA (NAME[0]) ? "operator %s" : "operator%s", NAME); \
  identifier = get_identifier (buffer);					    \
  IDENTIFIER_OPNAME_P (identifier) = 1;					    \
									    \
  oni = (ASSN_P								    \
	 ? &assignment_operator_name_info[(int) CODE]			    \
	 : &operator_name_info[(int) CODE]);				    \
  oni->identifier = identifier;						    \
  oni->name = NAME;							    \
  oni->mangled_name = MANGLING;                                             \
  oni->arity = ARITY;

#include "operators.def"
#undef DEF_OPERATOR

  operator_name_info[(int) ERROR_MARK].identifier
    = get_identifier ("<invalid operator>");

  /* Handle some special cases.  These operators are not defined in
     the language, but can be produced internally.  We may need them
     for error-reporting.  (Eventually, we should ensure that this
     does not happen.  Error messages involving these operators will
     be confusing to users.)  */

  operator_name_info [(int) INIT_EXPR].name
    = operator_name_info [(int) MODIFY_EXPR].name;
  operator_name_info [(int) EXACT_DIV_EXPR].name = "(ceiling /)";
  operator_name_info [(int) CEIL_DIV_EXPR].name = "(ceiling /)";
  operator_name_info [(int) FLOOR_DIV_EXPR].name = "(floor /)";
  operator_name_info [(int) ROUND_DIV_EXPR].name = "(round /)";
  operator_name_info [(int) CEIL_MOD_EXPR].name = "(ceiling %)";
  operator_name_info [(int) FLOOR_MOD_EXPR].name = "(floor %)";
  operator_name_info [(int) ROUND_MOD_EXPR].name = "(round %)";
  operator_name_info [(int) ABS_EXPR].name = "abs";
  operator_name_info [(int) FFS_EXPR].name = "ffs";
  operator_name_info [(int) BIT_ANDTC_EXPR].name = "&~";
  operator_name_info [(int) TRUTH_AND_EXPR].name = "strict &&";
  operator_name_info [(int) TRUTH_OR_EXPR].name = "strict ||";
  operator_name_info [(int) IN_EXPR].name = "in";
  operator_name_info [(int) RANGE_EXPR].name = "...";
  operator_name_info [(int) CONVERT_EXPR].name = "+";

  assignment_operator_name_info [(int) EXACT_DIV_EXPR].name
    = "(exact /=)";
  assignment_operator_name_info [(int) CEIL_DIV_EXPR].name
    = "(ceiling /=)";
  assignment_operator_name_info [(int) FLOOR_DIV_EXPR].name
    = "(floor /=)";
  assignment_operator_name_info [(int) ROUND_DIV_EXPR].name
    = "(round /=)";
  assignment_operator_name_info [(int) CEIL_MOD_EXPR].name
    = "(ceiling %=)";
  assignment_operator_name_info [(int) FLOOR_MOD_EXPR].name
    = "(floor %=)";
  assignment_operator_name_info [(int) ROUND_MOD_EXPR].name
    = "(round %=)";
}

/* The reserved keyword table.  */
struct resword
{
  const char *const word;
  const ENUM_BITFIELD(rid) rid : 16;
  const unsigned int disable   : 16;
};

/* Disable mask.  Keywords are disabled if (reswords[i].disable & mask) is
   _true_.  */
#define D_EXT		0x01	/* GCC extension */
#define D_ASM		0x02	/* in C99, but has a switch to turn it off */

CONSTRAINT(ridbits_fit, RID_LAST_MODIFIER < sizeof(unsigned long) * CHAR_BIT);

static const struct resword reswords[] =
{
  { "_Complex",		RID_COMPLEX,	0 },
  { "__FUNCTION__",	RID_FUNCTION_NAME, 0 },
  { "__PRETTY_FUNCTION__", RID_PRETTY_FUNCTION_NAME, 0 },
  { "__alignof", 	RID_ALIGNOF,	0 },
  { "__alignof__",	RID_ALIGNOF,	0 },
  { "__asm",		RID_ASM,	0 },
  { "__asm__",		RID_ASM,	0 },
  { "__attribute",	RID_ATTRIBUTE,	0 },
  { "__attribute__",	RID_ATTRIBUTE,	0 },
  { "__builtin_va_arg",	RID_VA_ARG,	0 },
  { "__complex",	RID_COMPLEX,	0 },
  { "__complex__",	RID_COMPLEX,	0 },
  { "__const",		RID_CONST,	0 },
  { "__const__",	RID_CONST,	0 },
  { "__extension__",	RID_EXTENSION,	0 },
  { "__func__",		RID_C99_FUNCTION_NAME,	0 },
  { "__imag",		RID_IMAGPART,	0 },
  { "__imag__",		RID_IMAGPART,	0 },
  { "__inline",		RID_INLINE,	0 },
  { "__inline__",	RID_INLINE,	0 },
  { "__label__",	RID_LABEL,	0 },
  { "__null",		RID_NULL,	0 },
  { "__real",		RID_REALPART,	0 },
  { "__real__",		RID_REALPART,	0 },
  { "__restrict",	RID_RESTRICT,	0 },
  { "__restrict__",	RID_RESTRICT,	0 },
  { "__signed",		RID_SIGNED,	0 },
  { "__signed__",	RID_SIGNED,	0 },
  { "__thread",		RID_THREAD,	0 },
  { "__typeof",		RID_TYPEOF,	0 },
  { "__typeof__",	RID_TYPEOF,	0 },
  { "__volatile",	RID_VOLATILE,	0 },
  { "__volatile__",	RID_VOLATILE,	0 },
  { "asm",		RID_ASM,	D_ASM },
  { "auto",		RID_AUTO,	0 },
  { "bool",		RID_BOOL,	0 },
  { "break",		RID_BREAK,	0 },
  { "case",		RID_CASE,	0 },
  { "catch",		RID_CATCH,	0 },
  { "char",		RID_CHAR,	0 },
  { "class",		RID_CLASS,	0 },
  { "const",		RID_CONST,	0 },
  { "const_cast",	RID_CONSTCAST,	0 },
  { "continue",		RID_CONTINUE,	0 },
  { "default",		RID_DEFAULT,	0 },
  { "delete",		RID_DELETE,	0 },
  { "do",		RID_DO,		0 },
  { "double",		RID_DOUBLE,	0 },
  { "dynamic_cast",	RID_DYNCAST,	0 },
  { "else",		RID_ELSE,	0 },
  { "enum",		RID_ENUM,	0 },
  { "explicit",		RID_EXPLICIT,	0 },
  { "export",		RID_EXPORT,	0 },
  { "extern",		RID_EXTERN,	0 },
  { "false",		RID_FALSE,	0 },
  { "float",		RID_FLOAT,	0 },
  { "for",		RID_FOR,	0 },
  { "friend",		RID_FRIEND,	0 },
  { "goto",		RID_GOTO,	0 },
  { "if",		RID_IF,		0 },
  { "inline",		RID_INLINE,	0 },
  { "int",		RID_INT,	0 },
  { "long",		RID_LONG,	0 },
  { "mutable",		RID_MUTABLE,	0 },
  { "namespace",	RID_NAMESPACE,	0 },
  { "new",		RID_NEW,	0 },
  { "operator",		RID_OPERATOR,	0 },
  { "private",		RID_PRIVATE,	0 },
  { "protected",	RID_PROTECTED,	0 },
  { "public",		RID_PUBLIC,	0 },
  { "register",		RID_REGISTER,	0 },
  { "reinterpret_cast",	RID_REINTCAST,	0 },
  { "return",		RID_RETURN,	0 },
  { "short",		RID_SHORT,	0 },
  { "signed",		RID_SIGNED,	0 },
  { "sizeof",		RID_SIZEOF,	0 },
  { "static",		RID_STATIC,	0 },
  { "static_cast",	RID_STATCAST,	0 },
  { "struct",		RID_STRUCT,	0 },
  { "switch",		RID_SWITCH,	0 },
  { "template",		RID_TEMPLATE,	0 },
  { "this",		RID_THIS,	0 },
  { "throw",		RID_THROW,	0 },
  { "true",		RID_TRUE,	0 },
  { "try",		RID_TRY,	0 },
  { "typedef",		RID_TYPEDEF,	0 },
  { "typename",		RID_TYPENAME,	0 },
  { "typeid",		RID_TYPEID,	0 },
  { "typeof",		RID_TYPEOF,	D_ASM|D_EXT },
  { "union",		RID_UNION,	0 },
  { "unsigned",		RID_UNSIGNED,	0 },
  { "using",		RID_USING,	0 },
  { "virtual",		RID_VIRTUAL,	0 },
  { "void",		RID_VOID,	0 },
  { "volatile",		RID_VOLATILE,	0 },
  { "wchar_t",          RID_WCHAR,	0 },
  { "while",		RID_WHILE,	0 },

};

void
init_reswords ()
{
  unsigned int i;
  tree id;
  int mask = ((flag_no_asm ? D_ASM : 0)
	      | (flag_no_gnu_keywords ? D_EXT : 0));

  ridpointers = (tree *) ggc_calloc ((int) RID_MAX, sizeof (tree));
  for (i = 0; i < ARRAY_SIZE (reswords); i++)
    {
      id = get_identifier (reswords[i].word);
      C_RID_CODE (id) = reswords[i].rid;
      ridpointers [(int) reswords[i].rid] = id;
      if (! (reswords[i].disable & mask))
	C_IS_RESERVED_WORD (id) = 1;
    }
}

static void
init_cp_pragma ()
{
  c_register_pragma (0, "vtable", handle_pragma_vtable);
  c_register_pragma (0, "unit", handle_pragma_unit);
  c_register_pragma (0, "interface", handle_pragma_interface);
  c_register_pragma (0, "implementation", handle_pragma_implementation);
  c_register_pragma ("GCC", "interface", handle_pragma_interface);
  c_register_pragma ("GCC", "implementation", handle_pragma_implementation);
  c_register_pragma ("GCC", "java_exceptions", handle_pragma_java_exceptions);
}

/* Initialize the C++ front end.  This function is very sensitive to
   the exact order that things are done here.  It would be nice if the
   initialization done by this routine were moved to its subroutines,
   and the ordering dependencies clarified and reduced.  */
bool
cxx_init (void)
{
  input_filename = "<internal>";

  init_reswords ();
  init_tree ();
  init_cp_semantics ();
  init_operators ();
  init_method ();
  init_error ();

  current_function_decl = NULL;

  class_type_node = build_int_2 (class_type, 0);
  TREE_TYPE (class_type_node) = class_type_node;
  ridpointers[(int) RID_CLASS] = class_type_node;

  record_type_node = build_int_2 (record_type, 0);
  TREE_TYPE (record_type_node) = record_type_node;
  ridpointers[(int) RID_STRUCT] = record_type_node;

  union_type_node = build_int_2 (union_type, 0);
  TREE_TYPE (union_type_node) = union_type_node;
  ridpointers[(int) RID_UNION] = union_type_node;

  enum_type_node = build_int_2 (enum_type, 0);
  TREE_TYPE (enum_type_node) = enum_type_node;
  ridpointers[(int) RID_ENUM] = enum_type_node;

  cxx_init_decl_processing ();

  /* Create the built-in __null node.  */
  null_node = build_int_2 (0, 0);
  TREE_TYPE (null_node) = c_common_type_for_size (POINTER_SIZE, 0);
  ridpointers[RID_NULL] = null_node;

  interface_unknown = 1;

  if (c_common_init () == false)
    return false;

  init_cp_pragma ();

  init_repo (main_input_filename);

  return true;
}

/* Helper function to load global variables with interface
   information.  */

void
extract_interface_info ()
{
  struct c_fileinfo *finfo = 0;

  if (flag_alt_external_templates)
    {
      tree til = tinst_for_decl ();

      if (til)
	finfo = get_fileinfo (TINST_FILE (til));
    }
  if (!finfo)
    finfo = get_fileinfo (input_filename);

  interface_only = finfo->interface_only;
  interface_unknown = finfo->interface_unknown;
}

/* Return nonzero if S is not considered part of an
   INTERFACE/IMPLEMENTATION pair.  Otherwise, return 0.  */

static int
interface_strcmp (s)
     const char *s;
{
  /* Set the interface/implementation bits for this scope.  */
  struct impl_files *ifiles;
  const char *s1;

  for (ifiles = impl_file_chain; ifiles; ifiles = ifiles->next)
    {
      const char *t1 = ifiles->filename;
      s1 = s;

      if (*s1 != *t1 || *s1 == 0)
	continue;

      while (*s1 == *t1 && *s1 != 0)
	s1++, t1++;

      /* A match.  */
      if (*s1 == *t1)
	return 0;

      /* Don't get faked out by xxx.yyy.cc vs xxx.zzz.cc.  */
      if (strchr (s1, '.') || strchr (t1, '.'))
	continue;

      if (*s1 == '\0' || s1[-1] != '.' || t1[-1] != '.')
	continue;

      /* A match.  */
      return 0;
    }

  /* No matches.  */
  return 1;
}

void
note_got_semicolon (type)
     tree type;
{
  if (!TYPE_P (type))
    abort ();
  if (CLASS_TYPE_P (type))
    CLASSTYPE_GOT_SEMICOLON (type) = 1;
}

void
note_list_got_semicolon (declspecs)
     tree declspecs;
{
  tree link;

  for (link = declspecs; link; link = TREE_CHAIN (link))
    {
      tree type = TREE_VALUE (link);
      if (type && TYPE_P (type))
	note_got_semicolon (type);
    }
  clear_anon_tags ();
}


/* Parse a #pragma whose sole argument is a string constant.
   If OPT is true, the argument is optional.  */
static tree
parse_strconst_pragma (name, opt)
     const char *name;
     int opt;
{
  tree result, x;
  enum cpp_ttype t;

  t = c_lex (&x);
  if (t == CPP_STRING)
    {
      result = x;
      if (c_lex (&x) != CPP_EOF)
	warning ("junk at end of #pragma %s", name);
      return result;
    }

  if (t == CPP_EOF && opt)
    return 0;

  error ("invalid #pragma %s", name);
  return (tree)-1;
}

static void
handle_pragma_vtable (dfile)
     cpp_reader *dfile ATTRIBUTE_UNUSED;
{
  parse_strconst_pragma ("vtable", 0);
  sorry ("#pragma vtable no longer supported");
}

static void
handle_pragma_unit (dfile)
     cpp_reader *dfile ATTRIBUTE_UNUSED;
{
  /* Validate syntax, but don't do anything.  */
  parse_strconst_pragma ("unit", 0);
}

static void
handle_pragma_interface (dfile)
     cpp_reader *dfile ATTRIBUTE_UNUSED;
{
  tree fname = parse_strconst_pragma ("interface", 1);
  struct c_fileinfo *finfo;
  const char *main_filename;

  if (fname == (tree)-1)
    return;
  else if (fname == 0)
    main_filename = lbasename (input_filename);
  else
    main_filename = TREE_STRING_POINTER (fname);

  finfo = get_fileinfo (input_filename);

  if (impl_file_chain == 0)
    {
      /* If this is zero at this point, then we are
	 auto-implementing.  */
      if (main_input_filename == 0)
	main_input_filename = input_filename;
    }

  interface_only = interface_strcmp (main_filename);
#ifdef MULTIPLE_SYMBOL_SPACES
  if (! interface_only)
#endif
    interface_unknown = 0;

  finfo->interface_only = interface_only;
  finfo->interface_unknown = interface_unknown;
}

/* Note that we have seen a #pragma implementation for the key MAIN_FILENAME.
   We used to only allow this at toplevel, but that restriction was buggy
   in older compilers and it seems reasonable to allow it in the headers
   themselves, too.  It only needs to precede the matching #p interface.

   We don't touch interface_only or interface_unknown; the user must specify
   a matching #p interface for this to have any effect.  */

static void
handle_pragma_implementation (dfile)
     cpp_reader *dfile ATTRIBUTE_UNUSED;
{
  tree fname = parse_strconst_pragma ("implementation", 1);
  const char *main_filename;
  struct impl_files *ifiles = impl_file_chain;

  if (fname == (tree)-1)
    return;

  if (fname == 0)
    {
      if (main_input_filename)
	main_filename = main_input_filename;
      else
	main_filename = input_filename;
      main_filename = lbasename (main_filename);
    }
  else
    {
      main_filename = TREE_STRING_POINTER (fname);
      if (cpp_included (parse_in, main_filename))
	warning ("#pragma implementation for %s appears after file is included",
		 main_filename);
    }

  for (; ifiles; ifiles = ifiles->next)
    {
      if (! strcmp (ifiles->filename, main_filename))
	break;
    }
  if (ifiles == 0)
    {
      ifiles = (struct impl_files*) xmalloc (sizeof (struct impl_files));
      ifiles->filename = main_filename;
      ifiles->next = impl_file_chain;
      impl_file_chain = ifiles;
    }
}

/* Indicate that this file uses Java-personality exception handling.  */
static void
handle_pragma_java_exceptions (dfile)
     cpp_reader *dfile ATTRIBUTE_UNUSED;
{
  tree x;
  if (c_lex (&x) != CPP_EOF)
    warning ("junk at end of #pragma GCC java_exceptions");

  choose_personality_routine (lang_java);
}

/* Return true if d is in a global scope.  */

static int
is_global (d)
  tree d;
{
  while (1)
    switch (TREE_CODE (d))
      {
      case ERROR_MARK:
	return 1;

      case OVERLOAD: d = OVL_FUNCTION (d); continue;
      case TREE_LIST: d = TREE_VALUE (d); continue;
      default:
        my_friendly_assert (DECL_P (d), 980629);

	return DECL_NAMESPACE_SCOPE_P (d);
      }
}

/* Issue an error message indicating that the lookup of NAME (an
   IDENTIFIER_NODE) failed.  */

void
unqualified_name_lookup_error (tree name)
{
  if (IDENTIFIER_OPNAME_P (name))
    {
      if (name != ansi_opname (ERROR_MARK))
	error ("`%D' not defined", name);
    }
  else if (current_function_decl == 0)
    error ("`%D' was not declared in this scope", name);
  else
    {
      if (IDENTIFIER_NAMESPACE_VALUE (name) != error_mark_node
	  || IDENTIFIER_ERROR_LOCUS (name) != current_function_decl)
	{
	  static int undeclared_variable_notice;

	  error ("`%D' undeclared (first use this function)", name);

	  if (! undeclared_variable_notice)
	    {
	      error ("(Each undeclared identifier is reported only once for each function it appears in.)");
	      undeclared_variable_notice = 1;
	    }
	}
      /* Prevent repeated error messages.  */
      SET_IDENTIFIER_NAMESPACE_VALUE (name, error_mark_node);
      SET_IDENTIFIER_ERROR_LOCUS (name, current_function_decl);
    }
}

tree
do_identifier (token, args)
     register tree token;
     tree args;
{
  register tree id;

  timevar_push (TV_NAME_LOOKUP);
  id = lookup_name (token, 0);

  /* Do Koenig lookup if appropriate (inside templates we build lookup
     expressions instead).

     [basic.lookup.koenig]: If the ordinary unqualified lookup of the name
     finds the declaration of a class member function, the associated
     namespaces and classes are not considered.  */

  if (args && !current_template_parms && (!id || is_global (id)))
    id = lookup_arg_dependent (token, id, args);

  if (id == error_mark_node)
    {
      /* lookup_name quietly returns error_mark_node if we're parsing,
	 as we don't want to complain about an identifier that ends up
	 being used as a declarator.  So we call it again to get the error
	 message.  */
      id = lookup_name (token, 0);
      POP_TIMEVAR_AND_RETURN (TV_NAME_LOOKUP, error_mark_node);
    }

  if (!id || (TREE_CODE (id) == FUNCTION_DECL
	      && DECL_ANTICIPATED (id)))
    {
      if (current_template_parms)
        POP_TIMEVAR_AND_RETURN (TV_NAME_LOOKUP,
                                build_min_nt (LOOKUP_EXPR, token));
      else if (IDENTIFIER_TYPENAME_P (token))
	/* A templated conversion operator might exist.  */
	POP_TIMEVAR_AND_RETURN (TV_NAME_LOOKUP, token);
      else
	{
	  unqualified_name_lookup_error (token);
	  POP_TIMEVAR_AND_RETURN (TV_NAME_LOOKUP, error_mark_node);
	}
    }

  id = check_for_out_of_scope_variable (id);

  /* TREE_USED is set in `hack_identifier'.  */
  if (TREE_CODE (id) == CONST_DECL)
    {
      /* Check access.  */
      if (IDENTIFIER_CLASS_VALUE (token) == id)
	enforce_access (CP_DECL_CONTEXT(id), id);
      if (!processing_template_decl || DECL_TEMPLATE_PARM_P (id))
	id = DECL_INITIAL (id);
    }
  else
    id = hack_identifier (id, token);

  /* We must look up dependent names when the template is
     instantiated, not while parsing it.  For now, we don't
     distinguish between dependent and independent names.  So, for
     example, we look up all overloaded functions at
     instantiation-time, even though in some cases we should just use
     the DECL we have here.  We also use LOOKUP_EXPRs to find things
     like local variables, rather than creating TEMPLATE_DECLs for the
     local variables and then finding matching instantiations.  */
  if (current_template_parms
      && (is_overloaded_fn (id)
	  || (TREE_CODE (id) == VAR_DECL
	      && CP_DECL_CONTEXT (id)
	      && TREE_CODE (CP_DECL_CONTEXT (id)) == FUNCTION_DECL)
	  || TREE_CODE (id) == PARM_DECL
	  || TREE_CODE (id) == RESULT_DECL
	  || TREE_CODE (id) == USING_DECL))
    id = build_min_nt (LOOKUP_EXPR, token);

  POP_TIMEVAR_AND_RETURN (TV_NAME_LOOKUP, id);
}

tree
do_scoped_id (token, id)
     tree token;
     tree id;
{
  timevar_push (TV_NAME_LOOKUP);
  if (!id || (TREE_CODE (id) == FUNCTION_DECL
	      && DECL_ANTICIPATED (id)))
    {
      if (processing_template_decl)
	{
	  id = build_min_nt (LOOKUP_EXPR, token);
	  LOOKUP_EXPR_GLOBAL (id) = 1;
	  POP_TIMEVAR_AND_RETURN (TV_NAME_LOOKUP, id);
	}
      if (IDENTIFIER_NAMESPACE_VALUE (token) != error_mark_node)
        error ("`::%D' undeclared (first use here)", token);
      id = error_mark_node;
      /* Prevent repeated error messages.  */
      SET_IDENTIFIER_NAMESPACE_VALUE (token, error_mark_node);
    }
  else
    {
      if (TREE_CODE (id) == ADDR_EXPR)
	mark_used (TREE_OPERAND (id, 0));
      else if (TREE_CODE (id) != OVERLOAD)
	mark_used (id);
    }
  if (TREE_CODE (id) == CONST_DECL && ! processing_template_decl)
    {
      /* XXX CHS - should we set TREE_USED of the constant? */
      id = DECL_INITIAL (id);
      /* This is to prevent an enum whose value is 0
	 from being considered a null pointer constant.  */
      id = build1 (NOP_EXPR, TREE_TYPE (id), id);
      TREE_CONSTANT (id) = 1;
    }

  if (processing_template_decl)
    {
      if (is_overloaded_fn (id))
	{
	  id = build_min_nt (LOOKUP_EXPR, token);
	  LOOKUP_EXPR_GLOBAL (id) = 1;
	  POP_TIMEVAR_AND_RETURN (TV_NAME_LOOKUP, id);
	}
      /* else just use the decl */
    }
  POP_TIMEVAR_AND_RETURN (TV_NAME_LOOKUP, convert_from_reference (id));
}

tree
identifier_typedecl_value (node)
     tree node;
{
  tree t, type;
  type = IDENTIFIER_TYPE_VALUE (node);
  if (type == NULL_TREE)
    return NULL_TREE;

  if (IDENTIFIER_BINDING (node))
    {
      t = IDENTIFIER_VALUE (node);
      if (t && TREE_CODE (t) == TYPE_DECL && TREE_TYPE (t) == type)
	return t;
    }
  if (IDENTIFIER_NAMESPACE_VALUE (node))
    {
      t = IDENTIFIER_NAMESPACE_VALUE (node);
      if (t && TREE_CODE (t) == TYPE_DECL && TREE_TYPE (t) == type)
	return t;
    }

  /* Will this one ever happen?  */
  if (TYPE_MAIN_DECL (type))
    return TYPE_MAIN_DECL (type);

  /* We used to do an internal error of 62 here, but instead we will
     handle the return of a null appropriately in the callers.  */
  return NULL_TREE;
}

#ifdef GATHER_STATISTICS
/* The original for tree_node_kind is in the toplevel tree.c; changes there
   need to be brought into here, unless this were actually put into a header
   instead.  */
/* Statistics-gathering stuff.  */
typedef enum
{
  d_kind,
  t_kind,
  b_kind,
  s_kind,
  r_kind,
  e_kind,
  c_kind,
  id_kind,
  op_id_kind,
  perm_list_kind,
  temp_list_kind,
  vec_kind,
  x_kind,
  lang_decl,
  lang_type,
  all_kinds
} tree_node_kind;

extern int tree_node_counts[];
extern int tree_node_sizes[];
#endif

tree
build_lang_decl (code, name, type)
     enum tree_code code;
     tree name;
     tree type;
{
  tree t;

  t = build_decl (code, name, type);
  retrofit_lang_decl (t);

  return t;
}

/* Add DECL_LANG_SPECIFIC info to T.  Called from build_lang_decl
   and pushdecl (for functions generated by the backend).  */

void
retrofit_lang_decl (t)
     tree t;
{
  struct lang_decl *ld;
  size_t size;

  if (CAN_HAVE_FULL_LANG_DECL_P (t))
    size = sizeof (struct lang_decl);
  else
    size = sizeof (struct lang_decl_flags);

  ld = (struct lang_decl *) ggc_alloc_cleared (size);

  ld->decl_flags.can_be_full = CAN_HAVE_FULL_LANG_DECL_P (t) ? 1 : 0;
  ld->decl_flags.u1sel = TREE_CODE (t) == NAMESPACE_DECL ? 1 : 0;
  ld->decl_flags.u2sel = 0;
  if (ld->decl_flags.can_be_full)
    ld->u.f.u3sel = TREE_CODE (t) == FUNCTION_DECL ? 1 : 0;

  DECL_LANG_SPECIFIC (t) = ld;
  if (current_lang_name == lang_name_cplusplus)
    SET_DECL_LANGUAGE (t, lang_cplusplus);
  else if (current_lang_name == lang_name_c)
    SET_DECL_LANGUAGE (t, lang_c);
  else if (current_lang_name == lang_name_java)
    SET_DECL_LANGUAGE (t, lang_java);
  else abort ();

#ifdef GATHER_STATISTICS
  tree_node_counts[(int)lang_decl] += 1;
  tree_node_sizes[(int)lang_decl] += size;
#endif
}

void
cxx_dup_lang_specific_decl (node)
     tree node;
{
  int size;
  struct lang_decl *ld;

  if (! DECL_LANG_SPECIFIC (node))
    return;

  if (!CAN_HAVE_FULL_LANG_DECL_P (node))
    size = sizeof (struct lang_decl_flags);
  else
    size = sizeof (struct lang_decl);
  ld = (struct lang_decl *) ggc_alloc (size);
  memcpy (ld, DECL_LANG_SPECIFIC (node), size);
  DECL_LANG_SPECIFIC (node) = ld;

#ifdef GATHER_STATISTICS
  tree_node_counts[(int)lang_decl] += 1;
  tree_node_sizes[(int)lang_decl] += size;
#endif
}

/* Copy DECL, including any language-specific parts.  */

tree
copy_decl (decl)
     tree decl;
{
  tree copy;

  copy = copy_node (decl);
  cxx_dup_lang_specific_decl (copy);
  return copy;
}

/* Replace the shared language-specific parts of NODE with a new copy.  */

static void
copy_lang_type (node)
     tree node;
{
  int size;
  struct lang_type *lt;

  if (! TYPE_LANG_SPECIFIC (node))
    return;

  if (TYPE_LANG_SPECIFIC (node)->u.h.is_lang_type_class)
    size = sizeof (struct lang_type);
  else
    size = sizeof (struct lang_type_ptrmem);
  lt = (struct lang_type *) ggc_alloc (size);
  memcpy (lt, TYPE_LANG_SPECIFIC (node), size);
  TYPE_LANG_SPECIFIC (node) = lt;

#ifdef GATHER_STATISTICS
  tree_node_counts[(int)lang_type] += 1;
  tree_node_sizes[(int)lang_type] += size;
#endif
}

/* Copy TYPE, including any language-specific parts.  */

tree
copy_type (type)
     tree type;
{
  tree copy;

  copy = copy_node (type);
  copy_lang_type (copy);
  return copy;
}

tree
cxx_make_type (code)
     enum tree_code code;
{
  register tree t = make_node (code);

  /* Create lang_type structure.  */
  if (IS_AGGR_TYPE_CODE (code)
      || code == BOUND_TEMPLATE_TEMPLATE_PARM)
    {
      struct lang_type *pi;

      pi = ((struct lang_type *)
	    ggc_alloc_cleared (sizeof (struct lang_type)));

      TYPE_LANG_SPECIFIC (t) = pi;
      pi->u.c.h.is_lang_type_class = 1;

#ifdef GATHER_STATISTICS
      tree_node_counts[(int)lang_type] += 1;
      tree_node_sizes[(int)lang_type] += sizeof (struct lang_type);
#endif
    }

  /* Set up some flags that give proper default behavior.  */
  if (IS_AGGR_TYPE_CODE (code))
    {
      SET_CLASSTYPE_INTERFACE_UNKNOWN_X (t, interface_unknown);
      CLASSTYPE_INTERFACE_ONLY (t) = interface_only;

      /* Make sure this is laid out, for ease of use later.  In the
	 presence of parse errors, the normal was of assuring this
	 might not ever get executed, so we lay it out *immediately*.  */
      build_pointer_type (t);
    }
  else
    /* We use TYPE_ALIAS_SET for the CLASSTYPE_MARKED bits.  But,
       TYPE_ALIAS_SET is initialized to -1 by default, so we must
       clear it here.  */
    TYPE_ALIAS_SET (t) = 0;

  /* We need to allocate a TYPE_BINFO even for TEMPLATE_TYPE_PARMs
     since they can be virtual base types, and we then need a
     canonical binfo for them.  Ideally, this would be done lazily for
     all types.  */
  if (IS_AGGR_TYPE_CODE (code) || code == TEMPLATE_TYPE_PARM
      || code == BOUND_TEMPLATE_TEMPLATE_PARM
      || code == TYPENAME_TYPE)
    TYPE_BINFO (t) = make_binfo (size_zero_node, t, NULL_TREE, NULL_TREE);

  return t;
}

tree
make_aggr_type (code)
     enum tree_code code;
{
  tree t = cxx_make_type (code);

  if (IS_AGGR_TYPE_CODE (code))
    SET_IS_AGGR_TYPE (t, 1);

  return t;
}

/* Return the type-qualifier corresponding to the identifier given by
   RID.  */

int
cp_type_qual_from_rid (rid)
     tree rid;
{
  if (rid == ridpointers[(int) RID_CONST])
    return TYPE_QUAL_CONST;
  else if (rid == ridpointers[(int) RID_VOLATILE])
    return TYPE_QUAL_VOLATILE;
  else if (rid == ridpointers[(int) RID_RESTRICT])
    return TYPE_QUAL_RESTRICT;

  abort ();
  return TYPE_UNQUALIFIED;
}
