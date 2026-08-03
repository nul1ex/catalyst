#include <stdafx.hpp>

namespace systems {

	std::uintptr_t convars::find( std::uint32_t name_hash )
	{
		static const auto cvar = g::memory.find_vtable_instance( g::modules.tier0, "CCvar" );
		if ( !cvar )
		{
			return 0;
		}

		const auto convars_ptr = g::memory.read<std::uintptr_t>( cvar + 0x50 );
		if ( !convars_ptr )
		{
			return 0;
		}

		for ( std::uint16_t current = 0; current != static_cast< std::uint16_t >( -1 ); )
		{
			const auto entry_addr = convars_ptr + ( static_cast< std::size_t >( current ) * 16 );
			const auto convar = g::memory.read<std::uintptr_t>( entry_addr );

			if ( convar )
			{
				const auto name_ptr = g::memory.read<std::uintptr_t>( convar );
				if ( name_ptr )
				{
					char name_buffer[ 128 ]{};
					if ( g::memory.read( name_ptr, name_buffer, sizeof( name_buffer ) - 1 ) )
					{
						name_buffer[ sizeof( name_buffer ) - 1 ] = '\0';

						if ( fnv1a::runtime_hash( name_buffer ) == name_hash )
						{
							return convar;
						}
					}
				}
			}

			current = g::memory.read<std::uint16_t>( entry_addr + 10 );
		}

		return 0;
	}

	template<typename T>
	T convars::get( std::uintptr_t cvar_ptr )
	{
		if ( !cvar_ptr )
		{
			return T{};
		}

		return g::memory.read<T>( cvar_ptr + 0x58 );
	}

	template int convars::get<int>( std::uintptr_t );
	template float convars::get<float>( std::uintptr_t );
	template bool convars::get<bool>( std::uintptr_t );
	template std::uint8_t convars::get<std::uint8_t>( std::uintptr_t );

} // namespace systems