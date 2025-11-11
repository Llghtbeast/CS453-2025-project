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

bool map_add(struct map_t *map, void const *source, size_t size, void *target)
{
    if (unlikely(!map)) return false;
    // Check if pointer already in map
    for (uint32_t i = 0; i < map->count; i++) {
        if (map->sources[i] == source && map->sizes[i] == size) {
            memcpy(map->targets[i], target, size);
            return true;
        }
    }
    // Check if there is enough space to add
    if (map->count >= MAX_MAP_SIZE) return false;
    map->sources[map->count] = source;
    map->sizes[map->count] = size;

    // Create a new container for value (maybe value stored at target changes)
    map->targets[map->count] = malloc(size);
    if(!map->targets[map->count]) {
        return false;
    }
    memcpy(map->targets[map->count], source, size);

    map->count++;
    return true;
}

bool map_contains(struct map_t *map, void const *source)
{
    if (unlikely(!map)) return false;
    // Check if pointer already in map
    for (uint32_t i = 0; i < map->count; i++) {
        if (map->targets[i] == source) return true;
    }
    return false;
}

bool map_get(struct map_t *map, void const *source, size_t size, void *target)
{
    for (size_t i = 0; i < map->count; i++)
    {
        if (map->sources[i] == source && map->sizes[i] == size) {
            memcpy(target, map->targets[i], size);
            return true;
        }
    }
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
