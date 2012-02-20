#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "oscar.h"

#define DEBUG 0
#define LOG(...) { if (DEBUG) fprintf(stderr, __VA_ARGS__); }

struct oscar {
    unsigned int cell_sz;       /* each cell is CELL_SZ bytes */
    unsigned int count;         /* number of cells */
    unsigned int freed;         /* how many were freed since last sweep */
    unsigned int sz;            /* size of RAW, in bytes */
    pool_id sweep;              /* lazy sweep index */
    oscar_memory_cb *mem_cb;    /* memory callback */
    void *mem_udata;            /* userdata for ^ */
    oscar_mark_cb *mark_cb;     /* marking callback */
    void *mark_udata;           /* userdata for ^ */
    oscar_free_cb *free_cb;     /* free callback */
    void *free_udata;           /* userdata for ^ */
    char *raw;                  /* raw memory for storage, COUNT cells */
    char *markbits;             /* mark bit array, at end of RAW */
};

/* An oscar_memory_cb that just calls malloc/free/realloc. */
void *oscar_generic_mem_cb(void *p, size_t old_sz,
                           size_t new_sz, void *udata) {
    if (p == NULL) { return malloc(new_sz); }
    if (new_sz == 0) { free(p); return NULL; }
    return realloc(p, new_sz);
}

static oscar *new_pool(unsigned int cell_sz, unsigned int sz,
                       oscar *p, char *raw,
                       oscar_memory_cb *mem_cb, void *mem_udata,
                       oscar_mark_cb *mark_cb, void *mark_udata,
                       oscar_free_cb *free_cb, void *free_udata) {
    unsigned int start_count = sz / cell_sz /* - mark bits*/;
    if (p == NULL || raw == NULL) return NULL;

    bzero(raw, sz);
    p->cell_sz = cell_sz;
    p->sz = sz;
    p->count = start_count;
    p->freed = 0;
    p->sweep = 0;
    p->mem_cb = mem_cb;
    p->mem_udata = mem_udata;
    p->mark_cb = mark_cb;
    p->mark_udata = mark_udata;
    p->free_cb = free_cb;
    p->free_udata = free_udata;
    p->raw = raw;
    p->markbits = raw + (cell_sz * start_count);
    return p;
}

/* Init a fixed-sized garbage-collected pool of as many CELL_SZ-byte cells as will
 * fit inside the BYTES bytes pointed to by MEMORY.
 * For the various callbacks, see their typedefs.
 * Return NULL on error, such as if the provided memory in insufficient. */
oscar *oscar_new_fixed(unsigned int cell_sz, unsigned int bytes, char *memory,
                       oscar_mark_cb *mark_cb, void *mark_udata,
                       oscar_free_cb *free_cb, void *free_udata) {
    unsigned int rem = bytes - sizeof(oscar);
    if (memory == NULL) return NULL;
    /* There needs to be room for at _least_ 1 cell and 1 mark bit
     * (though a one-cell GC pool is pretty useless...). */
    if (rem < 2*cell_sz) return NULL;
    
    return new_pool(cell_sz, rem, (oscar *) memory, memory + sizeof(oscar),
        NULL /* no memory cb -> don't malloc/reallocate/free */, NULL,
        mark_cb, mark_udata, free_cb, free_udata);
}

/* Init a garbage-collected pool of START_COUNT cells, each CELL_SZ bytes.
 * For the various callbacks, see their typedefs.
 * Returns NULL on error. */
oscar *oscar_new(unsigned int cell_sz, unsigned int start_count,
                 oscar_memory_cb *mem_cb, void *mem_udata,
                 oscar_mark_cb *mark_cb, void *mark_udata,
                 oscar_free_cb *free_cb, void *free_udata) {
    oscar *p = NULL;
    char *raw = NULL;
    unsigned int sz = cell_sz * start_count + (cell_sz / 8) + 1;
    if (cell_sz < sizeof(pool_id)) return NULL;
    if (start_count < 1) return NULL;
    if (mark_cb == NULL) return NULL;
    if (mem_cb == NULL) return NULL;

    p = mem_cb(NULL, 0, sizeof(*p), mem_udata);
    if (p == NULL) goto cleanup;

    raw = mem_cb(NULL, 0, sz, mem_udata);
    if (raw == NULL) goto cleanup;

    return new_pool(cell_sz, sz, p, raw, mem_cb, mem_udata,
        mark_cb, mark_udata, free_cb, free_udata);

cleanup:
    if (p) mem_cb(p, sizeof(*p), 0, mem_udata);
    if (raw) mem_cb(raw, sz, 0, mem_udata);
    return NULL;
}

unsigned int oscar_count(oscar *pool) { return pool->count; }

unsigned int oscar_freed(oscar *pool) { return pool->freed; }

/* Mark the ID'th cell as reachable. */
void oscar_mark(oscar *pool, pool_id id) {
    unsigned int byte = id / 8;
    char bit = 1 << (id % 8);
    if (id >= pool->count) return;
    pool->markbits[byte] |= bit;
}

/* Get a pointer to a cell, by ID. Returns NULL on error. */
void *oscar_get(oscar *pool, pool_id id) {
    void *p = NULL;
    if (id < pool->count) p = pool->raw + (id * pool->cell_sz);
    return p;
}

static int is_marked(char *markbits, pool_id id) {
    unsigned int byte = markbits[id / 8];
    char bit = 1 << (id % 8);
    LOG("id %d -> byte %d, bit %d -> %d\n", id, byte, bit, byte & bit);
    return byte & bit;
}

static pool_id find_unmarked(oscar *pool, pool_id start) {
    pool_id id = 0;
    for (id = start; id < pool->count; id++) {
        LOG(" -- find_unmarked, %d / %d\n", id, pool->count);
        if (!is_marked(pool->markbits, id)) {
            char *p = pool->raw + (pool->cell_sz * id);
            if (pool->free_cb) pool->free_cb(pool, id, pool->free_udata);
            pool->freed++;
            LOG("-- sweeping & returning unmarked cell, %d\n", id);
            bzero(p, pool->cell_sz);
            pool->sweep = id + 1;
            return id;
        }
    }
    return POOL_ID_NONE;
}

/* Get a fresh pool ID. Can cause a mark/sweep pass, and may cause
 * the pool's backing cells to move in memory (making any pointers stale).
 * Returns -1 on error. */
pool_id oscar_alloc(oscar *pool) {
    unsigned int quarter = pool->count >> 4;
    pool_id id = find_unmarked(pool, pool->sweep);
    if (id != POOL_ID_NONE) return id;

    /* If < 25% of the cells were recovered since last sweep,
     * try to grow the pool (if possible).
     * Note: does not try to shrink, because the pool is not compacted. */
    if (pool->freed < quarter) {
        unsigned int new_sz = 2 * pool->sz;
        char *old_raw = pool->raw;
        char *new_raw = pool->mem_cb(old_raw, pool->sz,
            new_sz, pool->mem_udata);
        LOG(" -- trying to grow\n");
        if (new_raw == NULL) return POOL_ID_NONE;
        pool->sz = new_sz;
        pool->raw = new_raw;
    }

    pool->freed = 0;

    /* Clear marks and re-mark */
    LOG(" -- about to mark\n");
    bzero(pool->markbits, (pool->count / 8) + 1);
    if (pool->mark_cb(pool, pool->mark_udata) < 0) return POOL_ID_NONE;
    pool->sweep = 0;            /* start from beginning */

    return find_unmarked(pool, 0);
}

/* Force a full GC mark/sweep. If free_cb is defined, it will be called
 * on every swept cell. Returns <0 on error. */
int oscar_force_gc(oscar *pool) {
    LOG(" -- forcing GC\n");
    pool->freed = 0;
    bzero(pool->markbits, (pool->count / 8) + 1);
    if (pool->mark_cb(pool, pool->mark_udata) < 0) return -1;

    for (pool_id id = 0; id < pool->count; id++) {
        if (!is_marked(pool->markbits, id)) {
            char *p = pool->raw + (pool->cell_sz * id);
            if (pool->free_cb) pool->free_cb(pool, id, pool->free_udata);
            pool->freed++;
            LOG("-- sweeping unmarked cell, %d\n", id);
            bzero(p, pool->cell_sz);
        }
    }
    pool->sweep = 0;
    return 0;
}


void oscar_free(oscar *pool) {
    unsigned int id = 0;
    if (pool->free_cb) {
        for (id = 0; id < pool->count; id++) {
            pool->free_cb(pool, id, pool->free_udata);
        }
    }

    if (pool->mem_cb) {  /* Don't free if using a fixed-size allocator. */
        pool->mem_cb(pool->raw, pool->sz, 0, pool->mem_udata);
        pool->mem_cb(pool, sizeof(*pool), 0, pool->mem_udata);
    }
}

void oscar_dump_mark_bits(oscar *pool) {
    unsigned int id = 0;
    for (id = 0; id < pool->count; id++){
        printf("%d: %d\n", id, is_marked(pool->markbits, id) ? 1 : 0);
    }
}
