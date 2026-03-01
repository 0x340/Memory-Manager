#include "memory.hpp"
#include <stdexcept>

// all the syscalls are for windows 10/11 for any other version you have to look it up and change them
nt_read_fn build_read_syscall()
{
    static uint8_t stub[] = {
        0x4C, 0x8B, 0xD1,
        0xB8, 0x3F, 0x00, 0x00, 0x00, // NtReadVirtualMemory
        0x0F, 0x05,
        0xC3
    };

    void* exec = VirtualAlloc(nullptr, sizeof(stub), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!exec)
        throw std::runtime_error("valloc failed for read stub");

    memcpy(exec, stub, sizeof(stub));
    return reinterpret_cast<nt_read_fn>(exec);
}

nt_write_fn build_write_syscall()
{
    static uint8_t stub[] = {
        0x4C, 0x8B, 0xD1,
        0xB8, 0x3A, 0x00, 0x00, 0x00, // NtWriteVirtualMemory
        0x0F, 0x05,
        0xC3
    };

    void* exec = VirtualAlloc(nullptr, sizeof(stub), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!exec)
        throw std::runtime_error("valloc failed for write stub");

    memcpy(exec, stub, sizeof(stub));
    return reinterpret_cast<nt_write_fn>(exec);
}

uint64_t find_process_id(std::string_view process_name)
{
    PROCESSENTRY32 entry{};
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;

    while (Process32Next(snap, &entry))
    {
        if (process_name == entry.szExeFile)
        {
            CloseHandle(snap);
            return entry.th32ProcessID;
        }
    }

    CloseHandle(snap);
    return 0;
}

memory_manager::memory_manager()
{
    m_read  = build_read_syscall();
    m_write = build_write_syscall();
}

memory_manager::~memory_manager()
{
    close();
}

bool memory_manager::open(std::string_view process_name)
{
    uint64_t pid = find_process_id(process_name);
    if (!pid)
        return false;

    proc_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, static_cast<DWORD>(pid));
    return proc_handle != nullptr;
}

void memory_manager::close()
{
    if (proc_handle)
    {
        CloseHandle(proc_handle);
        proc_handle = nullptr;
    }
}

uint64_t memory_manager::get_module_base(std::string_view module_name) const
{
    if (!proc_handle)
        return 0;

    HMODULE modules[1024]{};
    DWORD bytes_needed = 0;

    if (!EnumProcessModules(proc_handle, modules, sizeof(modules), &bytes_needed))
        return 0;

    int count = bytes_needed / sizeof(HMODULE);
    for (int i = 0; i < count; ++i)
    {
        char buf[MAX_PATH]{};
        if (GetModuleBaseNameA(proc_handle, modules[i], buf, sizeof(buf)))
        {
            if (module_name == buf)
                return reinterpret_cast<uint64_t>(modules[i]);
        }
    }

    return 0;
}

std::string memory_manager::read_raw_string(uint64_t address) const
{
    std::string result;
    result.reserve(128);

    for (int i = 0; i < 200; ++i)
    {
        char c = read<char>(address + i);
        if (c == '\0') break;
        result.push_back(c);
    }

    return result;
}

std::string memory_manager::read_string(uint64_t address) const
{
    // heap ptr at offset 0
    uint64_t len = read<uint64_t>(address + 0x18);
    if (len >= 16u)
        return read_raw_string(read<uint64_t>(address));

    return read_raw_string(address);
}
