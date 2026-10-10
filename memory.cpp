#include "memory.hpp"

#include <stdexcept>
#include <winternl.h>
#include <string>
#include <string_view>
#include <cwchar>
#include <cwctype>
#include <cstring>
#include <algorithm>
#include <vector>

namespace memory
{
    namespace helper
    {
        using nt_open_process_fn = NTSTATUS(WINAPI*)(PHANDLE, ACCESS_MASK, PVOID, PVOID);
        using nt_close_fn = NTSTATUS(WINAPI*)(HANDLE);
        using nt_allocate_virtual_fn = NTSTATUS(WINAPI*)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
        using nt_protect_virtual_fn = NTSTATUS(WINAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);
        using nt_query_virtual_fn = NTSTATUS(WINAPI*)(HANDLE, PVOID, ULONG, PVOID, SIZE_T, PSIZE_T);
        using nt_query_info_process_fn = NTSTATUS(WINAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
        using nt_query_system_info_fn = NTSTATUS(WINAPI*)(ULONG, PVOID, ULONG, PULONG);

        struct object_attributes_t
        {
            ULONG Length;
            HANDLE RootDirectory;
            PVOID ObjectName;
            ULONG Attributes;
            PVOID SecurityDescriptor;
            PVOID SecurityQualityOfService;
        };

        struct string_t
        {
            std::array<unsigned char, 16> buffer;
            std::size_t length;
            std::size_t capacity;
        };

        struct system_process_info_t
        {
            ULONG            NextEntryOffset;
            ULONG            NumberOfThreads;
            LARGE_INTEGER    WorkingSetPrivateSize;
            ULONG            HardFaultCount;
            ULONG            NumberOfThreadsHighWatermark;
            ULONGLONG        CycleTime;
            LARGE_INTEGER    CreateTime;
            LARGE_INTEGER    UserTime;
            LARGE_INTEGER    KernelTime;
            UNICODE_STRING   ImageName;
            KPRIORITY        BasePriority;
            HANDLE           UniqueProcessId;
            HANDLE           InheritedFromUniqueProcessId;
            ULONG            HandleCount;
            ULONG            SessionId;
            ULONG_PTR        UniqueProcessKey;
            SIZE_T           PeakVirtualSize;
            SIZE_T           VirtualSize;
            ULONG            PageFaultCount;
            SIZE_T           PeakWorkingSetSize;
            SIZE_T           WorkingSetSize;
            SIZE_T           QuotaPeakPagedPoolUsage;
            SIZE_T           QuotaPagedPoolUsage;
            SIZE_T           QuotaPeakNonPagedPoolUsage;
            SIZE_T           QuotaNonPagedPoolUsage;
            SIZE_T           PagefileUsage;
            SIZE_T           PeakPagefileUsage;
            SIZE_T           PrivatePageCount;
            LARGE_INTEGER    ReadOperationCount;
            LARGE_INTEGER    WriteOperationCount;
            LARGE_INTEGER    OtherOperationCount;
            LARGE_INTEGER    ReadTransferCount;
            LARGE_INTEGER    WriteTransferCount;
            LARGE_INTEGER    OtherTransferCount;
        };

        HANDLE g_proc_handle = nullptr;
        nt_read_fn g_read_syscall = nullptr;
        nt_write_fn g_write_syscall = nullptr;
        nt_open_process_fn g_open_process_syscall = nullptr;
        nt_close_fn g_close_syscall = nullptr;
        nt_allocate_virtual_fn g_allocate_virtual_syscall = nullptr;
        nt_protect_virtual_fn g_protect_virtual_syscall = nullptr;
        nt_query_virtual_fn g_query_virtual_syscall = nullptr;
        nt_query_info_process_fn g_query_info_process_syscall = nullptr;
        nt_query_system_info_fn g_query_system_info_syscall = nullptr;

        nt_read_fn build_read_syscall()
        {
            static constexpr std::array<std::uint8_t, 11> stub =
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
            static constexpr std::array<std::uint8_t, 11> stub =
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

        nt_open_process_fn build_open_process_syscall()
        {
            static constexpr std::array<std::uint8_t, 11> stub =
            {
                0x4C, 0x8B, 0xD1,
                0xB8, 0x26, 0x00, 0x00, 0x00, // NtOpenProcess
                0x0F, 0x05,
                0xC3
            };

            void* exec = VirtualAlloc(nullptr, stub.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

            if (!exec)
            {
                throw std::runtime_error(std::format("[mm] VirtualAlloc failure @ {}:{}", __FILE__, __LINE__));
            }

            std::memcpy(exec, stub.data(), stub.size());

            return {reinterpret_cast<nt_open_process_fn>(exec)};
        }

        nt_close_fn build_close_syscall()
        {
            static constexpr std::array<std::uint8_t, 11> stub =
            {
                0x4C, 0x8B, 0xD1,
                0xB8, 0x0F, 0x00, 0x00, 0x00, // NtClose
                0x0F, 0x05,
                0xC3
            };

            void* exec = VirtualAlloc(nullptr, stub.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

            if (!exec)
            {
                throw std::runtime_error(std::format("[mm] VirtualAlloc failure @ {}:{}", __FILE__, __LINE__));
            }

            std::memcpy(exec, stub.data(), stub.size());

            return {reinterpret_cast<nt_close_fn>(exec)};
        }

        nt_allocate_virtual_fn build_allocate_virtual_syscall()
        {
            static constexpr std::array<std::uint8_t, 11> stub =
            {
                0x4C, 0x8B, 0xD1,
                0xB8, 0x18, 0x00, 0x00, 0x00, // NtAllocateVirtualMemory
                0x0F, 0x05,
                0xC3
            };

            void* exec = VirtualAlloc(nullptr, stub.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

            if (!exec)
            {
                throw std::runtime_error(std::format("[mm] VirtualAlloc failure @ {}:{}", __FILE__, __LINE__));
            }

            std::memcpy(exec, stub.data(), stub.size());

            return {reinterpret_cast<nt_allocate_virtual_fn>(exec)};
        }

        nt_protect_virtual_fn build_protect_virtual_syscall()
        {
            static constexpr std::array<std::uint8_t, 11> stub =
            {
                0x4C, 0x8B, 0xD1,
                0xB8, 0x50, 0x00, 0x00, 0x00, // NtProtectVirtualMemory
                0x0F, 0x05,
                0xC3
            };

            void* exec = VirtualAlloc(nullptr, stub.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

            if (!exec)
            {
                throw std::runtime_error(std::format("[mm] VirtualAlloc failure @ {}:{}", __FILE__, __LINE__));
            }

            std::memcpy(exec, stub.data(), stub.size());

            return {reinterpret_cast<nt_protect_virtual_fn>(exec)};
        }

        nt_query_virtual_fn build_query_virtual_syscall()
        {
            static constexpr std::array<std::uint8_t, 11> stub =
            {
                0x4C, 0x8B, 0xD1,
                0xB8, 0x23, 0x00, 0x00, 0x00, // NtQueryVirtualMemory
                0x0F, 0x05,
                0xC3
            };

            void* exec = VirtualAlloc(nullptr, stub.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

            if (!exec)
            {
                throw std::runtime_error(std::format("[mm] VirtualAlloc failure @ {}:{}", __FILE__, __LINE__));
            }

            std::memcpy(exec, stub.data(), stub.size());

            return {reinterpret_cast<nt_query_virtual_fn>(exec)};
        }

        nt_query_info_process_fn build_query_info_process_syscall()
        {
            static constexpr std::array<std::uint8_t, 11> stub =
            {
                0x4C, 0x8B, 0xD1,
                0xB8, 0x19, 0x00, 0x00, 0x00, // NtQueryInformationProcess
                0x0F, 0x05,
                0xC3
            };

            void* exec = VirtualAlloc(nullptr, stub.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

            if (!exec)
            {
                throw std::runtime_error(std::format("[mm] VirtualAlloc failure @ {}:{}", __FILE__, __LINE__));
            }

            std::memcpy(exec, stub.data(), stub.size());

            return {reinterpret_cast<nt_query_info_process_fn>(exec)};
        }

        nt_query_system_info_fn build_query_system_info_syscall()
        {
            static constexpr std::array<std::uint8_t, 11> stub =
            {
                0x4C, 0x8B, 0xD1,
                0xB8, 0x36, 0x00, 0x00, 0x00, // NtQuerySystemInformation
                0x0F, 0x05,
                0xC3
            };

            void* exec = VirtualAlloc(nullptr, stub.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

            if (!exec)
            {
                throw std::runtime_error(std::format("[mm] VirtualAlloc failure @ {}:{}", __FILE__, __LINE__));
            }

            std::memcpy(exec, stub.data(), stub.size());

            return {reinterpret_cast<nt_query_system_info_fn>(exec)};
        }

        void init_syscalls() // build all the calls
        {
            g_read_syscall               = build_read_syscall();
            g_write_syscall              = build_write_syscall();
            g_open_process_syscall       = build_open_process_syscall();
            g_close_syscall              = build_close_syscall();
            g_allocate_virtual_syscall   = build_allocate_virtual_syscall();
            g_protect_virtual_syscall    = build_protect_virtual_syscall();
            g_query_virtual_syscall      = build_query_virtual_syscall();
            g_query_info_process_syscall = build_query_info_process_syscall();
            g_query_system_info_syscall  = build_query_system_info_syscall();
        }

        std::uintptr_t get_peb()
        {
            HANDLE h = g_proc_handle;

            if (!h || !g_query_info_process_syscall)
            {
                return {0};
            }

            PROCESS_BASIC_INFORMATION pbi{};
            ULONG return_len = 0;
            NTSTATUS status = g_query_info_process_syscall(h, ProcessBasicInformation, &pbi, sizeof(pbi), &return_len);

            if (!NT_SUCCESS(status))
            {
                return {0};
            }

            return {reinterpret_cast<std::uintptr_t>(pbi.PebBaseAddress)};
        }

        std::uintptr_t find_process_id(std::string_view process_name)
        {
            if (!g_query_system_info_syscall)
            {
                return {0};
            }

            int wide_len = MultiByteToWideChar(CP_UTF8, 0, process_name.data(), static_cast<int>(process_name.size()), nullptr, 0);

            if (wide_len == 0)
            {
                return {0};
            }

            std::wstring target(wide_len, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, process_name.data(), static_cast<int>(process_name.size()), &target[0], wide_len);

            ULONG size = 0x10000;
            std::vector<std::uint8_t> buffer(size);
            ULONG return_len = 0;
            NTSTATUS status = 0;

            while ((status = g_query_system_info_syscall(5, buffer.data(), size, &return_len)) == 0xC0000004) // 5 -> SystemProcessInformation
            {
                size = return_len + 0x1000;
                buffer.resize(size);
            }

            if (!NT_SUCCESS(status))
            {
                return {0};
            }

            std::uint8_t* ptr = buffer.data();

            while (ptr)
            {
                auto* spi = reinterpret_cast<system_process_info_t*>(ptr);

                if (spi->ImageName.Buffer && spi->ImageName.Length > 0)
                {
                    std::wstring name(spi->ImageName.Buffer, spi->ImageName.Length / sizeof(wchar_t));

                    if (_wcsicmp(name.c_str(), target.c_str()) == 0)
                    {
                        return {reinterpret_cast<std::uintptr_t>(spi->UniqueProcessId)};
                    }
                }

                if (spi->NextEntryOffset == 0)
                {
                    break;
                }

                ptr += spi->NextEntryOffset;
            }

            return {0};
        }

    } // namespace helper

    bool open_process(std::string_view process_name)
    {
        close_process();

        try
        {
            helper::init_syscalls();
        }
        catch (const std::runtime_error&)
        {
            close_process();
            return {false};
        }

        const std::uintptr_t pid = helper::find_process_id(process_name);

        if (!pid)
        {
            close_process();
            //
            return {false};
        }

        helper::object_attributes_t oa{};
        oa.Length = sizeof(helper::object_attributes_t);

        CLIENT_ID cid{};
        cid.UniqueProcess = reinterpret_cast<HANDLE>(pid);
        cid.UniqueThread  = nullptr;

        HANDLE handle = nullptr;
        NTSTATUS status = helper::g_open_process_syscall(&handle, PROCESS_ALL_ACCESS, &oa, &cid);

        if (!NT_SUCCESS(status) || !handle)
        {
            close_process();
            return {false};
        }

        helper::g_proc_handle = handle;

        return {true};
    }

    void close_process()
    {
        if (helper::g_proc_handle && helper::g_close_syscall)
        {
            helper::g_close_syscall(helper::g_proc_handle);
            helper::g_proc_handle = nullptr;
        }

        if (helper::g_read_syscall)
        {
            VirtualFree(reinterpret_cast<LPVOID>(helper::g_read_syscall), 0, MEM_RELEASE);
            helper::g_read_syscall = nullptr;
        }

        if (helper::g_write_syscall)
        {
            VirtualFree(reinterpret_cast<LPVOID>(helper::g_write_syscall), 0, MEM_RELEASE);
            helper::g_write_syscall = nullptr;
        }

        if (helper::g_open_process_syscall)
        {
            VirtualFree(reinterpret_cast<LPVOID>(helper::g_open_process_syscall), 0, MEM_RELEASE);
            helper::g_open_process_syscall = nullptr;
        }

        if (helper::g_close_syscall)
        {
            VirtualFree(reinterpret_cast<LPVOID>(helper::g_close_syscall), 0, MEM_RELEASE);
            helper::g_close_syscall = nullptr;
        }

        if (helper::g_allocate_virtual_syscall)
        {
            VirtualFree(reinterpret_cast<LPVOID>(helper::g_allocate_virtual_syscall), 0, MEM_RELEASE);
            helper::g_allocate_virtual_syscall = nullptr;
        }

        if (helper::g_protect_virtual_syscall)
        {
            VirtualFree(reinterpret_cast<LPVOID>(helper::g_protect_virtual_syscall), 0, MEM_RELEASE);
            helper::g_protect_virtual_syscall = nullptr;
        }

        if (helper::g_query_virtual_syscall)
        {
            VirtualFree(reinterpret_cast<LPVOID>(helper::g_query_virtual_syscall), 0, MEM_RELEASE);
            helper::g_query_virtual_syscall = nullptr;
        }

        if (helper::g_query_info_process_syscall)
        {
            VirtualFree(reinterpret_cast<LPVOID>(helper::g_query_info_process_syscall), 0, MEM_RELEASE);
            helper::g_query_info_process_syscall = nullptr;
        }

        if (helper::g_query_system_info_syscall)
        {
            VirtualFree(reinterpret_cast<LPVOID>(helper::g_query_system_info_syscall), 0, MEM_RELEASE);
            helper::g_query_system_info_syscall = nullptr;
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
        if (!helper::g_proc_handle || !helper::g_read_syscall)
        {
            return {0};
        }

        std::uintptr_t peb = helper::get_peb();

        if (!peb)
        {
            return {0};
        }

        std::uintptr_t ldr = read<std::uintptr_t>(peb + 0x18);

        if (!ldr)
        {
            return {0};
        }

        std::uintptr_t list_head = ldr + 0x10; // Ldr -> InMemoryOrderModuleList
        std::uintptr_t flink = read<std::uintptr_t>(list_head);

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
            std::uintptr_t dll_base = read<std::uintptr_t>(current + 0x30);     // DR_DATA_TABLE_ENTRY -> BaseDll
            USHORT name_length = read<USHORT>(current + 0x58);                  // -> BaseDllName.Length
            std::uintptr_t name_buffer = read<std::uintptr_t>(current + 0x60);  // ->BaseDllName.Buffer

            if (name_buffer && name_length > 0)
            {
                const std::size_t max_chars = 255;
                std::size_t wchar_count = name_length / sizeof(wchar_t);

                if (wchar_count > max_chars)
                {
                    wchar_count = max_chars;
                }

                std::wstring module_name_wide(wchar_count, L'\0');
                SIZE_T bytes_read = 0;
                NTSTATUS status = helper::g_read_syscall(helper::g_proc_handle, reinterpret_cast<PVOID>(name_buffer), &module_name_wide[0], wchar_count * sizeof(wchar_t), &bytes_read);

                if (NT_SUCCESS(status))
                {
                    if (_wcsicmp(module_name_wide.c_str(), wide_name.c_str()) == 0)
                    {
                        return {dll_base};
                    }
                }
            }

            current = read<std::uintptr_t>(current);
        }

        return {0};
    }

    void* allocate(std::uintptr_t address, std::size_t size, unsigned long type, unsigned long new_protect)
    {
        if (!helper::g_proc_handle || !helper::g_allocate_virtual_syscall)
        {
            return {nullptr};
        }
    
        PVOID base = reinterpret_cast<PVOID>(address);
        SIZE_T region_size = size;
    
        NTSTATUS status = helper::g_allocate_virtual_syscall(helper::g_proc_handle, &base, 0, &region_size, type, new_protect);
    
        if (!NT_SUCCESS(status))
        {
            return {nullptr};
        }
    
        return {base};
    }

    bool protect(std::uintptr_t address, std::size_t size, unsigned long new_protect, unsigned long* old_protect)
    {
        if (!helper::g_proc_handle || !helper::g_protect_virtual_syscall)
        {
            return {false};
        }

        PVOID base = reinterpret_cast<PVOID>(address);
        SIZE_T region_size = size;
        ULONG old = 0;

        NTSTATUS status = helper::g_protect_virtual_syscall(helper::g_proc_handle, &base, &region_size, new_protect, &old);

        if (!NT_SUCCESS(status))
        {
            return {false};
        }

        if (old_protect)
        {
            *old_protect = old;
        }

        return {true};
    }

    bool query(std::uintptr_t address, MEMORY_BASIC_INFORMATION& out)
    {
        if (!helper::g_proc_handle || !helper::g_query_virtual_syscall)
        {
            return {false};
        }

        SIZE_T return_len = 0;
        NTSTATUS status = helper::g_query_virtual_syscall(helper::g_proc_handle, reinterpret_cast<PVOID>(address), 0, &out, sizeof(out), &return_len);

        return {NT_SUCCESS(status)};
    }

    std::string read_string(std::uintptr_t address)
    {
        if (!helper::g_proc_handle || !helper::g_read_syscall)
        {
            return {};
        }

        string_t str = read<string_t>(address);

        if (str.length == 0)
        {
            return {};
        }

        std::uintptr_t data_ptr;

        if (str.capacity < 16)
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

        SIZE_T bytes_read = 0;
        NTSTATUS status = helper::g_read_syscall(helper::g_proc_handle, reinterpret_cast<PVOID>(data_ptr), result.data(), str.length, &bytes_read);

        if (!NT_SUCCESS(status))
        {
            return {};
        }

        return {result};
    }

    void write_string(std::uintptr_t address, std::string_view value)
    {
        if (!helper::g_proc_handle || !helper::g_write_syscall)
        {
            return;
        }

        string_t str = read<string_t>(address);

        if (str.capacity == 0)
        {
            return;
        }

        std::uintptr_t target_ptr;

        if (str.capacity < 16)
        {
            target_ptr = address;
        }
        else
        {
            std::memcpy(&target_ptr, str.buffer.data(), sizeof(target_ptr));

            if (!target_ptr)
            {
                return;
            }
        }

        std::size_t write_len = std::min(value.size(), str.capacity);

        if (write_len > 0)
        {
            SIZE_T bytes_written = 0;
            helper::g_write_syscall(helper::g_proc_handle, reinterpret_cast<PVOID>(target_ptr), reinterpret_cast<PVOID>(const_cast<char*>(value.data())), write_len, &bytes_written);
        }

        char null_char = '\0';
        helper::g_write_syscall(helper::g_proc_handle, reinterpret_cast<PVOID>(target_ptr + write_len), &null_char, 1, nullptr);

        write<std::size_t>(address + 0x10, write_len);
    }

} // namespace memory
