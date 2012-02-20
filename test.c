#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "oscar.h"
#include "greatest.h"

typedef struct link {
    void *d;                    /* data */
    pool_id n;                  /* next */
} link;

static void mark(oscar *p, pool_id id) {
    link *l = (link *) oscar_get(p, id);
    if (l == NULL) return;
    oscar_mark(p, id);
    if (l->n != 0) mark(p, l->n);
}

static int mark_it_donny(oscar *p, void *udata) {
    int *zero_is_live = (int *) udata;
    if (*zero_is_live) mark(p, 0); /* Assume ID 0 is the root. */
    return 0;
}

static int basic_test_freed[] = {0,0,0,0,0};

static void basic_free_hook(oscar *pool, pool_id id, void *udata) {
    basic_test_freed[id] = 1;
}

TEST basic_dynamic() {
    int zero_is_live = 1;
    oscar *p = oscar_new(sizeof(link), 5, oscar_generic_mem_cb, NULL,
        mark_it_donny, &zero_is_live,
        basic_free_hook, NULL);
    pool_id id = oscar_alloc(p);
    ASSERT_EQ(0, id);
    link *l = (link *) oscar_get(p, id);
    link *l1 = NULL;
    ASSERT(l->d == NULL);
    ASSERT_EQ(0, l->n);         /* [0] */
    ASSERT(l);

    id = oscar_alloc(p);
    ASSERT_EQ(1, id);
    l->n = id;                  /* [0] -> [1] */
    l = (link *) oscar_get(p, id);
    ASSERT(l);
    l1 = l;

    id = oscar_alloc(p);
    ASSERT_EQ(2, id);
    l->n = id;                  /* [0] -> [1] -> [2] */
    l = (link *) oscar_get(p, id);
    ASSERT(l);

    /* Allocate a couple cells that aren't kept live, to force GC */
    for (int i=0; i<5; i++) (void) oscar_alloc(p);
    id = oscar_alloc(p);
    ASSERT_EQ(4, id);
    l1->n = id;                 /* [0] -> [1] -> [4], 2 is garbage */
    
    /* Allocate a couple cells that aren't kept live, to force GC */
    for (int i=0; i<5; i++) (void) oscar_alloc(p);
    ASSERT_EQ(1, basic_test_freed[2]);

    for (int i=0; i<5; i++) basic_test_freed[i] = 0;

    zero_is_live = 0;           /* [0] is no longer root, all are garbage */
    oscar_force_gc(p);

    for (int i=0; i<5; i++) {
        ASSERT_EQ(1, basic_test_freed[i]);
    }
    
    oscar_free(p);
    PASS();
}

#if 0
/* This test is not yet complete... */
TEST basic_static() {
    int zero_is_live = 1;
    static char raw_mem[1024];
    oscar *p = oscar_new_fixed(sizeof(link), 5, oscar_generic_mem_cb, NULL,
        mark_it_donny, &zero_is_live,
        basic_free_hook, NULL);
    pool_id id = oscar_alloc(p);
    ASSERT_EQ(0, id);
    link *l = (link *) oscar_get(p, id);
    link *l1 = NULL;
    ASSERT(l->d == NULL);
    ASSERT_EQ(0, l->n);         /* [0] */
    ASSERT(l);

    id = oscar_alloc(p);
    ASSERT_EQ(1, id);
    l->n = id;                  /* [0] -> [1] */
    l = (link *) oscar_get(p, id);
    ASSERT(l);
    l1 = l;

    id = oscar_alloc(p);
    ASSERT_EQ(2, id);
    l->n = id;                  /* [0] -> [1] -> [2] */
    l = (link *) oscar_get(p, id);
    ASSERT(l);

    /* Allocate a couple cells that aren't kept live, to force GC */
    for (int i=0; i<5; i++) (void) oscar_alloc(p);
    id = oscar_alloc(p);
    ASSERT_EQ(4, id);
    l1->n = id;                 /* [0] -> [1] -> [4], 2 is garbage */
    
    /* Allocate a couple cells that aren't kept live, to force GC */
    for (int i=0; i<5; i++) (void) oscar_alloc(p);
    ASSERT_EQ(1, basic_test_freed[2]);

    for (int i=0; i<5; i++) basic_test_freed[i] = 0;

    zero_is_live = 0;           /* [0] is no longer root, all are garbage */
    oscar_force_gc(p);

    for (int i=0; i<5; i++) {
        ASSERT_EQ(1, basic_test_freed[i]);
    }
    
    oscar_free(p);
    PASS();
}
#endif

SUITE(suite) {
    RUN_TEST(basic_dynamic);
#if 0
    RUN_TEST(basic_static);
#endif
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();      /* command-line arguments, initialization. */
    RUN_SUITE(suite);
    GREATEST_MAIN_END();        /* display results */
}
