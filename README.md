# Memory Manager

It's currently **Windows 10 only** if you use anything else you have to update the syscalls for `NtReadVirtualMemory` & `NtWriteVirtualMemory`. It reads & writes memory in an process using raw syscall instead of going through `ntdll` which means it works when `ntdll` is hooked and being monitored.
It also doesnt use `EnumProcessModules` too cuz the api is also often monitored and since the goal is stealth here.

You can find a table to update the syscalls here: [Windows Syscall Table](https://j00ru.vexillium.org/syscalls/nt/64/)

### Guide
```cpp
#include "memory.hpp"
#include "windows.h"

int main()
{
    if (!memory::open_process("notepad.exe"))
    {
        return {1}; // if the app is dead
    }

    // get the base address
    uintptr_t base = memory::get_module_base("notepad.exe");

    // read a value    
    int value = memory::read<int>(base + 0x1234);

    // write a value
    memory::write<int>(base + 0x1234, 42);

    // query info about a address
    MEMORY_BASIC_INFORMATION mbi;
    bool success = memory::query((uintptr_t)mem, &mbi, sizeof(mbi));

    // allocate memory
    void* mem = memory::allocate(0x10000, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    // change protection flag
    DWORD old_prot;
    bool ok = memory::protect((uintptr_t)mem, 4096, PAGE_READONLY, &old_prot);

    // read a std::string
    std::string text = memory::read_string(base + 0x5678);

    // write a std::string
    memory::write_string(base + 0x5678, "Hello, world!");

    memory::close_process(); 

    return {0};
}
```

**Requirements**
- Windows 10
- c++ 20
