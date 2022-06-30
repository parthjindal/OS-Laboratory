## Virtual Memory Management System

This is a virtual memory management system with functionalities to(de)allocation of memory, creating, accessing and deleting library primitives and array primitives. An asynchronous garbage collector based on the Mark and Sweep algorithm has been implemented to reclaim unused memory with both periodic and manual invocation. All creation, access and deletion operations are implemented in O(1) time using a hybrid Linked List. The garbage collector also supports memory compaction(LISP2 Mark and Compact Scheme) and memory management statistics.

To generate statistics about the garbage collector's performance and latency:
- Record statistics by passing the GC_LOG flag while building the library. 
- Turn off the garbage collector by passing the gc flag while creating the VM as false. This will generate statistics in the "no_gc.csv" file.
- Turn on the garbage collector by passing the gc flag while creating the VM as true. This will generate statistics in the "gc.csv" file.
- Create the performance graph by running the "gc_plot.py" script.