# Memory Manager

This is a memory manager I made in C++. It is currently Windows 10 only if you use any other version you have to update the syscalls for NT read & write memory. It reads and writes memory in an external process using raw syscall stubs instead of going through `ntdll`, which means it works even when `ntdll` is hooked


### quick guide
```cpp
#include "memory.hpp"

// open process
if (!mm::g_mm->open("client.exe"))
{
    return 1; // returns if the process is dead 
}

// reading
<Type> <name> = mm::g_mm->read<<Type>>(address);
std::string <name> = mm::g_mm->read_string(address);

// writing
mm::g_mm->write<<type>>(address, value);
mm::g_mm->write_string(address, "<value>");

// module base
uintptr_t base = mm::g_mm->get_module_base("client.exe");
