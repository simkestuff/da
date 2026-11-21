/* da.h - dynamic array implementation for my usage
   ==================================================

   Work in progress, use at your own risk...

   WHAT IT IS
   ----------
   A growable array ("vector") for C, written as macros plus one small
   function. You declare a struct type for a given element type, then use the
   da_* macros on a POINTER to a variable of that type.

   HOW TO ADD IT TO A PROJECT
   --------------------------
   1. Copy this file into your project (name it da.h).
   2. In exactly ONE .c file, place:

          #define DA_IMPLEMENTATION
          #include "da.h"

      This compiles the function da__grow(). Without it you get a linker
      error like "undefined reference to da__grow".
   3. In every other file just write:  #include "da.h"
   Requires C99 (uses "static inline"). C++ is NOT supported.

   QUICK EXAMPLE
   -------------
          #include <stdio.h>
          #include "da.h"

          da_declare(int, Ints);          // declare the type (note the ';')

          int main(void)
          {
              Ints a = DA_INIT;           // ALWAYS initialise with DA_INIT

              for (int i = 0; i < 10; i++)
                  da_append(&a, i * i);   // pass a POINTER to the array

              da_ins(&a, 0, -1);          // insert -1 at the front
              da_del(&a, 1);              // delete element at index 1

              for (size_t i = 0; i < da_len(&a); i++)
                  printf("%d\n", da_get(&a, i));

              da_free(&a);                // release memory when done
              return 0;
          }

   THE STRUCT
   ----------
   da_declare(T, Name) creates:

          typedef struct Name {
              T *items;          // pointer to the elements (NULL when empty)
              size_t count;      // number of elements currently stored
              size_t capacity;   // number of elements that fit without growing
          } Name;

   The fields are public: you may read items/count/capacity directly, and
   use items[i] yourself. Do NOT write to count or capacity by hand unless
   you know what you are doing; use the macros instead.

   RULES FOR ALL MACROS  (read this part!)
   ---------------------------------------
   R1. The "da" argument is always a POINTER to the array:   da_append(&a, x)
       If you already have a pointer p, pass it as is:       da_append(p, x)

   R2. The "da" argument is evaluated SEVERAL times inside the macros, so it
       must not have side effects. OK: &a, p, ctx->list, &arr[3].
       NOT OK: &arr[next()], get_array_and_count_calls().

   R3. The other arguments (index i, count n, value v, item) are evaluated
       exactly once, so da_get(&a, i++) behaves as you would expect.

   R4. Macros that insert/append (da_append, da_ins, da_reserve, ...) may
       reallocate the memory. After ANY of them, every pointer you previously
       took into the array (for example int *p = &a.items[3]) is INVALID.
       Use indices instead of pointers if the array can grow.

   R5. Names starting with da__ (two underscores) are internal to the
       library. Do not use them in your own code.

   R6. Elements are copied with memcpy/memmove, so T should be a plain
       value type (numbers, pointers, plain structs). T must be a complete
       type: "void" is not allowed. For function pointers, typedef them first.

   ERROR HANDLING
   --------------
   Two different kinds of problems are handled in two different ways:

   * Programmer errors (index out of range, delete past the end, ...)
     are checked with DA_ASSERT, which is the standard assert() by default.
     IMPORTANT: assert is REMOVED when you compile with -DNDEBUG (usual for
     release builds). In that case NO bounds checking happens and a bad
     index is undefined behaviour (memory corruption). If you want the
     checks in release builds too, define before including this file:

          #define DA_ASSERT(cond) do { if (!(cond)) abort(); } while (0)

   * Runtime failures (out of memory, size overflow) are NOT asserts, so
     they are detected in every build. They call DA_ON_OOM(), which by
     default is abort(). The array is never left half-updated: on failure the
     old block is still valid and unchanged. If you redefine DA_ON_OOM, your
     version must NOT return (abort, exit, longjmp...); if it returned,
     the code would continue with a NULL pointer.

   CONFIGURATION (define BEFORE including da.h; all optional)
   ----------------------------------------------------------
     DA_INIT_CAP   capacity allocated on first growth (default 8).
                   It is a minimum: da_reserve(&a, 1000) on an empty array
                   allocates 1000, not 8. Afterwards the capacity doubles
                   when more room is needed.
     DA_ON_OOM()   what to do when allocation fails (default abort()).
     DA_ASSERT     assert-like macro for bounds checks (default assert).

   THINGS THAT ARE NOT PROVIDED
   ----------------------------
   * No thread safety: do not share one array between threads without
     your own locking.
   * No custom allocator hooks (it uses realloc/free from <stdlib.h>).
   * No pop/last/swap-remove/resize helpers. Use items[count - 1]
     and da_del for these, or add them yourself.
   * Two da_declare() lines with the same element type create two
     DIFFERENT, incompatible types. Declare each array type once (in a
     shared header) and reuse its name everywhere.
 */

#ifndef DA_H_
#define DA_H_

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

/* Capacity used by the first allocation (see CONFIGURATION above). */
#ifndef DA_INIT_CAP
#define DA_INIT_CAP 8
#endif

/* Called when memory cannot be allocated or a size computation overflows.
   Must not return. */
#ifndef DA_ON_OOM
#define DA_ON_OOM() abort()
#endif

/* Used for all bounds checks. Disappears under -DNDEBUG by default. */
#ifndef DA_ASSERT
#define DA_ASSERT assert
#endif


/* Initializer for an empty array. Use it when you define the variable:
       Ints a = DA_INIT;
   An array that is not initialised contains garbage and WILL crash.
   It can only be used in an initialiser, not as an assignment
   (to reset an array use da_free). */
#define DA_INIT {0}

/* da_declare(T, Name)  -- declare an array type holding elements of type T.

       da_declare(int, Ints);
       da_declare(char *, Strings);

   Write the ';' yourself after the macro.
   T must be a complete object type: void is not allowed, because the
   macros need sizeof(T). (For a function pointer type, make a typedef first.)
   Every da_declare creates a new distinct type, even for the same T. */
#define da_declare(T,name) typedef struct name { \
	T *items;				 \
	size_t count;				 \
	size_t capacity;			 \
} name

/* da_len(da) - number of elements stored.
   da_cap(da) - number of elements that fit without reallocating.
   (Both take a pointer: da_len(&a).) */
#define da_len(da) ((da)->count)
#define da_cap(da) ((da)->capacity)

/* Internal helper (do not call directly): checks that i < count and returns
   i. It is a function so the index is evaluated only once and so that
   da_get below can still be used as an lvalue. */
static inline size_t da__idx(size_t i, size_t count)
{
    DA_ASSERT(i < count && "index out of range");
    (void) count;   /* silences "unused" warning under NDEBUG */
    return i;
}

/* Internal (do not call directly): grows the block so it holds at least
   `want` elements. Defined below, under DA_IMPLEMENTATION. */
void *da__grow(void *items, size_t *cap, size_t elem_size, size_t want);

/* da_get(da, i) - element at index i, with a bounds check (i < count).
   It can also be assigned to:
       int x = da_get(&a, 2);
       da_get(&a, 2) = 99;
   A negative or too-large i triggers the assert (no check under NDEBUG).
   Only indices below da_len() are valid, even if capacity is larger.

   da_set(da, i, v) - same as da_get(da, i) = v, as an expression. */
#define da_get(da,i) ((da)->items[da__idx((i), (da)->count)])
#define da_set(da,i,v) ((void)(da_get((da), (i)) = (v)))

/* da_reserve(da, n) - make sure the capacity is at least n elements.
   Does NOT change count. The new memory is uninitialised, and you must not
   read items[count ... capacity-1] until you have stored values there
   (for example with da_append). Call it before a big series of
   da_append to avoid repeated reallocations.
   May move the array in memory (see rule R4).
   On failure it calls DA_ON_OOM(). */
#define da_reserve(da,capacity_expected)                              \
do {                                                                  \
    size_t da__want = (size_t) (capacity_expected);                   \
    if (da__want > (da)->capacity) {                                  \
        void *da__tmp = da__grow((da)->items, &(da)->capacity,        \
                                 sizeof(*(da)->items), da__want);     \
        if (da__tmp == NULL)                                          \
            DA_ON_OOM();                                              \
        (da)->items = da__tmp;                                        \
    }                                                                 \
} while (0)

/* da_append(da, item) - add one element at the end (amortised O(1)).
   Grows the array when needed, so it may move it in memory (rule R4).
   `item` is read after the possible reallocation, so
   da_append(&a, a.items[0]) is fine. */
#define da_append(da,item)               \
do {                                     \
    da_reserve((da), (da)->count + 1);   \
    (da)->items[(da)->count++] = (item); \
} while (0)

/* da_append_many(da, src, n) - copy n elements from the C array `src` to
   the end of the dynamic array. n == 0 does nothing (src may be NULL then).
       int v[3] = {1, 2, 3};
       da_append_many(&a, v, 3);
   WARNING: `src` must NOT point into the same dynamic array. If the array
   is reallocated, src would dangle, and memcpy does not allow overlap.
   If you need that, copy the data to a temporary buffer first. */
#define da_append_many(da,append_items,append_items_count)       \
do {                                                             \
    size_t da__n = (size_t) (append_items_count);                \
    if (da__n > 0) {                                             \
        da_reserve((da), (da)->count + da__n);                   \
        memcpy((da)->items + (da)->count,                        \
               (append_items),                                   \
               da__n * sizeof(*(da)->items));                    \
        (da)->count += da__n;                                    \
    }                                                            \
} while (0)

/* da_del_many(da, i, n) - delete n elements starting at index i; the
   elements after them move down (O(count - i), order is preserved).
   Requires i <= count and n <= count - i, otherwise the assert fires.
   Capacity is not reduced and the memory is not freed. Pointers to
   elements at or after i become invalid (the data moved). */
#define da_del_many(da,i,n)                                          \
do {                                                                 \
    size_t da__i = (size_t) (i);                                     \
    size_t da__n = (size_t) (n);                                     \
    DA_ASSERT(da__i <= (da)->count &&                                \
              da__n <= (da)->count - da__i &&                        \
              "index out of range");                                 \
    if (da__i + da__n < (da)->count) {                               \
        memmove((da)->items + da__i,                                 \
                (da)->items + da__i + da__n,                         \
                sizeof(*(da)->items) *                               \
                ((da)->count - da__i - da__n));                      \
    }                                                                \
    (da)->count -= da__n;                                            \
} while (0)

/* da_del(da, i) - delete the single element at index i (order preserved).
   To delete the last element use da_del(&a, da_len(&a) - 1) (only on a
   non-empty array). */
#define da_del(da,i) da_del_many(da,i,1)

/* da_clear(da) - set count to 0. Keeps the allocated memory and capacity so
   the array can be refilled without reallocating. It does NOT free anything:
   if the elements are pointers you own, free them yourself first. */
#define da_clear(da) ((void)((da)->count = 0))

/* da_free(da) - release the memory and reset the array to the empty state
   (items = NULL, count = 0, capacity = 0). Safe to call on an array that
   never allocated, and the array can be used again afterwards.
   Does NOT free what the elements point to if they are pointers. */
#define da_free(da)     \
do {                    \
    free((da)->items);  \
    (da)->items = NULL; \
    (da)->count = 0;    \
    (da)->capacity = 0; \
} while (0)

/* da_ins(da, i, v) - insert v at index i; the elements from i on move up by
   one (O(count - i), order is preserved).
   Valid range is 0 <= i <= count: i == count is the same as da_append, and
   it is the only valid index on an empty array.
   May reallocate (rule R4).
   WARNING: `v` is evaluated AFTER the elements have been shifted, so it
   must not read from the same array
   (da_ins(&a, 0, a.items[3]) would store the wrong value). */
#define da_ins(da,i,v)                                               \
do {                                                                 \
    size_t da__i = (size_t) (i);                                     \
    DA_ASSERT(da__i <= (da)->count && "index out of range");         \
    da_reserve((da), (da)->count + 1);                               \
    if (da__i < (da)->count) {                                       \
        memmove((da)->items + da__i + 1, (da)->items + da__i,        \
                sizeof(*(da)->items) * ((da)->count - da__i));       \
    }                                                                \
    (da)->items[da__i] = (v);                                        \
    (da)->count++;                                                   \
} while (0)

/* da_foreach(it, da) - loop over all elements, front to back.
   `it` must be a pointer to the element type, declared by you BEFORE the
   loop (a macro cannot declare a variable of the right type in plain C99):

       int *it;
       da_foreach(it, &a)
           printf("%d\n", *it);

   Inside the loop *it is the current element, and you can modify it
   (*it = 5). The `it` test also makes it safe on an array that never
   allocated (items == NULL).
   RULES:
   - Do not append to or insert into the array inside the loop: a
     reallocation leaves `it` dangling (rule R4).
   - Do not delete inside the loop: the elements shift down, so the next
     one is skipped. Loop over indices backwards if you need to delete
     while iterating.
   - `da` is evaluated on every iteration, so it must have no side effects
     (rule R2). */
#define da_foreach(it,da)                                            \
    for ((it) = (da)->items;                                         \
         (it) && (it) < (da)->items + (da)->count;                   \
         (it)++)

#endif // DA_H_


/* ======================================================================
   IMPLEMENTATION
   Compiled only in the ONE .c file that defines DA_IMPLEMENTATION before
   including this header (see the top of the file).
   ====================================================================== */
#ifdef DA_IMPLEMENTATION
/* Guard: harmless if the implementation is included twice in one file. */
#ifndef DA_IMPLEMENTATION_DONE_
#define DA_IMPLEMENTATION_DONE_

/* Internal. Makes the block hold at least `want` elements of `elem_size`
   bytes.
     items      current block (may be NULL)
     cap        in: current capacity, out: new capacity (only on success)
     elem_size  sizeof one element
     want       minimum number of elements needed
   Policy: from capacity 0 it allocates max(DA_INIT_CAP, want); otherwise it
   doubles the capacity until it is >= want.
   Returns the new block, or NULL on failure (allocation failed or the size
   would overflow size_t). On failure the old block is still valid and *cap
   is unchanged. The caller (da_reserve) turns NULL into DA_ON_OOM(). */
void *da__grow(void *items, size_t *cap, size_t elem_size, size_t want)
{
    size_t newcap = *cap;
    void *p;

    if (newcap == 0) {
        newcap = DA_INIT_CAP > want ? DA_INIT_CAP : want;
    } else {
        while (newcap < want) {
            if (newcap > SIZE_MAX / 2) {   /* doubling would wrap */
                newcap = want;
                break;
            }
            newcap *= 2;
        }
    }
    if (elem_size != 0 && newcap > SIZE_MAX / elem_size)
        return NULL;

    p = realloc(items, newcap * elem_size);
    if (p == NULL)
        return NULL;                       /* old block still valid */
    *cap = newcap;
    return p;
}



#endif // DA_IMPLEMENTATION_DONE_
#endif // DA_IMPLEMENTATION
