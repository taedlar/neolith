/* mapping.h - 1992/07/19 */

#ifndef _MAPPING_H
#define _MAPPING_H

#define MAP_POINTER_HASH(x) (((POINTER_INT)x) >> 4)

typedef struct mapping_node_s {
    struct mapping_node_s *next;
    svalue_t values[2];
} mapping_node_t;

#define MNB_SIZE 256

typedef struct mapping_node_block_s {
    struct mapping_node_block_s *next;
    mapping_node_t nodes[MNB_SIZE];
} mapping_node_block_t;

#define MAX_TABLE_SIZE 32768
#define MAP_HASH_TABLE_SIZE 8	/* must be a power of 2 */
#define FILL_PERCENT 80		/* must not be larger than 99 */

#define MAPSIZE(size) sizeof(mapping_t)

struct mapping_s {
    unsigned short ref;		/* how many times this map has been
				 * referenced */
#ifdef DEBUG
    int extra_ref;
#endif
    mapping_node_t **table;	/* the hash table */
    unsigned short table_size;	/* # of buckets in hash table == power of 2 */
    unsigned short unfilled;	/* # of buckets among 80% of total buckets that do not have entries */
    int count;			/* total # of nodes actually in mapping  */
};

typedef struct finfo_s {
    char *func;
    object_t *obj;
    svalue_t *extra;
    funptr_t *fp;
}       finfo_t;

typedef struct vinfo_s {
    array_t *v;
    int pos, size, w;
}       vinfo_t;

typedef struct minfo_s {
    mapping_t *map, *newmap;
}       minfo_t;

#define mapping_too_large() \
    error("Mapping too large.\n");

#ifndef max
#define max(x,y) ((x) > (y)) ? (x) : (y)
#endif

/*
 * mapping.c
 */
extern int num_mappings;
extern int total_mapping_size;
extern int total_mapping_nodes;

int msameval(svalue_t *, svalue_t *);
int mapping_save_size(mapping_t *);
mapping_t *mapTraverse(mapping_t *, int (*) (mapping_t *, mapping_node_t *, void *), void *);
mapping_t *load_mapping_from_aggregate(svalue_t *, int);
mapping_t *allocate_mapping(int);
void free_mapping(mapping_t *);
svalue_t *find_in_mapping(mapping_t *, svalue_t *);
svalue_t *find_string_in_mapping(mapping_t *, char *);
svalue_t *find_for_insert(mapping_t *, svalue_t *, int);
void absorb_mapping(mapping_t *, mapping_t *);
void mapping_delete(mapping_t *, svalue_t *);
mapping_t *add_mapping(mapping_t *, mapping_t *);
mapping_node_t *new_map_node(void);
void free_node(mapping_node_t *);
void map_mapping(svalue_t *, int);
void filter_mapping(svalue_t *, int);
mapping_t *compose_mapping(mapping_t *, mapping_t *, unsigned short);
array_t *mapping_indices(mapping_t *);
array_t *mapping_values(mapping_t *);
array_t *mapping_each(mapping_t *);
char *save_mapping(mapping_t *);
void dealloc_mapping(mapping_t *);
void mark_mapping_node_blocks(void);

void add_mapping_pair(mapping_t *, char *, int);
void add_mapping_string(mapping_t *, char *, char *);
void add_mapping_malloced_string(mapping_t *, char *, char *);
void add_mapping_object(mapping_t *, char *, object_t *);
void add_mapping_array(mapping_t *, char *, array_t *);
void add_mapping_shared_string(mapping_t *, char *, char *);

int growMap (mapping_t * m);
int restore_hash_string (char **val, svalue_t * sv);

#endif				/* _MAPPING_H */
