#ifndef VARIABLE_CACHE_H
#define VARIABLE_CACHE_H

#include <stdint.h>

#define cache_set stash
void cache_set(const char *name, uint64_t value);

#define cache_get(x) cache(x)
#define assert_cache(x) (assert(cache(x)), cache(x))
uint64_t cache_get(const char *name);

int serialized_to_file(const char *filepath);

int unserialized_from_file(const char *filepath);

#endif
