/* For copyright notice, see oscar.h. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "oscar.h"

/* Note: this uses __VA_ARGS__ (from C99), but the rest only
 * depends on C89. LOG(...) could be safely removed. */
#define DEBUG 0
#define LOG(...) { if (DEBUG) fprintf(stderr, __VA_ARGS__); }

struct oscar {
    unsigned int cell_sz;       /* each cell is CELL_SZ bytes */
    unsigned int count;         /* number of cells */
    unsigned int marked;        /* how many were marked */
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

static oscar *new_pool(unsigned int cell_sz, unsigned int count,
                       oscar *p, unsigned int raw_sz, char *raw,
                       oscar_memory_cb *mem_cb, void *mem_udata,
                       oscar_mark_cb *mark_cb, void *mark_udata,
                       oscar_free_cb *free_cb, void *free_udata) {
    if (p == NULL || raw == NULL) return NULL;

    bzero(raw, raw_sz);
    p->cell_sz = cell_sz;
    p->sz = raw_sz;
    p->count = count;
    p->marked = 0;
    p->sweep = 0;
    p->mem_cb = mem_cb;
    p->mem_udata = mem_udata;
    p->mark_cb = mark_cb;
    p->mark_udata = mark_udata;
    p->free_cb = free_cb;
    p->free_udata = free_udata;
    p->raw = raw;
    p->markbits = raw + (cell_sz * count);

    if (1) {                    /* ensure regions don't overlap */
        int i = 0;
        char *p_end = (char *) p + sizeof(*p);
        char *raw_end = raw + (cell_sz * count);
        char *markbits_end = p->markbits + (count / 8) + 1;
        LOG("p: %p p_end: %p\n", (void *) p, p_end);
        LOG("raw: %p raw_end: %p\n", raw, raw_end);
        for (i=0; i<count; i++) {
            char *cell = (char *) oscar_get(p, i);
            LOG("cell[%d] = %p ~ %p\n", i, cell, cell + cell_sz - 1);
        }
        LOG("markbits: %p markbits_end: %p\n", p->markbits, markbits_end);
        assert(p_end <= raw || (char *) p > markbits_end);
        assert(raw_end <= p->markbits);
        
    }
    return p;
}

/* Init a fixed-sized garbage-collected pool of as many CELL_SZ-byte cells as will
 * fit inside the BYTES bytes pointed to by MEMORY.
 * For the various callbacks, see their typedefs.
 * Return NULL on error, such as if the provided memory in insufficient. */
oscar *oscar_new_fixed(unsigned int cell_sz, unsigned int bytes, char *memory,
                      oscar_mark_cb *mark_cb, void *mark_udata,
                       oscar_free_cb *free_cb, void *free_udata) {
    /* The internal memory is laid out like so:
     * ['oscar' data structure, sizeof(oscar) bytes, 88 or so]
     * [CELL_SZ * COUNT bytes][COUNT/8 bytes of mark bits, rounded up] */
    unsigned int rem = 0, count = 0;

#define FAIL(msg) { fprintf(stderr, msg "\n"); return NULL; }
    if (cell_sz < sizeof(pool_id)) FAIL("cell_sz is too small");
    if ((cell_sz % sizeof(void *)) != 0)
        FAIL("cell_sz must be a multiple of sizeof(void *) due to alignment");
    if (memory == NULL) FAIL("NULL memory pool");
    /* There needs to be room for at _least_ 1 cell and 1 mark bit
     * (though a one-cell GC pool is pretty useless...). */
    rem = bytes - sizeof(oscar);
    if (bytes < sizeof(oscar) || rem < 2*cell_sz)
        FAIL("memory pool is too small for GC");
    if (mark_cb == NULL) FAIL("NULL mark_cb");
#undef FAIL

    count = rem / cell_sz;
    /* Reduce count as necessary to fit mark bits at the end. */
    while (count * cell_sz + (count / 8 + 1) > rem) count--;

    return new_pool(cell_sz, count, (oscar *) memory,
        rem, memory + sizeof(oscar),
        NULL /* no memory cb -> don't malloc/reallocate/free */, NULL,
        mark_cb, mark_udata, free_cb, free_udata);
}

/* Init a garbage-collected pool of START_COUNT cells, each CELL_SZ bytes.
 * For the various callbacks, see their typedefs.
 * Returns NULL on error (allocation failure or NULL callbacks). */
oscar *oscar_new(unsigned int cell_sz, unsigned int start_count,
                 oscar_memory_cb *mem_cb, void *mem_udata,
                 oscar_mark_cb *mark_cb, void *mark_udata,
                 oscar_free_cb *free_cb, void *free_udata) {
    oscar *p = NULL;
    char *raw = NULL;
    unsigned int raw_sz = cell_sz * start_count + (cell_sz / 8) + 1;
#define FAIL(msg) { fprintf(stderr, msg "\n"); return NULL; }
    if (cell_sz < sizeof(pool_id)) FAIL("cell_sz is too small");
    if ((cell_sz % sizeof(void *)) != 0)
        FAIL("cell_sz must be a multiple of sizeof(void *) due to alignment");
    if (start_count < 1) FAIL("bad count");
    if (mark_cb == NULL) FAIL("NULL mark_cb");
    if (mem_cb == NULL) FAIL("NULL mem_cb");
#undef FAIL

    p = mem_cb(NULL, 0, sizeof(*p), mem_udata);
    if (p == NULL) goto cleanup;

    raw = mem_cb(NULL, 0, raw_sz, mem_udata);
    if (raw == NULL) goto cleanup;

    return new_pool(cell_sz, start_count, p,
        raw_sz, raw, mem_cb, mem_udata,
        mark_cb, mark_udata, free_cb, free_udata);

cleanup:
    if (p) mem_cb(p, sizeof(*p), 0, mem_udata);
    if (raw) mem_cb(raw, raw_sz, 0, mem_udata);
    return NULL;
}

unsigned int oscar_count(oscar *pool) { return pool->count; }

/* Mark the ID'th cell as reachable.
 * TODO If this were changed to return a new pool_id, would
 * that be sufficient to permit generational GC? The user's
 * mark_cb could update references while marking. */
void oscar_mark(oscar *pool, pool_id id) {
    unsigned int byte = id / 8;
    char bit = 1 << (id % 8);
    if (id >= pool->count || pool->markbits[byte] & bit) return;
    LOG(" -- marking ID %u\n", id);
    pool->markbits[byte] |= bit;
    pool->marked++;
}

/* Get a pointer to a cell, by ID. Returns NULL on error. */
void *oscar_get(oscar *pool, pool_id id) {
    void *p = NULL;
    if (id < pool->count) p = pool->raw + (id * pool->cell_sz);
    return p;
}

static int check_and_clear_mark(char *markbits, pool_id id) {
    unsigned int byte_id = id / 8;
    unsigned int byte = markbits[byte_id];
    char bit = 1 << (id % 8);
    markbits[byte_id] &= ~bit;
    LOG("id %d -> byte %d, bit %d -> %d\n", id, byte, bit, byte & bit);
    return byte & bit;
}

static pool_id find_unmarked(oscar *pool, pool_id start) {
    pool_id id = 0;
    for (id = start; id < pool->count; id++) {
        LOG(" -- find_unmarked, %d / %d\n", id, pool->count);

        /* TODO available mark bits could be checked and swept a byte
         * or more at a time, amortizing the cost of the bit ops. */
        if (!check_and_clear_mark(pool->markbits, id)) {
            char *p = pool->raw + (pool->cell_sz * id);
            if (pool->free_cb) pool->free_cb(pool, id, pool->free_udata);
            LOG("-- sweeping & returning unmarked cell, %d\n", id);
            bzero(p, pool->cell_sz);
            pool->sweep = id + 1;
            return id;
        }
    }
    return OSCAR_ID_NONE;
}

/* Grow the GC pool, zeroing the new memory and moving the old mark bits. */
static int grow_pool(oscar *p) {
    unsigned int cell_sz = p->cell_sz;
    unsigned int new_sz = 2 * p->sz;
    unsigned int old_ct = p->count;
    char *old_raw = p->raw;
    unsigned int old_markbits_offset = p->markbits - old_raw;
    unsigned int count = 0, old_mark_bytes = 0;

    /* If successful, realloc will copy the old mark bits, but
     * won't move them to the intended new p->markbits. */
    char *new_raw = p->mem_cb(old_raw, p->sz, new_sz, p->mem_udata);
    if (new_raw == NULL) return -1; /* alloc fail */
    
    count = new_sz / cell_sz;
    while (count * cell_sz + (count / 8 + 1) > new_sz) count--;
    old_mark_bytes = (old_ct /8) + 1;
    p->markbits = new_raw + (cell_sz * count);

    /* Copy and clear the old mark bits. (They're cleared because otherwise
     * they would show up in the middle of an otherwise un-allocated cell.) */
    memcpy(p->markbits, new_raw + old_markbits_offset, old_mark_bytes);
    bzero(new_raw + old_markbits_offset,
        p->markbits - (new_raw + old_markbits_offset));
    /* Also zero the added mark bits. */
    bzero(p->markbits + old_mark_bytes, (count/8 + 1) - old_mark_bytes);

    p->sz = new_sz;
    p->raw = new_raw;
    p->count = count;
    return 0;
}

/* Get a fresh pool ID. Can cause a blocking sweep pass, and may cause
 * the pool's backing cells to move in memory (making any pointers stale).
 * Returns OSCAR_ID_NONE (-1) on error. */
pool_id oscar_alloc(oscar *pool) {
    pool_id id = find_unmarked(pool, pool->sweep);
    unsigned int three_quarters = 0;
    if (id != OSCAR_ID_NONE) return id;

    LOG(" -- about to mark\n");
    pool->marked = 0;

    /* Since the mark_cb is a user-supplied callback, it could potentially
     * interleave the marking step with other work that doesn't disrupt
     * the pool's data. Dangerous, but worth noting. */
    if (pool->mark_cb(pool, pool->mark_udata) < 0) return OSCAR_ID_NONE;

    /* If >= 75% of the cells were marked, try to grow the pool (if possible)
     * to avoid garbage collection churn.
     * Note: does not attempt to shrink, because the pool is not compacted. */
    three_quarters = (pool->count < 4 ? 1 : pool->count - (pool->count >> 2));
    LOG(" -- marked: %u, 3/4: %u\n", pool->marked, three_quarters);
    if (pool->mem_cb && pool->marked >= three_quarters) {
        LOG(" -- trying to grow\n");
        if (grow_pool(pool) < 0) {
            LOG(" -- growth failed\n");
            return OSCAR_ID_NONE;
        }
    }

    pool->sweep = 0;            /* start from beginning */

    return find_unmarked(pool, 0);
}

/* Force a full GC mark/sweep. If free_cb is defined, it will be called
 * on every swept cell. Returns <0 on error. */
int oscar_force_gc(oscar *pool) {
    pool_id id = 0;
    LOG(" -- forcing GC\n");
    pool->marked = 0;
    bzero(pool->markbits, (pool->count / 8) + 1);
    if (pool->mark_cb(pool, pool->mark_udata) < 0) return -1;

    for (id = 0; id < pool->count; id++) {
        if (!check_and_clear_mark(pool->markbits, id)) {
            if (pool->free_cb) pool->free_cb(pool, id, pool->free_udata);
            LOG("-- sweeping unmarked cell, %d\n", id);
        }
    }
    pool->sweep = 0;
    bzero(pool->raw, pool->sz);
    return 0;
}

/* Free the pool and its contents. If the memory was dynamically allocated,
 * it will be freed; if a free_cb is defined, it will be called on every cell. */
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
