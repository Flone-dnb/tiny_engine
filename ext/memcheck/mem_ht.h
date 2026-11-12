#pragma once

#include <stddef.h>

typedef struct mem_ht mem_ht;
typedef struct mem_ht_iterator mem_ht_iterator;

/* hashtable where keys have fixed size (size_t used),
 * specified "max_item_count" is not a hard limit,
 * in the "free" callback you also must free the specified value pointer,
 * specify NULL as the "free" callback if not needed */
mem_ht*
mem_ht_create(size_t max_item_count, size_t sizeof_value, void (*free_value)(void* value));
void mem_ht_destroy(mem_ht* ht);

/* copies data from pointers to hashmap entry
 * returns 1 if newly inserted, 0 if already existed previously (overwrote value) */
unsigned char mem_ht_insert(mem_ht* ht, size_t key, const void* value);

/* returns 1 if actually erased an item, 0 if was not found */
unsigned char mem_ht_erase(mem_ht* ht, size_t key);

/* removes all elements */
void mem_ht_clear(mem_ht* ht);

/* returns NULL if not found */
void* mem_ht_find(mem_ht* ht, size_t key);

size_t mem_ht_get_item_count(mem_ht* ht);

/* only use for debugging purposes, very long operation */
size_t mem_ht_calc_collision_count(mem_ht* ht);

mem_ht_iterator* mem_ht_iterator_create(mem_ht* ht);
void mem_ht_iterator_destroy(mem_ht_iterator* it);
/* returns NULL if reached end */
void* mem_ht_iterator_next(mem_ht_iterator* it);
