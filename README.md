# Memory Manager

This Project is a self contained Windows memory manager written in C++. It reads and writes memory in an external process using raw syscall stubs instead of going through ntdll, which means it works even when ntdll is hooked.


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
#include "memory_manager.hpp"

memory_manager mem;

if (!mem.open("client.exe"))
{
    // returns if proccess isnt avalible or couldnt get a handle
    return 1;
}
```

### 3. Read memory

```cpp
// read any type you want
float <name> = mem.read<float>(offset);
int <name> = mem.read<int>(address + offset);
bool <name> = mem.read<bool>(address + offset);
```

### 4. Write memory

```cpp
mem.write<float>(offset, value);
mem.write<int>(address + offset, value);
mem.write<bool>(address + offset, value);
```

### 5. Get a module base address

```cpp
uintptr_t base = mem.get_module_base("client.exe");
```

### 6. Read a string

```cpp
// handles SSO and heap-allocated strings
std::string name = mem.read_string(address);
```


## How the syscall stubs work

Instead of calling `ReadProcessMemory` / `WriteProcessMemory`  this manager builds a assembly stubs at runtime and calls the NT kernel directly.

```
mov r10, rcx; required for syscalls on x64
mov eax, 0x3F; number for NtReadVirtualMemory
syscall
ret
```

The stubs are allocated as executable memory with `VirtualAlloc` and cast to function pointers.

> **Note:** The syscall numbers (`0x3F` read, `0x3A` write) are for **Win 10 / Win 11** only if you use any other version you have update them


## Requirements

- Windows 10/11 (x64)
- C++ 17
- Link against `Psapi.lib`


## Notes

- The process handle is cleaned up automatically when `memory_manager` goes out.
- `read_string` handles the SSO layout used by MSVC's `std::string`  strings under 16 characters are stored inline longer ones store a pointer to heap memory at offset `0x0`.
- `find_process_id` is exposed as a free function if you need the PID separately.
