#include "memory.hpp"

#include <stdexcept>
#include <winternl.h>
#include <psapi.h>
#include <string>
#include <cwctype>
#include <cstring>
#include <algorithm>
#include <vector>

namespace mm
{
    namespace helper
    {
        HANDLE g_proc_handle = nullptr;
        nt_read_fn  g_read_syscall  = nullptr;
        nt_write_fn g_write_syscall = nullptr;

        nt_read_fn build_read_syscall()
        {
            static constexpr std::array<uint8_t, 11> stub =
            {
                0x4C, 0x8B, 0xD1,
                0xB8, 0x3F, 0x00, 0x00, 0x00, // NtReadVirtualMemory
                0x0F, 0x05,
                0xC3
            };

            void* exec = VirtualAlloc(nullptr, stub.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

            if (!exec)
            {
                throw std::runtime_error(std::format("[mm] VirtualAlloc failure @ {}:{}", __FILE__, __LINE__));
            }

            std::memcpy(exec, stub.data(), stub.size());

            return {reinterpret_cast<nt_read_fn>(exec)};
        }

        nt_write_fn build_write_syscall()
        {
            static constexpr std::array<uint8_t, 11> stub =
            {
                0x4C, 0x8B, 0xD1,
                0xB8, 0x3A, 0x00, 0x00, 0x00, // NtWriteVirtualMemory
                0x0F, 0x05,
                0xC3
            };

            void* exec = VirtualAlloc(nullptr, stub.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

            if (!exec)
            {
                throw std::runtime_error(std::format("[mm] VirtualAlloc failure @ {}:{}", __FILE__, __LINE__));
            }

            std::memcpy(exec, stub.data(), stub.size());

            return {reinterpret_cast<nt_write_fn>(exec)};
        }

        std::uintptr_t get_peb()
        {
            HANDLE h = g_proc_handle;

            if (!h)
            {
                return {0};
            }

            PROCESS_BASIC_INFORMATION pbi{};
            ULONG return_len = 0;
            NTSTATUS status = NtQueryInformationProcess(h, ProcessBasicInformation, &pbi, sizeof(pbi), &return_len);

            if (!NT_SUCCESS(status))
            {
                return {0};
            }

            return {reinterpret_cast<std::uintptr_t>(pbi.PebBaseAddress)};
        }

        std::uintptr_t find_process_id(std::string_view process_name)
        {
            PROCESSENTRY32 entry{};
            entry.dwSize = sizeof(PROCESSENTRY32);

            const std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&CloseHandle)> snap{ CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0), CloseHandle};

            if (snap.get() == INVALID_HANDLE_VALUE)
            {
                return {0};
            }

            if (Process32First(snap.get(), &entry))
            {
                do
                {
                    if (_stricmp(entry.szExeFile, process_name.data()) == 0)
                    {
                        return {entry.th32ProcessID};
                    }
                }
                while (Process32Next(snap.get(), &entry));
            }

            return {0};
        }
    } // helper

    bool open_process(std::string_view process_name)
    {
        close_process();

        const std::uintptr_t pid = helper::find_process_id(process_name);

        if (!pid)
        {
            return {false};
        }

        helper::g_proc_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, static_cast<DWORD>(pid));

        return {helper::g_proc_handle != nullptr};
    }

    void close_process()
    {
        if (helper::g_proc_handle)
        {
            CloseHandle(helper::g_proc_handle);
            helper::g_proc_handle = nullptr;
        }
    }

    HANDLE get_process_handle()
    {
        return helper::g_proc_handle;
    }

    nt_read_fn get_read_syscall()
    {
        return {helper::g_read_syscall};
    }

    nt_write_fn get_write_syscall()
    {
        return {helper::g_write_syscall};
    }

    std::uintptr_t get_module_base(std::string_view module_name)
    {
        HANDLE h = get_process_handle();

        if (!h)
        {
            return {0};
        }

        std::uintptr_t peb = helper::get_peb();

        if (!peb)
        {
            return {0};
        }

        std::uintptr_t ldr = 0;

        if (!read<std::uintptr_t>(peb + 0x18, ldr))
        {
            return {0};
        }

        if (!ldr)
        {
            return {0};
        }

        std::uintptr_t list_head = ldr + 0x10;
        std::uintptr_t flink = 0;

        if (!read<std::uintptr_t>(list_head, flink))
        {
            return {0};
        }

        if (!flink)
        {
            return {0};
        }

        int wide_len = MultiByteToWideChar(CP_UTF8, 0, module_name.data(), static_cast<int>(module_name.size()), nullptr, 0);

        if (wide_len == 0)
        {
            return {0};
        }

        std::wstring wide_name(wide_len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, module_name.data(), static_cast<int>(module_name.size()), &wide_name[0], wide_len);

        std::uintptr_t current = flink;

        while (current && current != list_head)
        {
            std::uintptr_t dll_base = 0;
            if (!read<std::uintptr_t>(current + 0x30, dll_base))
            {
                break;
            }

            USHORT name_length = 0;

            if (!read<USHORT>(current + 0x58, name_length))
            {
                break;
            }

            std::uintptr_t name_buffer = 0;

            if (!read<std::uintptr_t>(current + 0x60, name_buffer))
            {
                break;
            }

            if (name_buffer && name_length > 0)
            {
                const size_t max_chars = 255;
                size_t wchar_count = name_length / sizeof(wchar_t);

                if (wchar_count > max_chars)
                {
                    wchar_count = max_chars;
                }

                std::wstring module_name_wide(wchar_count, L'\0');
                SIZE_T bytes_read = 0;

                if (ReadProcessMemory(h, reinterpret_cast<LPCVOID>(name_buffer), &module_name_wide[0], wchar_count * sizeof(wchar_t), &bytes_read))
                {
                    if (_wcsicmp(module_name_wide.c_str(), wide_name.c_str()) == 0)
                    {
                        return {dll_base};
                    }
                }
            }

            if (!read<std::uintptr_t>(current, current))
            {
                break;
            }
        }

        return {0};
    }

    std::string read_string(std::uintptr_t address)
    {
        struct string_t
        {
            std::array<unsigned char, 16> buffer;
            std::size_t size;
            std::size_t length;
        };

        string_t str = read<string_t>(address);

        if (str.length == 0)
        {
            return {};
        }

        std::uintptr_t data_ptr;

        if (str.length < 16)
        {
            data_ptr = address;
        }
        else
        {
            std::memcpy(&data_ptr, str.buffer.data(), sizeof(data_ptr));

            if (!data_ptr)
            {
                return {};
            }
        }

        std::string result;
        result.resize(str.length);

        if (!ReadProcessMemory(get_process_handle(), (LPCVOID)data_ptr, result.data(), str.length, nullptr))
        {
            return {};
        }

        return result;
    }

    void write_string(std::uintptr_t address, std::string_view value)
    {
        std::size_t length   = read<std::size_t>(address + 0x18);
        std::size_t capacity = read<std::size_t>(address + 0x10);

        if (capacity == 0)
        {
            return;
        }

        bool is_heap = (length >= 0x10);
        std::uintptr_t target_ptr;

        if (is_heap)
        {
            target_ptr = read<std::uintptr_t>(address);
        }
        else
        {
            target_ptr = address;
        }

        if (!target_ptr)
        {
            return;
        }

        std::size_t write_len = std::min(value.size(), capacity - 1);

        if (write_len > 0)
        {
            WriteProcessMemory(get_process_handle(), (LPVOID)target_ptr, value.data(), write_len, nullptr);
        }

        char null_char = '\0';
        WriteProcessMemory(get_process_handle(), (LPVOID)(target_ptr + write_len), &null_char, 1, nullptr);

        write<std::size_t>(address + 0x18, write_len);
    }

} // mm
