/* dynamic array implementation for my usage

   To use it: copy this header file in project and in one of
   C files place:
   #define DA_IMPLEMENTATION
   #include "da.h"

   Work in progress, use at your own risk...

 */

#ifndef DA_H_
#define DA_H_

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef DA_INIT_CAP
#define DA_INIT_CAP 256
#endif

// using T=void is unsafe
#define da_declare(T,name) typedef struct { \
    T *items;                               \
    size_t count;                           \
    size_t capacity;                        \
} (name);

#define da_len(da) ((da)->count)
#define da_cap(da) ((da)->capacity)
#define da_get(da,i) (assert((i) < (da)->count && "index out of range"), \
                     (da)->items[(i)])

#define da_reserve(da,capacity_expected)                              \
do {                                                                  \
    size_t newcap = (da)->capacity;                                   \
    if ((capacity_expected) > newcap) {                               \
	if (newcap == 0) {                                            \
	    newcap = DA_INIT_CAP > (capacity_expected) ?              \
		DA_INIT_CAP : (capacity_expected);		      \
	} else {                                                      \
	    while (newcap < (capacity_expected))              	      \
		newcap *= 2;                                          \
	}                                                             \
	void *tmp = realloc((da)->items,                              \
			      sizeof(*(da)->items) * newcap);         \
	assert(tmp != NULL && "memory allocation failed");            \
	(da)->items = tmp;                                            \
	(da)->capacity = newcap;                                      \
    }                                                                 \
} while (0)

#define da_append(da,item)               \
do {                                     \
    da_reserve((da), (da)->count + 1);   \
    (da)->items[(da)->count++] = (item); \
} while (0)

#define da_append_many(da,append_items,append_items_count) \
do {                                                       \
    da_reserve((da), (da)->count + (append_items_count));  \
    memmove((da)->items + (da)->count,                     \
           (append_items),				   \
           (append_items_count) * sizeof(*(da)->items));   \
    (da)->count += (append_items_count);                   \
} while (0)

#define da_del_many(da,i,n)				        \
do {                                                            \
    assert((i) + (n) <= (da)->count &&                          \
           "index out of range");			        \
    if ((i) + (n) < (da)->count) {			        \
	memmove((da)->items + i,                                \
		(da)->items + i + n,                            \
		sizeof(*(da)->items) * ((da)->count - i - n));	\
    }                                                           \
    (da)->count -= n;                                           \
} while (0)

#define da_del(da,i) da_del_many(da,i,1)
#define da_clear(da) ((da)->count = 0)

#define da_free(da)     \
do {                    \
    free((da)->items);  \
    (da)->items = NULL; \
    (da)->count = 0;    \
    (da)->capacity = 0; \
} while (0)

#define da_ins(da,i,v)                                 \
do {                                                   \
    assert(i <= (da)->count && "index out of range");  \
    da_reserve((da), (da)->count + 1);                 \
    if ((i) < (da)->count) {                           \
        memmove((da)->items + i + 1, (da)->items + i,  \
	    sizeof(*(da)->items) * ((da)->count - i)); \
    }                                                  \
    (da)->items[i] = v;                                \
    (da)->count++;                                     \
} while (0)

#endif // DA_H_


#ifdef DA_IMPLEMENTATION


#endif // DA_IMPLEMENTATION

