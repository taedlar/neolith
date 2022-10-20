/* A Bison parser, made by GNU Bison 3.5.1.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.5.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 1 "grammar.y"

/*  $Id: grammar.y,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif	/* HAVE_CONFIG_H */

#include "std.h"
#include "compiler.h"
#include "scratchpad.h"
#include "applies.h"
#include "simul_efun.h"
#include "generate.h"
#include "rc.h"
#include "simulate.h"
#include "interpret.h"
#include "main.h"
#include "stralloc.h"
#include "lpc/object.h"

#include "LPC/function.h"
#include "LPC/runtime_config.h"

/*
 * This is the grammar definition of LPC, and its parse tree generator.
 */

/* down to one global :) 
   bits:
      SWITCH_CONTEXT     - we're inside a switch
      LOOP_CONTEXT       - we're inside a loop
      SWITCH_STRINGS     - a string case has been found
      SWITCH_NUMBERS     - a non-zero numeric case has been found
      SWITCH_RANGES      - a range has been found
      SWITCH_DEFAULT     - a default has been found
 */
int context;

/*
 * bison & yacc don't prototype this in y.tab.h
 */
int yyparse(void);


#line 124 "grammar.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Use api.header.include to #include this header
   instead of duplicating it here.  */
#ifndef YY_YY_GRAMMAR_H_INCLUDED
# define YY_YY_GRAMMAR_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    L_NUMBER = 258,
    L_REAL = 259,
    L_STRING = 260,
    L_BASIC_TYPE = 261,
    L_TYPE_MODIFIER = 262,
    L_DEFINED_NAME = 263,
    L_IDENTIFIER = 264,
    L_EFUN = 265,
    L_INC = 266,
    L_DEC = 267,
    L_ASSIGN = 268,
    L_LAND = 269,
    L_LOR = 270,
    L_LSH = 271,
    L_RSH = 272,
    L_ORDER = 273,
    L_NOT = 274,
    L_IF = 275,
    L_ELSE = 276,
    L_SWITCH = 277,
    L_CASE = 278,
    L_DEFAULT = 279,
    L_RANGE = 280,
    L_DOT_DOT_DOT = 281,
    L_WHILE = 282,
    L_DO = 283,
    L_FOR = 284,
    L_FOREACH = 285,
    L_IN = 286,
    L_BREAK = 287,
    L_CONTINUE = 288,
    L_RETURN = 289,
    L_ARROW = 290,
    L_INHERIT = 291,
    L_COLON_COLON = 292,
    L_ARRAY_OPEN = 293,
    L_MAPPING_OPEN = 294,
    L_FUNCTION_OPEN = 295,
    L_NEW_FUNCTION_OPEN = 296,
    L_SSCANF = 297,
    L_CATCH = 298,
    L_PARSE_COMMAND = 299,
    L_TIME_EXPRESSION = 300,
    L_CLASS = 301,
    L_NEW = 302,
    L_PARAMETER = 303,
    LOWER_THAN_ELSE = 304,
    L_EQ = 305,
    L_NE = 306
  };
#endif
/* Tokens.  */
#define L_NUMBER 258
#define L_REAL 259
#define L_STRING 260
#define L_BASIC_TYPE 261
#define L_TYPE_MODIFIER 262
#define L_DEFINED_NAME 263
#define L_IDENTIFIER 264
#define L_EFUN 265
#define L_INC 266
#define L_DEC 267
#define L_ASSIGN 268
#define L_LAND 269
#define L_LOR 270
#define L_LSH 271
#define L_RSH 272
#define L_ORDER 273
#define L_NOT 274
#define L_IF 275
#define L_ELSE 276
#define L_SWITCH 277
#define L_CASE 278
#define L_DEFAULT 279
#define L_RANGE 280
#define L_DOT_DOT_DOT 281
#define L_WHILE 282
#define L_DO 283
#define L_FOR 284
#define L_FOREACH 285
#define L_IN 286
#define L_BREAK 287
#define L_CONTINUE 288
#define L_RETURN 289
#define L_ARROW 290
#define L_INHERIT 291
#define L_COLON_COLON 292
#define L_ARRAY_OPEN 293
#define L_MAPPING_OPEN 294
#define L_FUNCTION_OPEN 295
#define L_NEW_FUNCTION_OPEN 296
#define L_SSCANF 297
#define L_CATCH 298
#define L_PARSE_COMMAND 299
#define L_TIME_EXPRESSION 300
#define L_CLASS 301
#define L_NEW 302
#define L_PARAMETER 303
#define LOWER_THAN_ELSE 304
#define L_EQ 305
#define L_NE 306

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 134 "grammar.y"

    POINTER_INT pointer_int;
    int number;
    float real;
    char *string;
    int type;
    struct { short num_arg; char flags; } argument;
    ident_hash_elem_t *ihe;
    parse_node_t *node;
    function_context_t *contextp;
    struct {
	parse_node_t *node;
        char num;
    } decl; /* 5 */
    struct {
	char num_local;
	char max_num_locals; 
	short context; 
	short save_current_type; 
	short save_exact_types;
    } func_block; /* 8 */

#line 301 "grammar.c"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_GRAMMAR_H_INCLUDED  */



#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))

/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1905

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  73
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  90
/* YYNRULES -- Number of rules.  */
#define YYNRULES  237
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  461

#define YYUNDEFTOK  2
#define YYMAXUTOK   306


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,    70,    60,    53,     2,
      64,    65,    59,    57,    67,    58,     2,    61,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    66,    63,
      56,     2,     2,    50,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    71,     2,    72,    52,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    68,    51,    69,    62,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    54,    55
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   209,   209,   213,   214,   218,   219,   227,   273,   278,
     283,   284,   288,   289,   290,   294,   295,   300,   317,   299,
     367,   371,   372,   373,   377,   381,   391,   392,   396,   397,
     402,   401,   447,   448,   465,   466,   484,   488,   495,   503,
     514,   518,   519,   532,   537,   546,   547,   551,   559,   563,
     564,   568,   569,   573,   580,   619,   629,   630,   631,   636,
     641,   640,   660,   666,   694,   705,   731,   736,   746,   747,
     753,   757,   758,   759,   760,   761,   762,   763,   768,   772,
     794,   807,   806,   820,   819,   833,   832,   856,   876,   881,
     898,   903,   912,   911,   931,   934,   938,   943,   952,   951,
     983,   989,   996,  1003,  1010,  1024,  1040,  1050,  1066,  1070,
    1074,  1078,  1082,  1086,  1094,  1098,  1102,  1106,  1110,  1114,
    1118,  1122,  1126,  1130,  1134,  1138,  1142,  1149,  1153,  1160,
    1183,  1188,  1212,  1218,  1224,  1230,  1234,  1257,  1279,  1293,
    1337,  1374,  1378,  1382,  1533,  1627,  1707,  1711,  1806,  1827,
    1848,  1870,  1879,  1890,  1914,  1936,  1957,  1958,  1959,  1960,
    1961,  1962,  1966,  1972,  1993,  1996,  2000,  2007,  2011,  2018,
    2024,  2037,  2041,  2045,  2052,  2062,  2080,  2087,  2196,  2197,
    2235,  2251,  2256,  2255,  2285,  2298,  2302,  2306,  2313,  2320,
    2324,  2328,  2371,  2421,  2422,  2426,  2428,  2427,  2487,  2520,
    2609,  2627,  2631,  2638,  2642,  2650,  2649,  2663,  2672,  2683,
    2682,  2696,  2701,  2715,  2723,  2724,  2728,  2735,  2736,  2742,
    2753,  2756,  2765,  2769,  2776,  2811,  2883,  2938,  2964,  2985,
    3010,  3027,  3028,  3042,  3057,  3072,  3106,  3110
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "L_NUMBER", "L_REAL", "L_STRING",
  "L_BASIC_TYPE", "L_TYPE_MODIFIER", "L_DEFINED_NAME", "L_IDENTIFIER",
  "L_EFUN", "L_INC", "L_DEC", "L_ASSIGN", "L_LAND", "L_LOR", "L_LSH",
  "L_RSH", "L_ORDER", "L_NOT", "L_IF", "L_ELSE", "L_SWITCH", "L_CASE",
  "L_DEFAULT", "L_RANGE", "L_DOT_DOT_DOT", "L_WHILE", "L_DO", "L_FOR",
  "L_FOREACH", "L_IN", "L_BREAK", "L_CONTINUE", "L_RETURN", "L_ARROW",
  "L_INHERIT", "L_COLON_COLON", "L_ARRAY_OPEN", "L_MAPPING_OPEN",
  "L_FUNCTION_OPEN", "L_NEW_FUNCTION_OPEN", "L_SSCANF", "L_CATCH",
  "L_PARSE_COMMAND", "L_TIME_EXPRESSION", "L_CLASS", "L_NEW",
  "L_PARAMETER", "LOWER_THAN_ELSE", "'?'", "'|'", "'^'", "'&'", "L_EQ",
  "L_NE", "'<'", "'+'", "'-'", "'*'", "'%'", "'/'", "'~'", "';'", "'('",
  "')'", "':'", "','", "'{'", "'}'", "'$'", "'['", "']'", "$accept", "all",
  "program", "possible_semi_colon", "inheritance", "real", "number",
  "optional_star", "block_or_semi", "identifier", "def", "$@1", "@2",
  "modifier_change", "member_name", "member_name_list", "member_list",
  "type_decl", "@3", "new_local_name", "atomic_type", "basic_type",
  "new_arg", "argument", "argument_list", "type_modifier_list", "type",
  "cast", "opt_basic_type", "name_list", "new_name", "block", "decl_block",
  "local_declarations", "$@4", "new_local_def", "single_new_local_def",
  "single_new_local_def_with_init", "local_name_list", "statements",
  "statement", "while", "$@5", "do", "$@6", "for", "$@7", "foreach_var",
  "foreach_vars", "foreach", "$@8", "for_expr", "first_for_expr", "switch",
  "$@9", "switch_block", "case", "case_label", "constant", "comma_expr",
  "expr0", "return", "expr_list", "expr_list_node", "expr_list2",
  "expr_list3", "expr_list4", "assoc_pair", "lvalue", "expr4", "@10",
  "@11", "expr_or_block", "catch", "@12", "sscanf", "parse_command",
  "time_expression", "@13", "lvalue_list", "string", "string_con1",
  "string_con2", "class_init", "opt_class_init", "function_call",
  "efun_override", "function_name", "cond", "optional_else_part", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
      63,   124,    94,    38,   305,   306,    60,    43,    45,    42,
      37,    47,   126,    59,    40,    41,    58,    44,   123,   125,
      36,    91,    93
};
# endif

#define YYPACT_NINF (-236)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-232)

#define yytable_value_is_error(Yyn) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -236,    42,    43,  -236,    38,  -236,   107,  -236,  -236,    30,
       0,  -236,  -236,  -236,  -236,    62,   238,  -236,  -236,  -236,
    -236,  -236,   325,   125,   -19,  -236,    62,   -11,    57,    67,
    -236,    81,  -236,     7,  -236,     0,   -10,    62,  -236,  -236,
    -236,  1703,   118,   325,  -236,  -236,  -236,  -236,   147,  -236,
    -236,   174,    14,    78,   216,   235,   235,  1703,   325,  1225,
     533,  1703,   271,   178,  -236,   196,  -236,   207,  -236,  1703,
    1703,   951,   213,  -236,  -236,   230,  1703,  1057,   246,    45,
    -236,  -236,  -236,  -236,  -236,    57,  -236,   223,   243,    24,
     288,    19,  1703,   325,   247,  1295,   119,  1363,  -236,    94,
    -236,  -236,  -236,   990,   257,  -236,   249,   169,   252,   264,
    -236,   275,  1057,   254,  1703,  1703,    -3,  1703,    -3,  1019,
    -236,  -236,    86,   331,  1703,     0,   113,  -236,   325,  -236,
    1703,  1703,  1703,  1703,  1703,  1703,  1703,  1703,  1703,  1703,
    1703,  1703,  1703,  1703,  1703,  1703,  1703,  -236,  -236,  1703,
     325,  1431,  1295,  1295,  -236,  -236,  -236,     0,  -236,   282,
       5,  -236,    29,     0,  1057,  -236,    24,   283,  -236,  -236,
    -236,   284,  1087,  1703,   285,   603,   293,  1703,  -236,   277,
    1738,  1703,  -236,  -236,  -236,  1760,  -236,   352,   297,  -236,
     124,   308,  -236,  1703,  -236,   506,   436,   205,   205,   136,
     363,  1193,   311,  1338,   296,   296,   136,   220,   220,  -236,
    -236,  -236,  1057,   310,  1703,    72,   317,   318,   337,  -236,
    -236,    24,   325,   294,   312,   319,  -236,  -236,  -236,  1057,
    -236,  -236,  -236,  1057,   322,  1703,  1703,   143,   743,  1703,
    -236,  -236,   324,  -236,   167,  1703,  1295,    97,   393,  -236,
    -236,  -236,  -236,    23,  -236,  -236,     0,  -236,   323,  -236,
    1786,  -236,    21,   326,   328,   329,  -236,   336,   342,   313,
     344,  1499,  -236,  -236,  -236,  -236,   341,   813,  -236,  -236,
    -236,  -236,  -236,   135,  -236,  -236,  1808,   203,  1295,  -236,
    1057,   346,   463,  -236,  1703,  -236,    96,  -236,  -236,  -236,
    -236,  -236,  -236,   235,   360,  -236,  1703,  1703,  1703,   883,
    1157,   166,  -236,  -236,  -236,   170,     0,  -236,  -236,  -236,
    1703,  -236,   325,   361,  -236,  1703,  -236,   109,   133,  -236,
     372,  -236,   241,   250,   255,   400,     0,   415,  -236,  -236,
     379,   376,  -236,  -236,  -236,   377,   414,  -236,   337,   380,
     383,  1786,   382,  -236,  -236,   134,  -236,  -236,  -236,   883,
    -236,  -236,   392,   337,  1703,  1567,   166,  1703,   445,     0,
    -236,   394,  1703,  -236,   439,   402,   883,  1703,  -236,  1057,
     398,  -236,   921,  1703,  -236,  -236,  1057,   883,  -236,  -236,
    -236,   258,  1635,  -236,  1057,  -236,   193,   399,   411,   883,
      74,   412,   673,  -236,  -236,  -236,  -236,   474,   476,   477,
      74,    28,  1126,   424,  -236,   673,   416,   673,   883,  -236,
    -236,  -236,   572,    74,  -236,   115,   115,   115,   115,   115,
     115,   115,   115,   115,   115,   115,   115,   115,   115,  -236,
    -236,  -236,  -236,  -236,   417,   115,   237,   237,   232,  1263,
    1330,  1397,  1200,  1200,   232,   225,   225,  -236,  -236,  -236,
    -236
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       4,     0,    45,     1,    45,    21,     5,    23,    22,    49,
      10,    46,     6,     3,    34,     0,     0,    24,    36,    50,
      47,    11,     0,     0,    51,   217,     0,     0,   214,    35,
      16,     0,    15,    53,    20,    10,     0,     0,     7,   218,
      30,     0,     0,     0,    52,   215,   216,    28,     0,     9,
       8,   196,   179,   180,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   205,     0,   209,     0,   181,     0,
       0,     0,     0,   161,   160,     0,     0,    54,     0,   156,
     195,   157,   158,   159,   193,   213,   178,     0,     0,    40,
      53,    45,     0,     0,     0,     0,     0,     0,   149,   177,
     150,   151,   232,   167,     0,   169,   165,     0,     0,   172,
     174,     0,   127,     0,     0,     0,     0,     0,     0,     0,
     153,   152,    34,     0,     0,    10,     0,   182,     0,   148,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   154,   155,     0,
       0,     0,     0,     0,    33,    32,    39,    10,    43,     0,
      41,    31,    49,    10,   130,   233,    40,     0,   230,   229,
     168,     0,     0,     0,     0,     0,     0,     0,   198,     0,
       0,     0,    59,   203,   206,     0,   210,     0,     0,    35,
       0,     0,   194,     0,   234,   133,   132,   141,   142,   139,
       0,   134,   135,   136,   137,   138,   140,   143,   144,   145,
     146,   147,   129,   184,     0,     0,     0,     0,    37,    18,
      42,     0,     0,    26,     0,     0,   225,   202,   170,   176,
     201,   175,   200,   128,     0,     0,     0,     0,     0,     0,
     220,   223,     0,    48,     0,     0,     0,     0,     0,   192,
     222,   226,    38,     0,    44,    25,    10,    29,     0,   199,
     211,   204,     0,     0,     0,     0,    83,     0,     0,     0,
       0,     0,    78,    60,    56,    77,     0,     0,    73,    74,
      57,    58,    75,     0,    76,    72,     0,     0,     0,   183,
     131,     0,     0,   191,     0,   189,     0,    14,    13,    19,
      12,    27,   197,     0,     0,    70,     0,     0,     0,     0,
       0,     0,    79,    80,   162,     0,    10,    55,    69,    71,
       0,   224,     0,     0,   227,     0,   190,     0,     0,   185,
     211,   207,     0,     0,     0,     0,    10,     0,    97,    96,
       0,    95,    87,    89,    88,    90,     0,   163,     0,    66,
       0,   211,     0,   221,   228,     0,   186,   188,   212,     0,
      98,    81,     0,     0,     0,     0,     0,     0,    62,    10,
      61,     0,     0,   187,   236,     0,     0,     0,    64,    65,
       0,    91,     0,     0,    67,   208,   219,     0,   235,    59,
      82,     0,     0,    92,    63,   237,     0,     0,     0,     0,
       0,     0,     0,    84,    85,    93,   123,     0,     0,     0,
       0,     0,   106,   107,   105,     0,     0,     0,     0,   125,
     124,   126,     0,     0,   103,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   101,
      99,   100,    86,   122,     0,     0,   115,   116,   113,   108,
     109,   110,   111,   112,   114,   117,   118,   119,   120,   121,
     104
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -236,  -236,  -236,  -236,  -236,  -236,  -236,    -6,  -236,    -4,
    -236,  -236,  -236,  -236,  -236,   228,  -236,  -236,  -236,  -201,
    -236,    -7,   265,   332,  -236,    -1,   408,  -236,  -236,   478,
    -236,   -97,  -236,   120,  -236,  -236,   202,  -236,   145,   239,
    -228,  -236,  -236,  -236,  -236,  -236,  -236,   149,  -236,  -236,
    -236,  -235,  -236,  -236,  -236,  -115,   121,    95,  1460,   -56,
     -32,  -236,   -79,  -166,   406,  -236,  -236,   351,   -48,   -42,
    -236,  -236,   410,  -236,  -236,  -236,  -236,  -236,  -236,  -197,
    -236,   -14,   -15,  -236,  -236,  -236,  -236,  -236,  -236,  -236
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,    13,     5,    73,    74,   222,   299,    75,
       6,    42,   253,     7,   223,   224,    91,     8,    47,   156,
      18,   157,   158,   159,   160,     9,    10,    76,    20,    23,
      24,   274,   275,   238,   316,   349,   344,   338,   350,   276,
     415,   278,   376,   279,   309,   280,   418,   345,   346,   281,
     399,   339,   340,   282,   375,   416,   417,   411,   412,   283,
     112,   284,   104,   105,   106,   108,   109,   110,    78,    79,
     193,    94,   184,    80,   116,    81,    82,    83,   118,   304,
      84,    36,    85,   353,   287,    86,    87,    88,   285,   388
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      28,    27,    19,    11,    22,   111,   228,    98,   100,    77,
     277,    28,    31,    99,    99,   126,   167,   252,    33,   183,
      41,   183,    28,    46,   297,   101,     4,   103,   107,    43,
      14,   220,   154,   155,    92,    14,    14,   120,   121,    90,
     188,   126,     3,    -2,   129,     4,    37,    37,    35,   277,
       4,   -15,    38,   423,   102,    45,  -177,  -177,  -177,    21,
     164,   181,    39,   103,   125,   182,    15,    25,   190,   228,
     123,   -17,   221,   216,   217,   123,    16,   406,    95,    25,
     150,   335,   103,   180,   305,   185,   298,   103,   161,   165,
     162,   182,   169,   407,   424,   215,    17,   248,   195,   196,
     197,   198,   199,   200,   201,   202,   203,   204,   205,   206,
     207,   208,   209,   210,   211,   -16,   151,   212,   406,   191,
     103,   103,   292,    93,   194,   237,    26,    32,    30,   150,
     380,   374,   408,   358,   407,   -15,   409,   244,   410,   177,
     103,   229,  -231,   107,   249,   233,   213,   368,   390,    40,
    -196,   218,   132,   133,   371,    19,   300,   398,   247,   395,
      92,   302,   378,   177,   177,   151,   168,   291,   329,   293,
      12,   405,    14,   408,   342,   343,   177,   409,   192,   445,
     177,   356,    89,   130,   131,   132,   133,   134,    34,   242,
     442,   177,   296,   142,   143,   144,   145,   146,   319,    14,
     177,   177,   177,   103,   260,   357,   373,   286,   261,   323,
     177,    93,   123,   290,   103,   315,   400,   401,   255,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   273,   289,   347,   177,   173,   327,   177,   328,   123,
      25,    51,   115,    52,    53,    54,    29,    30,   425,   426,
     332,   333,   334,    96,   341,   330,   103,   147,   148,   149,
     117,    99,   142,   143,   144,   145,   146,   128,   321,   355,
     322,   119,    58,    59,    60,    61,    62,   127,    64,   144,
     145,   146,    67,    68,   436,   437,   438,   152,   351,   434,
     435,   436,   437,   438,   434,   435,   436,   437,   438,    97,
     439,    41,   441,   336,   336,    72,   359,   153,   177,   341,
     348,   166,   132,   133,   134,   360,   172,   177,   352,   178,
     361,   391,   177,   397,   174,   177,   171,   132,   133,   134,
     363,   175,   379,    32,    30,   382,   341,   113,   114,   189,
     386,   176,   177,   234,   235,   154,   155,   219,   226,   227,
     230,   394,   141,   142,   143,   144,   145,   146,   232,   336,
     240,   256,   241,   348,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   243,   246,   257,   312,   130,   131,   132,
     133,   134,   250,   251,   258,    28,   413,   259,   288,   273,
     306,   182,   307,   308,    48,    28,    49,    50,    25,    51,
     310,    52,    53,    54,    55,    56,   311,   313,    28,   413,
     317,   324,    57,   135,   136,   137,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   331,   354,   362,   364,   245,
      58,    59,    60,    61,    62,    63,    64,    65,    66,   303,
      67,    68,   365,   177,   366,   367,   370,   369,   372,   294,
     130,    69,   132,   133,   134,    70,   377,    71,   383,   385,
     387,   392,   403,    72,    48,   295,    49,    50,    25,    51,
     389,    52,    53,    54,    55,    56,   404,   419,   414,   420,
     421,    37,    57,   460,   301,   440,   254,   136,   137,   138,
     139,   140,   141,   142,   143,   144,   145,   146,   225,   163,
      58,    59,    60,    61,    62,    63,    64,    65,    66,   396,
      67,    68,   337,    44,   384,   381,   318,   402,   444,   325,
     179,    69,   132,   133,   134,    70,   231,    71,   186,     0,
       0,     0,     0,    72,    48,   326,    49,    50,    25,    51,
       0,    52,    53,    54,    55,    56,     0,     0,     0,     0,
       0,     0,    57,     0,     0,     0,     0,   136,   137,   138,
     139,   140,   141,   142,   143,   144,   145,   146,     0,     0,
      58,    59,    60,    61,    62,    63,    64,    65,    66,     0,
      67,    68,     0,     0,     0,     0,     0,     0,   425,   426,
     427,    69,     0,     0,     0,    70,     0,    71,     0,     0,
       0,     0,     0,    72,    48,  -171,    49,    50,    25,    51,
       0,    52,    53,    54,    55,    56,     0,     0,     0,     0,
       0,     0,    57,   428,   429,   430,   431,   432,   433,   434,
     435,   436,   437,   438,     0,     0,     0,   443,     0,     0,
      58,    59,    60,    61,    62,    63,    64,    65,    66,     0,
      67,    68,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    69,     0,     0,     0,    70,     0,    71,     0,     0,
       0,     0,     0,    72,    48,  -173,    49,    50,    25,    51,
       0,    52,    53,    54,    55,    56,     0,     0,     0,     0,
       0,     0,    57,   263,     0,   264,   400,   401,     0,     0,
     265,   266,   267,   268,     0,   269,   270,   271,     0,     0,
      58,    59,    60,    61,    62,    63,    64,    65,    66,     0,
      67,    68,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    69,     0,     0,     0,    70,   272,    71,     0,     0,
       0,   182,  -102,    72,   262,     0,    49,    50,    25,   122,
       0,    52,    53,    54,    55,    56,     0,     0,     0,     0,
       0,     0,    57,   263,     0,   264,     0,     0,     0,     0,
     265,   266,   267,   268,     0,   269,   270,   271,     0,     0,
      58,    59,    60,    61,    62,    63,    64,    65,    66,   123,
      67,    68,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    69,     0,     0,     0,    70,   272,    71,     0,     0,
       0,   182,   -68,    72,   262,     0,    49,    50,    25,    51,
       0,    52,    53,    54,    55,    56,     0,     0,     0,     0,
       0,     0,    57,   263,     0,   264,     0,     0,     0,     0,
     265,   266,   267,   268,     0,   269,   270,   271,     0,     0,
      58,    59,    60,    61,    62,    63,    64,    65,    66,     0,
      67,    68,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    69,     0,     0,     0,    70,   272,    71,     0,     0,
       0,   182,   -68,    72,    48,     0,    49,    50,    25,    51,
       0,    52,    53,    54,    55,    56,     0,     0,     0,     0,
       0,     0,    57,   263,     0,   264,     0,     0,     0,     0,
     265,   266,   267,   268,     0,   269,   270,   271,     0,     0,
      58,    59,    60,    61,    62,    63,    64,    65,    66,     0,
      67,    68,     0,     0,     0,   130,   131,   132,   133,   134,
       0,    69,     0,     0,     0,    70,   272,    71,     0,     0,
       0,   182,    48,    72,    49,    50,    25,   122,     0,    52,
      53,    54,    55,    56,     0,     0,     0,     0,     0,     0,
      57,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,     0,     0,     0,   393,     0,    58,    59,
      60,    61,    62,    63,    64,    65,    66,   123,    67,    68,
       0,     0,     0,     0,   130,   131,   132,   133,   134,    69,
     124,     0,     0,    70,     0,    71,   170,     0,     0,     0,
      48,    72,    49,    50,    25,    51,     0,    52,    53,    54,
      55,    56,     0,     0,     0,     0,     0,     0,    57,     0,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,     0,     0,     0,     0,    58,    59,    60,    61,
      62,    63,    64,    65,    66,   187,    67,    68,     0,     0,
       0,   130,   131,   132,   133,   134,     0,    69,     0,     0,
       0,    70,     0,    71,  -164,     0,     0,     0,    48,    72,
      49,    50,    25,    51,     0,    52,    53,    54,    55,    56,
       0,     0,     0,     0,     0,     0,    57,   135,   136,   137,
     138,   139,   140,   141,   142,   143,   144,   145,   146,     0,
       0,     0,     0,     0,    58,    59,    60,    61,    62,    63,
      64,    65,    66,     0,    67,    68,     0,     0,     0,     0,
       0,     0,   425,   426,   427,    69,     0,     0,     0,    70,
       0,    71,  -166,     0,     0,     0,  -166,    72,    48,     0,
      49,    50,    25,   122,     0,    52,    53,    54,    55,    56,
       0,     0,     0,     0,     0,     0,    57,   428,   429,   430,
     431,   432,   433,   434,   435,   436,   437,   438,     0,     0,
       0,     0,     0,     0,    58,    59,    60,    61,    62,    63,
      64,    65,    66,   123,    67,    68,     0,     0,     0,   132,
     133,   134,     0,     0,     0,    69,   425,   426,   427,    70,
     -94,    71,     0,     0,     0,     0,    48,    72,    49,    50,
      25,    51,     0,    52,    53,    54,    55,    56,     0,     0,
       0,     0,     0,     0,    57,   137,   138,   139,   140,   141,
     142,   143,   144,   145,   146,     0,   433,   434,   435,   436,
     437,   438,    58,    59,    60,    61,    62,    63,    64,    65,
      66,     0,    67,    68,     0,     0,     0,     0,     0,   425,
     426,   427,     0,    69,     0,     0,     0,    70,     0,    71,
       0,     0,     0,     0,  -164,    72,    48,     0,    49,    50,
      25,    51,     0,    52,    53,    54,    55,    56,     0,     0,
       0,     0,     0,     0,    57,   429,   430,   431,   432,   433,
     434,   435,   436,   437,   438,     0,     0,     0,     0,     0,
       0,     0,    58,    59,    60,    61,    62,    63,    64,    65,
      66,     0,    67,    68,     0,     0,   425,   426,   427,     0,
       0,     0,     0,    69,   132,   133,   134,    70,     0,    71,
    -164,     0,     0,     0,    48,    72,    49,    50,    25,    51,
       0,    52,    53,    54,    55,    56,     0,     0,     0,     0,
       0,     0,    57,   430,   431,   432,   433,   434,   435,   436,
     437,   438,   139,   140,   141,   142,   143,   144,   145,   146,
      58,    59,    60,    61,    62,    63,    64,    65,    66,     0,
      67,    68,     0,   425,   426,   427,     0,     0,     0,     0,
       0,    69,   124,     0,     0,    70,     0,    71,     0,     0,
       0,     0,    48,    72,    49,    50,    25,    51,     0,    52,
      53,    54,    55,    56,     0,     0,     0,     0,     0,     0,
      57,   431,   432,   433,   434,   435,   436,   437,   438,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    58,    59,
      60,    61,    62,    63,    64,    65,    66,     0,    67,    68,
       0,     0,     0,     0,     0,     0,     0,   214,     0,    69,
       0,     0,     0,    70,     0,    71,     0,     0,     0,     0,
      48,    72,    49,    50,    25,    51,     0,    52,    53,    54,
      55,    56,     0,     0,     0,     0,     0,     0,    57,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    58,    59,    60,    61,
      62,    63,    64,    65,    66,     0,    67,    68,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    69,     0,     0,
       0,    70,   314,    71,     0,     0,     0,     0,    48,    72,
      49,    50,    25,    51,     0,    52,    53,    54,    55,    56,
       0,     0,     0,     0,     0,     0,    57,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    58,    59,    60,    61,    62,    63,
      64,    65,    66,     0,    67,    68,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    69,     0,     0,     0,    70,
     -94,    71,     0,     0,     0,     0,    48,    72,    49,    50,
      25,    51,     0,    52,    53,    54,    55,    56,     0,     0,
       0,     0,     0,     0,    57,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    58,    59,    60,    61,    62,    63,    64,    65,
      66,     0,    67,    68,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    69,     0,     0,     0,    70,     0,    71,
     -94,     0,     0,     0,    48,    72,    49,    50,    25,    51,
       0,    52,    53,    54,    55,    56,     0,     0,     0,     0,
       0,     0,    57,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      58,    59,    60,    61,    62,    63,    64,    65,    66,     0,
      67,    68,   130,   131,   132,   133,   134,     0,     0,     0,
       0,    69,     0,     0,     0,    70,     0,    71,     0,     0,
       0,     0,     0,    72,   130,   131,   132,   133,   134,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   135,   136,
     137,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     130,   131,   132,   133,   134,   236,     0,     0,     0,     0,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   130,   131,   132,   133,   134,   239,     0,     0,
       0,     0,     0,     0,     0,     0,   135,   136,   137,   138,
     139,   140,   141,   142,   143,   144,   145,   146,     0,     0,
       0,     0,     0,   303,     0,     0,     0,     0,   135,   136,
     137,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     422,     0,     0,     0,     0,   320,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   446,   447,   448,   449,   450,
     451,   452,   453,   454,   455,   456,   457,   458,   459,     0,
       0,     0,     0,     0,     0,   422
};

static const yytype_int16 yycheck[] =
{
      15,    15,     9,     4,    10,    61,   172,    55,    56,    41,
     238,    26,    16,    55,    56,    71,    95,   218,    22,   116,
      13,   118,    37,    37,     1,    57,     7,    59,    60,    35,
       6,    26,     8,     9,    13,     6,     6,    69,    70,    43,
     119,    97,     0,     0,    76,     7,    57,    57,    67,   277,
       7,    37,    63,    25,    58,    65,    11,    12,    13,    59,
      92,    64,     5,    95,    71,    68,    36,     5,   124,   235,
      46,    64,    67,   152,   153,    46,    46,     3,    64,     5,
      35,   309,   114,   115,    63,   117,    63,   119,    69,    93,
      91,    68,    96,    19,    66,   151,    66,    25,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,   141,
     142,   143,   144,   145,   146,    37,    71,   149,     3,   125,
     152,   153,    25,    37,   128,   181,    64,     8,     9,    35,
     365,   359,    58,   330,    19,    68,    62,   193,    64,    67,
     172,   173,    64,   175,    72,   177,   150,   348,   376,    68,
      64,   157,    16,    17,   351,   162,   253,   392,   214,   387,
      13,   258,   363,    67,    67,    71,    47,   246,    72,    72,
      63,   399,     6,    58,     8,     9,    67,    62,    65,    64,
      67,    72,    64,    14,    15,    16,    17,    18,    63,    65,
     418,    67,   248,    57,    58,    59,    60,    61,    63,     6,
      67,    67,    67,   235,   236,    72,    72,   239,    65,   288,
      67,    37,    46,   245,   246,   271,    23,    24,   222,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,   238,    65,    63,    67,    66,   292,    67,   294,    46,
       5,     6,    64,     8,     9,    10,     8,     9,    16,    17,
     306,   307,   308,    37,   310,   303,   288,    11,    12,    13,
      64,   303,    57,    58,    59,    60,    61,    37,    65,   325,
      67,    64,    37,    38,    39,    40,    41,    64,    43,    59,
      60,    61,    47,    48,    59,    60,    61,    64,   320,    57,
      58,    59,    60,    61,    57,    58,    59,    60,    61,    64,
     415,    13,   417,   310,   311,    70,    65,    64,    67,   365,
     316,    64,    16,    17,    18,    65,    67,    67,   322,    65,
      65,   377,    67,    65,    72,    67,    69,    16,    17,    18,
     336,    67,   364,     8,     9,   367,   392,    66,    67,     8,
     372,    66,    67,    66,    67,     8,     9,    65,    65,    65,
      65,   383,    56,    57,    58,    59,    60,    61,    65,   366,
       8,    67,    65,   369,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    65,    64,    63,    63,    14,    15,    16,
      17,    18,    65,    65,    65,   400,   400,    65,    64,   396,
      64,    68,    64,    64,     1,   410,     3,     4,     5,     6,
      64,     8,     9,    10,    11,    12,    64,    63,   423,   423,
      69,    65,    19,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    65,    65,    27,    13,    66,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    67,
      47,    48,    63,    67,    67,    31,    63,    67,    66,    56,
      14,    58,    16,    17,    18,    62,    64,    64,    13,    65,
      21,    63,    63,    70,     1,    72,     3,     4,     5,     6,
      68,     8,     9,    10,    11,    12,    65,     3,    66,     3,
       3,    57,    19,    66,   256,    69,   221,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,   166,    91,
      37,    38,    39,    40,    41,    42,    43,    44,    45,   389,
      47,    48,   310,    35,   369,   366,   277,   396,   423,    56,
     114,    58,    16,    17,    18,    62,   175,    64,   118,    -1,
      -1,    -1,    -1,    70,     1,    72,     3,     4,     5,     6,
      -1,     8,     9,    10,    11,    12,    -1,    -1,    -1,    -1,
      -1,    -1,    19,    -1,    -1,    -1,    -1,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    -1,    -1,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    -1,
      47,    48,    -1,    -1,    -1,    -1,    -1,    -1,    16,    17,
      18,    58,    -1,    -1,    -1,    62,    -1,    64,    -1,    -1,
      -1,    -1,    -1,    70,     1,    72,     3,     4,     5,     6,
      -1,     8,     9,    10,    11,    12,    -1,    -1,    -1,    -1,
      -1,    -1,    19,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    -1,    -1,    -1,    65,    -1,    -1,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    -1,
      47,    48,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    58,    -1,    -1,    -1,    62,    -1,    64,    -1,    -1,
      -1,    -1,    -1,    70,     1,    72,     3,     4,     5,     6,
      -1,     8,     9,    10,    11,    12,    -1,    -1,    -1,    -1,
      -1,    -1,    19,    20,    -1,    22,    23,    24,    -1,    -1,
      27,    28,    29,    30,    -1,    32,    33,    34,    -1,    -1,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    -1,
      47,    48,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    58,    -1,    -1,    -1,    62,    63,    64,    -1,    -1,
      -1,    68,    69,    70,     1,    -1,     3,     4,     5,     6,
      -1,     8,     9,    10,    11,    12,    -1,    -1,    -1,    -1,
      -1,    -1,    19,    20,    -1,    22,    -1,    -1,    -1,    -1,
      27,    28,    29,    30,    -1,    32,    33,    34,    -1,    -1,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    58,    -1,    -1,    -1,    62,    63,    64,    -1,    -1,
      -1,    68,    69,    70,     1,    -1,     3,     4,     5,     6,
      -1,     8,     9,    10,    11,    12,    -1,    -1,    -1,    -1,
      -1,    -1,    19,    20,    -1,    22,    -1,    -1,    -1,    -1,
      27,    28,    29,    30,    -1,    32,    33,    34,    -1,    -1,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    -1,
      47,    48,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    58,    -1,    -1,    -1,    62,    63,    64,    -1,    -1,
      -1,    68,    69,    70,     1,    -1,     3,     4,     5,     6,
      -1,     8,     9,    10,    11,    12,    -1,    -1,    -1,    -1,
      -1,    -1,    19,    20,    -1,    22,    -1,    -1,    -1,    -1,
      27,    28,    29,    30,    -1,    32,    33,    34,    -1,    -1,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    -1,
      47,    48,    -1,    -1,    -1,    14,    15,    16,    17,    18,
      -1,    58,    -1,    -1,    -1,    62,    63,    64,    -1,    -1,
      -1,    68,     1,    70,     3,     4,     5,     6,    -1,     8,
       9,    10,    11,    12,    -1,    -1,    -1,    -1,    -1,    -1,
      19,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    -1,    -1,    -1,    65,    -1,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      -1,    -1,    -1,    -1,    14,    15,    16,    17,    18,    58,
      59,    -1,    -1,    62,    -1,    64,    26,    -1,    -1,    -1,
       1,    70,     3,     4,     5,     6,    -1,     8,     9,    10,
      11,    12,    -1,    -1,    -1,    -1,    -1,    -1,    19,    -1,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    -1,    -1,    -1,    -1,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    -1,    -1,
      -1,    14,    15,    16,    17,    18,    -1,    58,    -1,    -1,
      -1,    62,    -1,    64,    65,    -1,    -1,    -1,     1,    70,
       3,     4,     5,     6,    -1,     8,     9,    10,    11,    12,
      -1,    -1,    -1,    -1,    -1,    -1,    19,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    -1,
      -1,    -1,    -1,    -1,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    -1,    47,    48,    -1,    -1,    -1,    -1,
      -1,    -1,    16,    17,    18,    58,    -1,    -1,    -1,    62,
      -1,    64,    65,    -1,    -1,    -1,    69,    70,     1,    -1,
       3,     4,     5,     6,    -1,     8,     9,    10,    11,    12,
      -1,    -1,    -1,    -1,    -1,    -1,    19,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    -1,    -1,
      -1,    -1,    -1,    -1,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    -1,    -1,    -1,    16,
      17,    18,    -1,    -1,    -1,    58,    16,    17,    18,    62,
      63,    64,    -1,    -1,    -1,    -1,     1,    70,     3,     4,
       5,     6,    -1,     8,     9,    10,    11,    12,    -1,    -1,
      -1,    -1,    -1,    -1,    19,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    -1,    56,    57,    58,    59,
      60,    61,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    -1,    47,    48,    -1,    -1,    -1,    -1,    -1,    16,
      17,    18,    -1,    58,    -1,    -1,    -1,    62,    -1,    64,
      -1,    -1,    -1,    -1,    69,    70,     1,    -1,     3,     4,
       5,     6,    -1,     8,     9,    10,    11,    12,    -1,    -1,
      -1,    -1,    -1,    -1,    19,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    -1,    47,    48,    -1,    -1,    16,    17,    18,    -1,
      -1,    -1,    -1,    58,    16,    17,    18,    62,    -1,    64,
      65,    -1,    -1,    -1,     1,    70,     3,     4,     5,     6,
      -1,     8,     9,    10,    11,    12,    -1,    -1,    -1,    -1,
      -1,    -1,    19,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    54,    55,    56,    57,    58,    59,    60,    61,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    -1,
      47,    48,    -1,    16,    17,    18,    -1,    -1,    -1,    -1,
      -1,    58,    59,    -1,    -1,    62,    -1,    64,    -1,    -1,
      -1,    -1,     1,    70,     3,     4,     5,     6,    -1,     8,
       9,    10,    11,    12,    -1,    -1,    -1,    -1,    -1,    -1,
      19,    54,    55,    56,    57,    58,    59,    60,    61,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    -1,    47,    48,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    56,    -1,    58,
      -1,    -1,    -1,    62,    -1,    64,    -1,    -1,    -1,    -1,
       1,    70,     3,     4,     5,     6,    -1,     8,     9,    10,
      11,    12,    -1,    -1,    -1,    -1,    -1,    -1,    19,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    -1,    47,    48,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    58,    -1,    -1,
      -1,    62,    63,    64,    -1,    -1,    -1,    -1,     1,    70,
       3,     4,     5,     6,    -1,     8,     9,    10,    11,    12,
      -1,    -1,    -1,    -1,    -1,    -1,    19,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    -1,    47,    48,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    58,    -1,    -1,    -1,    62,
      63,    64,    -1,    -1,    -1,    -1,     1,    70,     3,     4,
       5,     6,    -1,     8,     9,    10,    11,    12,    -1,    -1,
      -1,    -1,    -1,    -1,    19,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    -1,    47,    48,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    58,    -1,    -1,    -1,    62,    -1,    64,
      65,    -1,    -1,    -1,     1,    70,     3,     4,     5,     6,
      -1,     8,     9,    10,    11,    12,    -1,    -1,    -1,    -1,
      -1,    -1,    19,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    -1,
      47,    48,    14,    15,    16,    17,    18,    -1,    -1,    -1,
      -1,    58,    -1,    -1,    -1,    62,    -1,    64,    -1,    -1,
      -1,    -1,    -1,    70,    14,    15,    16,    17,    18,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      14,    15,    16,    17,    18,    67,    -1,    -1,    -1,    -1,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    14,    15,    16,    17,    18,    67,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    -1,    -1,
      -1,    -1,    -1,    67,    -1,    -1,    -1,    -1,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
     410,    -1,    -1,    -1,    -1,    67,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   425,   426,   427,   428,   429,
     430,   431,   432,   433,   434,   435,   436,   437,   438,    -1,
      -1,    -1,    -1,    -1,    -1,   445
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    74,    75,     0,     7,    77,    83,    86,    90,    98,
      99,    98,    63,    76,     6,    36,    46,    66,    93,    94,
     101,    59,    80,   102,   103,     5,    64,   154,   155,     8,
       9,    82,     8,    82,    63,    67,   154,    57,    63,     5,
      68,    13,    84,    80,   102,    65,   154,    91,     1,     3,
       4,     6,     8,     9,    10,    11,    12,    19,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    47,    48,    58,
      62,    64,    70,    78,    79,    82,   100,   133,   141,   142,
     146,   148,   149,   150,   153,   155,   158,   159,   160,    64,
      82,    89,    13,    37,   144,    64,    37,    64,   141,   142,
     141,   133,    82,   133,   135,   136,   137,   133,   138,   139,
     140,   132,   133,    66,    67,    64,   147,    64,   151,    64,
     133,   133,     6,    46,    59,    94,   132,    64,    37,   133,
      14,    15,    16,    17,    18,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    11,    12,    13,
      35,    71,    64,    64,     8,     9,    92,    94,    95,    96,
      97,    69,    98,    99,   133,    82,    64,   135,    47,    82,
      26,    69,    67,    66,    72,    67,    66,    67,    65,   137,
     133,    64,    68,   104,   145,   133,   145,    46,   135,     8,
     132,    80,    65,   143,    82,   133,   133,   133,   133,   133,
     133,   133,   133,   133,   133,   133,   133,   133,   133,   133,
     133,   133,   133,    82,    56,   132,   135,   135,    80,    65,
      26,    67,    80,    87,    88,    96,    65,    65,   136,   133,
      65,   140,    65,   133,    66,    67,    67,   132,   106,    67,
       8,    65,    65,    65,   132,    66,    64,   132,    25,    72,
      65,    65,    92,    85,    95,    82,    67,    63,    65,    65,
     133,    65,     1,    20,    22,    27,    28,    29,    30,    32,
      33,    34,    63,    94,   104,   105,   112,   113,   114,   116,
     118,   122,   126,   132,   134,   161,   133,   157,    64,    65,
     133,   135,    25,    72,    56,    72,   132,     1,    63,    81,
     104,    88,   104,    67,   152,    63,    64,    64,    64,   117,
      64,    64,    63,    63,    63,   132,   107,    69,   112,    63,
      67,    65,    67,   135,    65,    56,    72,   132,   132,    72,
     141,    65,   132,   132,   132,   113,    94,   109,   110,   124,
     125,   132,     8,     9,   109,   120,   121,    63,    80,   108,
     111,   133,    82,   156,    65,   132,    72,    72,   152,    65,
      65,    65,    27,    80,    13,    63,    67,    31,    92,    67,
      63,   152,    66,    72,   113,   127,   115,    64,    92,   133,
     124,   120,   133,    13,   111,    65,   133,    21,   162,    68,
     113,   132,    63,    65,   133,   113,   106,    65,   124,   123,
      23,    24,   129,    63,    65,   113,     3,    19,    58,    62,
      64,   130,   131,   154,    66,   113,   128,   129,   119,     3,
       3,     3,   131,    25,    66,    16,    17,    18,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,   128,
      69,   128,   113,    65,   130,    64,   131,   131,   131,   131,
     131,   131,   131,   131,   131,   131,   131,   131,   131,   131,
      66
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    73,    74,    75,    75,    76,    76,    77,    78,    79,
      80,    80,    81,    81,    81,    82,    82,    84,    85,    83,
      83,    83,    83,    83,    86,    87,    88,    88,    89,    89,
      91,    90,    92,    92,    93,    93,    94,    95,    95,    95,
      96,    96,    96,    97,    97,    98,    98,    99,   100,   101,
     101,   102,   102,   103,   103,   104,   105,   105,   105,   106,
     107,   106,   108,   108,   109,   110,   111,   111,   112,   112,
     112,   113,   113,   113,   113,   113,   113,   113,   113,   113,
     113,   115,   114,   117,   116,   119,   118,   120,   120,   120,
     121,   121,   123,   122,   124,   124,   125,   125,   127,   126,
     128,   128,   128,   129,   129,   129,   130,   130,   131,   131,
     131,   131,   131,   131,   131,   131,   131,   131,   131,   131,
     131,   131,   131,   131,   131,   131,   131,   132,   132,   133,
     133,   133,   133,   133,   133,   133,   133,   133,   133,   133,
     133,   133,   133,   133,   133,   133,   133,   133,   133,   133,
     133,   133,   133,   133,   133,   133,   133,   133,   133,   133,
     133,   133,   134,   134,   135,   135,   135,   136,   136,   137,
     137,   138,   138,   138,   139,   139,   140,   141,   142,   142,
     142,   142,   143,   142,   142,   142,   142,   142,   142,   142,
     142,   142,   142,   142,   142,   142,   144,   142,   142,   142,
     142,   142,   142,   145,   145,   147,   146,   148,   149,   151,
     150,   152,   152,   153,   154,   154,   154,   155,   155,   156,
     157,   157,   158,   158,   158,   158,   158,   158,   158,   159,
     159,   160,   160,   160,   160,   161,   162,   162
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     3,     0,     0,     1,     4,     1,     1,
       0,     1,     1,     1,     1,     1,     1,     0,     0,     9,
       3,     1,     1,     1,     2,     2,     1,     3,     0,     4,
       0,     7,     1,     1,     1,     2,     1,     2,     3,     1,
       0,     1,     2,     1,     3,     0,     2,     2,     4,     0,
       1,     1,     3,     2,     4,     4,     1,     1,     1,     0,
       0,     5,     2,     4,     3,     3,     1,     3,     0,     2,
       2,     2,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     0,     6,     0,     8,     0,    10,     1,     1,     1,
       1,     3,     0,     8,     0,     1,     1,     1,     0,    10,
       2,     2,     0,     3,     5,     2,     1,     1,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     1,     2,     2,     2,     1,     3,     3,
       3,     5,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     1,     1,     1,
       1,     1,     2,     3,     0,     1,     2,     1,     2,     1,
       3,     0,     1,     2,     1,     3,     3,     1,     1,     1,
       1,     1,     0,     5,     3,     6,     7,     8,     7,     5,
       6,     5,     4,     1,     3,     1,     0,     6,     3,     5,
       4,     4,     4,     1,     3,     0,     3,     7,     9,     0,
       3,     0,     3,     1,     1,     3,     3,     1,     2,     3,
       0,     3,     4,     4,     6,     4,     4,     6,     7,     3,
       3,     1,     2,     3,     3,     6,     0,     2
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YYUSE (yyoutput);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yytype], *yyvaluep);
# endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyo, yytype, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[+yyssp[yyi + 1 - yynrhs]],
                       &yyvsp[(yyi + 1) - (yynrhs)]
                                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
#  else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                yy_state_t *yyssp, int yytoken)
{
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Actual size of YYARG. */
  int yycount = 0;
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[+*yyssp];
      YYPTRDIFF_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
      yysize = yysize0;
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYPTRDIFF_T yysize1
                    = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
                    yysize = yysize1;
                  else
                    return 2;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    /* Don't count the "%s"s in the final size, but reserve room for
       the terminator.  */
    YYPTRDIFF_T yysize1 = yysize + (yystrlen (yyformat) - 2 * yycount) + 1;
    if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
      yysize = yysize1;
    else
      return 2;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss;
    yy_state_t *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYPTRDIFF_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
# undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 6:
#line 220 "grammar.y"
        {
	    yywarn(_("Extra ';' ignored."));
	}
#line 2107 "grammar.c"
    break;

  case 7:
#line 228 "grammar.y"
        {
	    object_t *ob;
	    inherit_t inherit;
	    int initializer;

	    (yyvsp[-3].type) |= global_modifiers;

	    if (var_defined)
		yyerror(_("Invalid inherit clause after variables declarations."));
	    ob = find_object2((yyvsp[-1].string));
	    if (ob == 0) {
		inherit_file = alloc_cstring((yyvsp[-1].string), "inherit");
		/* Return back to load_object() */
		YYACCEPT;
	    }
	    scratch_free((yyvsp[-1].string));

	    inherit.prog = ob->prog;
	    inherit.function_index_offset =
		mem_block[A_RUNTIME_FUNCTIONS].current_size /
		sizeof (runtime_function_u);
	    inherit.variable_index_offset =
		mem_block[A_VAR_TEMP].current_size /
		sizeof (variable_t);
	    inherit.type_mod = (yyvsp[-3].type);

	    add_to_mem_block(A_INHERITS, (char *)&inherit, sizeof inherit);
	    copy_variables(ob->prog, (yyvsp[-3].type));
	    copy_structures(ob->prog);
	    initializer = copy_functions(ob->prog, (yyvsp[-3].type));
	    if (initializer >= 0)
	      {
		/* initializer is an index into the object we're
		 * inheriting's function table; this finds the
		 * appropriate entry in our table and generates
		 * a call to it
		 */
		switch_to_block(A_INITIALIZER);
		generate_inherited_init_call(mem_block[A_INHERITS].current_size/sizeof(inherit_t) - 1, initializer);
		switch_to_block(A_PROGRAM);
	      }
	}
#line 2154 "grammar.c"
    break;

  case 8:
#line 274 "grammar.y"
        { CREATE_REAL((yyval.node), (yyvsp[0].real)); }
#line 2160 "grammar.c"
    break;

  case 9:
#line 279 "grammar.y"
        { CREATE_NUMBER((yyval.node), (yyvsp[0].number)); }
#line 2166 "grammar.c"
    break;

  case 10:
#line 283 "grammar.y"
                        { (yyval.type) = 0; }
#line 2172 "grammar.c"
    break;

  case 11:
#line 284 "grammar.y"
                        { (yyval.type) = TYPE_MOD_ARRAY; }
#line 2178 "grammar.c"
    break;

  case 12:
#line 288 "grammar.y"
                        { (yyval.node) = (yyvsp[0].decl).node; if (!(yyval.node)) CREATE_RETURN((yyval.node), 0); }
#line 2184 "grammar.c"
    break;

  case 13:
#line 289 "grammar.y"
                        { (yyval.node) = 0; }
#line 2190 "grammar.c"
    break;

  case 14:
#line 290 "grammar.y"
                        { (yyval.node) = 0; }
#line 2196 "grammar.c"
    break;

  case 15:
#line 294 "grammar.y"
                        { (yyval.string) = scratch_copy((yyvsp[0].ihe)->name); }
#line 2202 "grammar.c"
    break;

  case 17:
#line 300 "grammar.y"
        {
	    (yyvsp[-2].type) |= global_modifiers;
	    /* Handle type checking here so we know whether to typecheck
	       'argument' */
	    if ((yyvsp[-2].type) & ~NAME_TYPE_MOD) {
		exact_types = (yyvsp[-2].type) | (yyvsp[-1].type);
	    } else {
		if (pragmas & PRAGMA_STRICT_TYPES) {
		    if (strcmp((yyvsp[0].string), "create") != 0)
			yyerror(_("\"#pragma strict_types\" requires type of function"));
		    else
			exact_types = TYPE_VOID; /* default for create() */
		} else
		    exact_types = 0;
	    }
	}
#line 2223 "grammar.c"
    break;

  case 18:
#line 317 "grammar.y"
        {
	    char *p = (yyvsp[-4].string);
	    (yyvsp[-4].string) = make_shared_string((yyvsp[-4].string));
	    scratch_free(p);

	    /* (If we had nested functions, we would need to check
	     * here if we have enough space for locals)
	     *
	     * Define a prototype. If it is a real function, then the
	     * prototype will be replaced below.
	     */

	    (yyval.number) = NAME_UNDEFINED | NAME_PROTOTYPE;
	    if ((yyvsp[-1].argument).flags & ARG_IS_VARARGS) {
		(yyval.number) |= NAME_TRUE_VARARGS;
		(yyvsp[-6].type) |= NAME_VARARGS;
	    }
	    define_new_function((yyvsp[-4].string), (yyvsp[-1].argument).num_arg, 0, (yyval.number), (yyvsp[-6].type) | (yyvsp[-5].type));
	    /* This is safe since it is guaranteed to be in the
	       function table, so it can't be dangling */
	    free_string((yyvsp[-4].string)); 
	    context = 0;
	}
#line 2251 "grammar.c"
    break;

  case 19:
#line 341 "grammar.y"
        {
		/* Either a prototype or a block */
		if ((yyvsp[0].node)) {
		    int fun;

		    (yyvsp[-1].number) &= ~(NAME_UNDEFINED | NAME_PROTOTYPE);
		    if ((yyvsp[0].node)->kind != NODE_RETURN &&
			((yyvsp[0].node)->kind != NODE_TWO_VALUES
			 || (yyvsp[0].node)->r.expr->kind != NODE_RETURN)) {
			parse_node_t *replacement;
			CREATE_STATEMENTS(replacement, (yyvsp[0].node), 0);
			CREATE_RETURN(replacement->r.expr, 0);
			(yyvsp[0].node) = replacement;
		    }
		    if ((yyvsp[-3].argument).flags & ARG_IS_PROTO) {
			yyerror("Missing name for function argument");
		    }
		    fun = define_new_function((yyvsp[-6].string), (yyvsp[-3].argument).num_arg, 
					      max_num_locals - (yyvsp[-3].argument).num_arg,
					      (yyvsp[-1].number), (yyvsp[-8].type) | (yyvsp[-7].type));
		    if (fun != -1)
			COMPILER_FUNC(fun)->address =
			    generate_function(COMPILER_FUNC(fun), (yyvsp[0].node), max_num_locals);
		}
		free_all_local_names();
	    }
#line 2282 "grammar.c"
    break;

  case 20:
#line 368 "grammar.y"
            {
		if (!(yyvsp[-2].type)) yyerror("Missing type for global variable declaration");
	    }
#line 2290 "grammar.c"
    break;

  case 24:
#line 377 "grammar.y"
                                { global_modifiers = (yyvsp[-1].type); }
#line 2296 "grammar.c"
    break;

  case 25:
#line 382 "grammar.y"
        {
	    if ((current_type & ~NAME_TYPE_MOD) == TYPE_VOID)
		yyerror("Illegal to declare class member of type void.");
	    add_local_name((yyvsp[0].string), current_type | (yyvsp[-1].type));
	    scratch_free((yyvsp[0].string));
	}
#line 2307 "grammar.c"
    break;

  case 30:
#line 402 "grammar.y"
        {
	    ident_hash_elem_t *ihe;

	    ihe = find_or_add_ident(
		PROG_STRING((yyval.number) = store_prog_string((yyvsp[-1].string))),
		FOA_GLOBAL_SCOPE);
	    if (ihe->dn.class_num == -1)
		ihe->sem_value++;
	    else {
		/* Possibly, this should check if the definitions are
		   consistent */
		char buf[256];
		char *end = EndOf(buf);
		char *p;
		    
		p = strput(buf, end, "Illegal to redefine class ");
		p = strput(p, end, (yyvsp[-1].string));
		yyerror(buf);
	    }
	    ihe->dn.class_num = mem_block[A_CLASS_DEF].current_size / sizeof(class_def_t);
	}
#line 2333 "grammar.c"
    break;

  case 31:
#line 424 "grammar.y"
        {
	    class_def_t *sd;
	    class_member_entry_t *sme;
	    int i;

	    sd = (class_def_t *)allocate_in_mem_block(A_CLASS_DEF, sizeof(class_def_t));
	    i = sd->size = current_number_of_locals;
	    sd->index = mem_block[A_CLASS_MEMBER].current_size / sizeof(class_member_entry_t);
	    sd->name = (yyvsp[-2].number);

	    sme = (class_member_entry_t *)allocate_in_mem_block(A_CLASS_MEMBER, sizeof(class_member_entry_t) * current_number_of_locals);

	    while (i--) {
		sme[i].name = store_prog_string(locals_ptr[i]->name);
		sme[i].type = type_of_locals_ptr[i];
	    }

	    free_all_local_names();
	    scratch_free((yyvsp[-4].string));
	}
#line 2358 "grammar.c"
    break;

  case 33:
#line 449 "grammar.y"
        {
	    if ((yyvsp[0].ihe)->dn.local_num != -1) {
		char buff[256];
		char *end = EndOf(buff);
		char *p;
		    
		p = strput(buff, end, "Illegal to redeclare local name '");
		p = strput(p, end, (yyvsp[0].ihe)->name);
		p = strput(p, end, "'");
		yyerror(buff);
	    }
	    (yyval.string) = scratch_copy((yyvsp[0].ihe)->name);
	}
#line 2376 "grammar.c"
    break;

  case 35:
#line 467 "grammar.y"
        {
	    if ((yyvsp[0].ihe)->dn.class_num == -1) {
		char buf[256];
		char *end = EndOf(buf);
		char *p;
		    
		p = strput(buf, end, "Undefined class '");
		p = strput(p, end, (yyvsp[0].ihe)->name);
		p = strput(p, end, "'");
		yyerror(buf);
		(yyval.type) = TYPE_ANY;
	    } else 
		(yyval.type) = (yyvsp[0].ihe)->dn.class_num | TYPE_MOD_CLASS;
	}
#line 2395 "grammar.c"
    break;

  case 37:
#line 489 "grammar.y"
        {
	    if ((yyvsp[-1].type) == TYPE_VOID)
		yyerror("Illegal to declare argument of type void.");
            (yyval.number) = ARG_IS_PROTO;
            add_local_name("", (yyvsp[-1].type) | (yyvsp[0].type));
	}
#line 2406 "grammar.c"
    break;

  case 38:
#line 496 "grammar.y"
        {
	    if ((yyvsp[-2].type) == TYPE_VOID)
		yyerror("Illegal to declare argument of type void.");
            add_local_name((yyvsp[0].string), (yyvsp[-2].type) | (yyvsp[-1].type));
	    scratch_free((yyvsp[0].string));
	    (yyval.number) = 0;
	}
#line 2418 "grammar.c"
    break;

  case 39:
#line 504 "grammar.y"
        {
	    if (exact_types) yyerror("Missing type for argument");
	    add_local_name((yyvsp[0].string), TYPE_ANY);
	    scratch_free((yyvsp[0].string));
	    (yyval.number) = 0;
	}
#line 2429 "grammar.c"
    break;

  case 40:
#line 514 "grammar.y"
        {
	    (yyval.argument).num_arg = 0;
	    (yyval.argument).flags = 0;
	}
#line 2438 "grammar.c"
    break;

  case 42:
#line 520 "grammar.y"
        {
	    int x = type_of_locals_ptr[max_num_locals-1];

	    (yyval.argument) = (yyvsp[-1].argument);
	    (yyval.argument).flags |= ARG_IS_VARARGS;

	    if (x != TYPE_ANY && !(x & TYPE_MOD_ARRAY))
		yywarn("Variable to hold remainder of arguments should be an array.");
	}
#line 2452 "grammar.c"
    break;

  case 43:
#line 533 "grammar.y"
        {
	    (yyval.argument).num_arg = 1;
	    (yyval.argument).flags = (yyvsp[0].number);
	}
#line 2461 "grammar.c"
    break;

  case 44:
#line 538 "grammar.y"
        {
	    (yyval.argument) = (yyvsp[-2].argument);
	    (yyval.argument).num_arg++;
	    (yyval.argument).flags |= (yyvsp[0].number);
	}
#line 2471 "grammar.c"
    break;

  case 45:
#line 546 "grammar.y"
                                                { (yyval.type) = 0; }
#line 2477 "grammar.c"
    break;

  case 46:
#line 547 "grammar.y"
                                                { (yyval.type) = (yyvsp[-1].type) | (yyvsp[0].type); }
#line 2483 "grammar.c"
    break;

  case 47:
#line 552 "grammar.y"
        {
	    (yyval.type) = (yyvsp[-1].type) | (yyvsp[0].type);
	    current_type = (yyval.type);
	}
#line 2492 "grammar.c"
    break;

  case 48:
#line 559 "grammar.y"
                                                { (yyval.type) = (yyvsp[-2].type) | (yyvsp[-1].type); }
#line 2498 "grammar.c"
    break;

  case 49:
#line 563 "grammar.y"
                        { (yyval.type) = TYPE_UNKNOWN; }
#line 2504 "grammar.c"
    break;

  case 53:
#line 574 "grammar.y"
            {
		if ((current_type & ~NAME_TYPE_MOD) == TYPE_VOID)
		    yyerror("Illegal to declare global variable of type void.");
		define_new_variable((yyvsp[0].string), current_type | (yyvsp[-1].type) | global_modifiers);
		scratch_free((yyvsp[0].string));
	    }
#line 2515 "grammar.c"
    break;

  case 54:
#line 581 "grammar.y"
            {
		parse_node_t *expr;
		int type = 0;
		
		if ((yyvsp[-1].number) != F_ASSIGN)
		    yyerror("Only '=' is legal in initializers.");

		/* ignore current_type == 0, which gets a missing type error
		   later anyway */
		if (current_type) {
		    type = (current_type | (yyvsp[-3].type) | global_modifiers) & ~NAME_TYPE_MOD;
		    if ((current_type & ~NAME_TYPE_MOD) == TYPE_VOID)
			yyerror("Illegal to declare global variable of type void.");
		    if (!compatible_types(type, (yyvsp[0].node)->type)) {
			char buff[256];
			char *end = EndOf(buff);
			char *p;
			
			p = strput(buff, end, "Type mismatch ");
			p = get_two_types(p, end, type, (yyvsp[0].node)->type);
			p = strput(p, end, " when initializing ");
			p = strput(p, end, (yyvsp[-2].string));
			yyerror(buff);
		    }
		}
		switch_to_block(A_INITIALIZER);
		(yyvsp[0].node) = do_promotions((yyvsp[0].node), type);

		CREATE_BINARY_OP(expr, F_VOID_ASSIGN, 0, (yyvsp[0].node), 0);
		CREATE_OPCODE_1(expr->r.expr, F_GLOBAL_LVALUE, 0,
				define_new_variable((yyvsp[-2].string), current_type | (yyvsp[-3].type) | global_modifiers));
		generate(expr);
		switch_to_block(A_PROGRAM);
		scratch_free((yyvsp[-2].string));
	    }
#line 2555 "grammar.c"
    break;

  case 55:
#line 620 "grammar.y"
            {
		if ((yyvsp[-2].decl).node && (yyvsp[-1].node)) {
		    CREATE_STATEMENTS((yyval.decl).node, (yyvsp[-2].decl).node, (yyvsp[-1].node));
		} else (yyval.decl).node = ((yyvsp[-2].decl).node ? (yyvsp[-2].decl).node : (yyvsp[-1].node));
                (yyval.decl).num = (yyvsp[-2].decl).num;
            }
#line 2566 "grammar.c"
    break;

  case 59:
#line 636 "grammar.y"
        {
	    (yyval.decl).node = 0;
	    (yyval.decl).num = 0;
	}
#line 2575 "grammar.c"
    break;

  case 60:
#line 641 "grammar.y"
        {
	    if ((yyvsp[0].type) == TYPE_VOID)
		yyerror("Illegal to declare local variable of type void.");
	    /* can't do this in basic_type b/c local_name_list contains
	     * expr0 which contains cast which contains basic_type
	     */
	    current_type = (yyvsp[0].type);
	}
#line 2588 "grammar.c"
    break;

  case 61:
#line 650 "grammar.y"
        {
	    if ((yyvsp[-4].decl).node && (yyvsp[-1].decl).node) {
		CREATE_STATEMENTS((yyval.decl).node, (yyvsp[-4].decl).node, (yyvsp[-1].decl).node);
	    } else (yyval.decl).node = ((yyvsp[-4].decl).node ? (yyvsp[-4].decl).node : (yyvsp[-1].decl).node);
		(yyval.decl).num = (yyvsp[-4].decl).num + (yyvsp[-1].decl).num;
	}
#line 2599 "grammar.c"
    break;

  case 62:
#line 661 "grammar.y"
        {
	    add_local_name((yyvsp[0].string), current_type | (yyvsp[-1].type));
	    scratch_free((yyvsp[0].string));
	    (yyval.node) = 0;
	}
#line 2609 "grammar.c"
    break;

  case 63:
#line 667 "grammar.y"
        {
	    int type = (current_type | (yyvsp[-3].type)) & ~NAME_TYPE_MOD;

	    if ((yyvsp[-1].number) != F_ASSIGN)
		yyerror("Only '=' is allowed in initializers.");
	    if (!compatible_types((yyvsp[0].node)->type, type)) {
		char buff[256];
		char *end = EndOf(buff);
		char *p;
		    
		p = strput(buff, end, "Type mismatch ");
		p = get_two_types(p, end, type, (yyvsp[0].node)->type);
		p = strput(p, end, " when initializing ");
		p = strput(p, end, (yyvsp[-2].string));

		yyerror(buff);
	    }

	    (yyvsp[0].node) = do_promotions((yyvsp[0].node), type);

	    CREATE_UNARY_OP_1((yyval.node), F_VOID_ASSIGN_LOCAL, 0, (yyvsp[0].node),
			add_local_name((yyvsp[-2].string), current_type | (yyvsp[-3].type)));
	    scratch_free((yyvsp[-2].string));
	}
#line 2638 "grammar.c"
    break;

  case 64:
#line 695 "grammar.y"
            {
		if ((yyvsp[-2].type) == TYPE_VOID)
		    yyerror("Illegal to declare local variable of type void.");

		(yyval.number) = add_local_name((yyvsp[0].string), (yyvsp[-2].type) | (yyvsp[-1].type));
		scratch_free((yyvsp[0].string));
	    }
#line 2650 "grammar.c"
    break;

  case 65:
#line 706 "grammar.y"
            {
                int type = type_of_locals_ptr[(yyvsp[-2].number)];

		if ((yyvsp[-1].number) != F_ASSIGN)
		    yyerror("Only '=' is allowed in initializers.");
		if (!compatible_types((yyvsp[0].node)->type, type)) {
		    char buff[256];
		    char *end = EndOf(buff);
		    char *p;
		    
		    p = strput(buff, end, "Type mismatch ");
		    p = get_two_types(p, end, type, (yyvsp[0].node)->type);
		    p = strput(p, end, " when initializing.");
		    yyerror(buff);
		}

		(yyvsp[0].node) = do_promotions((yyvsp[0].node), type);

		/* this is an expression */
		CREATE_BINARY_OP((yyval.node), F_ASSIGN, 0, (yyvsp[0].node), 0);
                CREATE_OPCODE_1((yyval.node)->r.expr, F_LOCAL_LVALUE, 0, (yyvsp[-2].number));
	    }
#line 2677 "grammar.c"
    break;

  case 66:
#line 732 "grammar.y"
            {
                (yyval.decl).node = (yyvsp[0].node);
                (yyval.decl).num = 1;
            }
#line 2686 "grammar.c"
    break;

  case 67:
#line 737 "grammar.y"
            {
                if ((yyvsp[-2].node) && (yyvsp[0].decl).node) {
		    CREATE_STATEMENTS((yyval.decl).node, (yyvsp[-2].node), (yyvsp[0].decl).node);
                } else (yyval.decl).node = ((yyvsp[-2].node) ? (yyvsp[-2].node) : (yyvsp[0].decl).node);
                (yyval.decl).num = 1 + (yyvsp[0].decl).num;
            }
#line 2697 "grammar.c"
    break;

  case 68:
#line 746 "grammar.y"
                                { (yyval.node) = 0; }
#line 2703 "grammar.c"
    break;

  case 69:
#line 748 "grammar.y"
        {
	    if ((yyvsp[-1].node) && (yyvsp[0].node)) {
		CREATE_STATEMENTS((yyval.node), (yyvsp[-1].node), (yyvsp[0].node));
	    } else (yyval.node) = ((yyvsp[-1].node) ? (yyvsp[-1].node) : (yyvsp[0].node));
	}
#line 2713 "grammar.c"
    break;

  case 70:
#line 753 "grammar.y"
                                { (yyval.node) = 0; }
#line 2719 "grammar.c"
    break;

  case 71:
#line 757 "grammar.y"
                                { (yyval.node) = insert_pop_value((yyvsp[-1].node)); }
#line 2725 "grammar.c"
    break;

  case 77:
#line 764 "grammar.y"
           {
                (yyval.node) = (yyvsp[0].decl).node;
                pop_n_locals((yyvsp[0].decl).num);
            }
#line 2734 "grammar.c"
    break;

  case 78:
#line 769 "grammar.y"
            {
		(yyval.node) = 0;
	    }
#line 2742 "grammar.c"
    break;

  case 79:
#line 773 "grammar.y"
            {
		if (context & SPECIAL_CONTEXT) {
		    yyerror("Cannot break out of catch { } or time_expression { }");
		    (yyval.node) = 0;
		} else
		if (context & SWITCH_CONTEXT) {
		    CREATE_CONTROL_JUMP((yyval.node), CJ_BREAK_SWITCH);
		} else
		if (context & LOOP_CONTEXT) {
		    CREATE_CONTROL_JUMP((yyval.node), CJ_BREAK);
		    if (context & LOOP_FOREACH) {
			parse_node_t *replace;
			CREATE_STATEMENTS(replace, 0, (yyval.node));
			CREATE_OPCODE(replace->l.expr, F_EXIT_FOREACH, 0);
			(yyval.node) = replace;
		    }
		} else {
		    yyerror("break statement outside loop");
		    (yyval.node) = 0;
		}
	    }
#line 2768 "grammar.c"
    break;

  case 80:
#line 795 "grammar.y"
            {
		if (context & SPECIAL_CONTEXT)
		    yyerror("Cannot continue out of catch { } or time_expression { }");
		else
		if (!(context & LOOP_CONTEXT))
		    yyerror("continue statement outside loop");
		CREATE_CONTROL_JUMP((yyval.node), CJ_CONTINUE);
	    }
#line 2781 "grammar.c"
    break;

  case 81:
#line 807 "grammar.y"
            {
		(yyvsp[-3].number) = context;
		context = LOOP_CONTEXT;
	    }
#line 2790 "grammar.c"
    break;

  case 82:
#line 812 "grammar.y"
            {
		CREATE_LOOP((yyval.node), 1, (yyvsp[0].node), 0, optimize_loop_test((yyvsp[-3].node)));
		context = (yyvsp[-5].number);
	    }
#line 2799 "grammar.c"
    break;

  case 83:
#line 820 "grammar.y"
            {
		(yyvsp[0].number) = context;
		context = LOOP_CONTEXT;
	    }
#line 2808 "grammar.c"
    break;

  case 84:
#line 825 "grammar.y"
            {
		CREATE_LOOP((yyval.node), 0, (yyvsp[-5].node), 0, optimize_loop_test((yyvsp[-2].node)));
		context = (yyvsp[-7].number);
	    }
#line 2817 "grammar.c"
    break;

  case 85:
#line 833 "grammar.y"
            {
		(yyvsp[-7].number) = context;
		context = LOOP_CONTEXT;
	    }
#line 2826 "grammar.c"
    break;

  case 86:
#line 838 "grammar.y"
            {
		(yyval.decl).num = (yyvsp[-7].decl).num; /* number of declarations (0/1) */
		
		(yyvsp[-7].decl).node = insert_pop_value((yyvsp[-7].decl).node);
		(yyvsp[-3].node) = insert_pop_value((yyvsp[-3].node));
		if ((yyvsp[-3].node) && IS_NODE((yyvsp[-3].node), NODE_UNARY_OP, F_INC)
		    && IS_NODE((yyvsp[-3].node)->r.expr, NODE_OPCODE_1, F_LOCAL_LVALUE)) {
		    int lvar = (yyvsp[-3].node)->r.expr->l.number;
		    CREATE_OPCODE_1((yyvsp[-3].node), F_LOOP_INCR, 0, lvar);
		}

		CREATE_STATEMENTS((yyval.decl).node, (yyvsp[-7].decl).node, 0);
		CREATE_LOOP((yyval.decl).node->r.expr, 1, (yyvsp[0].node), (yyvsp[-3].node), optimize_loop_test((yyvsp[-5].node)));

		context = (yyvsp[-9].number);
	      }
#line 2847 "grammar.c"
    break;

  case 87:
#line 857 "grammar.y"
            {
		if ((yyvsp[0].ihe)->dn.local_num != -1) {
		    CREATE_OPCODE_1((yyval.decl).node, F_LOCAL_LVALUE, 0, (yyvsp[0].ihe)->dn.local_num);
		} else
	        if ((yyvsp[0].ihe)->dn.global_num != -1) {
		    CREATE_OPCODE_1((yyval.decl).node, F_GLOBAL_LVALUE, 0, (yyvsp[0].ihe)->dn.global_num);
		} else {
		    char buf[256];
		    char *end = EndOf(buf);
		    char *p;

		    p = strput(buf, end, "'");
		    p = strput(p, end, (yyvsp[0].ihe)->name);
		    p = strput(p, end, "' is not a local or a global variable.");
		    yyerror(buf);
		    CREATE_OPCODE_1((yyval.decl).node, F_GLOBAL_LVALUE, 0, 0);
		}
		(yyval.decl).num = 0;
            }
#line 2871 "grammar.c"
    break;

  case 88:
#line 877 "grammar.y"
            {
                CREATE_OPCODE_1((yyval.decl).node, F_LOCAL_LVALUE, 0, (yyvsp[0].number));
		(yyval.decl).num = 1;
            }
#line 2880 "grammar.c"
    break;

  case 89:
#line 882 "grammar.y"
            {
		char buf[256];
		char *end = EndOf(buf);
		char *p;
		
		p = strput(buf, end, "'");
		p = strput(p, end, (yyvsp[0].string));
		p = strput(p, end, "' is not a local or a global variable.");
		yyerror(buf);
		CREATE_OPCODE_1((yyval.decl).node, F_GLOBAL_LVALUE, 0, 0);
		scratch_free((yyvsp[0].string));
		(yyval.decl).num = 0;
	    }
#line 2898 "grammar.c"
    break;

  case 90:
#line 899 "grammar.y"
            {
		CREATE_FOREACH((yyval.decl).node, (yyvsp[0].decl).node, 0);
		(yyval.decl).num = (yyvsp[0].decl).num;
            }
#line 2907 "grammar.c"
    break;

  case 91:
#line 904 "grammar.y"
            {
		CREATE_FOREACH((yyval.decl).node, (yyvsp[-2].decl).node, (yyvsp[0].decl).node);
		(yyval.decl).num = (yyvsp[-2].decl).num + (yyvsp[0].decl).num;
            }
#line 2916 "grammar.c"
    break;

  case 92:
#line 912 "grammar.y"
            {
		(yyvsp[-3].decl).node->v.expr = (yyvsp[-1].node);
		(yyvsp[-5].number) = context;
		context = LOOP_CONTEXT | LOOP_FOREACH;
            }
#line 2926 "grammar.c"
    break;

  case 93:
#line 918 "grammar.y"
            {
		(yyval.decl).num = (yyvsp[-5].decl).num;

		CREATE_STATEMENTS((yyval.decl).node, (yyvsp[-5].decl).node, 0);
		CREATE_LOOP((yyval.decl).node->r.expr, 2, (yyvsp[0].node), 0, 0);
		CREATE_OPCODE((yyval.decl).node->r.expr->r.expr, F_NEXT_FOREACH, 0);
		
		context = (yyvsp[-7].number);
	    }
#line 2940 "grammar.c"
    break;

  case 94:
#line 931 "grammar.y"
            {
		CREATE_NUMBER((yyval.node), 1);
	    }
#line 2948 "grammar.c"
    break;

  case 96:
#line 939 "grammar.y"
            {
	 	(yyval.decl).node = (yyvsp[0].node);
		(yyval.decl).num = 0;
	    }
#line 2957 "grammar.c"
    break;

  case 97:
#line 944 "grammar.y"
            {
		(yyval.decl).node = (yyvsp[0].node);
		(yyval.decl).num = 1;
	    }
#line 2966 "grammar.c"
    break;

  case 98:
#line 952 "grammar.y"
            {
                (yyvsp[-3].number) = context;
                context &= LOOP_CONTEXT;
                context |= SWITCH_CONTEXT;
                (yyvsp[-2].number) = mem_block[A_CASES].current_size;
            }
#line 2977 "grammar.c"
    break;

  case 99:
#line 959 "grammar.y"
            {
                parse_node_t *node1, *node2;

                if ((yyvsp[-1].node)) {
		    CREATE_STATEMENTS(node1, (yyvsp[-2].node), (yyvsp[-1].node));
                } else node1 = (yyvsp[-2].node);

                if (context & SWITCH_STRINGS) {
                    NODE_NO_LINE(node2, NODE_SWITCH_STRINGS);
                } else if (context & SWITCH_RANGES) {
		    NODE_NO_LINE(node2, NODE_SWITCH_RANGES);
		} else {
                    NODE_NO_LINE(node2, NODE_SWITCH_NUMBERS);
                }
                node2->l.expr = (yyvsp[-7].node);
                node2->r.expr = node1;
                prepare_cases(node2, (yyvsp[-8].number));
                context = (yyvsp[-9].number);
		(yyval.node) = node2;
		pop_n_locals((yyvsp[-3].decl).num);
            }
#line 3003 "grammar.c"
    break;

  case 100:
#line 984 "grammar.y"
          {
               if ((yyvsp[0].node)){
		   CREATE_STATEMENTS((yyval.node), (yyvsp[-1].node), (yyvsp[0].node));
               } else (yyval.node) = (yyvsp[-1].node);
           }
#line 3013 "grammar.c"
    break;

  case 101:
#line 990 "grammar.y"
           {
               if ((yyvsp[0].node)){
		   CREATE_STATEMENTS((yyval.node), (yyvsp[-1].node), (yyvsp[0].node));
               } else (yyval.node) = (yyvsp[-1].node);
           }
#line 3023 "grammar.c"
    break;

  case 102:
#line 996 "grammar.y"
           {
               (yyval.node) = 0;
           }
#line 3031 "grammar.c"
    break;

  case 103:
#line 1004 "grammar.y"
            {
                (yyval.node) = (yyvsp[-1].node);
                (yyval.node)->v.expr = 0;

                add_to_mem_block(A_CASES, (char *)&((yyvsp[-1].node)), sizeof((yyvsp[-1].node)));
            }
#line 3042 "grammar.c"
    break;

  case 104:
#line 1011 "grammar.y"
            {
                if ( (yyvsp[-3].node)->kind != NODE_CASE_NUMBER
                    || (yyvsp[-1].node)->kind != NODE_CASE_NUMBER )
                    yyerror("String case labels not allowed as range bounds");
                if ((yyvsp[-3].node)->r.number > (yyvsp[-1].node)->r.number) break;

		context |= SWITCH_RANGES;

                (yyval.node) = (yyvsp[-3].node);
                (yyval.node)->v.expr = (yyvsp[-1].node);

                add_to_mem_block(A_CASES, (char *)&((yyvsp[-3].node)), sizeof((yyvsp[-3].node)));
            }
#line 3060 "grammar.c"
    break;

  case 105:
#line 1025 "grammar.y"
            {
                if (context & SWITCH_DEFAULT) {
                    yyerror("Duplicate default");
                    (yyval.node) = 0;
                    break;
                }
		(yyval.node) = new_node();
		(yyval.node)->kind = NODE_DEFAULT;
                (yyval.node)->v.expr = 0;
                add_to_mem_block(A_CASES, (char *)&((yyval.node)), sizeof((yyval.node)));
                context |= SWITCH_DEFAULT;
            }
#line 3077 "grammar.c"
    break;

  case 106:
#line 1041 "grammar.y"
            {
                if ((context & SWITCH_STRINGS) && (yyvsp[0].pointer_int))
                    yyerror("Mixed case label list not allowed");

                if ((yyvsp[0].pointer_int)) context |= SWITCH_NUMBERS;
		(yyval.node) = new_node();
		(yyval.node)->kind = NODE_CASE_NUMBER;
                (yyval.node)->r.expr = (parse_node_t *)(yyvsp[0].pointer_int);
            }
#line 3091 "grammar.c"
    break;

  case 107:
#line 1051 "grammar.y"
            {
		int str;
		
		str = store_prog_string((yyvsp[0].string));
                scratch_free((yyvsp[0].string));
                if (context & SWITCH_NUMBERS)
                    yyerror("Mixed case label list not allowed");
                context |= SWITCH_STRINGS;
		(yyval.node) = new_node();
		(yyval.node)->kind = NODE_CASE_STRING;
                (yyval.node)->r.number = str;
            }
#line 3108 "grammar.c"
    break;

  case 108:
#line 1067 "grammar.y"
            {
                (yyval.pointer_int) = (yyvsp[-2].pointer_int) | (yyvsp[0].pointer_int);
            }
#line 3116 "grammar.c"
    break;

  case 109:
#line 1071 "grammar.y"
            {
                (yyval.pointer_int) = (yyvsp[-2].pointer_int) ^ (yyvsp[0].pointer_int);
            }
#line 3124 "grammar.c"
    break;

  case 110:
#line 1075 "grammar.y"
            {
                (yyval.pointer_int) = (yyvsp[-2].pointer_int) & (yyvsp[0].pointer_int);
            }
#line 3132 "grammar.c"
    break;

  case 111:
#line 1079 "grammar.y"
            {
                (yyval.pointer_int) = (yyvsp[-2].pointer_int) == (yyvsp[0].pointer_int);
            }
#line 3140 "grammar.c"
    break;

  case 112:
#line 1083 "grammar.y"
            {
                (yyval.pointer_int) = (yyvsp[-2].pointer_int) != (yyvsp[0].pointer_int);
            }
#line 3148 "grammar.c"
    break;

  case 113:
#line 1087 "grammar.y"
            {
                switch((yyvsp[-1].number)){
                    case F_GE: (yyval.pointer_int) = (yyvsp[-2].pointer_int) >= (yyvsp[0].pointer_int); break;
                    case F_LE: (yyval.pointer_int) = (yyvsp[-2].pointer_int) <= (yyvsp[0].pointer_int); break;
                    case F_GT: (yyval.pointer_int) = (yyvsp[-2].pointer_int) >  (yyvsp[0].pointer_int); break;
                }
            }
#line 3160 "grammar.c"
    break;

  case 114:
#line 1095 "grammar.y"
            {
                (yyval.pointer_int) = (yyvsp[-2].pointer_int) < (yyvsp[0].pointer_int);
            }
#line 3168 "grammar.c"
    break;

  case 115:
#line 1099 "grammar.y"
            {
                (yyval.pointer_int) = (yyvsp[-2].pointer_int) << (yyvsp[0].pointer_int);
            }
#line 3176 "grammar.c"
    break;

  case 116:
#line 1103 "grammar.y"
            {
                (yyval.pointer_int) = (yyvsp[-2].pointer_int) >> (yyvsp[0].pointer_int);
            }
#line 3184 "grammar.c"
    break;

  case 117:
#line 1107 "grammar.y"
            {
                (yyval.pointer_int) = (yyvsp[-2].pointer_int) + (yyvsp[0].pointer_int);
            }
#line 3192 "grammar.c"
    break;

  case 118:
#line 1111 "grammar.y"
            {
                (yyval.pointer_int) = (yyvsp[-2].pointer_int) - (yyvsp[0].pointer_int);
            }
#line 3200 "grammar.c"
    break;

  case 119:
#line 1115 "grammar.y"
            {
                (yyval.pointer_int) = (yyvsp[-2].pointer_int) * (yyvsp[0].pointer_int);
            }
#line 3208 "grammar.c"
    break;

  case 120:
#line 1119 "grammar.y"
            {
                if ((yyvsp[0].pointer_int)) (yyval.pointer_int) = (yyvsp[-2].pointer_int) % (yyvsp[0].pointer_int); else yyerror("Modulo by zero");
            }
#line 3216 "grammar.c"
    break;

  case 121:
#line 1123 "grammar.y"
            {
                if ((yyvsp[0].pointer_int)) (yyval.pointer_int) = (yyvsp[-2].pointer_int) / (yyvsp[0].pointer_int); else yyerror("Division by zero");
            }
#line 3224 "grammar.c"
    break;

  case 122:
#line 1127 "grammar.y"
            {
                (yyval.pointer_int) = (yyvsp[-1].pointer_int);
            }
#line 3232 "grammar.c"
    break;

  case 123:
#line 1131 "grammar.y"
            {
		(yyval.pointer_int) = (yyvsp[0].number);
	    }
#line 3240 "grammar.c"
    break;

  case 124:
#line 1135 "grammar.y"
            {
                (yyval.pointer_int) = -(yyvsp[0].number);
            }
#line 3248 "grammar.c"
    break;

  case 125:
#line 1139 "grammar.y"
            {
                (yyval.pointer_int) = !(yyvsp[0].number);
            }
#line 3256 "grammar.c"
    break;

  case 126:
#line 1143 "grammar.y"
            {
                (yyval.pointer_int) = ~(yyvsp[0].number);
            }
#line 3264 "grammar.c"
    break;

  case 127:
#line 1150 "grammar.y"
            {
		(yyval.node) = (yyvsp[0].node);
	    }
#line 3272 "grammar.c"
    break;

  case 128:
#line 1154 "grammar.y"
            {
		CREATE_TWO_VALUES((yyval.node), (yyvsp[0].node)->type, insert_pop_value((yyvsp[-2].node)), (yyvsp[0].node));
	    }
#line 3280 "grammar.c"
    break;

  case 129:
#line 1161 "grammar.y"
            {
	        parse_node_t *l = (yyvsp[-2].node), *r = (yyvsp[0].node);
		/* set this up here so we can change it below */
		
		CREATE_BINARY_OP((yyval.node), (yyvsp[-1].number), r->type, r, l);

		if (exact_types && !compatible_types(r->type, l->type) &&
		    !((yyvsp[-1].number) == F_ADD_EQ
		      && r->type == TYPE_STRING && 
		      COMP_TYPE(l->type, TYPE_NUMBER))) {
		    char buf[256];
		    char *end = EndOf(buf);
		    char *p;
		    
		    p = strput(buf, end, "Bad assignment ");
		    p = get_two_types(p, end, l->type, r->type);
		    p = strput(p, end, ".");
		    yyerror(buf);
		}

		(yyval.node)->l.expr = do_promotions(r, l->type);
	    }
#line 3307 "grammar.c"
    break;

  case 130:
#line 1184 "grammar.y"
            {
		yyerror("Illegal LHS");
		CREATE_ERROR((yyval.node));
	    }
#line 3316 "grammar.c"
    break;

  case 131:
#line 1189 "grammar.y"
            {
		parse_node_t *p1 = (yyvsp[-2].node), *p2 = (yyvsp[0].node);

		if (exact_types && !compatible_types2(p1->type, p2->type)) {
		    char buf[256];
		    char *end = EndOf(buf);
		    char *p;
		    
		    p = strput(buf, end, "Types in ?: do not match ");
		    p = get_two_types(p, end, p1->type, p2->type);
		    p = strput(p, end, ".");
		    yywarn(buf);
		}

		/* optimize if last expression did F_NOT */
		if (IS_NODE((yyvsp[-4].node), NODE_UNARY_OP, F_NOT)) {
		    /* !a ? b : c  --> a ? c : b */
		    CREATE_IF((yyval.node), (yyvsp[-4].node)->r.expr, p2, p1);
		} else {
		    CREATE_IF((yyval.node), (yyvsp[-4].node), p1, p2);
		}
		(yyval.node)->type = ((p1->type == p2->type) ? p1->type : TYPE_ANY);
	    }
#line 3344 "grammar.c"
    break;

  case 132:
#line 1213 "grammar.y"
            {
		CREATE_LAND_LOR((yyval.node), F_LOR, (yyvsp[-2].node), (yyvsp[0].node));
		if (IS_NODE((yyvsp[-2].node), NODE_LAND_LOR, F_LOR))
		    (yyvsp[-2].node)->kind = NODE_BRANCH_LINK;
	    }
#line 3354 "grammar.c"
    break;

  case 133:
#line 1219 "grammar.y"
            {
		CREATE_LAND_LOR((yyval.node), F_LAND, (yyvsp[-2].node), (yyvsp[0].node));
		if (IS_NODE((yyvsp[-2].node), NODE_LAND_LOR, F_LAND))
		    (yyvsp[-2].node)->kind = NODE_BRANCH_LINK;
	    }
#line 3364 "grammar.c"
    break;

  case 134:
#line 1225 "grammar.y"
            {
		if (is_boolean((yyvsp[-2].node)) && is_boolean((yyvsp[0].node)))
		    yywarn("bitwise operation on boolean values.");
		(yyval.node) = binary_int_op((yyvsp[-2].node), (yyvsp[0].node), F_OR, "|");		
	    }
#line 3374 "grammar.c"
    break;

  case 135:
#line 1231 "grammar.y"
            {
		(yyval.node) = binary_int_op((yyvsp[-2].node), (yyvsp[0].node), F_XOR, "^");
	    }
#line 3382 "grammar.c"
    break;

  case 136:
#line 1235 "grammar.y"
            {
		int t1 = (yyvsp[-2].node)->type, t3 = (yyvsp[0].node)->type;
		if (is_boolean((yyvsp[-2].node)) && is_boolean((yyvsp[0].node)))
		    yywarn("bitwise operation on boolean values.");
		if ((t1 & TYPE_MOD_ARRAY) || (t3 & TYPE_MOD_ARRAY)) {
		    if (t1 != t3) {
			if ((t1 != TYPE_ANY) && (t3 != TYPE_ANY) &&
			    !(t1 & t3 & TYPE_MOD_ARRAY)) {
			    char buf[256];
			    char *end = EndOf(buf);
			    char *p;
			    
			    p = strput(buf, end, "Incompatible types for & ");
			    p = get_two_types(p, end, t1, t3);
			    p = strput(p, end, ".");
			    yyerror(buf);
			}
			t1 = TYPE_ANY | TYPE_MOD_ARRAY;
		    } 
		    CREATE_BINARY_OP((yyval.node), F_AND, t1, (yyvsp[-2].node), (yyvsp[0].node));
		} else (yyval.node) = binary_int_op((yyvsp[-2].node), (yyvsp[0].node), F_AND, "&");
	    }
#line 3409 "grammar.c"
    break;

  case 137:
#line 1258 "grammar.y"
            {
		if (exact_types && !compatible_types2((yyvsp[-2].node)->type, (yyvsp[0].node)->type)){
		    char buf[256];
		    char *end = EndOf(buf);
		    char *p;
		    
		    p = strput(buf, end, "== always false because of incompatible types ");
		    p = get_two_types(p, end, (yyvsp[-2].node)->type, (yyvsp[0].node)->type);
		    p = strput(p, end, ".");
		    yyerror(buf);
		}
		/* x == 0 -> !x */
		if (IS_NODE((yyvsp[-2].node), NODE_NUMBER, 0)) {
		    CREATE_UNARY_OP((yyval.node), F_NOT, TYPE_NUMBER, (yyvsp[0].node));
		} else
		if (IS_NODE((yyvsp[0].node), NODE_NUMBER, 0)) {
		    CREATE_UNARY_OP((yyval.node), F_NOT, TYPE_NUMBER, (yyvsp[-2].node));
		} else {
		    CREATE_BINARY_OP((yyval.node), F_EQ, TYPE_NUMBER, (yyvsp[-2].node), (yyvsp[0].node));
		}
	    }
#line 3435 "grammar.c"
    break;

  case 138:
#line 1280 "grammar.y"
            {
		if (exact_types && !compatible_types2((yyvsp[-2].node)->type, (yyvsp[0].node)->type)){
		    char buf[256];
		    char *end = EndOf(buf);
		    char *p;

		    p = strput(buf, end, "!= always true because of incompatible types ");
		    p = get_two_types(p, end, (yyvsp[-2].node)->type, (yyvsp[0].node)->type);
		    p = strput(p, end, ".");
		    yyerror(buf);
		}
                CREATE_BINARY_OP((yyval.node), F_NE, TYPE_NUMBER, (yyvsp[-2].node), (yyvsp[0].node));
	    }
#line 3453 "grammar.c"
    break;

  case 139:
#line 1294 "grammar.y"
            {
		if (exact_types) {
		    int t1 = (yyvsp[-2].node)->type;
		    int t3 = (yyvsp[0].node)->type;

		    if (!COMP_TYPE(t1, TYPE_NUMBER) 
			&& !COMP_TYPE(t1, TYPE_STRING)) {
			char buf[256];
			char *end = EndOf(buf);
			char *p;
			
			p = strput(buf, end, "Bad left argument to '");
			p = strput(p, end, get_f_name((yyvsp[-1].number)));
			p = strput(p, end, "' : \"");
			p = get_type_name(p, end, t1);
			p = strput(p, end, "\"");
			yyerror(buf);
		    } else if (!COMP_TYPE(t3, TYPE_NUMBER) 
			       && !COMP_TYPE(t3, TYPE_STRING)) {
                        char buf[256];
			char *end = EndOf(buf);
			char *p;
			
                        p = strput(buf, end, "Bad right argument to '");
                        p = strput(p, end, get_f_name((yyvsp[-1].number)));
                        p = strput(p, end, "' : \"");
                        p = get_type_name(p, end, t3);
			p = strput(p, end, "\"");
			yyerror(buf);
		    } else if (!compatible_types2(t1,t3)) {
			char buf[256];
			char *end = EndOf(buf);
			char *p;
			
			p = strput(buf, end, "Arguments to ");
			p = strput(p, end, get_f_name((yyvsp[-1].number)));
			p = strput(p, end, " do not have compatible types : ");
			p = get_two_types(p, end, t1, t3);
			yyerror(buf);
		    }
		}
                CREATE_BINARY_OP((yyval.node), (yyvsp[-1].number), TYPE_NUMBER, (yyvsp[-2].node), (yyvsp[0].node));
	    }
#line 3501 "grammar.c"
    break;

  case 140:
#line 1338 "grammar.y"
            {
                if (exact_types) {
                    int t1 = (yyvsp[-2].node)->type, t3 = (yyvsp[0].node)->type;

                    if (!COMP_TYPE(t1, TYPE_NUMBER) 
			&& !COMP_TYPE(t1, TYPE_STRING)) {
                        char buf[256];
			char *end = EndOf(buf);
			char *p;
			
			p = strput(buf, end, "Bad left argument to '<' : \"");
                        p = get_type_name(p, end, t1);
			p = strput(p, end, "\"");
                        yyerror(buf);
                    } else if (!COMP_TYPE(t3, TYPE_NUMBER)
			       && !COMP_TYPE(t3, TYPE_STRING)) {
                        char buf[200];
			char *end = EndOf(buf);
			char *p;
			
                        p = strput(buf, end, "Bad right argument to '<' : \"");
                        p = get_type_name(p, end, t3);
                        p = strput(p, end, "\"");
                        yyerror(buf);
                    } else if (!compatible_types2(t1,t3)) {
                        char buf[256];
			char *end = EndOf(buf);
			char *p;
			
			p = strput(buf, end, "Arguments to < do not have compatible types : ");
			p = get_two_types(p, end, t1, t3);
                        yyerror(buf);
                    }
                }
                CREATE_BINARY_OP((yyval.node), F_LT, TYPE_NUMBER, (yyvsp[-2].node), (yyvsp[0].node));
            }
#line 3542 "grammar.c"
    break;

  case 141:
#line 1375 "grammar.y"
            {
		(yyval.node) = binary_int_op((yyvsp[-2].node), (yyvsp[0].node), F_LSH, "<<");
	    }
#line 3550 "grammar.c"
    break;

  case 142:
#line 1379 "grammar.y"
            {
		(yyval.node) = binary_int_op((yyvsp[-2].node), (yyvsp[0].node), F_RSH, ">>");
	    }
#line 3558 "grammar.c"
    break;

  case 143:
#line 1383 "grammar.y"
            {
		int result_type;

		if (exact_types) {
		    int t1 = (yyvsp[-2].node)->type, t3 = (yyvsp[0].node)->type;

		    if (t1 == t3){
#ifdef CAST_CALL_OTHERS
			if (t1 == TYPE_UNKNOWN){
			    yyerror("Bad arguments to '+' (unknown vs unknown)");
			    result_type = TYPE_ANY;
			} else
#endif
			    result_type = t1;
		    }
		    else if (t1 == TYPE_ANY) {
			if (t3 == TYPE_FUNCTION) {
			    yyerror("Bad right argument to '+' (function)");
			    result_type = TYPE_ANY;
			} else result_type = t3;
		    } else if (t3 == TYPE_ANY) {
			if (t1 == TYPE_FUNCTION) {
			    yyerror("Bad left argument to '+' (function)");
			    result_type = TYPE_ANY;
			} else result_type = t1;
		    } else {
			switch(t1) {
			    case TYPE_STRING:
			    {
				if (t3 == TYPE_REAL || t3 == TYPE_NUMBER){
				    result_type = TYPE_STRING;
				} else goto add_error;
				break;
			    }
			    case TYPE_NUMBER:
			    {
				if (t3 == TYPE_REAL || t3 == TYPE_STRING)
				    result_type = t3;
				else goto add_error;
				break;
			    }
			case TYPE_REAL:
			    {
				if (t3 == TYPE_NUMBER) result_type = TYPE_REAL;
				else if (t3 == TYPE_STRING) result_type = TYPE_STRING;
				else goto add_error;
				break;
			    }
			    default:
			    {
				if (t1 & t3 & TYPE_MOD_ARRAY) {
				    result_type = TYPE_ANY|TYPE_MOD_ARRAY;
				    break;
				}
add_error:
				{
				    char buf[256];
				    char *end = EndOf(buf);
				    char *p;
				    
				    p = strput(buf, end, "Invalid argument types to '+' ");
				    p = get_two_types(p, end, t1, t3);
				    yyerror(buf);
				    result_type = TYPE_ANY;
				}
			    }
			}
		    }
		} else 
		    result_type = TYPE_ANY;

		switch ((yyvsp[-2].node)->kind) {
		case NODE_NUMBER:
		    /* 0 + X */
		    if ((yyvsp[-2].node)->v.number == 0 &&
			((yyvsp[0].node)->type == TYPE_NUMBER || (yyvsp[0].node)->type == TYPE_REAL)) {
			(yyval.node) = (yyvsp[0].node);
			break;
		    }
		    if ((yyvsp[0].node)->kind == NODE_NUMBER) {
			(yyval.node) = (yyvsp[-2].node);
			(yyvsp[-2].node)->v.number += (yyvsp[0].node)->v.number;
			break;
		    }
		    if ((yyvsp[0].node)->kind == NODE_REAL) {
			(yyval.node) = (yyvsp[0].node);
			(yyvsp[0].node)->v.real += (yyvsp[-2].node)->v.number;
			break;
		    }
		    /* swapping the nodes may help later constant folding */
		    if ((yyvsp[0].node)->type != TYPE_STRING && (yyvsp[0].node)->type != TYPE_ANY)
			CREATE_BINARY_OP((yyval.node), F_ADD, result_type, (yyvsp[0].node), (yyvsp[-2].node));
		    else
			CREATE_BINARY_OP((yyval.node), F_ADD, result_type, (yyvsp[-2].node), (yyvsp[0].node));
		    break;
		case NODE_REAL:
		    if ((yyvsp[0].node)->kind == NODE_NUMBER) {
			(yyval.node) = (yyvsp[-2].node);
			(yyvsp[-2].node)->v.real += (yyvsp[0].node)->v.number;
			break;
		    }
		    if ((yyvsp[0].node)->kind == NODE_REAL) {
			(yyval.node) = (yyvsp[-2].node);
			(yyvsp[-2].node)->v.real += (yyvsp[0].node)->v.real;
			break;
		    }
		    /* swapping the nodes may help later constant folding */
		    if ((yyvsp[0].node)->type != TYPE_STRING && (yyvsp[0].node)->type != TYPE_ANY)
			CREATE_BINARY_OP((yyval.node), F_ADD, result_type, (yyvsp[0].node), (yyvsp[-2].node));
		    else
			CREATE_BINARY_OP((yyval.node), F_ADD, result_type, (yyvsp[-2].node), (yyvsp[0].node));
		    break;
		case NODE_STRING:
		    if ((yyvsp[0].node)->kind == NODE_STRING) {
			/* Combine strings */
			int n1, n2;
			char *new, *s1, *s2;
			int l;

			n1 = (yyvsp[-2].node)->v.number;
			n2 = (yyvsp[0].node)->v.number;
			s1 = PROG_STRING(n1);
			s2 = PROG_STRING(n2);
			new = (char *)DXALLOC( (l = strlen(s1))+strlen(s2)+1, TAG_COMPILER, "combine string" );
			strcpy(new, s1);
			strcat(new + l, s2);
			/* free old strings (ordering may help shrink table) */
			if (n1 > n2) {
			    free_prog_string(n1); free_prog_string(n2);
			} else {
			    free_prog_string(n2); free_prog_string(n1);
			}
			(yyval.node) = (yyvsp[-2].node);
			(yyval.node)->v.number = store_prog_string(new);
			FREE(new);
			break;
		    }
		    CREATE_BINARY_OP((yyval.node), F_ADD, result_type, (yyvsp[-2].node), (yyvsp[0].node));
		    break;
		default:
		    /* X + 0 */
		    if (IS_NODE((yyvsp[0].node), NODE_NUMBER, 0) &&
			((yyvsp[-2].node)->type == TYPE_NUMBER || (yyvsp[-2].node)->type == TYPE_REAL)) {
			(yyval.node) = (yyvsp[-2].node);
			break;
		    }
		    CREATE_BINARY_OP((yyval.node), F_ADD, result_type, (yyvsp[-2].node), (yyvsp[0].node));
		    break;
		}
	    }
#line 3713 "grammar.c"
    break;

  case 144:
#line 1534 "grammar.y"
            {
		int result_type;

		if (exact_types) {
		    int t1 = (yyvsp[-2].node)->type, t3 = (yyvsp[0].node)->type;

		    if (t1 == t3){
			switch(t1){
			    case TYPE_ANY:
			    case TYPE_NUMBER:
			    case TYPE_REAL:
			        result_type = t1;
				break;
			    default:
				if (!(t1 & TYPE_MOD_ARRAY)){
				    type_error("Bad argument number 1 to '-'", t1);
				    result_type = TYPE_ANY;
				} else result_type = t1;
			}
		    } else if (t1 == TYPE_ANY){
			switch(t3){
			    case TYPE_REAL:
			    case TYPE_NUMBER:
			        result_type = t3;
				break;
			    default:
				if (!(t3 & TYPE_MOD_ARRAY)){
				    type_error("Bad argument number 2 to '-'", t3);
				    result_type = TYPE_ANY;
				} else result_type = t3;
			}
		    } else if (t3 == TYPE_ANY){
			switch(t1){
			    case TYPE_REAL:
			    case TYPE_NUMBER:
			        result_type = t1;
				break;
			    default:
				if (!(t1 & TYPE_MOD_ARRAY)){
				    type_error("Bad argument number 1 to '-'", t1);
				    result_type = TYPE_ANY;
				} else result_type = t1;
			}
		    } else if ((t1 == TYPE_REAL && t3 == TYPE_NUMBER) ||
			       (t3 == TYPE_REAL && t1 == TYPE_NUMBER)){
			result_type = TYPE_REAL;
		    } else if (t1 & t3 & TYPE_MOD_ARRAY){
			result_type = TYPE_MOD_ARRAY|TYPE_ANY;
		    } else {
			char buf[256];
			char *end = EndOf(buf);
			char *p;
			
			p = strput(buf, end, "Invalid types to '-' ");
			p = get_two_types(p, end, t1, t3);
			yyerror(buf);
			result_type = TYPE_ANY;
		    }
		} else result_type = TYPE_ANY;
		
		switch ((yyvsp[-2].node)->kind) {
		case NODE_NUMBER:
		    if ((yyvsp[-2].node)->v.number == 0) {
			CREATE_UNARY_OP((yyval.node), F_NEGATE, (yyvsp[0].node)->type, (yyvsp[0].node));
		    } else if ((yyvsp[0].node)->kind == NODE_NUMBER) {
			(yyval.node) = (yyvsp[-2].node);
			(yyvsp[-2].node)->v.number -= (yyvsp[0].node)->v.number;
		    } else if ((yyvsp[0].node)->kind == NODE_REAL) {
			(yyval.node) = (yyvsp[0].node);
			(yyvsp[0].node)->v.real = (yyvsp[-2].node)->v.number - (yyvsp[0].node)->v.real;
		    } else {
			CREATE_BINARY_OP((yyval.node), F_SUBTRACT, result_type, (yyvsp[-2].node), (yyvsp[0].node));
		    }
		    break;
		case NODE_REAL:
		    if ((yyvsp[0].node)->kind == NODE_NUMBER) {
			(yyval.node) = (yyvsp[-2].node);
			(yyvsp[-2].node)->v.real -= (yyvsp[0].node)->v.number;
		    } else if ((yyvsp[0].node)->kind == NODE_REAL) {
			(yyval.node) = (yyvsp[-2].node);
			(yyvsp[-2].node)->v.real -= (yyvsp[0].node)->v.real;
		    } else {
			CREATE_BINARY_OP((yyval.node), F_SUBTRACT, result_type, (yyvsp[-2].node), (yyvsp[0].node));
		    }
		    break;
		default:
		    /* optimize X-0 */
		    if (IS_NODE((yyvsp[0].node), NODE_NUMBER, 0)) {
			(yyval.node) = (yyvsp[-2].node);
		    } 
		    CREATE_BINARY_OP((yyval.node), F_SUBTRACT, result_type, (yyvsp[-2].node), (yyvsp[0].node));
		}
	    }
#line 3811 "grammar.c"
    break;

  case 145:
#line 1628 "grammar.y"
            {
		int result_type;

		if (exact_types){
		    int t1 = (yyvsp[-2].node)->type, t3 = (yyvsp[0].node)->type;

		    if (t1 == t3){
			switch(t1){
			    case TYPE_MAPPING:
			    case TYPE_ANY:
			    case TYPE_NUMBER:
			    case TYPE_REAL:
			        result_type = t1;
				break;
			default:
				type_error("Bad argument number 1 to '*'", t1);
				result_type = TYPE_ANY;
			}
		    } else if (t1 == TYPE_ANY || t3 == TYPE_ANY){
			int t = (t1 == TYPE_ANY) ? t3 : t1;
			switch(t){
			    case TYPE_NUMBER:
			    case TYPE_REAL:
			    case TYPE_MAPPING:
			        result_type = t;
				break;
			    default:
				type_error((t1 == TYPE_ANY) ?
					   "Bad argument number 2 to '*'" :
					   "Bad argument number 1 to '*'",
					   t);
				result_type = TYPE_ANY;
			}
		    } else if ((t1 == TYPE_NUMBER && t3 == TYPE_REAL) ||
			       (t1 == TYPE_REAL && t3 == TYPE_NUMBER)){
			result_type = TYPE_REAL;
		    } else {
			char buf[256];
			char *end = EndOf(buf);
			char *p;
			
			p = strput(buf, end, "Invalid types to '*' ");
			p = get_two_types(p, end, t1, t3);
			yyerror(buf);
			result_type = TYPE_ANY;
		    }
		} else result_type = TYPE_ANY;

		switch ((yyvsp[-2].node)->kind) {
		case NODE_NUMBER:
		    if ((yyvsp[0].node)->kind == NODE_NUMBER) {
			(yyval.node) = (yyvsp[-2].node);
			(yyval.node)->v.number *= (yyvsp[0].node)->v.number;
			break;
		    }
		    if ((yyvsp[0].node)->kind == NODE_REAL) {
			(yyval.node) = (yyvsp[0].node);
			(yyvsp[0].node)->v.real *= (yyvsp[-2].node)->v.number;
			break;
		    }
		    CREATE_BINARY_OP((yyval.node), F_MULTIPLY, result_type, (yyvsp[0].node), (yyvsp[-2].node));
		    break;
		case NODE_REAL:
		    if ((yyvsp[0].node)->kind == NODE_NUMBER) {
			(yyval.node) = (yyvsp[-2].node);
			(yyvsp[-2].node)->v.real *= (yyvsp[0].node)->v.number;
			break;
		    }
		    if ((yyvsp[0].node)->kind == NODE_REAL) {
			(yyval.node) = (yyvsp[-2].node);
			(yyvsp[-2].node)->v.real *= (yyvsp[0].node)->v.real;
			break;
		    }
		    CREATE_BINARY_OP((yyval.node), F_MULTIPLY, result_type, (yyvsp[0].node), (yyvsp[-2].node));
		    break;
		default:
		    CREATE_BINARY_OP((yyval.node), F_MULTIPLY, result_type, (yyvsp[-2].node), (yyvsp[0].node));
		}
	    }
#line 3895 "grammar.c"
    break;

  case 146:
#line 1708 "grammar.y"
            {
		(yyval.node) = binary_int_op((yyvsp[-2].node), (yyvsp[0].node), F_MOD, "%");
	    }
#line 3903 "grammar.c"
    break;

  case 147:
#line 1712 "grammar.y"
            {
		int result_type;

		if (exact_types){
		    int t1 = (yyvsp[-2].node)->type, t3 = (yyvsp[0].node)->type;

		    if (t1 == t3){
			switch(t1){
			    case TYPE_NUMBER:
			    case TYPE_REAL:
			case TYPE_ANY:
			        result_type = t1;
				break;
			    default:
				type_error("Bad argument 1 to '/'", t1);
				result_type = TYPE_ANY;
			}
		    } else if (t1 == TYPE_ANY || t3 == TYPE_ANY){
			int t = (t1 == TYPE_ANY) ? t3 : t1;
			if (t == TYPE_REAL || t == TYPE_NUMBER)
			    result_type = t; 
			else {
			    type_error(t1 == TYPE_ANY ?
				       "Bad argument 2 to '/'" :
				       "Bad argument 1 to '/'", t);
			    result_type = TYPE_ANY;
			}
		    } else if ((t1 == TYPE_NUMBER && t3 == TYPE_REAL) ||
			       (t1 == TYPE_REAL && t3 == TYPE_NUMBER)) {
			result_type = TYPE_REAL;
		    } else {
			char buf[256];
			char *end = EndOf(buf);
			char *p;
			
			p = strput(buf, end, "Invalid types to '/' ");
			p = get_two_types(p, end, t1, t3);
			yyerror(buf);
			result_type = TYPE_ANY;
		    }
		} else result_type = TYPE_ANY;		    

		/* constant expressions */
		switch ((yyvsp[-2].node)->kind) {
		case NODE_NUMBER:
		    if ((yyvsp[0].node)->kind == NODE_NUMBER) {
			if ((yyvsp[0].node)->v.number == 0) {
			    yyerror("Divide by zero in constant");
			    (yyval.node) = (yyvsp[-2].node);
			    break;
			}
			(yyval.node) = (yyvsp[-2].node);
			(yyvsp[-2].node)->v.number /= (yyvsp[0].node)->v.number;
			break;
		    }
		    if ((yyvsp[0].node)->kind == NODE_REAL) {
			if ((yyvsp[0].node)->v.real == 0.0) {
			    yyerror("Divide by zero in constant");
			    (yyval.node) = (yyvsp[-2].node);
			    break;
			}
			(yyval.node) = (yyvsp[0].node);
			(yyvsp[0].node)->v.real = ((yyvsp[-2].node)->v.number / (yyvsp[0].node)->v.real);
			break;
		    }
		    CREATE_BINARY_OP((yyval.node), F_DIVIDE, result_type, (yyvsp[-2].node), (yyvsp[0].node));
		    break;
		case NODE_REAL:
		    if ((yyvsp[0].node)->kind == NODE_NUMBER) {
			if ((yyvsp[0].node)->v.number == 0) {
			    yyerror("Divide by zero in constant");
			    (yyval.node) = (yyvsp[-2].node);
			    break;
			}
			(yyval.node) = (yyvsp[-2].node);
			(yyvsp[-2].node)->v.real /= (yyvsp[0].node)->v.number;
			break;
		    }
		    if ((yyvsp[0].node)->kind == NODE_REAL) {
			if ((yyvsp[0].node)->v.real == 0.0) {
			    yyerror("Divide by zero in constant");
			    (yyval.node) = (yyvsp[-2].node);
			    break;
			}
			(yyval.node) = (yyvsp[-2].node);
			(yyvsp[-2].node)->v.real /= (yyvsp[0].node)->v.real;
			break;
		    }
		    CREATE_BINARY_OP((yyval.node), F_DIVIDE, result_type, (yyvsp[-2].node), (yyvsp[0].node));
		    break;
		default:
		    CREATE_BINARY_OP((yyval.node), F_DIVIDE, result_type, (yyvsp[-2].node), (yyvsp[0].node));
		}
	    }
#line 4002 "grammar.c"
    break;

  case 148:
#line 1807 "grammar.y"
            {
		(yyval.node) = (yyvsp[0].node);
		(yyval.node)->type = (yyvsp[-1].type);

		if (exact_types &&
		    (yyvsp[0].node)->type != (yyvsp[-1].type) &&
		    (yyvsp[0].node)->type != TYPE_ANY && 
		    (yyvsp[0].node)->type != TYPE_UNKNOWN &&
		    (yyvsp[-1].type) != TYPE_VOID) {
		    char buf[256];
		    char *end = EndOf(buf);
		    char *p;
		    
		    p = strput(buf, end, "Cannot cast ");
		    p = get_type_name(p, end, (yyvsp[0].node)->type);
		    p = strput(p, end, "to ");
		    p = get_type_name(p, end, (yyvsp[-1].type));
		    yyerror(buf);
		}
	    }
#line 4027 "grammar.c"
    break;

  case 149:
#line 1828 "grammar.y"
            {
		CREATE_UNARY_OP((yyval.node), F_PRE_INC, 0, (yyvsp[0].node));
                if (exact_types){
                    switch((yyvsp[0].node)->type){
                        case TYPE_NUMBER:
                        case TYPE_ANY:
                        case TYPE_REAL:
                        {
                            (yyval.node)->type = (yyvsp[0].node)->type;
                            break;
                        }

                        default:
                        {
                            (yyval.node)->type = TYPE_ANY;
                            type_error("Bad argument 1 to ++x", (yyvsp[0].node)->type);
                        }
                    }
                } else (yyval.node)->type = TYPE_ANY;
	    }
#line 4052 "grammar.c"
    break;

  case 150:
#line 1849 "grammar.y"
            {
		CREATE_UNARY_OP((yyval.node), F_PRE_DEC, 0, (yyvsp[0].node));
                if (exact_types){
                    switch((yyvsp[0].node)->type){
                        case TYPE_NUMBER:
                        case TYPE_ANY:
                        case TYPE_REAL:
                        {
                            (yyval.node)->type = (yyvsp[0].node)->type;
                            break;
                        }

                        default:
                        {
                            (yyval.node)->type = TYPE_ANY;
                            type_error("Bad argument 1 to --x", (yyvsp[0].node)->type);
                        }
                    }
                } else (yyval.node)->type = TYPE_ANY;

	    }
#line 4078 "grammar.c"
    break;

  case 151:
#line 1871 "grammar.y"
            {
		if ((yyvsp[0].node)->kind == NODE_NUMBER) {
		    (yyval.node) = (yyvsp[0].node);
		    (yyval.node)->v.number = !((yyval.node)->v.number);
		} else {
		    CREATE_UNARY_OP((yyval.node), F_NOT, TYPE_NUMBER, (yyvsp[0].node));
		}
	    }
#line 4091 "grammar.c"
    break;

  case 152:
#line 1880 "grammar.y"
            {
		if (exact_types && !IS_TYPE((yyvsp[0].node)->type, TYPE_NUMBER))
		    type_error("Bad argument to ~", (yyvsp[0].node)->type);
		if ((yyvsp[0].node)->kind == NODE_NUMBER) {
		    (yyval.node) = (yyvsp[0].node);
		    (yyval.node)->v.number = ~(yyval.node)->v.number;
		} else {
		    CREATE_UNARY_OP((yyval.node), F_COMPL, TYPE_NUMBER, (yyvsp[0].node));
		}
	    }
#line 4106 "grammar.c"
    break;

  case 153:
#line 1891 "grammar.y"
            {
		int result_type;
                if (exact_types){
		    int t = (yyvsp[0].node)->type;
		    if (!COMP_TYPE(t, TYPE_NUMBER)){
			type_error("Bad argument to unary '-'", t);
			result_type = TYPE_ANY;
		    } else result_type = t;
		} else result_type = TYPE_ANY;

		switch ((yyvsp[0].node)->kind) {
		case NODE_NUMBER:
		    (yyval.node) = (yyvsp[0].node);
		    (yyval.node)->v.number = -(yyval.node)->v.number;
		    break;
		case NODE_REAL:
		    (yyval.node) = (yyvsp[0].node);
		    (yyval.node)->v.real = -(yyval.node)->v.real;
		    break;
		default:
		    CREATE_UNARY_OP((yyval.node), F_NEGATE, result_type, (yyvsp[0].node));
		}
	    }
#line 4134 "grammar.c"
    break;

  case 154:
#line 1915 "grammar.y"
            {
		CREATE_UNARY_OP((yyval.node), F_POST_INC, 0, (yyvsp[-1].node));
		(yyval.node)->v.number = F_POST_INC;
                if (exact_types){
                    switch((yyvsp[-1].node)->type){
                        case TYPE_NUMBER:
		    case TYPE_ANY:
                        case TYPE_REAL:
                        {
                            (yyval.node)->type = (yyvsp[-1].node)->type;
                            break;
                        }

                        default:
                        {
                            (yyval.node)->type = TYPE_ANY;
                            type_error("Bad argument 1 to x++", (yyvsp[-1].node)->type);
                        }
                    }
                } else (yyval.node)->type = TYPE_ANY;
	    }
#line 4160 "grammar.c"
    break;

  case 155:
#line 1937 "grammar.y"
            {
		CREATE_UNARY_OP((yyval.node), F_POST_DEC, 0, (yyvsp[-1].node));
                if (exact_types){
                    switch((yyvsp[-1].node)->type){
		    case TYPE_NUMBER:
		    case TYPE_ANY:
		    case TYPE_REAL:
		    {
			(yyval.node)->type = (yyvsp[-1].node)->type;
			break;
		    }

		    default:
		    {
			(yyval.node)->type = TYPE_ANY;
			type_error("Bad argument 1 to x--", (yyvsp[-1].node)->type);
		    }
                    }
                } else (yyval.node)->type = TYPE_ANY;
	    }
#line 4185 "grammar.c"
    break;

  case 162:
#line 1967 "grammar.y"
            {
		if (exact_types && !IS_TYPE(exact_types, TYPE_VOID))
		    yywarn("Non-void functions must return a value.");
		CREATE_RETURN((yyval.node), 0);
	    }
#line 4195 "grammar.c"
    break;

  case 163:
#line 1973 "grammar.y"
            {
		if (exact_types && !compatible_types((yyvsp[-1].node)->type, exact_types & ~NAME_TYPE_MOD)) {
		    char buf[256];
		    char *end = EndOf(buf);
		    char *p;
		    
		    p = strput(buf, end, "Type of returned value doesn't match function return type ");
		    p = get_two_types(p, end, (yyvsp[-1].node)->type, exact_types & ~NAME_TYPE_MOD);
		    yyerror(buf);
		}
		if (IS_NODE((yyvsp[-1].node), NODE_NUMBER, 0)) {
		    CREATE_RETURN((yyval.node), 0);
		} else {
		    CREATE_RETURN((yyval.node), (yyvsp[-1].node));
		}
	    }
#line 4216 "grammar.c"
    break;

  case 164:
#line 1993 "grammar.y"
            {
		CREATE_EXPR_LIST((yyval.node), 0);
	    }
#line 4224 "grammar.c"
    break;

  case 165:
#line 1997 "grammar.y"
            {
		CREATE_EXPR_LIST((yyval.node), (yyvsp[0].node));
	    }
#line 4232 "grammar.c"
    break;

  case 166:
#line 2001 "grammar.y"
            {
		CREATE_EXPR_LIST((yyval.node), (yyvsp[-1].node));
	    }
#line 4240 "grammar.c"
    break;

  case 167:
#line 2008 "grammar.y"
            {
		CREATE_EXPR_NODE((yyval.node), (yyvsp[0].node), 0);
	    }
#line 4248 "grammar.c"
    break;

  case 168:
#line 2012 "grammar.y"
            {
		CREATE_EXPR_NODE((yyval.node), (yyvsp[-1].node), 1);
	    }
#line 4256 "grammar.c"
    break;

  case 169:
#line 2019 "grammar.y"
            {
		(yyvsp[0].node)->kind = 1;

		(yyval.node) = (yyvsp[0].node);
	    }
#line 4266 "grammar.c"
    break;

  case 170:
#line 2025 "grammar.y"
            {
		(yyvsp[0].node)->kind = 0;

		(yyval.node) = (yyvsp[-2].node);
		(yyval.node)->kind++;
		(yyval.node)->l.expr->r.expr = (yyvsp[0].node);
		(yyval.node)->l.expr = (yyvsp[0].node);
	    }
#line 4279 "grammar.c"
    break;

  case 171:
#line 2037 "grammar.y"
            {
		/* this is a dummy node */
		CREATE_EXPR_LIST((yyval.node), 0);
	    }
#line 4288 "grammar.c"
    break;

  case 172:
#line 2042 "grammar.y"
            {
		CREATE_EXPR_LIST((yyval.node), (yyvsp[0].node));
	    }
#line 4296 "grammar.c"
    break;

  case 173:
#line 2046 "grammar.y"
            {
		CREATE_EXPR_LIST((yyval.node), (yyvsp[-1].node));
	    }
#line 4304 "grammar.c"
    break;

  case 174:
#line 2053 "grammar.y"
            {
		(yyval.node) = new_node_no_line();
		(yyval.node)->kind = 2;
		(yyval.node)->v.expr = (yyvsp[0].node);
		(yyval.node)->r.expr = 0;
		(yyval.node)->type = 0;
		/* we keep track of the end of the chain in the left nodes */
		(yyval.node)->l.expr = (yyval.node);
            }
#line 4318 "grammar.c"
    break;

  case 175:
#line 2063 "grammar.y"
            {
		parse_node_t *expr;

		expr = new_node_no_line();
		expr->kind = 0;
		expr->v.expr = (yyvsp[0].node);
		expr->r.expr = 0;
		expr->type = 0;
		
		(yyvsp[-2].node)->l.expr->r.expr = expr;
		(yyvsp[-2].node)->l.expr = expr;
		(yyvsp[-2].node)->kind += 2;
		(yyval.node) = (yyvsp[-2].node);
	    }
#line 4337 "grammar.c"
    break;

  case 176:
#line 2081 "grammar.y"
            {
		CREATE_TWO_VALUES((yyval.node), 0, (yyvsp[-2].node), (yyvsp[0].node));
            }
#line 4345 "grammar.c"
    break;

  case 177:
#line 2088 "grammar.y"
            {
#define LV_ILLEGAL 1
#define LV_RANGE 2
#define LV_INDEX 4
                /* Restrictive lvalues, but I think they make more sense :) */
                (yyval.node) = (yyvsp[0].node);
                switch((yyval.node)->kind) {
		default:
		    yyerror("Illegal lvalue");
		    break;
		case NODE_PARAMETER:
		    (yyval.node)->kind = NODE_PARAMETER_LVALUE;
		    break;
		case NODE_TERNARY_OP:
		    (yyval.node)->v.number = (yyval.node)->r.expr->v.number;
		case NODE_OPCODE_1:
		case NODE_UNARY_OP_1:
		case NODE_BINARY_OP:
		    if ((yyval.node)->v.number >= F_LOCAL && (yyval.node)->v.number <= F_MEMBER)
			(yyval.node)->v.number++; /* make it an lvalue */
		    else if ((yyval.node)->v.number >= F_INDEX 
			     && (yyval.node)->v.number <= F_RE_RANGE) {
                        parse_node_t *node = (yyval.node);
                        int flag = 0;
                        do {
                            switch(node->kind) {
			    case NODE_PARAMETER:
				node->kind = NODE_PARAMETER_LVALUE;
				flag |= LV_ILLEGAL;
				break;
			    case NODE_TERNARY_OP:
				node->v.number = node->r.expr->v.number;
			    case NODE_OPCODE_1:
			    case NODE_UNARY_OP_1:
			    case NODE_BINARY_OP:
				if (node->v.number >= F_LOCAL 
				    && node->v.number <= F_MEMBER) {
				    node->v.number++;
				    flag |= LV_ILLEGAL;
				    break;
				} else if (node->v.number == F_INDEX ||
					 node->v.number == F_RINDEX) {
				    node->v.number++;
				    flag |= LV_INDEX;
				    break;
				} else if (node->v.number >= F_ADD_EQ
					   && node->v.number <= F_ASSIGN) {
				    if (!(flag & LV_INDEX)) {
					yyerror("Illegal lvalue, a possible lvalue is (x <assign> y)[a]");
				    }
				    if (node->r.expr->kind == NODE_BINARY_OP||
					node->r.expr->kind == NODE_TERNARY_OP){
					if (node->r.expr->v.number >= F_NN_RANGE_LVALUE && node->r.expr->v.number <= F_NR_RANGE_LVALUE)
					    yyerror("Illegal to have (x[a..b] <assign> y) to be the beginning of an lvalue");
				    }
				    flag = LV_ILLEGAL;
				    break;
				} else if (node->v.number >= F_NN_RANGE
					 && node->v.number <= F_RE_RANGE) {
				    if (flag & LV_RANGE) {
					yyerror("Can't do range lvalue of range lvalue.");
					flag |= LV_ILLEGAL;
					break;
				    }
                                    if (flag & LV_INDEX){
					yyerror("Can't do indexed lvalue of range lvalue.");
					flag |= LV_ILLEGAL;
					break;
				    }
				    if (node->v.number == F_NE_RANGE) {
					/* x[foo..] -> x[foo..<1] */
					parse_node_t *rchild = node->r.expr;
					node->kind = NODE_TERNARY_OP;
					CREATE_BINARY_OP(node->r.expr,
							 F_NR_RANGE_LVALUE,
							 0, 0, rchild);
					CREATE_NUMBER(node->r.expr->l.expr, 1);
				    } else if (node->v.number == F_RE_RANGE) {
					/* x[<foo..] -> x[<foo..<1] */
					parse_node_t *rchild = node->r.expr;
					node->kind = NODE_TERNARY_OP;
					CREATE_BINARY_OP(node->r.expr,
							 F_RR_RANGE_LVALUE,
							 0, 0, rchild);
					CREATE_NUMBER(node->r.expr->l.expr, 1);
				    } else
					node->r.expr->v.number++;
				    flag |= LV_RANGE;
				    node = node->r.expr->r.expr;
				    continue;
				}
			    default:
				yyerror("Illegal lvalue");
				flag = LV_ILLEGAL;
				break;
			    }   
                            if ((flag & LV_ILLEGAL) || !(node = node->r.expr)) break;
                        } while (1);
                        break;
		    } else 
			yyerror("Illegal lvalue");
		    break;
                }
            }
#line 4454 "grammar.c"
    break;

  case 179:
#line 2198 "grammar.y"
            {
              int i;
              if ((i = (yyvsp[0].ihe)->dn.local_num) != -1) {
		  CREATE_OPCODE_1((yyval.node), F_LOCAL, type_of_locals_ptr[i],i & 0xff);
		  if (current_function_context)
		      current_function_context->num_locals++;
              } else
		  if ((i = (yyvsp[0].ihe)->dn.global_num) != -1) {
		      if (current_function_context)
			  current_function_context->bindable = FP_NOT_BINDABLE;
                          CREATE_OPCODE_1((yyval.node), F_GLOBAL,
				      VAR_TEMP(i)->type & ~NAME_TYPE_MOD, i);
		      if (VAR_TEMP(i)->type & NAME_HIDDEN) {
			  char buf[256];
			  char *end = EndOf(buf);
			  char *p;

			  p = strput(buf, end, "Illegal to use private variable '");
			  p = strput(p, end, (yyvsp[0].ihe)->name);
			  p = strput(p, end, "'");
			  yyerror(buf);
		      }
		  } else {
		      char buf[256];
		      char *end = EndOf(buf);
		      char *p;
		      
		      p = strput(buf, end, "Undefined variable '");
		      p = strput(p, end, (yyvsp[0].ihe)->name);
		      p = strput(p, end, "'");
		      if (current_number_of_locals < CONFIG_INT(__MAX_LOCAL_VARIABLES__)) {
			  add_local_name((yyvsp[0].ihe)->name, TYPE_ANY);
		      }
		      CREATE_OPCODE_1((yyval.node), F_LOCAL, TYPE_ANY, 0);
		      yyerror(buf);
		  }
	    }
#line 4496 "grammar.c"
    break;

  case 180:
#line 2236 "grammar.y"
            {
		char buf[256];
		char *end = EndOf(buf);
		char *p;
		
		p = strput(buf, end, "Undefined variable '");
		p = strput(p, end, (yyvsp[0].string));
		p = strput(p, end, "'");
                if (current_number_of_locals < CONFIG_INT(__MAX_LOCAL_VARIABLES__)) {
                    add_local_name((yyvsp[0].string), TYPE_ANY);
                }
                CREATE_OPCODE_1((yyval.node), F_LOCAL, TYPE_ANY, 0);
                yyerror(buf);
                scratch_free((yyvsp[0].string));
            }
#line 4516 "grammar.c"
    break;

  case 181:
#line 2252 "grammar.y"
            {
		CREATE_PARAMETER((yyval.node), TYPE_ANY, (yyvsp[0].number));
            }
#line 4524 "grammar.c"
    break;

  case 182:
#line 2256 "grammar.y"
            {
		(yyval.contextp) = current_function_context;
		/* already flagged as an error */
		if (current_function_context)
		    current_function_context = current_function_context->parent;
            }
#line 4535 "grammar.c"
    break;

  case 183:
#line 2263 "grammar.y"
            {
		parse_node_t *node;

		current_function_context = (yyvsp[-2].contextp);

		if (!current_function_context || current_function_context->num_parameters == -2) {
		    /* This was illegal, and error'ed when the '$' token
		     * was returned.
		     */
		    CREATE_ERROR((yyval.node));
		} else {
		    CREATE_OPCODE_1((yyval.node), F_LOCAL, (yyvsp[-1].node)->type,
				    current_function_context->values_list->kind++);

		    node = new_node_no_line();
		    node->type = 0;
		    current_function_context->values_list->l.expr->r.expr = node;
		    current_function_context->values_list->l.expr = node;
		    node->r.expr = 0;
		    node->v.expr = (yyvsp[-1].node);
		}
	    }
#line 4562 "grammar.c"
    break;

  case 184:
#line 2286 "grammar.y"
            {
		if (!IS_CLASS((yyvsp[-2].node)->type)) {
		    yyerror("Left argument of -> is not a class");
		    CREATE_ERROR((yyval.node));
		} else {
		    CREATE_UNARY_OP_1((yyval.node), F_MEMBER, 0, (yyvsp[-2].node), 0);
		    (yyval.node)->l.number = lookup_class_member(CLASS_IDX((yyvsp[-2].node)->type),
						       (yyvsp[0].string),
						       &((yyval.node)->type));
		}
		scratch_free((yyvsp[0].string));
            }
#line 4579 "grammar.c"
    break;

  case 185:
#line 2299 "grammar.y"
            {
                (yyval.node) = make_range_node(F_NN_RANGE, (yyvsp[-5].node), (yyvsp[-3].node), (yyvsp[-1].node));
            }
#line 4587 "grammar.c"
    break;

  case 186:
#line 2303 "grammar.y"
            {
                (yyval.node) = make_range_node(F_RN_RANGE, (yyvsp[-6].node), (yyvsp[-3].node), (yyvsp[-1].node));
            }
#line 4595 "grammar.c"
    break;

  case 187:
#line 2307 "grammar.y"
            {
		if ((yyvsp[-1].node)->kind == NODE_NUMBER && (yyvsp[-1].node)->v.number <= 1)
		    (yyval.node) = make_range_node(F_RE_RANGE, (yyvsp[-7].node), (yyvsp[-4].node), 0);
		else
		    (yyval.node) = make_range_node(F_RR_RANGE, (yyvsp[-7].node), (yyvsp[-4].node), (yyvsp[-1].node));
            }
#line 4606 "grammar.c"
    break;

  case 188:
#line 2314 "grammar.y"
            {
		if ((yyvsp[-1].node)->kind == NODE_NUMBER && (yyvsp[-1].node)->v.number <= 1)
		    (yyval.node) = make_range_node(F_NE_RANGE, (yyvsp[-6].node), (yyvsp[-4].node), 0);
		else
		    (yyval.node) = make_range_node(F_NR_RANGE, (yyvsp[-6].node), (yyvsp[-4].node), (yyvsp[-1].node));
            }
#line 4617 "grammar.c"
    break;

  case 189:
#line 2321 "grammar.y"
            {
                (yyval.node) = make_range_node(F_NE_RANGE, (yyvsp[-4].node), (yyvsp[-2].node), 0);
            }
#line 4625 "grammar.c"
    break;

  case 190:
#line 2325 "grammar.y"
            {
                (yyval.node) = make_range_node(F_RE_RANGE, (yyvsp[-5].node), (yyvsp[-2].node), 0);
            }
#line 4633 "grammar.c"
    break;

  case 191:
#line 2329 "grammar.y"
            {
                if (IS_NODE((yyvsp[-4].node), NODE_CALL, F_AGGREGATE)
		    && (yyvsp[-1].node)->kind == NODE_NUMBER) {
                    int i = (yyvsp[-1].node)->v.number;
                    if (i < 1 || i > (yyvsp[-4].node)->l.number)
                        yyerror("Illegal index to array constant.");
                    else {
                        parse_node_t *node = (yyvsp[-4].node)->r.expr;
                        i = (yyvsp[-4].node)->l.number - i;
                        while (i--)
                            node = node->r.expr;
                        (yyval.node) = node->v.expr;
                        break;
                    }
                }
		CREATE_BINARY_OP((yyval.node), F_RINDEX, 0, (yyvsp[-1].node), (yyvsp[-4].node));
                if (exact_types) {
		    switch((yyvsp[-4].node)->type) {
		    case TYPE_MAPPING:
			yyerror("Illegal index for mapping.");
		    case TYPE_ANY:
			(yyval.node)->type = TYPE_ANY;
			break;
		    case TYPE_STRING:
		    case TYPE_BUFFER:
			(yyval.node)->type = TYPE_NUMBER;
			if (!IS_TYPE((yyvsp[-1].node)->type,TYPE_NUMBER))
			    type_error("Bad type of index", (yyvsp[-1].node)->type);
			break;
			
		    default:
			if ((yyvsp[-4].node)->type & TYPE_MOD_ARRAY) {
			    (yyval.node)->type = (yyvsp[-4].node)->type & ~TYPE_MOD_ARRAY;
			    if (!IS_TYPE((yyvsp[-1].node)->type,TYPE_NUMBER))
				type_error("Bad type of index", (yyvsp[-1].node)->type);
			} else {
			    type_error("Value indexed has a bad type ", (yyvsp[-4].node)->type);
			    (yyval.node)->type = TYPE_ANY;
			}
		    }
		} else (yyval.node)->type = TYPE_ANY;
            }
#line 4680 "grammar.c"
    break;

  case 192:
#line 2372 "grammar.y"
            {
		/* Something stupid like ({ 1, 2, 3 })[1]; we take the
		 * time to optimize this because people who don't understand
		 * the preprocessor often write things like:
		 *
		 * #define MY_ARRAY ({ "foo", "bar", "bazz" })
		 * ...
		 * ... MY_ARRAY[1] ...
		 *
		 * which of course expands to the above.
		 */
                if (IS_NODE((yyvsp[-3].node), NODE_CALL, F_AGGREGATE) && (yyvsp[-1].node)->kind == NODE_NUMBER) {
                    int i = (yyvsp[-1].node)->v.number;
                    if (i < 0 || i >= (yyvsp[-3].node)->l.number)
                        yyerror("Illegal index to array constant.");
                    else {
                        parse_node_t *node = (yyvsp[-3].node)->r.expr;
                        while (i--)
                            node = node->r.expr;
                        (yyval.node) = node->v.expr;
                        break;
                    }
                }
                CREATE_BINARY_OP((yyval.node), F_INDEX, 0, (yyvsp[-1].node), (yyvsp[-3].node));
                if (exact_types) {
		    switch((yyvsp[-3].node)->type) {
		    case TYPE_MAPPING:
		    case TYPE_ANY:
			(yyval.node)->type = TYPE_ANY;
			break;
		    case TYPE_STRING:
		    case TYPE_BUFFER:
			(yyval.node)->type = TYPE_NUMBER;
			if (!IS_TYPE((yyvsp[-1].node)->type,TYPE_NUMBER))
			    type_error("Bad type of index", (yyvsp[-1].node)->type);
			break;
			
		    default:
			if ((yyvsp[-3].node)->type & TYPE_MOD_ARRAY) {
			    (yyval.node)->type = (yyvsp[-3].node)->type & ~TYPE_MOD_ARRAY;
			    if (!IS_TYPE((yyvsp[-1].node)->type,TYPE_NUMBER))
				type_error("Bad type of index", (yyvsp[-1].node)->type);
			} else {
			    type_error("Value indexed has a bad type ", (yyvsp[-3].node)->type);
			    (yyval.node)->type = TYPE_ANY;
			}
                    }
                } else (yyval.node)->type = TYPE_ANY;
            }
#line 4734 "grammar.c"
    break;

  case 194:
#line 2423 "grammar.y"
            {
		(yyval.node) = (yyvsp[-1].node);
	    }
#line 4742 "grammar.c"
    break;

  case 196:
#line 2428 "grammar.y"
            {
	        if ((yyvsp[0].type) != TYPE_FUNCTION) yyerror("Reserved type name unexpected.");
		(yyval.func_block).num_local = current_number_of_locals;
		(yyval.func_block).max_num_locals = max_num_locals;
		(yyval.func_block).context = context;
		(yyval.func_block).save_current_type = current_type;
		(yyval.func_block).save_exact_types = exact_types;
	        if (type_of_locals_ptr + max_num_locals + CONFIG_INT(__MAX_LOCAL_VARIABLES__) >= &type_of_locals[type_of_locals_size])
		    reallocate_locals();
		deactivate_current_locals();
		locals_ptr += current_number_of_locals;
		type_of_locals_ptr += max_num_locals;
		runtime_locals_ptr += current_number_of_locals;
		max_num_locals = current_number_of_locals = 0;
		push_function_context();
		current_function_context->num_parameters = -1;
		exact_types = TYPE_ANY;
		context = 0;
            }
#line 4766 "grammar.c"
    break;

  case 197:
#line 2448 "grammar.y"
            {
		if ((yyvsp[-2].argument).flags & ARG_IS_PROTO) {
		    yyerror("Missing name for function argument");
		}
		if ((yyvsp[-2].argument).flags & ARG_IS_VARARGS) {
		    yyerror("Anonymous varargs functions aren't implemented");
		}
		if (!(yyvsp[0].decl).node) {
		    CREATE_RETURN((yyval.node), 0);
		} else if ((yyvsp[0].decl).node->kind != NODE_RETURN &&
			   ((yyvsp[0].decl).node->kind != NODE_TWO_VALUES || (yyvsp[0].decl).node->r.expr->kind != NODE_RETURN)) {
		    parse_node_t *replacement;
		    CREATE_STATEMENTS(replacement, (yyvsp[0].decl).node, 0);
		    CREATE_RETURN(replacement->r.expr, 0);
		    (yyvsp[0].decl).node = replacement;
		}
		
		(yyval.node) = new_node();
		(yyval.node)->kind = NODE_ANON_FUNC;
		(yyval.node)->type = TYPE_FUNCTION;
		(yyval.node)->l.number = (max_num_locals - (yyvsp[-2].argument).num_arg);
		(yyval.node)->r.expr = (yyvsp[0].decl).node;
		(yyval.node)->v.number = (yyvsp[-2].argument).num_arg;
		if (current_function_context->bindable)
		    (yyval.node)->v.number |= 0x10000;
		free_all_local_names();
		
		current_number_of_locals = (yyvsp[-4].func_block).num_local;
		max_num_locals = (yyvsp[-4].func_block).max_num_locals;
		context = (yyvsp[-4].func_block).context;
		current_type = (yyvsp[-4].func_block).save_current_type;
		exact_types = (yyvsp[-4].func_block).save_exact_types;
		pop_function_context();
		
		locals_ptr -= current_number_of_locals;
		type_of_locals_ptr -= max_num_locals;
		runtime_locals_ptr -= current_number_of_locals;
		reactivate_current_locals();
	    }
#line 4810 "grammar.c"
    break;

  case 198:
#line 2488 "grammar.y"
            {
		(yyval.node) = new_node();
		(yyval.node)->kind = NODE_FUNCTION_CONSTRUCTOR;
		(yyval.node)->type = TYPE_FUNCTION;
		(yyval.node)->r.expr = 0;
		switch ((yyvsp[-2].number) & 0xff) {
		case FP_L_VAR:
		    yyerror("Illegal to use local variable in a functional.");
		    CREATE_NUMBER((yyval.node)->l.expr, 0);
		    (yyval.node)->l.expr->r.expr = 0;
		    (yyval.node)->l.expr->l.expr = 0;
		    (yyval.node)->v.number = FP_FUNCTIONAL;
		    break;
		case FP_G_VAR:
		    CREATE_OPCODE_1((yyval.node)->l.expr, F_GLOBAL, 0, (yyvsp[-2].number) >> 8);
		    (yyval.node)->v.number = FP_FUNCTIONAL | FP_NOT_BINDABLE;
		    if (VAR_TEMP((yyval.node)->l.expr->l.number)->type & NAME_HIDDEN) {
		      char buf[256];
		      char *end = EndOf(buf);
		      char *p;
		      
		      p = strput(buf, end, "Illegal to use private variable '");
		      p = strput(p, end, VAR_TEMP((yyval.node)->l.expr->l.number)->name);
		      p = strput(p, end, "'");
		      yyerror(buf);
		    }
		    break;
		default:
		    (yyval.node)->v.number = (yyvsp[-2].number);
		    break;
		}
	    }
#line 4847 "grammar.c"
    break;

  case 199:
#line 2521 "grammar.y"
            {
		(yyval.node) = new_node();
		(yyval.node)->kind = NODE_FUNCTION_CONSTRUCTOR;
		(yyval.node)->type = TYPE_FUNCTION;
		(yyval.node)->v.number = (yyvsp[-4].number);
		(yyval.node)->r.expr = (yyvsp[-2].node);
		
		switch ((yyvsp[-4].number) & 0xff) {
		case FP_EFUN: {
		    int *argp;
		    int f = (yyvsp[-4].number) >>8;
		    int num = (yyvsp[-2].node)->kind;
		    int max_arg = predefs[f].max_args;
		    
		    if (num > max_arg && max_arg != -1) {
			parse_node_t *pn = (yyvsp[-2].node);
			
			while (pn) {
			    if (pn->type & 1) break;
			    pn = pn->r.expr;
			}
			
			if (!pn) {
			    char bff[256];
			    char *end = EndOf(bff);
			    char *p;
			    
			    p = strput(bff, end, "Too many arguments to ");
			    p = strput(p, end, predefs[f].word);
			    yyerror(bff);
			}
		    } else if (max_arg != -1 && exact_types) {
			/*
			 * Now check all types of arguments to efuns.
			 */
			int i, argn, tmp;
			parse_node_t *enode = (yyvsp[-2].node);
			argp = &efun_arg_types[predefs[f].arg_index];
			
			for (argn = 0; argn < num; argn++) {
			    if (enode->type & 1) break;
			    
			    tmp = enode->v.expr->type;
			    for (i=0; !compatible_types(tmp, argp[i])
				 && argp[i] != 0; i++)
				;
			    if (argp[i] == 0) {
				char buf[256];
				char *end = EndOf(buf);
				char *p;

				p = strput(buf, end, "Bad argument ");
				p = strput_int(p, end, argn+1);
				p = strput(p, end, " to efun ");
				p = strput(p, end, predefs[f].word);
				p = strput(p, end, "()");
				yyerror(buf);
			    } else {
				/* this little section necessary b/c in the
				   case float | int we dont want to do
				   promoting. */
				if (tmp == TYPE_NUMBER && argp[i] == TYPE_REAL) {
				    for (i++; argp[i] && argp[i] != TYPE_NUMBER; i++)
					;
				    if (!argp[i])
					enode->v.expr = promote_to_float(enode->v.expr);
				}
				if (tmp == TYPE_REAL && argp[i] == TYPE_NUMBER) {
				    for (i++; argp[i] && argp[i] != TYPE_REAL; i++)
					;
				    if (!argp[i])
					enode->v.expr = promote_to_int(enode->v.expr);
				}
			    }
			    while (argp[i] != 0)
				i++;
			    argp += i + 1;
			    enode = enode->r.expr;
			}
		    }
		    break;
		}
		case FP_L_VAR:
		case FP_G_VAR:
		    yyerror("Can't give parameters to functional.");
		    break;
		}
	    }
#line 4940 "grammar.c"
    break;

  case 200:
#line 2610 "grammar.y"
             {
		 if (current_function_context->num_locals)
		     yyerror("Illegal to use local variable in functional.");
		 if (current_function_context->values_list->r.expr)
		     current_function_context->values_list->r.expr->kind = current_function_context->values_list->kind;
		 
		 (yyval.node) = new_node();
		 (yyval.node)->kind = NODE_FUNCTION_CONSTRUCTOR;
		 (yyval.node)->type = TYPE_FUNCTION;
		 (yyval.node)->l.expr = (yyvsp[-2].node);
		 if ((yyvsp[-2].node)->kind == NODE_STRING)
		     yywarn("Function pointer returning string constant is NOT a function call");
		 (yyval.node)->r.expr = current_function_context->values_list->r.expr;
		 (yyval.node)->v.number = FP_FUNCTIONAL + current_function_context->bindable
		     + (current_function_context->num_parameters << 8);
		 pop_function_context();
             }
#line 4962 "grammar.c"
    break;

  case 201:
#line 2628 "grammar.y"
            {
		CREATE_CALL((yyval.node), F_AGGREGATE_ASSOC, TYPE_MAPPING, (yyvsp[-2].node));
	    }
#line 4970 "grammar.c"
    break;

  case 202:
#line 2632 "grammar.y"
            {
		CREATE_CALL((yyval.node), F_AGGREGATE, TYPE_ANY | TYPE_MOD_ARRAY, (yyvsp[-2].node));
	    }
#line 4978 "grammar.c"
    break;

  case 203:
#line 2639 "grammar.y"
            {
		(yyval.node) = (yyvsp[0].decl).node;
	    }
#line 4986 "grammar.c"
    break;

  case 204:
#line 2643 "grammar.y"
            {
		(yyval.node) = insert_pop_value((yyvsp[-1].node));
	    }
#line 4994 "grammar.c"
    break;

  case 205:
#line 2650 "grammar.y"
            {
		(yyval.number) = context;
		context = SPECIAL_CONTEXT;
	    }
#line 5003 "grammar.c"
    break;

  case 206:
#line 2655 "grammar.y"
            {
		CREATE_CATCH((yyval.node), (yyvsp[0].node));
		context = (yyvsp[-1].number);
	    }
#line 5012 "grammar.c"
    break;

  case 207:
#line 2664 "grammar.y"
            {
		int p = (yyvsp[-1].node)->v.number;
		CREATE_LVALUE_EFUN((yyval.node), TYPE_NUMBER, (yyvsp[-1].node));
		CREATE_BINARY_OP_1((yyval.node)->l.expr, F_SSCANF, 0, (yyvsp[-4].node), (yyvsp[-2].node), p);
	    }
#line 5022 "grammar.c"
    break;

  case 208:
#line 2673 "grammar.y"
            {
		int p = (yyvsp[-1].node)->v.number;
		CREATE_LVALUE_EFUN((yyval.node), TYPE_NUMBER, (yyvsp[-1].node));
		CREATE_TERNARY_OP_1((yyval.node)->l.expr, F_PARSE_COMMAND, 0, 
				    (yyvsp[-6].node), (yyvsp[-4].node), (yyvsp[-2].node), p);
	    }
#line 5033 "grammar.c"
    break;

  case 209:
#line 2683 "grammar.y"
            {
		(yyval.number) = context;
		context = SPECIAL_CONTEXT;
	    }
#line 5042 "grammar.c"
    break;

  case 210:
#line 2688 "grammar.y"
            {
		CREATE_TIME_EXPRESSION((yyval.node), (yyvsp[0].node));
		context = (yyvsp[-1].number);
	    }
#line 5051 "grammar.c"
    break;

  case 211:
#line 2696 "grammar.y"
            {
	        (yyval.node) = new_node_no_line();
		(yyval.node)->r.expr = 0;
	        (yyval.node)->v.number = 0;
	    }
#line 5061 "grammar.c"
    break;

  case 212:
#line 2702 "grammar.y"
            {
		parse_node_t *insert;
		
		(yyval.node) = (yyvsp[0].node);
		insert = new_node_no_line();
		insert->r.expr = (yyvsp[0].node)->r.expr;
		insert->l.expr = (yyvsp[-1].node);
		(yyvsp[0].node)->r.expr = insert;
		(yyval.node)->v.number++;
	    }
#line 5076 "grammar.c"
    break;

  case 213:
#line 2716 "grammar.y"
            {
		CREATE_STRING((yyval.node), (yyvsp[0].string));
		scratch_free((yyvsp[0].string));
	    }
#line 5085 "grammar.c"
    break;

  case 215:
#line 2725 "grammar.y"
            {
		(yyval.string) = (yyvsp[-1].string);
	    }
#line 5093 "grammar.c"
    break;

  case 216:
#line 2729 "grammar.y"
            {
		(yyval.string) = scratch_join((yyvsp[-2].string), (yyvsp[0].string));
	    }
#line 5101 "grammar.c"
    break;

  case 218:
#line 2737 "grammar.y"
            {
		(yyval.string) = scratch_join((yyvsp[-1].string), (yyvsp[0].string));
	    }
#line 5109 "grammar.c"
    break;

  case 219:
#line 2743 "grammar.y"
    {
	(yyval.node) = new_node();
	(yyval.node)->l.expr = (parse_node_t *)(yyvsp[-2].string);
	(yyval.node)->v.expr = (yyvsp[0].node);
	(yyval.node)->r.expr = 0;
    }
#line 5120 "grammar.c"
    break;

  case 220:
#line 2753 "grammar.y"
    {
	(yyval.node) = 0;
    }
#line 5128 "grammar.c"
    break;

  case 221:
#line 2757 "grammar.y"
    {
	(yyval.node) = (yyvsp[0].node);
	(yyval.node)->r.expr = (yyvsp[-2].node);
    }
#line 5137 "grammar.c"
    break;

  case 222:
#line 2766 "grammar.y"
            {
	      (yyval.node) = validate_efun_call((yyvsp[-3].number),(yyvsp[-1].node));
	    }
#line 5145 "grammar.c"
    break;

  case 223:
#line 2770 "grammar.y"
            {
		ident_hash_elem_t *ihe;

		ihe = lookup_ident("clone_object");
		(yyval.node) = validate_efun_call(ihe->dn.efun_num, (yyvsp[-1].node));
            }
#line 5156 "grammar.c"
    break;

  case 224:
#line 2777 "grammar.y"
            {
		parse_node_t *node;
		
		if ((yyvsp[-2].ihe)->dn.class_num == -1) {
		    char buf[256];
		    char *end = EndOf(buf);
		    char *p;
		    
		    p = strput(buf, end, "Undefined class '");
		    p = strput(p, end, (yyvsp[-2].ihe)->name);
		    p = strput(p, end, "'");
		    yyerror(buf);
		    CREATE_ERROR((yyval.node));
		    node = (yyvsp[-1].node);
		    while (node) {
			scratch_free((char *)node->l.expr);
			node = node->r.expr;
		    }
		} else {
		    int type = (yyvsp[-2].ihe)->dn.class_num | TYPE_MOD_CLASS;
		    
		    if ((node = (yyvsp[-1].node))) {
			CREATE_TWO_VALUES((yyval.node), type, 0, 0);
			(yyval.node)->l.expr = reorder_class_values((yyvsp[-2].ihe)->dn.class_num,
							node);
			CREATE_OPCODE_1((yyval.node)->r.expr, F_NEW_CLASS,
					type, (yyvsp[-2].ihe)->dn.class_num);
			
		    } else {
			CREATE_OPCODE_1((yyval.node), F_NEW_EMPTY_CLASS,
					type, (yyvsp[-2].ihe)->dn.class_num);
		    }
		}
            }
#line 5195 "grammar.c"
    break;

  case 225:
#line 2812 "grammar.y"
            {
	      int f;

	      (yyval.node) = (yyvsp[-1].node);
	      if ((f = (yyvsp[-3].ihe)->dn.function_num) != -1) {
		  if (FUNCTION_FLAGS(f) & NAME_HIDDEN) {
		      char buf[256];
		      char *end = EndOf(buf);
		      char *p;
		      
		      p = strput(buf, end, "Illegal to call private function '");
		      p = strput(p, end, (yyvsp[-3].ihe)->name);
		      p = strput(p, end, "'");
		      yyerror(buf);
		  }
		  if (current_function_context)
		      current_function_context->bindable = FP_NOT_BINDABLE;

		  (yyval.node)->kind = NODE_CALL_1;
		  (yyval.node)->v.number = F_CALL_FUNCTION_BY_ADDRESS;
		  (yyval.node)->l.number = f;
		  (yyval.node)->type = validate_function_call(f, (yyvsp[-1].node)->r.expr);
	      } else
	      if ((f=(yyvsp[-3].ihe)->dn.simul_num) != -1) {
		  (yyval.node)->kind = NODE_CALL_1;
		  (yyval.node)->v.number = F_SIMUL_EFUN;
		  (yyval.node)->l.number = f;
		  (yyval.node)->type = (SIMUL(f)->type) & ~NAME_TYPE_MOD;
	      } else 
	      if ((f=(yyvsp[-3].ihe)->dn.efun_num) != -1) {
		  (yyval.node) = validate_efun_call(f, (yyvsp[-1].node));
	      } else {
		/* This here is a really nasty case that only occurs with
		 * exact_types off.  The user has done something gross like:
		 *
		 * func() { int f; f(); } // if f was prototyped we wouldn't
		 * f() { }                // need this case
		 *
		 * Don't complain, just grok it.
		 */
		int cf, f;

		if (current_function_context)
		    current_function_context->bindable = FP_NOT_BINDABLE;
		
		cf = define_new_function((yyvsp[-3].ihe)->name, 0, 0, 
					 NAME_UNDEFINED | NAME_PROTOTYPE, 0);
		f = COMPILER_FUNC(cf)->runtime_index;
		(yyval.node)->kind = NODE_CALL_1;
		(yyval.node)->v.number = F_CALL_FUNCTION_BY_ADDRESS;
		(yyval.node)->l.number = f;
		(yyval.node)->type = TYPE_ANY; /* just a guess */
		if (exact_types) {
		    char buf[256];
		    char *end = EndOf(buf);
		    char *p;
		    char *n = (yyvsp[-3].ihe)->name;
		    if (*n == ':') n++;
		    /* prevent some errors; by making it look like an
		     * inherited function we prevent redeclaration errors
		     * if it shows up later
		     */
		    FUNCTION_FLAGS(f) &= ~NAME_UNDEFINED;
		    FUNCTION_FLAGS(f) |= NAME_INHERITED;
		    COMPILER_FUNC(cf)->type |= NAME_VARARGS;
		    p = strput(buf, end, "Undefined function ");
		    p = strput(p, end, n);
		    yyerror(buf);
		}
	      }
	    }
#line 5271 "grammar.c"
    break;

  case 226:
#line 2884 "grammar.y"
            {
	      char *name = (yyvsp[-3].string);

	      (yyval.node) = (yyvsp[-1].node);
	      
	      if (*name == ':'){
		  arrange_call_inherited(name + 1, (yyval.node));
	      } else {
		  int f;
		  ident_hash_elem_t *ihe;
		  
		  if (current_function_context)
		      current_function_context->bindable = FP_NOT_BINDABLE;

		  f = (ihe = lookup_ident(name)) ? ihe->dn.function_num : -1;
		  (yyval.node)->kind = NODE_CALL_1;
		  (yyval.node)->v.number = F_CALL_FUNCTION_BY_ADDRESS;
		  if (f!=-1) {
		      /* The only way this can happen is if function_name
		       * below made the function name.  The lexer would
		       * return L_DEFINED_NAME instead.
		       */
		      (yyval.node)->type = validate_function_call(f, (yyvsp[-1].node)->r.expr);
		  } else {
		      f = define_new_function(name, 0, 0, 
					      NAME_UNDEFINED | NAME_PROTOTYPE, 0);
		      f = COMPILER_FUNC(f)->runtime_index;
		  }
		  (yyval.node)->l.number = f;
		  /*
		   * Check if this function has been defined.
		   * But, don't complain yet about functions defined
		   * by inheritance.
		   */
		  if (exact_types && (FUNCTION_FLAGS(f) & NAME_UNDEFINED)) {
		      char buf[256];
		      char *end = EndOf(buf);
		      char *p;
		      char *n = (yyvsp[-3].string);
		      if (*n == ':') n++;
		      /* prevent some errors */
		      FUNCTION_FLAGS(f) &= ~NAME_UNDEFINED;
		      FUNCTION_FLAGS(f) |= (NAME_INHERITED | NAME_VARARGS);
		      p = strput(buf, end, "Undefined function ");
		      p = strput(p, end, n);
		      yyerror(buf);
		  }
		  if (!(FUNCTION_FLAGS(f) & NAME_UNDEFINED))
		      (yyval.node)->type = FUNCTION_DEF(f)->type;
		  else
		      (yyval.node)->type = TYPE_ANY;  /* Just a guess */
	      }
	      scratch_free(name);
	  }
#line 5330 "grammar.c"
    break;

  case 227:
#line 2939 "grammar.y"
            {
		parse_node_t *expr, *expr2;
		(yyval.node) = (yyvsp[-1].node);
		(yyval.node)->kind = NODE_EFUN;
		(yyval.node)->l.number = (yyval.node)->v.number + 2;
		(yyval.node)->v.number = F_CALL_OTHER;
#ifdef CAST_CALL_OTHERS
		(yyval.node)->type = TYPE_UNKNOWN;
#else
                (yyval.node)->type = TYPE_ANY;
#endif		  
		expr = new_node_no_line();
		expr->type = 0;
		expr->v.expr = (yyvsp[-5].node);

		expr2 = new_node_no_line();
		expr2->type = 0;
		CREATE_STRING(expr2->v.expr, (yyvsp[-3].string));
		scratch_free((yyvsp[-3].string));

		/* insert the two nodes */
		expr2->r.expr = (yyval.node)->r.expr;
		expr->r.expr = expr2;
		(yyval.node)->r.expr = expr;
	    }
#line 5360 "grammar.c"
    break;

  case 228:
#line 2965 "grammar.y"
            {
	        parse_node_t *expr;

		(yyval.node) = (yyvsp[-1].node);
		(yyval.node)->kind = NODE_EFUN;
		(yyval.node)->l.number = (yyval.node)->v.number + 1;
		(yyval.node)->v.number = F_EVALUATE;
#ifdef CAST_CALL_OTHERS
		(yyval.node)->type = TYPE_UNKNOWN;
#else
		(yyval.node)->type = TYPE_ANY;
#endif
		expr = new_node_no_line();
		expr->type = 0;
		expr->v.expr = (yyvsp[-4].node);
		expr->r.expr = (yyval.node)->r.expr;
		(yyval.node)->r.expr = expr;
	    }
#line 5383 "grammar.c"
    break;

  case 229:
#line 2985 "grammar.y"
                                               {
	svalue_t *res;
	ident_hash_elem_t *ihe;

	(yyval.number) = (ihe = lookup_ident((yyvsp[0].string))) ? ihe->dn.efun_num : -1;
	if ((yyval.number) == -1) {
	    char buf[256];
	    char *end = EndOf(buf);
	    char *p;
	    
	    p = strput(buf, end, "Unknown efun: ");
	    p = strput(p, end, (yyvsp[0].string));
	    yyerror(buf);
	} else {
	    push_malloced_string(the_file_name(current_file));
	    share_and_push_string((yyvsp[0].string));
	    push_malloced_string(add_slash(main_file_name()));
	    res = safe_apply_master_ob(APPLY_VALID_OVERRIDE, 3);
	    if (!MASTER_APPROVED(res)) {
		yyerror("Invalid simulated efunction override");
		(yyval.number) = -1;
	    }
	}
	scratch_free((yyvsp[0].string));
      }
#line 5413 "grammar.c"
    break;

  case 230:
#line 3010 "grammar.y"
                                 {
	ident_hash_elem_t *ihe;
	svalue_t *res;
	
	ihe = lookup_ident("clone_object");
	push_malloced_string(the_file_name(current_file));
	push_constant_string("clone_object");
	push_malloced_string(add_slash(main_file_name()));
	res = safe_apply_master_ob(APPLY_VALID_OVERRIDE, 3);
	if (!MASTER_APPROVED(res)) {
	    yyerror("Invalid simulated efunction override");
	    (yyval.number) = -1;
	} else (yyval.number) = ihe->dn.efun_num;
      }
#line 5432 "grammar.c"
    break;

  case 232:
#line 3029 "grammar.y"
            {
		int l = strlen((yyvsp[0].string)) + 1;
		char *p;
		/* here we be a bit cute.  we put a : on the front so we
		 * don't have to strchr for it.  Here we do:
		 * "name" -> ":::name"
		 */
		(yyval.string) = scratch_realloc((yyvsp[0].string), l + 3);
		p = (yyval.string) + l;
		while (p--,l--)
		    *(p+3) = *p;
		memcpy((yyval.string), ":::", 3);
	    }
#line 5450 "grammar.c"
    break;

  case 233:
#line 3043 "grammar.y"
            {
		int z, l = strlen((yyvsp[0].string)) + 1;
		char *p;
		/* <type> and "name" -> ":type::name" */
		z = strlen(compiler_type_names[(yyvsp[-2].type)]) + 3; /* length of :type:: */
		(yyval.string) = scratch_realloc((yyvsp[0].string), l + z);
		p = (yyval.string) + l;
		while (p--,l--)
		    *(p+z) = *p;
		(yyval.string)[0] = ':';
		strncpy((yyval.string) + 1, compiler_type_names[(yyvsp[-2].type)], z - 3);
		(yyval.string)[z-2] = ':';
		(yyval.string)[z-1] = ':';
	    }
#line 5469 "grammar.c"
    break;

  case 234:
#line 3058 "grammar.y"
            {
		int l = strlen((yyvsp[-2].string));
		/* "ob" and "name" -> ":ob::name" */
		(yyval.string) = scratch_alloc(l + strlen((yyvsp[0].string)) + 4);
		*((yyval.string)) = ':';
		strcpy((yyval.string) + 1, (yyvsp[-2].string));
		strcpy((yyval.string) + l + 1, "::");
		strcpy((yyval.string) + l + 3, (yyvsp[0].string));
		scratch_free((yyvsp[-2].string));
		scratch_free((yyvsp[0].string));
	    }
#line 5485 "grammar.c"
    break;

  case 235:
#line 3073 "grammar.y"
            {
		/* x != 0 -> x */
		if (IS_NODE((yyvsp[-3].node), NODE_BINARY_OP, F_NE)) {
		    if (IS_NODE((yyvsp[-3].node)->r.expr, NODE_NUMBER, 0))
			(yyvsp[-3].node) = (yyvsp[-3].node)->l.expr;
		    else if (IS_NODE((yyvsp[-3].node)->l.expr, NODE_NUMBER, 0))
			     (yyvsp[-3].node) = (yyvsp[-3].node)->r.expr;
		}

		/* TODO: should optimize if (0), if (1) here.  
		 * Also generalize this.
		 */

		if ((yyvsp[-1].node) == 0) {
		    if ((yyvsp[0].node) == 0) {
			/* if (x) ; -> x; */
			(yyval.node) = insert_pop_value((yyvsp[-3].node));
			break;
		    } else {
			/* if (x) {} else y; -> if (!x) y; */
			parse_node_t *repl;
			
			CREATE_UNARY_OP(repl, F_NOT, TYPE_NUMBER, (yyvsp[-3].node));
			(yyvsp[-3].node) = repl;
			(yyvsp[-1].node) = (yyvsp[0].node);
			(yyvsp[0].node) = 0;
		    }
		}
		CREATE_IF((yyval.node), (yyvsp[-3].node), (yyvsp[-1].node), (yyvsp[0].node));
	    }
#line 5520 "grammar.c"
    break;

  case 236:
#line 3107 "grammar.y"
            {
		(yyval.node) = 0;
	    }
#line 5528 "grammar.c"
    break;

  case 237:
#line 3111 "grammar.y"
            {
		(yyval.node) = (yyvsp[0].node);
            }
#line 5536 "grammar.c"
    break;


#line 5540 "grammar.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *, YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;


#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif


/*-----------------------------------------------------.
| yyreturn -- parsing is finished, return the result.  |
`-----------------------------------------------------*/
yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[+*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 3115 "grammar.y"




