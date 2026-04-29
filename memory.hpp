#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <string>
#include <cstdint>
#include <array>
#include <memory>
#include <format>
//
using nt_read_fn  = NTSTATUS(WINAPI*)(HANDLE, PVOID, PVOID, ULONG, PULONG);
using nt_write_fn = NTSTATUS(WINAPI*)(HANDLE, PVOID, PVOID, ULONG, PULONG);
//
namespace mm
{
    namespace internal
    {
        nt_read_fn  build_read_syscall();
        nt_write_fn build_write_syscall();
        //
        std::uintptr_t find_process_id(std::string_view process_name);
        //
        class memory_manager
        {
            public:
                HANDLE proc_handle = nullptr;
                //
                memory_manager();
                ~memory_manager();

                //helper fn
                HANDLE get_handle();
                bool open(std::string_view process_name);
                void close();
                //
                std::uintptr_t get_module_base(std::string_view module_name) const;
                std::string read_string(std::uintptr_t address) const;
                void write_string(std::uintptr_t address, std::string_view value) const;
                //
                template<typename T>
                //
                T read(std::uintptr_t address) const
                {
                    T val {};
                    m_read(proc_handle, reinterpret_cast<PVOID>(address), &val, sizeof(T), nullptr);
                    //
                    return val;
                }
                //
                template<typename T>
                void write(std::uintptr_t address, T value) const
                {
                    m_write(proc_handle, reinterpret_cast<PVOID>(address), &value, sizeof(T), nullptr);
                }

            private:
                nt_read_fn  m_read  = nullptr;
                nt_write_fn m_write = nullptr;
                //
                std::string read_raw_string(std::uintptr_t address) const;
            };
        }
    extern std::shared_ptr<internal::memory_manager> g_mm;
}
