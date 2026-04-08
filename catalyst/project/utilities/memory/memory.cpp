#include <stdafx.hpp>

namespace detail {

	static constexpr std::size_t k_chunk_size{ 0x10000 };

	struct scan_result
	{
		std::uintptr_t m_address{};
		bool m_found{};
	};

	struct section_info
	{
		std::uintptr_t m_base{};
		std::size_t m_size{};
		std::uint32_t m_characteristics{};
		char m_name[ 9 ]{};
	};

	struct pattern_byte
	{
		std::uint8_t m_value{};
		bool m_wildcard{};
	};

	template <typename F>
	static scan_result scan_chunked( const memory& mem, std::uintptr_t base, std::size_t size, std::size_t overlap, F&& matcher )
	{
		std::vector<std::uint8_t> buffer( k_chunk_size + overlap );

		for ( std::size_t offset = 0; offset < size; offset += k_chunk_size )
		{
			const auto read_size = std::min( k_chunk_size + overlap, size - offset );
			if ( !mem.read( base + offset, buffer.data( ), read_size ) )
			{
				continue;
			}

			const auto match_offset = matcher( buffer.data( ), read_size );
			if ( match_offset != std::size_t( -1 ) )
			{
				return { base + offset + match_offset, true };
			}
		}

		return {};
	}

	static std::vector<section_info> get_sections( const memory& mem, std::uintptr_t module_base )
	{
		std::vector<section_info> sections;

		const auto dos = mem.read<IMAGE_DOS_HEADER>( module_base );
		if ( dos.e_magic != IMAGE_DOS_SIGNATURE )
		{
			return sections;
		}

		const auto nt = mem.read<IMAGE_NT_HEADERS>( module_base + dos.e_lfanew );
		if ( nt.Signature != IMAGE_NT_SIGNATURE )
		{
			return sections;
		}

		const auto first = module_base + dos.e_lfanew + offsetof( IMAGE_NT_HEADERS, OptionalHeader ) + nt.FileHeader.SizeOfOptionalHeader;

		sections.reserve( nt.FileHeader.NumberOfSections );

		for ( std::uint16_t i = 0; i < nt.FileHeader.NumberOfSections; ++i )
		{
			const auto hdr = mem.read<IMAGE_SECTION_HEADER>( first + i * sizeof( IMAGE_SECTION_HEADER ) );

			auto& sec = sections.emplace_back( );
			sec.m_base = module_base + hdr.VirtualAddress;
			sec.m_size = hdr.Misc.VirtualSize;
			sec.m_characteristics = hdr.Characteristics;
			std::memcpy( sec.m_name, hdr.Name, 8 );
			sec.m_name[ 8 ] = '\0';
		}

		return sections;
	}

	static std::vector<pattern_byte> parse_pattern( std::string_view pattern )
	{
		std::vector<pattern_byte> result;
		result.reserve( 64 );

		auto hex = [ ]( char c ) -> int
			{
				if ( c >= '0' && c <= '9' ) return c - '0';
				if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
				if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
				return -1;
			};

		for ( std::size_t i = 0; i < pattern.size( ); )
		{
			if ( pattern[ i ] == ' ' || pattern[ i ] == '\t' )
			{
				++i;
				continue;
			}

			if ( pattern[ i ] == '?' )
			{
				result.push_back( { 0, true } );
				i += ( i + 1 < pattern.size( ) && pattern[ static_cast< std::size_t >( i ) + 1 ] == '?' ) ? 2 : 1;
				continue;
			}

			const auto high = hex( pattern[ i++ ] );
			if ( high < 0 )
			{
				continue;
			}

			const auto low = ( i < pattern.size( ) ) ? hex( pattern[ i ] ) : -1;
			if ( low >= 0 )
			{
				result.push_back( { static_cast< std::uint8_t >( ( high << 4 ) | low ), false } );
				++i;
			}
			else
			{
				result.push_back( { static_cast< std::uint8_t >( high ), false } );
			}
		}

		return result;
	}

} // namespace detail

bool memory::initialize( std::wstring_view process_name )
{
	const auto snap = ::CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	if ( snap == INVALID_HANDLE_VALUE )
	{
		return false;
	}

	PROCESSENTRY32W entry{ .dwSize = sizeof( PROCESSENTRY32W ) };
	if ( ::Process32FirstW( snap, &entry ) )
	{
		do
		{
			if ( ::lstrcmpiW( entry.szExeFile, process_name.data( ) ) == 0 )
			{
				this->m_id = entry.th32ProcessID;
				break;
			}
		} while ( ::Process32NextW( snap, &entry ) );
	}

	::CloseHandle( snap );

	if ( !this->m_id )
	{
		return false;
	}

	this->m_handle = ::OpenProcess( PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, FALSE, this->m_id );
	if ( !this->m_handle )
	{
		return false;
	}

	const auto mod_snap = ::CreateToolhelp32Snapshot( TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, this->m_id );
	if ( mod_snap != INVALID_HANDLE_VALUE )
	{
		MODULEENTRY32W me{ .dwSize = sizeof( MODULEENTRY32W ) };
		if ( ::Module32FirstW( mod_snap, &me ) )
		{
			this->m_base = reinterpret_cast< std::uintptr_t >( me.modBaseAddr );
		}

		::CloseHandle( mod_snap );
	}

	g::console.print( "attached to process." );

	return true;
}

bool memory::read( std::uintptr_t address, void* buffer, std::size_t size ) const
{
	if ( !this->m_handle || !buffer || !size ) [[unlikely]]
	{
		return false;
	}

	std::size_t bytes{ 0 };
	return ::ReadProcessMemory( this->m_handle, reinterpret_cast< const void* >( address ), buffer, size, &bytes ) && bytes == size;
}

bool memory::write( std::uintptr_t address, const void* buffer, std::size_t size ) const
{
	if ( !this->m_handle || !buffer || !size ) [[unlikely]]
	{
		return false;
	}

	std::size_t bytes{ 0 };
	return ::WriteProcessMemory( this->m_handle, reinterpret_cast< void* >( address ), buffer, size, &bytes ) && bytes == size;
}

std::uintptr_t memory::get_module( std::string_view name ) const
{
	const auto snap = ::CreateToolhelp32Snapshot( TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, this->m_id );
	if ( snap == INVALID_HANDLE_VALUE )
	{
		return 0;
	}

	wchar_t wide[ MAX_PATH + 1 ];
	const auto len = ::MultiByteToWideChar( CP_UTF8, 0, name.data( ), static_cast< int >( name.size( ) ), wide, MAX_PATH );

	if ( len <= 0 )
	{
		::CloseHandle( snap );
		return 0;
	}

	wide[ len ] = L'\0';

	std::uintptr_t result{ 0 };
	MODULEENTRY32W entry{ .dwSize = sizeof( MODULEENTRY32W ) };

	if ( ::Module32FirstW( snap, &entry ) )
	{
		do
		{
			if ( ::lstrcmpW( entry.szModule, wide ) == 0 )
			{
				result = reinterpret_cast< std::uintptr_t >( entry.modBaseAddr );
				break;
			}
		} while ( ::Module32NextW( snap, &entry ) );
	}

	::CloseHandle( snap );
	return result;
}

std::size_t memory::get_module_size( std::uintptr_t module_base ) const
{
	const auto snap = ::CreateToolhelp32Snapshot( TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, this->m_id );
	if ( snap == INVALID_HANDLE_VALUE )
	{
		return 0;
	}

	std::size_t result{ 0 };
	MODULEENTRY32W entry{ .dwSize = sizeof( MODULEENTRY32W ) };

	if ( ::Module32FirstW( snap, &entry ) )
	{
		do
		{
			if ( reinterpret_cast< std::uintptr_t >( entry.modBaseAddr ) == module_base )
			{
				result = entry.modBaseSize;
				break;
			}
		} while ( ::Module32NextW( snap, &entry ) );
	}

	::CloseHandle( snap );
	return result;
}

std::string memory::read_string( std::uintptr_t address, std::size_t max_len ) const
{
	char buffer[ 256 ]{};
	const auto len = std::min( max_len, sizeof( buffer ) - 1 );
	this->read( address, buffer, len );
	buffer[ len ] = '\0';
	return buffer;
}

std::uintptr_t memory::find_pattern( std::uintptr_t module_base, std::string_view pattern ) const
{
	if ( !module_base || pattern.empty( ) )
	{
		return 0;
	}

	const auto parsed = detail::parse_pattern( pattern );
	if ( parsed.empty( ) )
	{
		return 0;
	}

	const auto module_size = this->get_module_size( module_base );
	if ( !module_size )
	{
		return 0;
	}

	const auto result = detail::scan_chunked( *this, module_base, module_size, parsed.size( ), [ & ]( const std::uint8_t* data, std::size_t size ) -> std::size_t
		{
			for ( std::size_t i = 0; i + parsed.size( ) <= size; ++i )
			{
				auto match{ true };

				for ( std::size_t j = 0; j < parsed.size( ); ++j )
				{
					if ( !parsed[ j ].m_wildcard && data[ i + j ] != parsed[ j ].m_value )
					{
						match = false;
						break;
					}
				}

				if ( match )
				{
					return i;
				}
			}

			return std::size_t( -1 );
		} );

	return result.m_address;
}

std::uintptr_t memory::find_qword_in_sections( std::uintptr_t module_base, std::uintptr_t value, std::uint32_t section_filter ) const
{
	for ( const auto& sec : detail::get_sections( *this, module_base ) )
	{
		if ( !( sec.m_characteristics & section_filter ) || sec.m_size < 8 )
		{
			continue;
		}

		const auto result = detail::scan_chunked( *this, sec.m_base, sec.m_size, 8, [ & ]( const std::uint8_t* data, std::size_t size ) -> std::size_t
			{
				for ( std::size_t i = 0; i + 8 <= size; i += 8 )
				{
					if ( *reinterpret_cast< const std::uintptr_t* >( data + i ) == value )
					{
						return i;
					}
				}

				return std::size_t( -1 );
			} );

		if ( result.m_found )
		{
			return result.m_address;
		}
	}

	return 0;
}

std::uintptr_t memory::find_vtable( std::uintptr_t module_base, std::string_view class_name ) const
{
	const auto descriptor_name = ".?AV" + std::string( class_name ) + "@@";
	std::uintptr_t type_descriptor{ 0 };

	for ( const auto& sec : detail::get_sections( *this, module_base ) )
	{
		constexpr auto required = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;
		if ( ( sec.m_characteristics & required ) != required || sec.m_size <= descriptor_name.size( ) )
		{
			continue;
		}

		const auto result = detail::scan_chunked( *this, sec.m_base, sec.m_size, descriptor_name.size( ), [ & ]( const std::uint8_t* data, std::size_t size ) -> std::size_t
			{
				for ( std::size_t i = 0; i + descriptor_name.size( ) < size; ++i )
				{
					if ( std::memcmp( data + i, descriptor_name.data( ), descriptor_name.size( ) + 1 ) == 0 )
					{
						return i;
					}
				}

				return std::size_t( -1 );
			} );

		if ( result.m_found )
		{
			type_descriptor = result.m_address - 0x10;
			break;
		}
	}

	if ( !type_descriptor )
	{
		return 0;
	}

	const auto descriptor_rva = static_cast< std::uint32_t >( type_descriptor - module_base );
	std::uintptr_t col_address{ 0 };

	for ( const auto& sec : detail::get_sections( *this, module_base ) )
	{
		if ( std::string_view{ sec.m_name }.find( ".rdata" ) == std::string_view::npos || sec.m_size < 0x30 )
		{
			continue;
		}

		const auto result = detail::scan_chunked( *this, sec.m_base, sec.m_size, 0x30, [ & ]( const std::uint8_t* data, std::size_t size ) -> std::size_t
			{
				for ( std::size_t i = 0; i + 0x30 <= size; i += 8 )
				{
					if ( reinterpret_cast< const std::uint32_t* >( data + i )[ 3 ] == descriptor_rva )
					{
						return i;
					}
				}

				return std::size_t( -1 );
			} );

		if ( result.m_found )
		{
			col_address = result.m_address;
			break;
		}
	}

	if ( !col_address )
	{
		return 0;
	}

	const auto col_ref = this->find_qword_in_sections( module_base, col_address, IMAGE_SCN_MEM_READ );
	return col_ref ? col_ref + 8 : 0;
}

std::uintptr_t memory::find_vtable_instance( std::uintptr_t module_base, std::string_view class_name ) const
{
	const auto vtable = this->find_vtable( module_base, class_name );
	if ( !vtable )
	{
		return 0;
	}

	return this->find_qword_in_sections( module_base, vtable, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE );
}
std::uintptr_t memory::allocate( std::uintptr_t address, std::size_t size, std::uint32_t protection ) const
{
	if ( !this->m_handle )
	{
		return 0;
	}

	return reinterpret_cast< std::uintptr_t >( ::VirtualAllocEx( this->m_handle, reinterpret_cast< void* >( address ), size, MEM_COMMIT | MEM_RESERVE, protection ) );
}

bool memory::free( std::uintptr_t address ) const
{
	if ( !this->m_handle || !address )
	{
		return false;
	}

	return ::VirtualFreeEx( this->m_handle, reinterpret_cast< void* >( address ), 0, MEM_RELEASE ) != 0;
}
