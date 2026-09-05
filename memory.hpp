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


using nt_read_fn  = NTSTATUS(WINAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
using nt_write_fn = NTSTATUS(WINAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);

namespace mm
{

    bool open_process(std::string_view process_name);
    void close_process();
    HANDLE get_process_handle(); // can be nullptr

    std::uintptr_t get_module_base(std::string_view module_name);

    std::string read_string(std::uintptr_t address);
    void write_string(std::uintptr_t address, std::string_view value);

    nt_read_fn  get_read_syscall();
    nt_write_fn get_write_syscall();

    template<typename T>

    T read(std::uintptr_t address)
    {
        T val{};
        nt_read_fn read_fn = get_read_syscall();
        read_fn(get_process_handle(), reinterpret_cast<PVOID>(address), &val, sizeof(T), nullptr);
        //
        return {val};
    }

    template<typename T>

    void write(std::uintptr_t address, T value)
    {
        nt_write_fn write_fn = get_write_syscall();
        write_fn(get_process_handle(), reinterpret_cast<PVOID>(address), &value, sizeof(T), nullptr);
    }

} // mm
