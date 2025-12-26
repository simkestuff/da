# Dynamic array for C

Single header file library that implements Dynamic Array for C.


To use it: copy this header file in project and in one of C files place:

```
   #define DA_IMPLEMENTATION
   #include "da.h"
```

Work in progress, use at your own risk...

## Example

```
#include <stdio.h>

#define DA_INIT_CAP 4
#define DA_IMPLEMENTATION
#include "da.h"

da_declare(int, Array);

int main(void)
{
    Array a = {0};
    da_append(&a, 12);
    da_append(&a, 19);
    int b[] = { 11, 12, 12, 13, 24, 4, 7, 12 };
    da_append_many(&a, b, 8);

    printf("array size %zu, capacity %zu\n", da_len(&a), da_cap(&a));
    for (size_t i = 0; i < da_len(&a); i++)
	printf("%d ", da_get(&a, i));
    printf("\n");

    da_del_many(&a, 7, 2);
    for (size_t i = 0; i < da_len(&a); i++)
	printf("%d ", da_get(&a, i));
    printf("\n");
    printf("array size %zu, capacity %zu\n", da_len(&a), da_cap(&a));

    da_ins(&a, 3, 99);
    for (size_t i = 0; i < da_len(&a); i++)
	printf("%d ", da_get(&a, i));
    printf("\n");
    printf("array size %zu, capacity %zu\n", da_len(&a), da_cap(&a));

    da_free(&a);
    return 0;
}

```
