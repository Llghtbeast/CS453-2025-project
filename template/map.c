#include "map.h"

struct map_t *map_init()
{
    struct map_t *m = (struct map_t *) malloc(sizeof(struct map_t));
    if (!m) return NULL;
    m->count = 0;
    memset(m->keys, 0, sizeof(m->keys));
    memset(m->values, 0, sizeof(m->values));
    return m;
}

bool map_add(struct map_t *map, void *ptr, int32_t value)
{
    if (!map) return false;
    // Check if pointer already in map
    for (uint32_t i = 0; i < map->count; i++) {
        if (map->keys[i] == ptr) {
            map->values[i] = value;
            return true;
        }
    }
    // Check if there is enough space to add
    if (map->count >= MAX_MAP_SIZE) return false;
    map->keys[map->count++] = ptr;
    map->values[map->count++] = value;
    return true;
}

bool map_contains(struct map_t *map, void *ptr)
{
    if (!map) return false;
    // Check if pointer already in map
    for (uint32_t i = 0; i < map->count; i++) {
        if (map->keys[i] == ptr) return true;
    }
}
