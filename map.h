/* Copyright (c) 2023 Leo Kuznetsov

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#if !defined(MAP_H)
#define MAP_H

#include <stdint.h>
#include <stdbool.h>

#if defined(_MSC_VER) && !defined(__cplusplus) && !defined(inline)
#define inline __inline
#endif

// k - key, b - bytes in key, v - value, h - hash
typedef struct map_entry_s {
    const void* k; size_t b; const void* v; uint32_t h;
} map_entry_t;

// e - entries, c - capacity, n - number of entries
typedef struct map_s { map_entry_t* e; size_t c; size_t n; } map_t;

// All memory management for map entries, keys and values is responsibility
// of the caller.

enum { // Maximum map occupancy is 7/8 (inclusive) of a capacity:
    map_occupancy_numerator = 7,
    map_occupancy_denominator = 8
}; // It is responsibility of a caller not to overflow map.

// Caller may choose dynamically grow map outsise of this code.

void     map_put(map_t* m, const void* k, size_t b, const void* v);
void*    map_get(const map_t* m, const void* k, size_t b);
bool     map_remove(map_t* m, const void* k, size_t b);
intmax_t map_index(const map_t* m, const void* k, size_t b);

#if defined(MAP_IMPLEMENTTATION)

uint32_t map_hash(const uint8_t* k, size_t bytes) { // not best but simplest
    // https://en.wikipedia.org/wiki/Jenkins_hash_function
    uint32_t h = 0;
    for (size_t i = 0; i < bytes; i++) {
        h += k[i];
        h += (h << 10);
        h ^= (h >> 6);
    }
    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);
    // never returns 0 or negatives
    return (h | (h == 0)) & INT32_MAX;
}

inline
bool map_is_deleted(uint32_t hash) { return (hash >> 31) != 0; }

inline
intmax_t map_entry_index(const map_t* m, uint32_t h) {
    const size_t mask = m->c - 1;
    assert((m->c & mask) == 0); // only power of 2 capacities
	return (int32_t)(h & mask);
}

inline
intmax_t map_probe_distance(const map_t* m, uint32_t h, intmax_t p) {
    const size_t mask = m->c - 1;
	return (p + m->c - map_entry_index(m, h)) & mask;
}

inline
void map_store(map_t* m, intmax_t p, uint32_t h, const void* k, size_t b, const void* v) {
    m->e[p].h = h;
    m->e[p].k = k;
    m->e[p].b = b;
    m->e[p].v = v;
    m->n++;
}

inline
void map_swap_uint32(uint32_t *a, uint32_t *b) {
    uint32_t s = *a; *a = *b; *b = s;
}

inline
void map_swap_ptr(void* *a, void* *b) {
    void* s = *a; *a = *b; *b = s;
}

void map_insert_helper(map_t* m, uint32_t h, const void* k, size_t b, const void* v) {
    const size_t mask = m->c - 1;
    intmax_t i = map_entry_index(m, h);
    intmax_t d = 0; // distance
    for (;;) {
        map_entry_t* e = &m->e[i];
        const uint32_t eh = e->h;
        if (eh == 0) { map_store(m, i, h, k, b, v);  return; }
        // If the existing entry has probed less than us, then swap
        // places with existing entry, and keep going to find
        // another slot.
        intmax_t existing_entry_probe_distance = map_probe_distance(m, eh, i);
        if (existing_entry_probe_distance < d) {
            if (map_is_deleted(eh)) { map_store(m, i, h, k, b, v); return; }
            map_swap_uint32(&h, &e->h);
            map_swap_ptr(&k, &e->k);
            map_swap_ptr(&v, &e->v);
            d = existing_entry_probe_distance;
        }
        i = (i + 1) & mask;
        d++;
    }
}

intmax_t map_index(const map_t* m, const void* k, size_t b) {
    const size_t mask = m->c - 1;
    const uint32_t h = map_hash(k, b);
    intmax_t p = map_entry_index(m, h);
    intmax_t d = 0; // distance
    for (;;) {
        const map_entry_t* e = &m->e[p];
        const uint32_t eh = e->h;
        if (eh == 0) {
            return -1;
        } else if (d > map_probe_distance(m, eh, p)) {
            return -1;
        } else if (eh == h && e->b == b && memcmp(e->k, k, b) == 0) {
            return p;
        }
        p = (p + 1) & mask;
        d++;
    }
}

void map_put(map_t* m, const void* k, size_t b, const void* v) {
    assert(m->c >= 8); // caller must provide at least 8 elements
    size_t map_occupancy = m->c * map_occupancy_numerator /
                                  map_occupancy_denominator;
    assert(m->n < map_occupancy);
	map_insert_helper(m, map_hash(k, b), k, b, v);
}

void* map_get(const map_t* m, const void* k, size_t b) {
	const intmax_t p = map_index(m, k, b);
    return p != -1 ? &m->e[p].v : (void*)0;
}

bool map_remove(map_t* m, const void* k, size_t b) {
    enum { num_delete_fixups = 4 };
    // const uint32_t h = map_hash(k, b);
	const intmax_t i = map_index(m, k, b);
	if (i >= 0) {
        const size_t mask = m->c - 1;
        m->n--;
        m->e[i].h  |= 0x80000000; // mark as deleted
        intmax_t ix = i;
        for (intmax_t j = 0; j < num_delete_fixups; j++) {
            const intmax_t next = (ix + 1) & mask;
            if (next == i) break; // looped around
            map_entry_t* ei = &m->e[ix];
            const map_entry_t* en = &m->e[next];
            const uint32_t eh = en->h;
            // if the next entry is empty or has a zero probe distance,
            // then any query will always fail on the next entry anyway,
            // so might as well clear the current one too.
            if (eh == 0 || map_probe_distance(m, eh, next) == 0) {
                ei->h = 0;
                return true;
            }
            // Now, the next entry has probe count of at least 1,
            // so it could use our deleted slot to improve its
            // query perf.
            // First, pretend we're a dupe of the next entry.
            ei->h = en->h;
            // Since we're now a dupe of the next entry, ordering is
            // arbitrary, which means we're allowed to swap places.
            ei->v = en->v;
            ix = next;
        }
        m->e[ix].h |= 0x80000000; // mark as deleted
    }
	return i >= 0;
}

/*  Implementtation is based on:
 *  https://www.sebastiansylvan.com/post/robin-hood-hashing-should-be-your-default-hash-table-implementation/
 *  https://www.sebastiansylvan.com/post/more-on-robin-hood-hashing-2/
 *  https://gist.github.com/ssylvan/5538011
 *  https://web.archive.org/web/20230525061232/https://www.sebastiansylvan.com/post/robin-hood-hashing-should-be-your-default-hash-table-implementation/
 *  https://web.archive.org/web/20220523130535/https://www.sebastiansylvan.com/post/more-on-robin-hood-hashing-2/
 *  Author is aware that there are many way more efficient but more complex hash functions:
 *  https://web.archive.org/web/20230603165743/http://www.burtleburtle.net/bob/hash/doobs.html
 */

#endif // MAP_IMPLEMENTTATION

#endif