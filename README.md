# Memory Manager

Its is currently **Windows 10 only** if you use any other version you have to update the syscalls for NT read & write memory. It reads and writes memory in an external process using raw syscall stubs instead of going through `ntdll`, which means it works even when `ntdll` is hooked. It also doesnt use `EnumProcessModules` to cuz the api can be hooked and that can result in detections


### Guide
```cpp
#include "memory.hpp"

int main()
{
    if (!mm::open_process("notepad.exe"))
    {
        return {1}; // if the app is dead
    }

    // get the base address
    uintptr_t base = mm::get_module_base("notepad.exe");

    // read a value    
    int value = mm::read<int>(base + 0x1234);

    // write a value
    mm::write<int>(base + 0x1234, 42);

    // read a std::string
    std::string text = mm::read_string(base + 0x5678);

    // write a std::string
    mm::write_string(base + 0x5678, "Hello, world!");

    mm::close_process(); 

    return {0};
}
```
