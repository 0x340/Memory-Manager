# Memory Manager

This project is a self-contained Windows memory manager written in C++. It reads and writes memory in an external process using raw syscall stubs instead of going through `ntdll`, which means it works even when `ntdll` is hooked

## How to use it

### 1. Add the files to your project
Just copy `memory.hpp` and `memory.cpp` into your project and include the header wherever you need it

### 2. Open a process and use memory
```cpp
#include "memory.hpp"

// Open process
if (!mm::g_mm->open("client.exe")) {
    // Returns if process isn't available or couldn't get a handle
    return 1;
}

// Reading memory
<Type> <name> = mm::g_mm->read<<Type>>(address + offset);
std::string <name> = mm::g_mm->read_string(address + offset);

// Writing memory
mm::g_mm->write<<Type>>(address + offset, value);
mm::g_mm->write_string(address + offset, "<value>");

// module base address
uintptr_t base = mm::g_mm->get_module_base("client.exe");
