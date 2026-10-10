# Memory Manager

It's currently **Windows 10 only** if you use anything else you have to update the syscalls used in the code.
You can find a table to update the syscalls here: [Windows Syscall Table](https://j00ru.vexillium.org/syscalls/nt/64/)

Since Im going for stealth, none of the often monitored & hooked APIs are being used, insted its using the raw stub syscall for each.

APIs Im avoiding
- ``OpenProcess``
- ``CloseHandle``
- ``EnumProcessModules / EnumProcessModulesEx``
- ``GetModuleBaseName / GetModuleFileNameEx``
- ``CreateToolhelp32Snapshot``
- ``VirtualQueryEx``
- ``VirtualProtectEx``
- ``VirtualAllocEx``
- ``ReadProcessMemory``
- ``WriteProcessMemory``
- ``Process32First / Process32Next``

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
    MEMORY_BASIC_INFORMATION mbi{};
    bool success = memory::query(base + 0x1234, mbi);

    // change protection flags
    DWORD old_prot;
    bool ok = memory::protect((uintptr_t)mem, 4096, PAGE_READONLY, &old_prot);

    // allocate memory
    void* mem = memory::allocate(0x10000, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    // read a std::string
    std::string text = memory::read_string(base + 0x5678);

    // write a std::string
    memory::write_string(base + 0x5678, "Hello, world!");

    memory::close_process(); 

    return {0};
}
```

**Requirements**
- Windows 10 x64
- std:c++ 20
- Administrator
