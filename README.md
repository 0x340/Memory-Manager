# Memory Manager

This project is a self-contained Windows memory manager written in C++. It reads and writes memory in an external process using raw syscall stubs instead of going through `ntdll`, which means it works even when `ntdll` is hooked.

## File Content

| File | Content |
|---|---|
| `memory.hpp` | Class declaration + read/write |
| `memory.cpp` | Syscall stubs, process helpers, string reading |

## How to use it

### 1. Add the files to your project
Just copy `memory.hpp` and `memory.cpp` into your project and include the header wherever you need it.

### 2. Open a process
```cpp
#include "memory.hpp"

if (!mm::g_mm->open("client.exe")) {
    // Returns if process isn't available or couldn't get a handle
    return 1;
}
