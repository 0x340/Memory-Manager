#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <string>
#include <cstdint>

using nt_read_fn  = NTSTATUS(WINAPI*)(HANDLE, PVOID, PVOID, ULONG, PULONG);
using nt_write_fn = NTSTATUS(WINAPI*)(HANDLE, PVOID, PVOID, ULONG, PULONG);

nt_read_fn  build_read_syscall();
nt_write_fn build_write_syscall();

uint64_t find_process_id(std::string_view process_name);

class memory_manager
{
public:
    HANDLE proc_handle = nullptr;

    memory_manager();
    ~memory_manager();

    bool open(std::string_view process_name);
    void close();

    uint64_t get_module_base(std::string_view module_name) const;

    template<typename T>
    T read(uintptr_t address) const
    {
        T val{};
        m_read(proc_handle, reinterpret_cast<PVOID>(address), &val, sizeof(T), nullptr);
        return val;
    }

    template<typename T>
    void write(uintptr_t address, T value) const
    {
        m_write(proc_handle, reinterpret_cast<PVOID>(address), &value, sizeof(T), nullptr);
    }

    std::string read_string(uintptr_t address) const;

private:
    nt_read_fn  m_read  = nullptr;
    nt_write_fn m_write = nullptr;

    std::string read_raw_string(uintptr_t address) const;
};
