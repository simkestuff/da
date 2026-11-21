# da.h: a small dynamic array for C

A single-header, growable array ("vector") for C99. You declare a struct type for an element type, then work on it through a set of `da_*` macros. One small function does the memory growth.

> Work in progress, use at your own risk.

- **One file**, no dependencies beyond the C standard library
- **Any element type**: `int`, pointers, plain structs
- **Amortised O(1)** append (capacity doubles)
- **Bounds-checked** access, insert and delete (in debug builds, see [Error handling](#error-handling))
- **C only** (C99 or later). C++ is not supported.

## Installation

1. Copy `da.h` into your project.
2. In **exactly one** `.c` file, define `DA_IMPLEMENTATION` before the include:

   ```c
   #define DA_IMPLEMENTATION
   #include "da.h"
   ```

3. In every other file, just `#include "da.h"`.

If you forget step 2 you get a linker error such as `undefined reference to 'da__grow'`.

## Quick start

```c
#include <stdio.h>
#include "da.h"

da_declare(int, Ints);              // declare the array type (note the ';')

int main(void)
{
    Ints a = DA_INIT;               // always initialise with DA_INIT

    for (int i = 0; i < 10; i++)
        da_append(&a, i * i);       // pass a POINTER to the array

    da_ins(&a, 0, -1);              // insert -1 at the front
    da_del(&a, 1);                  // delete the element at index 1

    for (size_t i = 0; i < da_len(&a); i++)
        printf("%d\n", da_get(&a, i));

    da_free(&a);                    // release the memory when done
    return 0;
}
```

Build:

```sh
gcc -std=c99 -Wall -Wextra -pedantic main.c -o main
```

## The array type

`da_declare(T, Name)` creates:

```c
typedef struct Name {
    T *items;          // the elements (NULL while nothing is allocated)
    size_t count;      // how many elements are stored
    size_t capacity;   // how many fit before the next reallocation
} Name;
```

The fields are public. You can read `items`, `count` and `capacity` directly. Don't write to `count` or `capacity` by hand; use the macros.

```c
da_declare(int, Ints);
da_declare(char *, Strings);

typedef void (*Callback)(void);     // function pointers need a typedef first
da_declare(Callback, Callbacks);
```

Notes:

- Write the `;` yourself after `da_declare(...)`.
- `T` must be a complete type. `void` is not allowed.
- Every `da_declare` creates a **new, distinct type**, even for the same `T`. Declare each array type once (for example in a shared header) and reuse the name.

## API reference

In every macro below, `da` is a **pointer** to the array, so write `&a`, or just `p` if you already have a pointer.

| Macro | What it does |
|---|---|
| `DA_INIT` | Initialiser for an empty array: `Ints a = DA_INIT;` |
| `da_len(da)` | Number of stored elements |
| `da_cap(da)` | Elements that fit without reallocating |
| `da_get(da, i)` | Element at index `i` (bounds-checked). Can be assigned to: `da_get(&a, 2) = 99;` |
| `da_set(da, i, v)` | Same as `da_get(da, i) = v`, as an expression |
| `da_reserve(da, n)` | Make sure the capacity is at least `n`. Does not change `count` |
| `da_append(da, item)` | Add one element at the end |
| `da_append_many(da, src, n)` | Copy `n` elements from the C array `src` to the end |
| `da_ins(da, i, v)` | Insert `v` at index `i`, shifting the rest up (`0 <= i <= count`) |
| `da_del(da, i)` | Delete the element at index `i`, shifting the rest down |
| `da_del_many(da, i, n)` | Delete `n` elements starting at `i` |
| `da_clear(da)` | Set `count` to 0, keeping the allocated memory |
| `da_free(da)` | Free the memory and reset to the empty state |

### Details

**`da_get` / `da_set`**: valid indices are `0 .. count - 1`, even if the capacity is larger. A negative or too-large index triggers the assert.

**`da_reserve`**: the new memory is uninitialised. Don't read `items[count .. capacity-1]` until you've stored values there. Call it before a long series of `da_append` calls to avoid repeated reallocations. The first allocation is `max(DA_INIT_CAP, n)`; after that the capacity doubles until it fits.

**`da_append`**: the item is read *after* any reallocation, so `da_append(&a, a.items[0])` is fine.

**`da_append_many`**: `n == 0` does nothing (and `src` may be `NULL` then). `src` must **not** point into the same dynamic array: it could dangle after a reallocation, and `memcpy` doesn't allow overlap. Copy to a temporary first if you need that.

```c
int v[3] = {1, 2, 3};
da_append_many(&a, v, 3);
```

**`da_ins`**: `i == count` is valid and equals an append. It is the only valid index on an empty array. The value `v` is evaluated *after* the elements have been shifted, so it must not read from the same array: `da_ins(&a, 0, a.items[3])` stores the wrong value.

**`da_del` / `da_del_many`**: order is preserved, and the cost is O(count - i). The assert requires `i <= count` and `n <= count - i`. Capacity is not reduced. To delete the last element of a non-empty array: `da_del(&a, da_len(&a) - 1)`.

**`da_clear`**: does not free anything. If your elements are pointers you own, free them first.

**`da_free`**: safe on an array that never allocated, and the array is reusable afterwards. It does not free what pointer elements point to.

## Rules (please read)

1. **Pass a pointer.** `da_append(&a, x)`, not `da_append(a, x)`.
2. **No side effects in the `da` argument.** It's evaluated several times inside the macros. OK: `&a`, `p`, `ctx->list`. Not OK: `&arr[next()]`.
3. **Other arguments are evaluated once.** Index, count and value arguments are safe to write as `i++` or a function call.
4. **Appending, inserting and reserving can move the array in memory.** Any pointer you took into it (`int *p = &a.items[3];`) is then invalid. Use indices if the array may grow. Deleting also invalidates pointers to elements at or after the deleted position.
5. **Names starting with `da__` are internal.** Don't use them in your own code.
6. **Elements are copied with `memcpy` / `memmove`**, so use plain value types.
7. **Iterating while modifying.** Don't append or insert while looping over the array with a pointer. When deleting inside a loop, go backwards over the indices:

   ```c
   for (size_t i = da_len(&a); i-- > 0; )
       if (should_remove(da_get(&a, i)))
           da_del(&a, i);
   ```

## Error handling

Two kinds of problems are handled differently.

**Programmer errors** (index out of range, deleting past the end) are checked with `DA_ASSERT`, which is the standard `assert()` by default.

> **Important:** `assert` is removed when you compile with `-DNDEBUG`, which is common for release builds. Then **no bounds checking happens** and a bad index is undefined behaviour (memory corruption).

To keep the checks in release builds, define this before including the header:

```c
#define DA_ASSERT(cond) do { if (!(cond)) abort(); } while (0)
```

**Runtime failures** (out of memory, size overflow) are **not** asserts. They are detected in every build and call `DA_ON_OOM()`, which is `abort()` by default. The array is never left half-updated: when a growth fails, the old block and the old capacity are unchanged.

If you redefine `DA_ON_OOM`, your version must **not return** (use `abort`, `exit`, `longjmp`, ...). If it returned, the code would continue with a `NULL` pointer.

## Configuration

Define these **before** including `da.h`. All are optional.

| Macro | Default | Meaning |
|---|---|---|
| `DA_INIT_CAP` | `8` | Capacity allocated on the first growth. It's a minimum: `da_reserve(&a, 1000)` on an empty array allocates 1000, not 8 |
| `DA_ON_OOM()` | `abort()` | Called when allocation fails or a size computation overflows. Must not return |
| `DA_ASSERT` | `assert` | Used for all bounds checks |

Example:

```c
#define DA_INIT_CAP 64
#define DA_IMPLEMENTATION
#include "da.h"
```

## Limitations

- **No thread safety.** Don't share one array between threads without your own locking. Reading from several threads is fine as long as nobody writes.
- **C only.** No C++ support (the implicit `void *` to `T *` conversion is a compile error there).
- **Shallow `const`.** A `const Ints *` stops you from changing `count` and `capacity`, but `da_get` / `da_set` still compile and can modify elements.
- **Only the `da` argument has restrictions** on side effects, but it can't be fixed in plain C macros (see rule 2).
- **Allocator:** uses `realloc` / `free` from `<stdlib.h>`; no custom allocator hooks.
- **Not provided:** `pop`, `last`, unordered O(1) delete, `resize`, `shrink_to_fit` and a `foreach` macro. For loops use an index loop, as in the examples above.
- **Distinct types:** two `da_declare` lines with the same element type are incompatible with each other.

## Internals

- Almost everything is a macro. The only real function is `da__grow`, defined under `DA_IMPLEMENTATION`.
- `da__grow` doubles the capacity (or jumps straight to the requested size if that is larger), checks for `size_t` overflow, and returns `NULL` on failure without touching the old block.
- `da__idx` is a small `static inline` helper that does the bounds check for `da_get`. It's a function so the index is evaluated only once and `da_get` can still be used as an lvalue.

## Testing

A quick way to check your build, including memory errors and undefined behaviour:

```sh
gcc -std=c99 -Wall -Wextra -pedantic -fsanitize=address,undefined \
    main.c -o main && ./main
```

Also try a build with `-DNDEBUG` to confirm that your program doesn't rely on the asserts.
