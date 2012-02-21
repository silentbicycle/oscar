/* 
 * Copyright (c) 2012 Scott Vokes <vokes.s@gmail.com>
 *  
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef OSCAR_H
#define OSCAR_H

/* Unsigned int for pool IDs, can be defined at compile-time
 * to use <4 bytes. */
#ifdef OSCAR_POOL_ID_TYPE
typedef OSCAR_POOL_ID_TYPE pool_id;
#else
typedef uint32_t pool_id;
#endif

/* Opaque struct for the GC internals. */
typedef struct oscar oscar;

/* Special sentinel value for "no ID". */
#define OSCAR_ID_NONE ((pool_id) -1)

/* Function to mark the current root set, using oscar_mark on each
 * reachable pool ID. The UDATA is passed along from when the pool
 * was defined. Should return <0 on error. >=0 results are ignored. */
typedef int (oscar_mark_cb)(oscar *pool, void *udata);

/* If non-NULL, this will be called whenever an unreachable cell is about to
 * be swept. If the cell has not previously been allocated into, then the
 * cell will contain (CELL_SZ) 0 bytes. */
typedef void (oscar_free_cb)(oscar *pool, pool_id id, void *udata);

/* Callback to malloc / realloc / free memory.
 * p == NULL, new_sz == N           -> behave as malloc(N)
 * p == x, new_sz == 0              -> behave as free(x)
 * p == x, old_sz != 0, new_sz != 0 -> behave as realloc(x, new_sz)
 * Note: the realloc-like behavior is expected to copy old_sz bytes over. */
typedef void *(oscar_memory_cb)(void *p, size_t old_sz,
                                size_t new_sz, void *udata);

/* An oscar_memory_cb that just calls malloc/free/realloc. */
void *oscar_generic_mem_cb(void *p, size_t old_sz, size_t new_sz, void *udata);

/* Init a fixed-sized garbage-collected pool of as many CELL_SZ-byte
 * cells as will fit inside the BYTES bytes pointed to by MEMORY.
 * For the various callbacks, see their typedefs.
 * Return NULL on error, such as if the provided memory in insufficient. */
oscar *oscar_new_fixed(unsigned int cell_sz, unsigned int bytes, char *memory,
                       oscar_mark_cb *mark_cb, void *mark_udata,
                       oscar_free_cb *free_cb, void *free_udata);

/* Init a resizable garbage-collected pool of START_COUNT cells,
 * each CELL_SZ bytes. For the various callbacks, see their typedefs. */
oscar *oscar_new(unsigned int cell_sz, unsigned int start_count,
    oscar_memory_cb *mem_cb, void *mem_udata,
    oscar_mark_cb *mark_cb, void *mark_udata,
    oscar_free_cb *free_cb, void *free_udata);

/* Get the current cell count. */
unsigned int oscar_count(oscar *pool);

/* Mark the ID'th cell as reachable. */
void oscar_mark(oscar *pool, pool_id id);

/* Get a pointer to a cell, by ID.
 * Note that the pointer may become stale if oscar_alloc causes the pool
 * to resize, or if the cell is swept. Returns NULL on error. */
void *oscar_get(oscar *pool, pool_id id);

/* Get a fresh pool ID. Can cause a mark/sweep pass, and may cause
 * the pool's backing cells to move in memory (making any pointers stale).
 * Returns -1 on error. */
pool_id oscar_alloc(oscar *pool);

/* Force a full GC mark/sweep. If free_cb is defined, it will be called
 * on every swept cell. Returns <0 on error. */
int oscar_force_gc(oscar *pool);

/* Free the pool and its contents. If the memory was dynamically allocated,
 * it will be freed; if a free_cb is defined, it will be called on every cell. */
void oscar_free(oscar *pool);

#endif
