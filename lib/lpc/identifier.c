#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "hash.h"
#include "identifier.h"
#include "lex.h"

ident_hash_elem_list_t *ihe_list = NULL;

static size_t num_keywords = 0;
static int num_free = 0;
static ident_hash_elem_t **ident_hash_table; /* ident hash, current position */
static ident_hash_elem_t **ident_hash_head;  /* ident hash, head of permanent idents */
static ident_hash_elem_t **ident_hash_tail;  /* ident hash, tail of permanent idents */

static ident_hash_elem_t *ident_dirty_list = 0;

/* identifier hash table stuff, size must be an even power of two */
#define IDENT_HASH_SIZE 1024
#define IdentHash(s) (whashstr((s), 20) & (IDENT_HASH_SIZE - 1))

/* The identifier table is hashed for speed.  The hash chains are circular
 * linked lists, so that we can rotate them, since identifier lookup is
 * rather irregular (i.e. we're likely to be asked about the same one
 * quite a number of times in a row).  This isn't as fast as moving entries
 * to the front but is done this way for two reasons:
 *
 * 1. this allows us to keep permanent identifiers consecutive and clean
 *    up faster
 * 2. it would only be faster in cases where two identifiers with the same
 *    hash value are used often within close proximity in the source.
 *    This should be rare, esp since the hash table is fairly sparse.
 *
 * ident_hash_table[hash] points to our current position (last successful lookup) in the bucket
 * ident_hash_head[hash] points to the first permanent identifier in the bucket
 * ident_hash_tail[hash] points to the last permanent identifier in the bucket
 * ident_dirty_list is a linked list of identifiers that need to be cleaned
 * when we're done; this happens if you define a global or function with
 * the same name (hashed) as an efun or simul efun.
 */

#define CHECK_ELEM(x, y, z) if (!strcmp((x)->name, (y))) { \
      if (((x)->token & IHE_RESWORD) || ((x)->sem_value)) { z } \
      else return 0; }

/**
 * Lookup an identifier by name in the identifier hash table.
 * Each bucket in the hash table is a circular linked list.
 * In the case of collisions, the bucket is searched starting from current position (last lookup).
 * When found, the found element is rotated to the front of the list for faster future access.
 * 
 * @param name The name of the identifier.
 * @return A pointer to the identifier hash element, or NULL if not found.
 */
ident_hash_elem_t *lookup_ident (const char *name) {
  int h = IdentHash (name);
  ident_hash_elem_t *hptr, *hptr2;

  if (ident_hash_table && (hptr = ident_hash_table[h])) /* non-empty bucket */
    {
      CHECK_ELEM (hptr, name, return hptr;); /* if found, already at front */
      hptr2 = hptr->next;
      while (hptr2 != hptr)
        {
          CHECK_ELEM (hptr2, name, ident_hash_table[h] = hptr2; return hptr2;); /* if found, rotate to here */
          hptr2 = hptr2->next;
        }
    }
  return 0; /* not found */
}

/**
 * Find or add a permanent identifier. The following identifiers are added as permanent:
 * - efuns
 * - simul efuns
 * (reserved words are also permanent, but they are added by add_keyword() in lex.c)
 * @param name The name of the identifier. No reference is made to the string after this call.
 * @return A pointer to the identifier hash element.
 */
ident_hash_elem_t* find_or_add_perm_ident (char *name, short token_flag) {
  int h = IdentHash (name);
  ident_hash_elem_t *hptr, *hptr2;

  if ((hptr = ident_hash_table[h]))
    {
      if (!strcmp (hptr->name, name))
        {
          hptr->token |= token_flag;
          return hptr; /* found */
        }
      hptr2 = hptr->next;
      while (hptr2 != hptr)
        {
          if (!strcmp (hptr2->name, name))
            {
              hptr2->token |= token_flag;
              return hptr2; /* found */
            }
          hptr2 = hptr2->next;
        }
      /* collision, add to slot, a circular linked list */
      hptr = ALLOCATE (ident_hash_elem_t, TAG_PERM_IDENT, "find_or_add_perm_ident:1");
      hptr->next = ident_hash_head[h]->next;
      ident_hash_head[h]->next = hptr;
      if (ident_hash_head[h] == ident_hash_tail[h])
        ident_hash_tail[h] = hptr;
    }
  else
    {
      /* no collision, add to hash table */
      hptr = (ident_hash_table[h] = ALLOCATE (ident_hash_elem_t, TAG_PERM_IDENT, "find_or_add_perm_ident:2"));
      ident_hash_head[h] = hptr; /* first permanent ident */
      ident_hash_tail[h] = hptr; /* last permanent ident */
      hptr->next = hptr;
    }
  
  /* a new permanent identifier is added */
  hptr->name = name;
  hptr->token = token_flag;
  hptr->sem_value = 0;
  hptr->dn.simul_num = -1;
  hptr->dn.local_num = -1;
  hptr->dn.global_num = -1;
  hptr->dn.efun_num = -1;
  hptr->dn.function_num = -1;
  hptr->dn.class_num = -1;
  return hptr;
}

typedef struct lname_linked_buf_s {
  struct lname_linked_buf_s *next;
  char block[4096];
} lname_linked_buf_t;

static lname_linked_buf_t *lnamebuf = 0;
static size_t lb_index = 4096;

/**
 * Allocate a local name string.
 * @param name The name to allocate.
 * @return A pointer to the allocated name string.
 */
static char *alloc_local_name (const char *name) {
  size_t len = strlen (name) + 1; /* include null terminator */
  char *res;

  if (lb_index + len > sizeof (lnamebuf->block))
    {
      lname_linked_buf_t *new_buf;
      new_buf = ALLOCATE (lname_linked_buf_t, TAG_COMPILER, "alloc_local_name");
      new_buf->next = lnamebuf;
      lnamebuf = new_buf; /* add to head of list */
      lb_index = 0;
    }
  res = &(lnamebuf->block[lb_index]);
  strcpy (res, name);
  lb_index += len;
  return res;
}

/**
 * Free unused identifiers from the identifier hash table.
 */
void free_unused_identifiers () {
  ident_hash_elem_list_t *ihel, *next;
  lname_linked_buf_t *lnb, *lnbn;
  int i;

  /* clean up dirty idents */
  while (ident_dirty_list)
    {
      if (ident_dirty_list->dn.function_num != -1)
        {
          ident_dirty_list->dn.function_num = -1;
          ident_dirty_list->sem_value--;
        }
      if (ident_dirty_list->dn.global_num != -1)
        {
          ident_dirty_list->dn.global_num = -1;
          ident_dirty_list->sem_value--;
        }
      if (ident_dirty_list->dn.class_num != -1)
        {
          ident_dirty_list->dn.class_num = -1;
          ident_dirty_list->sem_value--;
        }
      ident_dirty_list = ident_dirty_list->next_dirty;
    }

  /* remove non-permanent identifier hash elements from hash table */
  for (i = 0; i < IDENT_HASH_SIZE; i++)
    if ((ident_hash_table[i] = ident_hash_head[i]))
      ident_hash_tail[i]->next = ident_hash_head[i];

  /* free all non-permanent identifier hash elements */
  ihel = ihe_list;
  while (ihel)
    {
      next = ihel->next;
      FREE (ihel);
      ihel = next;
    }
  ihe_list = 0;
  num_free = 0;

  /* free all allocated local name buffers */
  lnb = lnamebuf;
  while (lnb)
    {
      lnbn = lnb->next;
      FREE (lnb);
      lnb = lnbn;
    }
  lnamebuf = 0;
  lb_index = 4096;
}

/**
 * Quickly allocate an identifier hash element.
 * @return A pointer to the allocated identifier hash element.
 */
static ident_hash_elem_t* quick_alloc_ident_entry () {
  if (num_free)
    {
      num_free--;
    }
  else
    {
      ident_hash_elem_list_t *ihel;
      ihel = ALLOCATE (ident_hash_elem_list_t, TAG_COMPILER, "quick_alloc_ident_entry");
      ihel->next = ihe_list;
      ihe_list = ihel;
      num_free = sizeof (ihe_list->items) / sizeof (ihe_list->items[0]) - 1;
    }
  return &(ihe_list->items[num_free]);
}

ident_hash_elem_t *find_or_add_ident (char *name, int flags) {

  int h = IdentHash (name);
  ident_hash_elem_t *hptr, *hptr2;

  if ((hptr = ident_hash_table[h]))
    {
      if (!strcmp (hptr->name, name))
        {
          /* found */
          if ((hptr->token & IHE_PERMANENT) && (flags & FOA_GLOBAL_SCOPE)
              && (hptr->dn.function_num == -1)
              && (hptr->dn.global_num == -1)
              && (hptr->dn.class_num == -1))
            {
              hptr->next_dirty = ident_dirty_list;
              ident_dirty_list = hptr;
            }
          return hptr;
        }
      hptr2 = hptr->next;
      while (hptr2 != hptr)
        {
          if (!strcmp (hptr2->name, name))
            {
              /* found */
              if ((hptr2->token & IHE_PERMANENT) && (flags & FOA_GLOBAL_SCOPE)
                  && (hptr2->dn.function_num == -1)
                  && (hptr2->dn.global_num == -1)
                  && (hptr2->dn.class_num == -1))
                {
                  hptr2->next_dirty = ident_dirty_list;
                  ident_dirty_list = hptr2;
                }
              ident_hash_table[h] = hptr2;	/* rotate */
              return hptr2;
            }
          hptr2 = hptr2->next;
        }
    }

  hptr = quick_alloc_ident_entry ();
  if (!(hptr2 = ident_hash_tail[h]) && !(hptr2 = ident_hash_table[h]))
    {
      /* empty slot */
      ident_hash_table[h] = hptr->next = hptr;
    }
  else
    {
      /* collision, insert to the circular linked list */
      hptr->next = hptr2->next;
      hptr2->next = hptr;
    }

  if (flags & FOA_NEEDS_MALLOC)
    {
      hptr->name = alloc_local_name (name);
    }
  else
    {
      hptr->name = name;
    }
  hptr->token = 0;
  hptr->sem_value = 0;
  hptr->dn.simul_num = -1;
  hptr->dn.local_num = -1;
  hptr->dn.global_num = -1;
  hptr->dn.efun_num = -1;
  hptr->dn.function_num = -1;
  hptr->dn.class_num = -1;
  return hptr;
}

/**
 * Add a keyword to the identifier hash table.
 * 
 * A keyword is a permanent identifier that is always defined (can be found by lookup_ident
 * regardless of sem_value).
 * 
 * @param name The name of the keyword.
 * @param entry Pointer to the keyword entry. This must point to a mutable memory location
 * that remains valid until deinit_identifiers() is called.
 */
void add_keyword (const char *name, keyword_t* entry) {
  int h = IdentHash (name);

  if (ident_hash_table[h])
    {
      entry->next = ident_hash_head[h]->next;
      ident_hash_head[h]->next = (ident_hash_elem_t *) entry;
      if (ident_hash_head[h] == ident_hash_tail[h])
        ident_hash_tail[h] = (ident_hash_elem_t *) entry;
    }
  else
    {
      ident_hash_head[h] = (ident_hash_elem_t *) entry;
      ident_hash_tail[h] = (ident_hash_elem_t *) entry;
      ident_hash_table[h] = (ident_hash_elem_t *) entry;
      entry->next = (ident_hash_elem_t *) entry;
    }
  entry->token |= IHE_RESWORD;
}

/**
 * @brief Initialize identifier management structures.
 */
void init_identifiers () {
  int i;

  /* allocate all three tables together */
  ident_hash_table = CALLOCATE (IDENT_HASH_SIZE * 3, ident_hash_elem_t *,
                                TAG_IDENT_TABLE, "init_identifiers");
  ident_hash_head = (ident_hash_elem_t **) & ident_hash_table[IDENT_HASH_SIZE];
  ident_hash_tail = (ident_hash_elem_t **) & ident_hash_table[2 * IDENT_HASH_SIZE];

  /* clean all three tables */
  for (i = 0; i < IDENT_HASH_SIZE * 3; i++)
    {
      ident_hash_table[i] = 0;
    }

  /* add keywords */
  num_keywords = init_keywords ();
}

/**
 * Deinitialize identifier management structures.
 * All identifiers including permanents are freed.
 * (reserved words are not freed since they point to static memory locations)
 */
void deinit_identifiers () {
  int i, n = 0, r = 0;
  free_unused_identifiers ();
  /* free permanent identifiers without IHE_RESWORD flag */
  for (i = 0; i < IDENT_HASH_SIZE; i++)
    {
      ident_hash_elem_t *head, *hptr = head = ident_hash_table[i];
      while (hptr)
        {
          ident_hash_elem_t *tmp = hptr;
          hptr = hptr->next;
          if (!(tmp->token & IHE_RESWORD))
            {
              /* non-reserved words permanent identifiers includes efuns and simul_efuns.
               * They are allocated in find_or_add_perm_ident() and never freed when finished compiling.
               * So we free them here.
               */
              FREE (tmp);
              n++;
            }
          else
            r++; /* reserved words are actually global variables of type keyword_t, and cannot be freed */
          
          if (hptr == head) /* end of circular linked list */
            break;
        }
      ident_hash_table[i] = NULL;
    }
  FREE (ident_hash_table);
  ident_hash_table = NULL;
  ident_hash_head = NULL;
  ident_hash_tail = NULL;
  debug_info ("freed %d permanent identifiers (leaked %d).", n, r - (int)num_keywords);
}
