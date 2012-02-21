/*
 * Copyright (c) 2011 Scott Vokes <vokes.s@gmail.com>
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

#ifndef GREATEST_H
#define GREATEST_H

/* A unit testing system for C, contained in 1 file.
 * It doesn't use dynamic allocation or depend on anything
 * beyond ANSI C89.
 *
 * If C99 is available, then suites can pass arguments to tests for
 * parametric testing. */

#include <stdlib.h>
#include <string.h>
#include <time.h>


/***********
 * Options *
 ***********/

/* Default column width for non-verbose output. */
#ifndef GREATEST_DEFAULT_WIDTH
#define GREATEST_DEFAULT_WIDTH 72
#endif

/* FILE *, for test logging. */
#ifndef GREATEST_STDOUT
#define GREATEST_STDOUT stdout
#endif

/* Remove GREATEST_ prefix from most commonly used symbols? */
#ifndef GREATEST_USE_ABBREVS
#define GREATEST_USE_ABBREVS 1
#endif


/*********
 * Types *
 *********/

/* Info for the current running suite. */
typedef struct greatest_suite_info {
    unsigned int tests_run;
    unsigned int passed;
    unsigned int failed;
    unsigned int skipped;

    /* timers, pre/post running suite and individual tests */
    clock_t pre_suite;
    clock_t post_suite;
    clock_t pre_test;
    clock_t post_test;
} greatest_suite_info;

/* Type for a suite function. */
typedef void (greatest_suite_cb)(void);

typedef struct greatest_run_info {
    unsigned int verbose;       /* verbose flag */
    unsigned int list_only;     /* list suite/tests only flag */
    unsigned int tests_run;     /* total test count */

    /* Overall pass/fail/skip counts. */
    unsigned int passed;
    unsigned int failed;
    unsigned int skipped;

    /* currently running test suite */
    greatest_suite_info suite;

    /* info to print about the most recent failure */
    char *fail_file;
    unsigned int fail_line;
    char *fail_msg;

    /* formatting info for ".....s...F"-style output */
    unsigned int col;
    unsigned int width;

    /* only run a specific suite or test */
    char *suite_filter;
    char *test_filter;

    /* overall timers */
    clock_t begin;
    clock_t end;
} greatest_run_info;

/* Global var for the current testing context.
 * Initialized by GREATEST_MAIN_DEFS(). */
extern greatest_run_info greatest_info;


/**********************
 * Exported functions *
 **********************/

void greatest_do_pass(const char *name);
void greatest_do_fail(const char *name);
void greatest_do_skip(const char *name);
int greatest_pre_test(const char *name);
void greatest_post_test(const char *name, int res);
void greatest_usage(const char *name);


/**********
 * Macros *
 **********/

/* Define a suite. */
#define GREATEST_SUITE(name) void name()

/* Start defining a test function.
 * The arguments are not included, to allow parametric testing. */
#define GREATEST_TEST static int

/* Run a suite. */
#define GREATEST_RUN_SUITE(s_name) greatest_run_suite(s_name, #s_name)

/* Run a test in the current suite.
 * If __VA_ARGS__ (C99) is supported, allow parametric testing. */
#if __STDC_VERSION__ >= 19901L
#define GREATEST_RUN_TESTp(test, ...)                                   \
    do {                                                                \
        if (greatest_pre_test(#test) == 1) {                            \
            int res = test(__VA_ARGS__);                                \
            greatest_post_test(#test, res);                             \
        } else if (greatest_info.list_only) {                           \
            fprintf(GREATEST_STDOUT, "  %s\n", #test);                  \
        }                                                               \
    } while (0)
#endif

#define GREATEST_RUN_TEST(test)                                         \
    do {                                                                \
        if (greatest_pre_test(#test) == 1) {                            \
            int res = test();                                           \
            greatest_post_test(#test, res);                             \
        } else if (greatest_info.list_only) {                           \
            fprintf(GREATEST_STDOUT, "  %s\n", #test);                  \
        }                                                               \
    } while (0)

/* Check if the test runner is in verbose mode. */
#define GREATEST_IS_VERBOSE() (greatest_info.verbose)

/* Message-less forms. */
#define GREATEST_PASS() return 0
#define GREATEST_FAIL() GREATEST_FAILm(NULL)
#define GREATEST_SKIP() GREATEST_SKIPm(NULL)
#define GREATEST_ASSERT(cond) GREATEST_ASSERTm(#cond, cond)
#define GREATEST_ASSERT_FALSE(cond) GREATEST_ASSERT_FALSEm(#cond, cond)
#define GREATEST_ASSERT_EQ(exp, got) GREATEST_ASSERT_EQm("!= " #exp, exp, got)
#define GREATEST_ASSERT_STR_EQ(exp, got) GREATEST_ASSERT_STR_EQm("!= " #exp, exp, got)

/* The following forms take an additional message argument first,
 * to be displayed by the test runner. */

/* Fail if a condition is not true, with message. */
#define GREATEST_ASSERTm(msg, cond)                                     \
    do {                                                                \
        greatest_info.fail_msg = msg;                                   \
        greatest_info.fail_file = __FILE__;                             \
        greatest_info.fail_line = __LINE__;                             \
        if (!(cond)) return -1;                                         \
        greatest_info.fail_msg = NULL;                                  \
    } while (0)

#define GREATEST_ASSERT_FALSEm(msg, cond)                               \
    do {                                                                \
        greatest_info.fail_msg = msg;                                   \
        greatest_info.fail_file = __FILE__;                             \
        greatest_info.fail_line = __LINE__;                             \
        if ((cond)) return -1;                                          \
        greatest_info.fail_msg = NULL;                                  \
    } while (0)

#define GREATEST_ASSERT_EQm(msg, exp, got)                              \
    do {                                                                \
        greatest_info.fail_msg = msg;                                   \
        greatest_info.fail_file = __FILE__;                             \
        greatest_info.fail_line = __LINE__;                             \
        if ((exp) != (got)) return -1;                                  \
        greatest_info.fail_msg = NULL;                                  \
    } while (0)

#define GREATEST_ASSERT_STR_EQm(msg, exp, got)                          \
    do {                                                                \
        char *exp_s = (exp);                                            \
        char *got_s = (got);                                            \
        greatest_info.fail_msg = msg;                                   \
        greatest_info.fail_file = __FILE__;                             \
        greatest_info.fail_line = __LINE__;                             \
        if (0 != strcmp(exp_s, got_s)) {                                \
            fprintf(GREATEST_STDOUT,                                    \
                "Expected:\n####\n%s\n####\n", exp_s);                  \
            fprintf(GREATEST_STDOUT,                                    \
                "Got:\n####\n%s\n####\n", got_s);                       \
            return -1;                                                  \
        }                                                               \
        greatest_info.fail_msg = NULL;                                  \
    } while (0)

#define GREATEST_FAILm(msg)                                             \
    do {                                                                \
        greatest_info.fail_file = __FILE__;                             \
        greatest_info.fail_line = __LINE__;                             \
        greatest_info.fail_msg = msg;                                   \
        return -1;                                                      \
    } while (0)

#define GREATEST_SKIPm(msg)                                             \
    do {                                                                \
        greatest_info.fail_msg = msg;                                   \
        return 1;                                                       \
    } while (0)

#define GREATEST_SET_TIME(name)                                         \
    name = clock();                                                     \
    if (name == (int) -1) {                                             \
        fprintf(GREATEST_STDOUT,                                        \
            "clock error: %s\n", #name);                                \
        exit(EXIT_FAILURE);                                             \
    }

#define GREATEST_CLOCK_DIFF(c1, c2)                                     \
    fprintf(GREATEST_STDOUT, " (%lu ticks, %.3f sec)",                  \
        (long unsigned int) (c2) - (c1),                                \
        ((c2) - (c1))/ (1.0 * CLOCKS_PER_SEC))                          \

/* Include several function definitions in the main test file. */
#define GREATEST_MAIN_DEFS()                                            \
    greatest_run_info greatest_info = {0, 0, 0,                         \
                                       0, 0, 0,                         \
                                       {0, 0, 0, 0,                     \
                                        0, 0, 0, 0},                    \
                                       NULL, 0, NULL,                   \
                                       0, GREATEST_DEFAULT_WIDTH,       \
                                       NULL, NULL,                      \
                                       0, 0};                           \
                                                                        \
int greatest_pre_test(const char *name) {                               \
    if (!greatest_info.list_only                                        \
        && (greatest_info.test_filter == NULL ||                        \
            0 == strcmp(name, greatest_info.test_filter))) {            \
        GREATEST_SET_TIME(greatest_info.suite.pre_test);                \
        return 1;               /* test should be run */                \
    } else {                                                            \
        return 0;               /* skipped */                           \
    }                                                                   \
}                                                                       \
                                                                        \
void greatest_post_test(const char *name, int res) {                    \
    GREATEST_SET_TIME(greatest_info.suite.post_test);                   \
    if (res < 0) {                                                      \
        greatest_do_fail(name);                                         \
    } else if (res > 0) {                                               \
        greatest_do_skip(name);                                         \
    } else if (res == 0) {                                              \
        greatest_do_pass(name);                                         \
    }                                                                   \
    greatest_info.suite.tests_run++;                                    \
    greatest_info.col++;                                                \
    if (greatest_info.verbose) {                                        \
        GREATEST_CLOCK_DIFF(greatest_info.suite.pre_test,               \
            greatest_info.suite.post_test);                             \
        fprintf(GREATEST_STDOUT, "\n");                                 \
    } else if (greatest_info.col % greatest_info.width == 0) {          \
        fprintf(GREATEST_STDOUT, "\n");                                 \
        greatest_info.col = 0;                                          \
    }                                                                   \
}                                                                       \
                                                                        \
static void greatest_run_suite(greatest_suite_cb *suite_cb,             \
                               char *suite_name) {                      \
    if (greatest_info.suite_filter &&                                   \
        0 != strcmp(suite_name, greatest_info.suite_filter))            \
        return;                                                         \
    greatest_info.suite.tests_run = 0;                                  \
    greatest_info.suite.failed = 0;                                     \
    greatest_info.suite.passed = 0;                                     \
    greatest_info.suite.skipped = 0;                                    \
    greatest_info.suite.pre_suite = 0;                                  \
    greatest_info.suite.post_suite = 0;                                 \
    greatest_info.suite.pre_test = 0;                                   \
    greatest_info.suite.post_test = 0;                                  \
    greatest_info.col = 0;                                              \
    fprintf(GREATEST_STDOUT, "\n* Suite %s:\n", suite_name);            \
    GREATEST_SET_TIME(greatest_info.suite.pre_suite);                   \
    suite_cb();                                                         \
    GREATEST_SET_TIME(greatest_info.suite.post_suite);                  \
    if (greatest_info.suite.tests_run > 0) {                            \
        fprintf(GREATEST_STDOUT,                                        \
            "\n%u tests - %u pass, %u fail, %u skipped",                \
            greatest_info.suite.tests_run,                              \
            greatest_info.suite.passed,                                 \
            greatest_info.suite.failed,                                 \
            greatest_info.suite.skipped);                               \
        GREATEST_CLOCK_DIFF(greatest_info.suite.pre_suite,              \
            greatest_info.suite.post_suite);                            \
        fprintf(GREATEST_STDOUT, "\n");                                 \
    }                                                                   \
    greatest_info.passed += greatest_info.suite.passed;                 \
    greatest_info.failed += greatest_info.suite.failed;                 \
    greatest_info.skipped += greatest_info.suite.skipped;               \
    greatest_info.tests_run += greatest_info.suite.tests_run;           \
}                                                                       \
                                                                        \
void greatest_do_pass(const char *name) {                               \
    if (greatest_info.verbose) {                                        \
        fprintf(GREATEST_STDOUT, "PASS %s:", name );                    \
    } else {                                                            \
        fprintf(GREATEST_STDOUT, ".");                                  \
    }                                                                   \
    greatest_info.suite.passed++;                                       \
}                                                                       \
                                                                        \
void greatest_do_fail(const char *name) {                               \
    if (greatest_info.verbose) {                                        \
        fprintf(GREATEST_STDOUT,                                        \
            "FAIL %s: \"%s\" (%s:%u)",                                  \
            name, greatest_info.fail_msg ? greatest_info.fail_msg : "", \
            greatest_info.fail_file, greatest_info.fail_line);          \
    } else {                                                            \
        fprintf(GREATEST_STDOUT, "F");                                  \
        /* add linebreak if in line of '.'s */                          \
        if (greatest_info.col % greatest_info.width != 0)               \
            fprintf(GREATEST_STDOUT, "\n");                             \
        greatest_info.col = 0;                                          \
        fprintf(GREATEST_STDOUT, "FAIL %s: \"%s\" (%s:%u)\n",           \
            name,                                                       \
            greatest_info.fail_msg ? greatest_info.fail_msg : "",       \
            greatest_info.fail_file, greatest_info.fail_line);          \
    }                                                                   \
    greatest_info.suite.failed++;                                       \
}                                                                       \
                                                                        \
void greatest_do_skip(const char *name) {                               \
    if (greatest_info.verbose) {                                        \
        fprintf(GREATEST_STDOUT, "SKIP %s: \"%s\"",                     \
            name,                                                       \
            greatest_info.fail_msg ?                                    \
            greatest_info.fail_msg : "skipped" );                       \
    } else {                                                            \
        fprintf(GREATEST_STDOUT, "s");                                  \
    }                                                                   \
    greatest_info.suite.skipped++;                                      \
}                                                                       \
                                                                        \
void greatest_usage(const char *name) {                                 \
    fprintf(GREATEST_STDOUT,                                            \
        "Usage: %s [-hlv] [-s SUITE] [-t TEST]\n"                       \
        "  -h        print this Help\n"                                 \
        "  -l        List suites and their tests, then exit\n"          \
        "  -v        Verbose output\n"                                  \
        "  -s SUITE  only run suite named SUITE\n"                      \
        "  -t TEST   only run test named TEST\n",                       \
        name);                                                          \
}                                                                       \
/* (hack to eat the semicolon following GREATEST_MAIN_DEFS) */          \
struct GREATEST_TRAILING_COMMA_EATER

/* Handle command-line arguments, etc. */
#define GREATEST_MAIN_BEGIN()                                           \
    do {                                                                \
        int i = 0;                                                      \
        for (i = 1; i < argc; i++) {                                    \
            if (0 == strcmp("-t", argv[i])) {                           \
                if (argc <= i + 1) {                                    \
                    greatest_usage(argv[0]);                            \
                    exit(EXIT_FAILURE);                                 \
                }                                                       \
                greatest_info.test_filter = argv[i+1];                  \
                i++;                                                    \
            } else if (0 == strcmp("-s", argv[i])) {                    \
                if (argc <= i + 1) {                                    \
                    greatest_usage(argv[0]);                            \
                    exit(EXIT_FAILURE);                                 \
                }                                                       \
                greatest_info.suite_filter = argv[i+1];                 \
                i++;                                                    \
            } else if (0 == strcmp("-v", argv[i])) {                    \
                greatest_info.verbose = 1;                              \
            } else if (0 == strcmp("-l", argv[i])) {                    \
                greatest_info.list_only = 1;                            \
            } else if (0 == strcmp("-h", argv[i])) {                    \
                greatest_usage(argv[0]);                                \
                exit(EXIT_SUCCESS);                                     \
            } else {                                                    \
                fprintf(GREATEST_STDOUT,                                \
                    "Unknown argument '%s'\n", argv[i]);                \
                greatest_usage(argv[0]);                                \
                exit(EXIT_FAILURE);                                     \
            }                                                           \
        }                                                               \
    } while (0);                                                        \
    GREATEST_SET_TIME(greatest_info.begin)

#define GREATEST_MAIN_END()                                             \
    do {                                                                \
        if (!greatest_info.list_only) {                                 \
            GREATEST_SET_TIME(greatest_info.end);                       \
            fprintf(GREATEST_STDOUT,                                    \
                "\nTotal: %u tests", greatest_info.tests_run);          \
            GREATEST_CLOCK_DIFF(greatest_info.begin,                    \
                greatest_info.end);                                     \
            fprintf(GREATEST_STDOUT, "\n");                             \
            fprintf(GREATEST_STDOUT,                                    \
                "Pass: %u, fail: %u, skip: %u.\n",                      \
                greatest_info.passed,                                   \
                greatest_info.failed, greatest_info.skipped);           \
        }                                                               \
        return (greatest_info.failed > 0                                \
            ? EXIT_FAILURE : EXIT_SUCCESS);                             \
    } while (0)

/* Make abbreviations without the GREATEST_ prefix for the
 * most commonly used symbols. */
#if GREATEST_USE_ABBREVS
#define TEST GREATEST_TEST
#define SUITE GREATEST_SUITE
#define RUN_TEST GREATEST_RUN_TEST
#define RUN_SUITE GREATEST_RUN_SUITE
#define ASSERT GREATEST_ASSERT
#define ASSERTm GREATEST_ASSERTm
#define ASSERT_FALSE GREATEST_ASSERT_FALSE
#define ASSERT_EQ GREATEST_ASSERT_EQ
#define ASSERT_STR_EQ GREATEST_ASSERT_STR_EQ
#define ASSERT_FALSEm GREATEST_ASSERT_FALSEm
#define ASSERT_EQm GREATEST_ASSERT_EQm
#define ASSERT_STR_EQm GREATEST_ASSERT_STR_EQm
#define PASS GREATEST_PASS
#define FAIL GREATEST_FAIL
#define SKIP GREATEST_SKIP
#define FAILm GREATEST_FAILm
#define SKIPm GREATEST_SKIPm

#if __STDC_VERSION__ >= 19901L
#endif /* C99 */
#define RUN_TESTp GREATEST_RUN_TESTp
#endif /* USE_ABBREVS */

#endif
