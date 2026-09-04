/*

	MIT License

	Copyright (c) 2026 0x340

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.


*/




#pragma once

#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <string>
#include <cstdint>
#include <array>
#include <memory>
#include <format>

using nt_read_fn  = NTSTATUS(WINAPI*)(HANDLE, PVOID, PVOID, ULONG, PULONG);
using nt_write_fn = NTSTATUS(WINAPI*)(HANDLE, PVOID, PVOID, ULONG, PULONG);

namespace mm
{
    namespace internal
    {
        nt_read_fn build_read_syscall();
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

                // helper fn
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
                //
            private:
                nt_read_fn  m_read  = nullptr;
                nt_write_fn m_write = nullptr;
                //
                std::string read_raw_string(std::uintptr_t address) const;
            };
        }
        //
    extern std::shared_ptr<internal::memory_manager> g_mm;
}
