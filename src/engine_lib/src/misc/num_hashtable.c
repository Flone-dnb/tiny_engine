#include <misc/num_hashtable.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

struct te_num_hashtable_iterator {
    te_num_hashtable* ht;
    size_t entry_idx;
    size_t item_idx;
};

typedef struct te_num_hashtable_entry_data {
    size_t key;
    unsigned char* value;
} te_num_hashtable_entry_data;

typedef struct te_num_hashtable_entry {
    te_num_hashtable_entry_data* items;
    size_t items_count;
    size_t items_capacity;
} te_num_hashtable_entry;

struct te_num_hashtable {
    void (*free_value)(void* value);

    te_num_hashtable_entry** entries; /* can have NULLs */
    size_t sizeof_value;
    size_t entry_count;
    size_t total_item_count;
    size_t entry_count_msb_idx;
};

size_t
round_to_power_of_2(size_t x) {
    x--;

    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;

    if (sizeof(size_t) == 8) {
        x |= x >> 32;
    }

    return x + 1;
}

size_t
calc_msb_from_power_of_2_value(size_t n) {
    size_t pos = 0;
    while ((n & 1) == 0) {
        n >>= 1;
        pos++;
    }
    return pos;
}

te_num_hashtable*
num_hashtable_create(
    size_t max_item_count, size_t sizeof_value, void (*free_value)(void* value)) {
    te_num_hashtable* ht;

    if (max_item_count < 2) {
        abort();
    }

    ht = malloc(sizeof(te_num_hashtable));
    ht->free_value = free_value == NULL ? free : free_value;
    ht->sizeof_value = sizeof_value;
    ht->total_item_count = 0;
    ht->entry_count = round_to_power_of_2(max_item_count);
    ht->entry_count_msb_idx = calc_msb_from_power_of_2_value(ht->entry_count);

    ht->entries = malloc(sizeof(te_num_hashtable_entry*) * ht->entry_count);
    memset(ht->entries, 0, sizeof(te_num_hashtable_entry*) * ht->entry_count);

    return ht;
}

void
num_hashtable_destroy(te_num_hashtable* ht) {
    size_t i, j;

    for (i = 0; i < ht->entry_count; i++) {
        te_num_hashtable_entry* entry = ht->entries[i];
        if (entry == NULL) {
            continue;
        }
        for (j = 0; j < entry->items_count; j++) {
            ht->free_value(entry->items[j].value);
        }
        free(entry->items);
        free(entry);
    }
    free(ht->entries);

    free(ht);
}

static size_t
get_entry_idx(te_num_hashtable* ht, size_t key) {
    /* Knuth's multiplicative hash */
#if ULONG_MAX == 0xFFFFFFFFUL
    size_t fractional_part = key * 2654435769U;
    return fractional_part >> (32 - ht->entry_count_msb_idx);
#else
    size_t mult = 0x9E3779B9UL; /* using mult because ULL not available in C89 */
    mult = (mult << 32) | 0x7F4A7C15UL;
    return (key * mult) >> (64 - ht->entry_count_msb_idx);
#endif
}

static void
init_entry(
    te_num_hashtable* ht, te_num_hashtable_entry* entry, size_t key, const void* value) {
    entry->items = malloc(sizeof(te_num_hashtable_entry_data));
    entry->items_capacity = 1;
    entry->items_count = 1;

    entry->items[0].key = key;

    entry->items[0].value = malloc(ht->sizeof_value);
    memcpy(entry->items[0].value, value, ht->sizeof_value);
}

unsigned char
num_hashtable_insert(te_num_hashtable* ht, size_t key, const void* value) {
    te_num_hashtable_entry* entry;
    te_num_hashtable_entry_data* item;
    size_t entry_idx;
    size_t i;

    entry_idx = get_entry_idx(ht, key);
    if (ht->entries[entry_idx] == NULL) {
        ht->entries[entry_idx] = malloc(sizeof(te_num_hashtable_entry));
        entry = ht->entries[entry_idx];
        init_entry(ht, entry, key, value);
        ht->total_item_count += 1;
        return 1;
    }

    entry = ht->entries[entry_idx];
    for (i = 0; i < entry->items_count; i++) {
        item = &entry->items[i];
        if (key == item->key) {
            ht->free_value(item->value);
            memcpy(item->value, value, ht->sizeof_value);
            return 0;
        }
    }

    if (entry->items_count == entry->items_capacity) {
        te_num_hashtable_entry_data* new_items;

        if (entry->items_capacity < 4) {
            entry->items_capacity += 1;
        } else {
            entry->items_capacity = (size_t)((float)ceil((float)entry->items_capacity * 1.5f));
        }

        new_items = malloc(sizeof(te_num_hashtable_entry_data) * entry->items_capacity);
        memcpy(
            new_items, entry->items, sizeof(te_num_hashtable_entry_data) * entry->items_count);

        free(entry->items);
        entry->items = new_items;
    }

    item = &entry->items[entry->items_count];
    entry->items_count += 1;

    item->key = key;

    item->value = malloc(ht->sizeof_value);
    memcpy(item->value, value, ht->sizeof_value);

    ht->total_item_count += 1;
    return 1;
}

unsigned char
num_hashtable_erase(te_num_hashtable* ht, size_t key) {
    te_num_hashtable_entry* entry;
    te_num_hashtable_entry_data* item;
    size_t entry_idx;
    size_t i;

    entry_idx = get_entry_idx(ht, key);
    if (ht->entries[entry_idx] == NULL) {
        return 0;
    }

    entry = ht->entries[entry_idx];
    for (i = 0; i < entry->items_count; i++) {
        item = &entry->items[i];
        if (key != item->key) {
            continue;
        }

        ht->free_value(item->value);

        if (entry->items_count == 1) {
            free(entry->items);
            entry->items = NULL;
            entry->items_count = 0;
            entry->items_capacity = 0;

            free(entry);
            ht->entries[entry_idx] = NULL;
        } else if (i == entry->items_count - 1) {
            entry->items_count -= 1;
        } else {
            memcpy(
                entry->items + i, entry->items + (entry->items_count - 1),
                sizeof(te_num_hashtable_entry_data));
            entry->items_count -= 1;
        }

        ht->total_item_count -= 1;
        return 1;
    }

    return 0;
}

void
num_hashtable_clear(te_num_hashtable* ht) {
    te_num_hashtable_entry* entry;
    te_num_hashtable_entry_data* item;
    size_t entry_idx;
    size_t i;

    for (entry_idx = 0; entry_idx < ht->entry_count; entry_idx++) {
        if (ht->entries[entry_idx] == NULL) {
            continue;
        }
        entry = ht->entries[entry_idx];

        for (i = 0; i < entry->items_count; i++) {
            item = &entry->items[i];
            ht->free_value(item->value);
        }
        free(entry->items);
        entry->items = NULL;
        entry->items_capacity = 0;
        entry->items_count = 0;

        free(entry);
        ht->entries[entry_idx] = NULL;
    }
}

void*
num_hashtable_find(te_num_hashtable* ht, size_t key) {
    size_t entry_idx;

    entry_idx = get_entry_idx(ht, key);
    if (ht->entries[entry_idx] == NULL) {
        return NULL;
    } else {
        te_num_hashtable_entry* entry;
        te_num_hashtable_entry_data* item;
        size_t i;

        entry = ht->entries[entry_idx];
        for (i = 0; i < entry->items_count; i++) {
            item = &entry->items[i];
            if (key != item->key) {
                continue;
            }

            return item->value;
        }

        return NULL;
    }
}

size_t
num_hashtable_get_item_count(te_num_hashtable* ht) {
    return ht->total_item_count;
}

size_t
num_hashtable_calc_collision_count(te_num_hashtable* ht) {
    size_t count;
    size_t i;

    count = 0;
    for (i = 0; i < ht->entry_count; i++) {
        if (ht->entries[i] == NULL) {
            continue;
        }
        count += ht->entries[i]->items_count - 1;
    }

    return count;
}

te_num_hashtable_iterator*
num_hashtable_iterator_create(te_num_hashtable* ht) {
    te_num_hashtable_iterator* it = malloc(sizeof(te_num_hashtable_iterator));
    it->ht = ht;
    it->entry_idx = 0;
    it->item_idx = 0;

    while (it->ht->entries[it->entry_idx] == NULL && it->entry_idx < it->ht->entry_count) {
        it->entry_idx += 1;
    }

    return it;
}

void
num_hashtable_iterator_destroy(te_num_hashtable_iterator* it) {
    free(it);
}

void*
num_hashtable_iterator_next(te_num_hashtable_iterator* it) {
    if (it->entry_idx >= it->ht->entry_count) {
        return NULL;
    } else {
        void* value;
        te_num_hashtable_entry* entry = it->ht->entries[it->entry_idx];

        value = entry->items[it->item_idx].value;

        if (it->item_idx + 1 < entry->items_count) {
            it->item_idx += 1;
            return value;
        }

        it->entry_idx += 1;
        it->item_idx = 0;

        while (it->ht->entries[it->entry_idx] == NULL && it->entry_idx < it->ht->entry_count) {
            it->entry_idx += 1;
        }

        return value;
    }
}

size_t
calc_string_hash(const char* str) {
    size_t hash;
    size_t prime;
    int c;

    if (sizeof(size_t) >= 8) {
        /* 64-bit FNV offset basis: 14695981039346656037 */
        hash = ((size_t)0xCBF29CE4UL << 32) | (size_t)0x428A2F98UL;

        /* 64-bit FNV prime: 1099511628211 */
        prime = ((size_t)0x00000100UL << 32) | (size_t)0x000001B3UL;
    } else {
        /* 32-bit constants */
        hash = (size_t)2166136261UL;
        prime = (size_t)16777619UL;
    }

    while ((c = (unsigned char)*str++)) {
        hash ^= (size_t)c;
        hash *= prime;
    }

    return hash;
}
