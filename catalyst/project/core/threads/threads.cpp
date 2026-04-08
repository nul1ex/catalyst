#include <stdafx.hpp>

#include <timeapi.h>
#pragma comment( lib, "winmm.lib" )

namespace threads {

	void game( )
	{
		std::string last_map{};

		timeBeginPeriod( 1 );
		std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );

		while ( true )
		{
			systems::g_local.update( );

			if ( systems::g_local.valid( ) )
			{
				systems::g_entities.refresh( );
				systems::g_collector.run( );
				systems::g_voxels.update( );

				const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
				if ( global_vars )
				{
					const auto map_ptr = g::memory.read<std::uintptr_t>( global_vars + 0x188 );
					const auto current_map = map_ptr ? g::memory.read_string( map_ptr ) : std::string{};

					if ( !current_map.empty( ) && current_map != "<empty>" && current_map != last_map ) 
					{
						g::console.print( "map change: {} -> {}", last_map.empty( ) ? "none" : last_map, current_map );
						last_map = current_map;
						systems::g_bvh.clear( );
						g::console.print( "parsing bvh for {}...", current_map );
						systems::g_bvh.parse( );
						g::console.success( "bvh parsed." );
					}
				}
			}
			else
			{
				if ( !last_map.empty( ) )
				{
					last_map = {};
					systems::g_bvh.clear( );
				}
			}

			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
		}
	}

	void combat( )
	{
		timeBeginPeriod( 1 );

		constexpr auto target_tps{ 128 };
		constexpr auto tick_interval = std::chrono::nanoseconds( 1'000'000'000 / target_tps );
		auto next_tick = std::chrono::steady_clock::now( );

		while ( true )
		{
			if ( systems::g_local.valid( ) )
			{
				features::combat::g_shared.tick( );

				if ( systems::g_bvh.valid( ) )
				{
					features::combat::g_legit.tick( );
				}

				features::movement::g_movement.tick( );

				features::misc::g_misc.tick( );
				features::esp::g_footsteps.tick( );
				features::misc::g_impacts.tick( );

				features::misc::g_nade_helper.tick( );
			}

			next_tick += tick_interval;

			const auto now = std::chrono::steady_clock::now( );
			if ( next_tick < now )
			{
				next_tick = now;
				continue;
			}

			std::this_thread::sleep_until( next_tick - std::chrono::milliseconds( 1 ) );

			while ( std::chrono::steady_clock::now( ) < next_tick )
			{
				_mm_pause( );
			}
		}
	}
	void write( )
	{
		while ( true )
		{
			if ( systems::g_local.valid( ) )
			{
				features::misc::g_misc.tick_write( );
			}

			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
		}
	}


	void skinchanger( )
	{
		while ( true )
		{

			if ( systems::g_local.valid( ) )
			{
				features::skinchanger::g_skinchanger.tick( );
			}

			std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
		}
	}

} // namespace threads
