#include <mem_ht.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

struct mem_ht_iterator {
    mem_ht* ht;
    size_t entry_idx;
    size_t item_idx;
};

typedef struct mem_ht_entry_data {
    size_t key;
    unsigned char* value;
} mem_ht_entry_data;

typedef struct mem_ht_entry {
    mem_ht_entry_data* items;
    size_t items_count;
    size_t items_capacity;
} mem_ht_entry;

struct mem_ht {
    void (*free_value)(void* value);

    mem_ht_entry** entries; /* can have NULLs */
    size_t sizeof_value;
    size_t entry_count;
    size_t total_item_count;
    size_t entry_count_msb_idx;
};

static size_t
round_to_power_of_2(size_t x) {
    x--;

    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;

#if !defined(MEMCHECK_32BIT)
    x |= x >> 32;
#endif

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

mem_ht*
mem_ht_create(size_t max_item_count, size_t sizeof_value, void (*free_value)(void* value)) {
    mem_ht* ht;

    if (max_item_count < 2) {
        abort();
    }

    ht = malloc(sizeof(mem_ht));
    ht->free_value = free_value == NULL ? free : free_value;
    ht->sizeof_value = sizeof_value;
    ht->total_item_count = 0;
    ht->entry_count = round_to_power_of_2(max_item_count);
    ht->entry_count_msb_idx = calc_msb_from_power_of_2_value(ht->entry_count);

    ht->entries = malloc(sizeof(mem_ht_entry*) * ht->entry_count);
    memset(ht->entries, 0, sizeof(mem_ht_entry*) * ht->entry_count);

    return ht;
}

void
mem_ht_destroy(mem_ht* ht) {
    size_t i, j;

    for (i = 0; i < ht->entry_count; i++) {
        mem_ht_entry* entry = ht->entries[i];
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
get_entry_idx(mem_ht* ht, size_t key) {
    /* Knuth's multiplicative hash */
#if defined(MEMCHECK_32BIT)
    size_t fractional_part = key * 2654435769U;
    return fractional_part >> (32 - ht->entry_count_msb_idx);
#else
    size_t mult = 0x9E3779B9UL; /* using mult because ULL not available in C89 */
    mult = (mult << 32) | 0x7F4A7C15UL;
    return (key * mult) >> (64 - ht->entry_count_msb_idx);
#endif
}

static void
init_entry(mem_ht* ht, mem_ht_entry* entry, size_t key, const void* value) {
    entry->items = malloc(sizeof(mem_ht_entry_data));
    entry->items_capacity = 1;
    entry->items_count = 1;

    entry->items[0].key = key;

    entry->items[0].value = malloc(ht->sizeof_value);
    memcpy(entry->items[0].value, value, ht->sizeof_value);
}

unsigned char
mem_ht_insert(mem_ht* ht, size_t key, const void* value) {
    mem_ht_entry* entry;
    mem_ht_entry_data* item;
    size_t entry_idx;
    size_t i;

    entry_idx = get_entry_idx(ht, key);
    if (ht->entries[entry_idx] == NULL) {
        ht->entries[entry_idx] = malloc(sizeof(mem_ht_entry));
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
        mem_ht_entry_data* new_items;

        if (entry->items_capacity < 4) {
            entry->items_capacity += 1;
        } else {
            entry->items_capacity = (size_t)((float)ceil((float)entry->items_capacity * 1.5f));
        }

        new_items = malloc(sizeof(mem_ht_entry_data) * entry->items_capacity);
        memcpy(new_items, entry->items, sizeof(mem_ht_entry_data) * entry->items_count);

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
mem_ht_erase(mem_ht* ht, size_t key) {
    mem_ht_entry* entry;
    mem_ht_entry_data* item;
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
                sizeof(mem_ht_entry_data));
            entry->items_count -= 1;
        }

        ht->total_item_count -= 1;
        return 1;
    }

    return 0;
}

void
mem_ht_clear(mem_ht* ht) {
    mem_ht_entry* entry;
    mem_ht_entry_data* item;
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
mem_ht_find(mem_ht* ht, size_t key) {
    size_t entry_idx;

    entry_idx = get_entry_idx(ht, key);
    if (ht->entries[entry_idx] == NULL) {
        return NULL;
    } else {
        mem_ht_entry* entry;
        mem_ht_entry_data* item;
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
mem_ht_get_item_count(mem_ht* ht) {
    return ht->total_item_count;
}

size_t
mem_ht_calc_collision_count(mem_ht* ht) {
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

mem_ht_iterator*
mem_ht_iterator_create(mem_ht* ht) {
    mem_ht_iterator* it = malloc(sizeof(mem_ht_iterator));
    it->ht = ht;
    it->entry_idx = 0;
    it->item_idx = 0;

    while (it->ht->entries[it->entry_idx] == NULL && it->entry_idx < it->ht->entry_count) {
        it->entry_idx += 1;
    }

    return it;
}

void
mem_ht_iterator_destroy(mem_ht_iterator* it) {
    free(it);
}

void*
mem_ht_iterator_next(mem_ht_iterator* it) {
    if (it->entry_idx >= it->ht->entry_count) {
        return NULL;
    } else {
        void* value;
        mem_ht_entry* entry = it->ht->entries[it->entry_idx];

        value = entry->items[it->item_idx].value;

        if (it->item_idx + 1 < entry->items_count) {
            it->item_idx += 1;
            return value;
        }

        it->entry_idx += 1;
        it->item_idx = 0;

        while (it->entry_idx < it->ht->entry_count && it->ht->entries[it->entry_idx] == NULL) {
            it->entry_idx += 1;
        }

        return value;
    }
}
