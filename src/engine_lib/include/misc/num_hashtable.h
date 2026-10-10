#pragma once

#include <stddef.h>

typedef struct te_num_hashtable te_num_hashtable;
typedef struct te_num_hashtable_iterator te_num_hashtable_iterator;

/* hashtable where keys have fixed size (size_t used),
 * specified "max_item_count" is not a hard limit,
 * in the "free" callback you also must free the specified value pointer,
 * specify NULL as the "free" callback if not needed */
te_num_hashtable* num_hashtable_create(
    size_t max_item_count, size_t sizeof_value, void (*free_value)(void* value));
void num_hashtable_destroy(te_num_hashtable* ht);

/* copies data from the value pointer to a newly allocated space with sizeof_value
 * so if you specified a pointer to a heap allocated value free the pointer you specified
 * after this function returns (the data is copied to a different pointer now)
 * returns 1 if newly inserted, 0 if already existed previously (overwrote value) */
unsigned char num_hashtable_insert(te_num_hashtable* ht, size_t key, const void* value);

/* returns 1 if actually erased an item, 0 if was not found */
unsigned char num_hashtable_erase(te_num_hashtable* ht, size_t key);

/* removes all elements */
void num_hashtable_clear(te_num_hashtable* ht);

/* returns NULL if not found */
void* num_hashtable_find(te_num_hashtable* ht, size_t key);

size_t num_hashtable_get_item_count(te_num_hashtable* ht);

/* only use for debugging purposes, very long operation */
size_t num_hashtable_calc_collision_count(te_num_hashtable* ht);

te_num_hashtable_iterator* num_hashtable_iterator_create(te_num_hashtable* ht);
void num_hashtable_iterator_destroy(te_num_hashtable_iterator* it);
/* returns NULL if reached end */
void* num_hashtable_iterator_next(te_num_hashtable_iterator* it);

size_t calc_string_hash(const char* str);
