#include "memory.hpp"
#include <stdexcept>

nt_read_fn mm::internal::build_read_syscall( )
    {
    static constexpr std::array<uint8_t, 11> stub = {
        0x4C, 0x8B, 0xD1,
        0xB8, 0x3F, 0x00, 0x00, 0x00,
        0x0F, 0x05,
        0xC3
        };

    void* exec = VirtualAlloc( nullptr, stub.size( ), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
    if ( !exec )
        throw std::runtime_error( std::format( "[mm] VirtualAlloc failure @ {}:{}", __FILE__, __LINE__ ) );

    std::memcpy( exec, stub.data( ), stub.size( ) );
    return reinterpret_cast<nt_read_fn>( exec );
    }

nt_write_fn mm::internal::build_write_syscall( )
    {
    static constexpr std::array<uint8_t, 11> stub = {
        0x4C, 0x8B, 0xD1,
        0xB8, 0x3A, 0x00, 0x00, 0x00,
        0x0F, 0x05,
        0xC3
        };

    void* exec = VirtualAlloc( nullptr, stub.size( ), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
    if ( !exec )
        throw std::runtime_error( std::format( "[mm] VirtualAlloc failure @ {}:{}", __FILE__, __LINE__ ) );

    std::memcpy( exec, stub.data( ), stub.size( ) );
    return reinterpret_cast<nt_write_fn>( exec );
    }

std::uintptr_t mm::internal::find_process_id( std::string_view process_name )
    {
    PROCESSENTRY32 entry { .dwSize = sizeof( PROCESSENTRY32 ) };

    const std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype( &CloseHandle )> snap {
        CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 ), CloseHandle
        };

    if ( !Process32First( snap.get( ), &entry ) )
        return 0;

    while ( Process32Next( snap.get( ), &entry ) )
        {
        if ( process_name == entry.szExeFile )
            return entry.th32ProcessID;
        }

    return 0;
    }

mm::internal::memory_manager::memory_manager( )
    {
    m_read  = build_read_syscall( );
    m_write = build_write_syscall( );
    }

mm::internal::memory_manager::~memory_manager( )
    {
    close( );
    }

bool mm::internal::memory_manager::open( std::string_view process_name )
    {
    const std::uintptr_t pid = find_process_id( process_name );
    if ( !pid ) return false;

    proc_handle = OpenProcess( PROCESS_ALL_ACCESS, FALSE, static_cast<DWORD>( pid ) );
    return proc_handle != nullptr;
    }

void mm::internal::memory_manager::close( )
    {
    if ( !proc_handle ) return;
    CloseHandle( proc_handle );
    proc_handle = nullptr;
    }

std::uintptr_t mm::internal::memory_manager::get_module_base( std::string_view module_name ) const
    {
    if ( !proc_handle ) return 0;

    std::array<HMODULE, 1024> modules {};
    DWORD bytes_needed = 0;

    if ( !EnumProcessModules( proc_handle, modules.data( ), sizeof( modules ), &bytes_needed ) )
        return 0;

    const std::size_t count = bytes_needed / sizeof( HMODULE );

    const auto it = std::find_if( modules.begin( ), modules.begin( ) + count, [&]( HMODULE mod )
        {
        std::array<char, MAX_PATH> buf {};
        return GetModuleBaseNameA( proc_handle, mod, buf.data( ), buf.size( ) ) && module_name == buf.data( );
        } );

    return it != modules.begin( ) + count ? reinterpret_cast<std::uintptr_t>( *it ) : 0;
    }

std::string mm::internal::memory_manager::read_raw_string( std::uintptr_t address ) const
    {
    std::string result {};
    result.reserve( 128 );

    for ( int i = 0; i < 200; ++i )
        {
        const char c = read<char>( address + i );
        if ( c == '\0' ) break;
        result += c;
        }

    return result;
    }

std::string mm::internal::memory_manager::read_string( std::uintptr_t address ) const
    {
    const bool is_heap = read<std::uintptr_t>( address + 0x18 ) >= 16u;
    return read_raw_string( is_heap ? read<std::uintptr_t>( address ) : address );
    }

void mm::internal::memory_manager::write_string( std::uintptr_t address, std::string_view value ) const
    {
    const bool is_heap             = read<std::uintptr_t>( address + 0x18 ) >= 16u;
    const std::uintptr_t target    = is_heap ? read<std::uintptr_t>( address ) : address;

    for ( std::size_t i = 0; i < value.size( ); ++i )
        write<char>( target + i, value[ i ] );

    write<char>( target + value.size( ), '\0' );
    }

HANDLE mm::internal::memory_manager::get_handle( )
    {
    return proc_handle;
    }

std::shared_ptr<mm::internal::memory_manager> mm::g_mm = std::make_shared<mm::internal::memory_manager>( );
