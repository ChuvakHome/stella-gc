#include <errno.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gc.h"
#include "runtime.h"

#include "list.h"

#ifndef GC_STATS_OBJECTS_TO_DUMP
  #define GC_STATS_OBJECTS_TO_DUMP 16
#endif

#ifndef MAX_ALLOC_SIZE
  #define MAX_ALLOC_SIZE 4096ULL
#endif

#ifdef STELLA_GC_DEBUG
  #define GC_DEBUG(msg, ...) fprintf(stderr, msg "\n",##__VA_ARGS__);
#else
  #define GC_DEBUG(msg, ...)
#endif

#define STELLA_OBJECT_SIZE(obj) (sizeof(stella_object) + STELLA_OBJECT_HEADER_FIELD_COUNT(obj->object_header) * sizeof(void *))

#define FOR_EACH_ROOT(root_node_ptr) LINKED_LIST_FOR_EACH(&roots, root_node_ptr)

#define FOR_EACH_OBJECT(start, end, obj) for (stella_object *obj = start; (char *) obj < (char *) (end); obj = (stella_object *) ((char *) obj + STELLA_OBJECT_SIZE(obj)))

/** Names of tags. */
static const char *tags_strings[] = {
  [TAG_ZERO] = "TAG_ZERO",
  [TAG_SUCC] = "TAG_SUCC",
  [TAG_FALSE] = "TAG_FALSE",
  [TAG_TRUE] = "TAG_TRUE",
  [TAG_FN] = "TAG_FN",
  [TAG_REF] = "TAG_REF",
  [TAG_UNIT] = "TAG_UNIT",
  [TAG_TUPLE] = "TAG_TUPLE",
  [TAG_INL] = "TAG_INL",
  [TAG_INR] = "TAG_INR",
  [TAG_EMPTY] = "TAG_EMPTY",
  [TAG_CONS] = "TAG_CONS"
};

/** Flag indicating the collector is initialized, i.e. all variables are properly
 * set, from-space and to-space regions are allocated.
 */
static bool initialized = false;

static linked_list roots;

/** Variable used for collecting statistics. */
static unsigned long long write_barrier_activations_count = 0ULL;
static unsigned long long write_operations_count = 0ULL;

static unsigned long long read_barrier_activations_count = 0ULL;
static unsigned long long read_operations_count = 0ULL;

static unsigned long long total_objects_count = 0ULL;
static unsigned long long total_memory_size = 0ULL;

static unsigned long long total_gc_cycles_count = 0ULL;

static unsigned long long resident_objects_count = 0ULL;
static unsigned long long resident_memory_size = 0ULL;

static unsigned long long max_resident_objects_count = 0ULL;
static unsigned long long max_resident_memory_size = 0ULL;

/** Variables used for operations with current heap. */
static size_t heap_size = MAX_ALLOC_SIZE * 2;
static void *from_space = NULL, *to_space = NULL;
static void *scanptr, *nextptr, *limitptr;

/** Checks if pointer belongs to the region with given beginning. The region can
 * be from-space or to-space. It's important to check upper bound as less-than,
 * stella object cannot be located at end of region.
 */
static bool belongs_to(const void *ptr, const void *region_begin) {
  return ptr >= region_begin && ptr < (region_begin + heap_size / 2);
}

/** Checks if pointer points to object that is not in from-space. For pointers to
 * objects that are not in from-space the function returns true.
 */
static bool is_object_forwarded(const void *p) {
  if (!belongs_to(p, from_space)) { return true; }

  const stella_object *stobj = p;

  return belongs_to(stobj->object_fields[0], to_space);
}

static void raise_no_memory_error(void) {
  print_gc_state();

  errno = ENOMEM;
  perror("Out of memory error");
  exit(ENOMEM);
}

static void* alloc_region(size_t sz) {
  if (sz > MAX_ALLOC_SIZE) { raise_no_memory_error(); }

  return malloc(sz);
}

/** Initializes list of roots, from-space, to-space and pointers: scan, next,
 * limit.
 */
static void initialize(void) {
  if (!initialized) {
    roots = linked_list_create();

    const size_t area_size = heap_size / 2;
    from_space = alloc_region(area_size);
    to_space = alloc_region(area_size);

    scanptr = nextptr = to_space;

    limitptr = ((char *) to_space) + area_size;

    initialized = true;
  }
}

static void update_resident_object_stats(size_t objects_count, size_t size_in_bytes) {
  resident_objects_count += objects_count;
  resident_memory_size += size_in_bytes;

  if (resident_memory_size > max_resident_memory_size) {
    max_resident_objects_count = resident_objects_count;
    max_resident_memory_size = resident_memory_size;
  }
}

static void reset_resident_object_stats(void) {
  resident_objects_count = 0;
  resident_memory_size = 0;
}

static void update_gc_stats(size_t objects_count, size_t size_in_bytes) {
  total_objects_count += objects_count;
  total_memory_size += size_in_bytes;

  update_resident_object_stats(objects_count, size_in_bytes);
}

/** Semi-DFS for copying objects in from-space to to-space region. This implementation
 * tries to copy as many objects as it can.
 */
static void chase(stella_object *obj) {
  while (obj != NULL) {
    void *r = NULL;

    const size_t object_size = STELLA_OBJECT_SIZE(obj);

    void *new_next = (char *) nextptr + object_size;
    if (new_next > limitptr) { raise_no_memory_error(); }

    stella_object *q = nextptr;
    nextptr = new_next;

    /** Copying of the object in from-space to to-space region with old pointers. */
    q->object_header = obj->object_header;

    const int fields_count = STELLA_OBJECT_HEADER_FIELD_COUNT(obj->object_header);
    void **fields = obj->object_fields;

    /** Copy all fields to the destination object. */
    for (int i = 0; i < fields_count; ++i) {
      void *p = fields[i];

      if (!is_object_forwarded(p)) { r = p; }

      q->object_fields[i] = p;
    }

    update_resident_object_stats(1, object_size);

    /** Write pointer to the new object to first field of old object. */
    obj->object_fields[0] = q;
    obj = r;
  }
}

/** Forwards the given pointer. If the pointer belongs to from-space, the object
 * will be copied to to-space, if needed, and then pointer to the copy will be
 * returned. If the pointer does not belongs to from-space, nothing will be done,
 * the same pointer will be returned.
 */
static void* forward(void *p) {
  if (belongs_to(p, from_space)) {
    stella_object *obj = p;

    if (!is_object_forwarded(obj)) { chase(obj); }

    return obj->object_fields[0];
  } else {
    return p;
  }
}

/** Starts copying GC procedure. At first (if GC_NO_INCREMENT disabled) it checks has
 * previous GC finished, and if has not, abort the program (it's definitely not
 * enough memory). Then it swaps pointers to managed memory, forwards the program
 * root and reset residency statistics.
 */
static void do_copy_gc(void) {
  // print_gc_state();

  #ifndef GC_NO_INCREMENT
    if (scanptr < nextptr) { raise_no_memory_error(); }
  #endif

  reset_resident_object_stats();
  ++total_gc_cycles_count;

  /** Swap from-space and to-space pointers. */
  void *tmp = from_space;
  from_space = to_space;
  to_space = tmp;

  scanptr = nextptr = to_space;
  limitptr = (char *) to_space + heap_size / 2;

  linked_list_node *node;

  FOR_EACH_ROOT(node) {
    void **r = node->data;

    void *before = *r;
    *r = forward(*r);
  }
}

/** Function for copying not scanned objects. Some objects (e.g. roots) could
 * be already forwarded to to-space, but they could have pointers to from-space.
 * All objects between to-space and scan are already completely copied and all
 * their pointers has been forwarded. Objects between scan and next has been
 * copied with raw pointers. This function scan objects and forward their
 * pointers. Scanning continues until the total size of the newly scanned objects
 * is greater than or equal to the size given as an argument.
 */
static void advance_scan_pointer(size_t size_in_bytes) {
  size_t scanned_region_size = 0;

  while (scanptr < nextptr && scanned_region_size < size_in_bytes) {
    stella_object *obj = scanptr;

    const int fields_count = STELLA_OBJECT_HEADER_FIELD_COUNT(obj->object_header);

    for (int i = 0; i < fields_count; ++i) {
      void *field = obj->object_fields[i];
      obj->object_fields[i] = forward(field);

      if (nextptr > limitptr) { raise_no_memory_error(); }
    }

    const size_t object_size = STELLA_OBJECT_SIZE(obj);

    scanned_region_size += object_size;
    scanptr = (char *) scanptr + object_size;
  }
}

/** Copies all reachable objects. */
static void copy_all_reachable_objects(void) {
  /** Actually copying of all reachable objects will be equivalent to advancing
   * scan as far as possible, until scan reaches next.
   */
  advance_scan_pointer(SIZE_MAX);
}

/** Tries allocate memory in the to-space. If there is not enough memory for
 * allocation, copying garbage collection (GC) will be run. Copying GC forwardes
 * program roots and copies some objects (if GC_NO_INCREMENT enabled all objects)
 * to the to-space region.
 */
void* gc_alloc(size_t size_in_bytes) {
  initialize();

  void* allocated_object = NULL;

  #ifndef GC_NO_INCREMENT
    void *new_limit = (char *) limitptr - size_in_bytes;

    if (new_limit < nextptr) {
      do_copy_gc();

      new_limit = (char *) limitptr - size_in_bytes;
    }

    limitptr = new_limit;

    advance_scan_pointer(size_in_bytes);

    update_gc_stats(1, size_in_bytes);

    allocated_object = limitptr;
  #else
    void *new_next = (char *) nextptr + size_in_bytes;

    if (new_next > limitptr) {
      do_copy_gc();
      copy_all_reachable_objects();

      new_next = (char *) nextptr + size_in_bytes;
    }

    if (new_next > limitptr) { raise_no_memory_error(); }

    // print_gc_state();

    update_gc_stats(1, size_in_bytes);

    allocated_object = nextptr;
    nextptr = new_next;
  #endif

  GC_DEBUG("allocated %zu bytes at <%p>", size_in_bytes, allocated_object);

  return allocated_object;
}

/** Pushes program root to list of program roots. */
void gc_push_root(void **object) {
  initialize();

  linked_list_node *new_node = linked_list_push(&roots, object);

  if (new_node == NULL) { raise_no_memory_error(); }
}

/** Pops program root. If list of roots is empty, program will be failed. */
void gc_pop_root(void **object) {
  if (linked_list_length(&roots) == 0) { exit(-1); }

  linked_list_pop(&roots);
}

/** Checks if the given pointer points to from-space or to-space region. */
static bool is_gc_managed(const void *p) {
  return belongs_to(p, from_space) || belongs_to(p, to_space);
}

/** Read barrier checks if pointer in field to read points to from-space. In this
 * case pointer will be forwarded and field value will be updated, the updated
 * pointer will point to to-space.
 */
void gc_read_barrier(void *object, int field_index) {
  if (is_gc_managed(object)) { ++read_operations_count; }

  #ifndef GC_NO_INCREMENT
    stella_object *stobj = object;

    void *ptr = stobj->object_fields[field_index];

    if (belongs_to(ptr, from_space)) {
      ++read_barrier_activations_count;
      stobj->object_fields[field_index] = forward(ptr);
    }
  #endif
}

/** Write barrier just increments number of write operations and does nothing
 * with pointer because Baker's algorithm doesn't need it.
 */
void gc_write_barrier(void *object, int field_index, void *contents) {
  if (is_gc_managed(object)) { ++write_operations_count; }
}

/** Prints the following garbage collector statistics:
 * 1. Total memory allocation - total size and number of objects allocated for
 * for program during entire time the program is running.
 * 2. Total GC cycles count - number of requests to collect the garbage.
 * 3. Maximum residency – maximum number of objects (and occupied memory size)
 * existing at same time in the GC workspace.
 * Total memory use - total number of reads and write operations of the program.
 * GC counts only using of its managed memory, i. e. operations with objects
 * allocated on the heap.
 * 5. Read barrier activation – total number of read barrier activations, i. e.
 * pointers forwarding needs.
 * 6. Write barrier activation – total number of same barrier activations, i. e.
 * pointers forwarding needs. Actually is always 0, because of Baker's algorithm
 * features.
 */
void print_gc_alloc_stats() {
  printf("Total memory allocation:  %'llu bytes (%'llu objects)\n"
         "Total GC cycles count:    %'llu\n"
         "Maximum residency:        %'llu bytes (%'llu objects)\n"
         "Total memory use:         %'llu reads and %'llu writes\n"
         "Read barrier activation:  %'llu activation(s)\n"
         "Write barrier activation: %'llu activation(s)",
      total_memory_size, total_objects_count,
      total_gc_cycles_count,
      max_resident_memory_size, max_resident_objects_count,
      read_operations_count, write_operations_count,
      read_barrier_activations_count,
      write_barrier_activations_count
  );
}

static void dump_stella_object(const stella_object *obj) {
  printf("  stella_object at <%p> {\n"
         "    tag: %s\n",
         obj, tags_strings[STELLA_OBJECT_HEADER_TAG(obj->object_header)]
  );

  const int fields_count = STELLA_OBJECT_HEADER_FIELD_COUNT(obj->object_header);

  for (int i = 0; i < fields_count; ++i) {
    printf("    field #%d: <%p>\n", i, obj->object_fields[i]);
  }

  printf("  }\n");
}

static void dump_stella_objects(const void *start, const void *end) {
  const void *p = start;
  size_t objects_dumped = 0;

  while (p < end && objects_dumped <= GC_STATS_OBJECTS_TO_DUMP) {
    const stella_object *obj = p;

    dump_stella_object(obj);

    p = (const char *) p + STELLA_OBJECT_SIZE(obj);
    ++objects_dumped;
  }
}

/** Prints various information about current GC state, e. g. variables values,
 * state of the objects in the heap, allocated and free memory size, program
 * roots (see `print_gc_roots`).
 */
void print_gc_state(void) {
  printf("\n------------------------------------------------------------\n");
  printf("Garbage collector (GC) variables:\n");

  printf("from-space: %p, to-space: %p\n", from_space, to_space);

  #ifndef GC_NO_INCREMENT
  printf("scan: %p, next: %p, limit: %p\n", scanptr, nextptr, limitptr);
  #else
  printf("scan: %p, next: %p\n", scanptr, nextptr);
  #endif

  printf("\nHeap state:\n");
  dump_stella_objects(to_space, nextptr);

  #ifndef GC_NO_INCREMENT
    dump_stella_objects(limitptr, to_space + heap_size / 2);
  #endif

  print_gc_roots();

  const size_t allocated_memory = resident_memory_size;
  printf("Allocated memory: %'zu bytes\n", allocated_memory);

  const size_t free_memory = heap_size / 2 - allocated_memory;
  printf("Free memory: %'zu bytes\n", free_memory);
}

/** Prints information about program roots. If a root points to the heap, the
 * pointed object will be dumped, otherwise the raw root pointer will be printed.
 */
void print_gc_roots(void) {
  printf("\nRoots:\n");
  linked_list_node *node;

  size_t i = 1;

  FOR_EACH_ROOT(node) {
    void **r = node->data;

    const void *p = *r;

    printf("Root #%zu:\n", i);

    if (is_gc_managed(p)) {
      dump_stella_object(p);
    } else {
      printf("  <%p>\n", p);
    }

    ++i;
  }
}
