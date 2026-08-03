#include <stdafx.hpp>

bool offsets::initialize( )
{
	// csgo_input
	{
		const auto initial = g::memory.find_pattern( g::modules.client, "48 89 05 ? ? ? ? 0F 57 C0 0F 11 05" );
		if ( !initial )
		{
			return false;
		}

		csgo_input = g::memory.resolve_rip( initial );
		if ( !csgo_input )
		{
			return false;
		}
	}

	// entity_list
	{
		const auto initial = g::memory.find_pattern( g::modules.client, "48 8B 0D ? ? ? ? 8B FD C1 EF 0E" );
		if ( !initial )
		{
			return false;
		}

		entity_list = g::memory.resolve_rip( initial );
		if ( !entity_list )
		{
			return false;
		}
	}

	// local_player_controller
	{
		const auto initial = g::memory.find_pattern( g::modules.client, "48 8B 05 ? ? ? ? 41 89 BE" );
		if ( !initial )
		{
			return false;
		}

		local_player_controller = g::memory.resolve_rip( initial );
		if ( !local_player_controller )
		{
			return false;
		}
	}

	// global_vars
	{
		const auto initial = g::memory.find_pattern( g::modules.client, "48 89 15 ? ? ? ? 48 89 42" );
		if ( !initial )
		{
			return false;
		}

		global_vars = g::memory.resolve_rip( initial );
		if ( !global_vars )
		{
			return false;
		}
	}

	// view_matrix
	{
		const auto initial = g::memory.find_pattern( g::modules.client, "C6 86 F0 12 00 00 01 48 8D 0D ? ? ? ?" );
		if ( !initial )
		{
			return false;
		}

		view_matrix = g::memory.resolve_rip( initial + 0x7 );
		if ( !view_matrix )
		{
			return false;
		}
	}

	return true;
}