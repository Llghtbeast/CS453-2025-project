#include "map.h"

struct map_t *map_init()
{
    struct map_t *m = (struct map_t *) malloc(sizeof(struct map_t));
    if (!m) return NULL;
    m->count = 0;
    memset(m->addresses, 0, sizeof(m->addresses));
    memset(m->sizes, 0, sizeof(m->sizes));
    memset(m->values, 0, sizeof(m->values));
    return m;
}

bool map_add(struct map_t *map, void *ptr, size_t size, void *source)
{
    if (unlikely(!map)) return false;
    // Check if pointer already in map
    for (uint32_t i = 0; i < map->count; i++) {
        if (map->addresses[i] == ptr) {
            map->sizes[i] = size;
            map->values[i] = source;
            return true;
        }
    }
    // Check if there is enough space to add
    if (map->count >= MAX_MAP_SIZE) return false;
    map->addresses[map->count++] = ptr;
    map->sizes[map->count] = size;
    map->values[map->count++] = source;
    return true;
}

bool map_contains(struct map_t *map, void *ptr)
{
    if (unlikely(!map)) return false;
    // Check if pointer already in map
    for (uint32_t i = 0; i < map->count; i++) {
        if (map->addresses[i] == ptr) return true;
    }
}

bool map_get(struct map_t *map, void *ptr, void **size, void *target)
{
    return false;
}

void map_free(struct map_t *map)
{
    if (unlikely(!map)) return;
    free(map);
}

uint32_t map_size(struct map_t *map)
{
    return map->count;
}
