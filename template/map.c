#include "map.h"

struct map_t *map_init()
{
    struct map_t *m = (struct map_t *) malloc(sizeof(struct map_t));
    if (!m) return NULL;
    m->count = 0;
    memset(m->targets, 0, sizeof(m->targets));
    memset(m->sizes, 0, sizeof(m->sizes));
    memset(m->sources, 0, sizeof(m->sources));
    return m;
}

bool map_add(struct map_t *map, void *source, size_t size, void *target)
{
    if (unlikely(!map)) return false;
    // Check if pointer already in map
    for (uint32_t i = 0; i < map->count; i++) {
        if (map->sources[i] == source) {
            map->sizes[i] = size;
            map->targets[i] = target;
            return true;
        }
    }
    // Check if there is enough space to add
    if (map->count >= MAX_MAP_SIZE) return false;
    map->sources[map->count] = source;
    map->sizes[map->count] = size;
    map->targets[map->count++] = target;
    return true;
}

bool map_contains(struct map_t *map, void *source)
{
    if (unlikely(!map)) return false;
    // Check if pointer already in map
    for (uint32_t i = 0; i < map->count; i++) {
        if (map->targets[i] == source) return true;
    }
}

bool map_get(struct map_t *map, void *source, void *size, void *target)
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
