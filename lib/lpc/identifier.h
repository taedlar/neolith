#pragma once

/* to speed up cleaning the hash table, and identify the union */
#define IHE_RESWORD    0x8000
#define IHE_EFUN       0x4000
#define IHE_SIMUL      0x2000
#define IHE_PERMANENT  (IHE_RESWORD | IHE_EFUN | IHE_SIMUL)
#define TOKEN_MASK     0x0fff

#define INDENT_HASH_SIZE 1024 /* must be a power of 2 */

/* identifier semantics */
typedef struct defined_name_s {
  short local_num, global_num, efun_num;
  short function_num, simul_num, class_num;
} defined_name_t;

typedef struct ident_hash_elem_s {
    char *name;
    short token;                /* only flags */
    short sem_value;            /* 0: reserved word or not defined, >1 a count of the ambiguity */
    struct ident_hash_elem_s *next;
/* the fields above must correspond to struct keyword_t */
    struct ident_hash_elem_s *next_dirty;
    defined_name_t dn;
} ident_hash_elem_t;

typedef struct ident_hash_elem_list_s {
  struct ident_hash_elem_list_s *next;
  ident_hash_elem_t items[128];
} ident_hash_elem_list_t;

extern ident_hash_elem_list_t *ihe_list;

typedef struct keyword_s {
    char *word;
    unsigned short token;       /* flags here too */
    short sem_value;            /* semantic value for predefined tokens */
    ident_hash_elem_t *next;
/* the fields above must correspond to struct ident_hash_elem */
    short min_args;             /* Minimum number of arguments. */
    short max_args;             /* Maximum number of arguments. */
    short ret_type;             /* The return type used by the compiler. */
    unsigned short arg_type1;   /* Type of argument 1 */
    unsigned short arg_type2;   /* Type of argument 2 */
    unsigned short arg_type3;   /* Type of argument 3 */
    unsigned short arg_type4;   /* Type of argument 4 */
    short arg_index;            /* Index pointing to where to find arg type */
    short Default;              /* an efun to use as default for last argument */
} keyword_t;

/* for find_or_add_ident */
#define FOA_GLOBAL_SCOPE       0x1
#define FOA_NEEDS_MALLOC       0x2
ident_hash_elem_t *find_or_add_ident(char *, int);

ident_hash_elem_t *find_or_add_perm_ident(char *, short);
ident_hash_elem_t *lookup_ident(const char *);
void free_unused_identifiers(void);
void add_keyword (const char *name, keyword_t * entry);
void init_identifiers(void);
void deinit_identifiers(void);
