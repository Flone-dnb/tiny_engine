# memcheck

A small and silly memory leak checker for Windows and Linux. Usage:

- Include in your root CMakeLists.txt as the first subproject:

```
# only for Linux:
add_link_options(...) # <- copy-paste from CMakeLists.txt next to this readme file

# for both Windows and Linux:
add_subdirectory(<path>/memcheck)
link_libraries(memcheck)
```

- Init/deinit in C:

```C
#include <memcheck.h>

int main(void) {
    memcheck_init();

    // your code

    memcheck_deinit();
}
```

Checks all malloc, free and similar memory operations (including linked libraries, on Linux requires dependencies that allocate memory to be linked dynamically). In `deinit` prints if any memory leaks occurred.

# Prerequisites

- Compiler that supports C89/C90
- [CMake](https://cmake.org/download/)

