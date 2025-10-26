# About

Simple incremental garbage collector based Baker's algorithm with semi-dfs copying strategy.

## Build
1. At first, you should compile Stella program, for example, you can use the following command:
`sudo docker run -i fizruk/stella compile <test_cases/factorial.st> factorial.c`,
or you can use [online compile](https://fizruk.github.io/stella/playground/).
1. Copy content of translated Stella program to C source file.
1. Run the following command:
   `gcc -std=c11 -DSTELLA_GC_STATS -DSTELLA_RUNTIME_STATS factorial.c stella/list.c stella/runtime.c stella/gc.c -o factorial`.
   Supported preprocessor options (flag -D) are listed below.

### Preprocessor options
|           Name           |                           Description                         |
|--------------------------|---------------------------------------------------------------|
|      MAX_ALLOC_SIZE      | Maximum heap size for objects allocations.                    |
|     GC_NO_INCREMENT      | Disables GC increment mode.                                   |
|       STELLA_DEBUG       | Enables some Stella debug messages.                           |
|     STELLA_GC_DEBUG      | Enables some GC debug messages.                               |
|     STELLA_GC_STATS      | Enables output GC statistic at end of the program.            |
| GC_STATS_OBJECTS_TO_DUMP | Defines maximum number of object to dump when output GC state |
|   STELLA_RUNTIME_STATS   | Enables output runtime statistic at end of the program.       |

<details>
  <summary>Example of GC statistics output:</summary>

  ```
  Garbage collector (GC) statistics:
  Total memory allocation:  415 417 328 bytes (25 963 563 objects)
  Total GC cycles count:    0
  Maximum residency:        415 417 328 bytes (25 963 563 objects)
  Total memory use:         64 397 665 reads and 0 writes
  Read barrier activation:  0 activation(s)
  Write barrier activation: 0 activation(s)
  ```
</details>

<details>
  <summary>Example of GC state output:</summary>

  ```
  Garbage collector (GC) variables:
from-space: 0x7fb909000000, to-space: 0x7fb8f8700000
scan: 0x7fb8f8700000, next: 0x7fb904d16260

Heap state:
  stella_object at <0x7fb8f8700000> {
    tag: TAG_SUCC
    field #0: <0x10b4401b8>
  }
  stella_object at <0x7fb8f8700010> {
    tag: TAG_SUCC
    field #0: <0x7fb8f8700000>
  }
  stella_object at <0x7fb8f8700020> {
    tag: TAG_SUCC
    field #0: <0x7fb8f8700010>
  }
  stella_object at <0x7fb8f8700030> {
    tag: TAG_SUCC
    field #0: <0x7fb8f8700020>
  }
  stella_object at <0x7fb8f8700040> {
    tag: TAG_SUCC
    field #0: <0x7fb8f8700030>
  }
  stella_object at <0x7fb8f8700050> {
    tag: TAG_SUCC
    field #0: <0x7fb8f8700040>
  }
  stella_object at <0x7fb8f8700060> {
    tag: TAG_SUCC
    field #0: <0x7fb8f8700050>
  }
  stella_object at <0x7fb8f8700070> {
    tag: TAG_SUCC
    field #0: <0x7fb8f8700060>
  }
  stella_object at <0x7fb8f8700080> {
    tag: TAG_SUCC
    field #0: <0x7fb8f8700070>
  }
  stella_object at <0x7fb8f8700090> {
    tag: TAG_SUCC
    field #0: <0x7fb8f8700080>
  }
  stella_object at <0x7fb8f87000a0> {
    tag: TAG_SUCC
    field #0: <0x10b4401b8>
  }
  stella_object at <0x7fb8f87000b0> {
    tag: TAG_FN
    field #0: <0x10b43e7d0>
  }
  stella_object at <0x7fb8f87000c0> {
    tag: TAG_FN
    field #0: <0x10b43e6a0>
    field #1: <0x7fb8f8700080>
  }
  stella_object at <0x7fb8f87000d8> {
    tag: TAG_FN
    field #0: <0x10b43e540>
    field #1: <0x7fb8f87000a0>
  }
  stella_object at <0x7fb8f87000f0> {
    tag: TAG_SUCC
    field #0: <0x7fb8f8700080>
  }
  stella_object at <0x7fb8f8700100> {
    tag: TAG_FN
    field #0: <0x10b43e450>
    field #1: <0x7fb8f87000f0>
  }
  stella_object at <0x7fb8f8700118> {
    tag: TAG_FN
    field #0: <0x10b43e2e0>
    field #1: <0x7fb8f8700100>
  }

Roots:
Root #1:
  <0x10b4400e0>
Root #2:
  stella_object at <0x7fb8f8700090> {
    tag: TAG_SUCC
    field #0: <0x7fb8f8700080>
  }
Root #3:
  stella_object at <0x7fb8f8700090> {
    tag: TAG_SUCC
    field #0: <0x7fb8f8700080>
  }
Root #4:
  stella_object at <0x7fb8f8700090> {
    tag: TAG_SUCC
    field #0: <0x7fb8f8700080>
  }
Root #5:
  stella_object at <0x7fb8f87000a0> {
    tag: TAG_SUCC
    field #0: <0x10b4401b8>
  }
Root #6:
  stella_object at <0x7fb8f87000b0> {
    tag: TAG_FN
    field #0: <0x10b43e7d0>
  }
Root #7:
  stella_object at <0x7fb8f87000b0> {
    tag: TAG_FN
    field #0: <0x10b43e7d0>
  }
Root #8:
  stella_object at <0x7fb8f8700090> {
    tag: TAG_SUCC
    field #0: <0x7fb8f8700080>
  }
Root #9:
  stella_object at <0x7fb8f8700000> {
    tag: TAG_SUCC
    field #0: <0x10b4401b8>
  }
Root #10:
  stella_object at <0x7fb8fe2a1b00> {
    tag: TAG_SUCC
    field #0: <0x7fb8fe2a1ae0>
  }
Root #11:
  stella_object at <0x7fb8f87000b0> {
    tag: TAG_FN
    field #0: <0x10b43e7d0>
  }
Root #12:
  stella_object at <0x7fb8fe2a1b28> {
    tag: TAG_FN
    field #0: <0x10b43e540>
    field #1: <0x7fb8fe2a1b00>
  }
Root #13:
  stella_object at <0x7fb8fe2a1b40> {
    tag: TAG_SUCC
    field #0: <0x7fb8f8700000>
  }
Root #14:
  stella_object at <0x7fb8f8700000> {
    tag: TAG_SUCC
    field #0: <0x10b4401b8>
  }
Root #15:
  stella_object at <0x7fb8fe2a1b40> {
    tag: TAG_SUCC
    field #0: <0x7fb8f8700000>
  }
Root #16:
  stella_object at <0x7fb8fe2a1b00> {
    tag: TAG_SUCC
    field #0: <0x7fb8fe2a1ae0>
  }
Allocated memory: 207 708 768 bytes
Free memory: 12 bytes
  ```
</details>
