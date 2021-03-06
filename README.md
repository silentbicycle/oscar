oscar, for when memory leaks are making you grouchy.

`oscar` provides a mark and sweep garbage collector for a pool of equally-sized chunks of memory. It uses lazy sweeping, roughly as described [here]. The pool can be grown an demand, or kept inside a fixed-size pool of statically allocated memory. Marking and freeing behavior are specified via callbacks.

[here]: http://people.csail.mit.edu/gregs/ll1-discuss-archive-html/msg00761.html

The GC should be reasonably efficient (though it is not generational). Its tests pass on OS X and OpenBSD, and it runs without Valgrind warnings.

The documentation is inside `oscar.h`. For usage examples, see the test suite (`test.c`).

For more information about garbage collectors, I recommend Paul R. Wilson's ["Uniprocessor Garbage Collection Techniques"](http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.47.2438), Jones, Hosking, and Moss's _The Garbage Collection Handbook_, and Jones & Lins' _Garbage Collection: Algorithms for Automatic Dynamic Memory Management_.
