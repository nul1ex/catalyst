#include <stdafx.hpp>
#include "sounds.h"
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

namespace features::misc {

	void misc_features::on_render( zdraw::draw_list& draw_list )
	{
		std::unique_lock lock( this->m_mutex );
		this->draw_watermark( draw_list );
		this->draw_spectators( draw_list );
		this->draw_bomb_timer( draw_list );
		this->draw_hit_markers( draw_list );
		this->draw_damage_indicators( draw_list );
	}

	void misc_features::tick_write( )
	{
		this->fov_changer( );
	}

	void misc_features::tick( )
	{
		this->hitsounds( );

		const auto now = std::chrono::steady_clock::now( );
		std::unique_lock lock( this->m_mutex );

		for ( const auto& player : systems::g_collector.players( ) )
		{
			if ( !systems::g_local.is_enemy( player.team ) )
			{
				m_health_history.erase( player.pawn );
				continue;
			}

			auto& history = m_health_history[ player.pawn ];
			
			// If first time seeing them, just set history and skip damage check
			if ( history.health == -1 )
			{
				history.health = player.health;
				history.origin = player.origin;
				continue;
			}

			if ( player.health < history.health )
			{
				if ( settings::g_esp.m_damage_indicator.enabled )
				{
					// Show damage indicator
					m_damage_indicators.push_back({
						player.origin + math::vector3{ 0.0f, 0.0f, 40.0f },
						static_cast<float>( history.health - player.health ),
						false, // Todo: check hitgroup for headshot
						now,
						static_cast<float>( settings::g_esp.m_damage_indicator.duration ) / 1000.0f
					});
				}
			}

			if ( player.health <= 0 )
			{
				m_health_history.erase( player.pawn );
			}
			else
			{
				history.health = player.health;
				history.origin = player.origin;
			}
		}
	}

	void misc_features::fov_changer()
	{
		if (!settings::g_misc.m_fov_changer.enabled)
			return;

		const auto local_pawn = systems::g_local.pawn();
		if (!local_pawn)
			return;

		const auto camera_services = g::memory.read<std::uintptr_t>(
			local_pawn + SCHEMA("C_BasePlayerPawn", "m_pCameraServices"_hash)
		);
		if (!camera_services)
			return;

		// Optional: skip when scoped (common preference)
		const bool is_scoped = g::memory.read<bool>(
			local_pawn + SCHEMA("C_CSPlayerPawn", "m_bIsScoped"_hash)
		);

		if (is_scoped && settings::g_misc.m_fov_changer.disable_when_scoped)
			return;
 
		const auto target_fov = static_cast<std::uint32_t>(settings::g_misc.m_fov_changer.fov);
		const auto target_viewmodel_fov = static_cast<float>(settings::g_misc.m_fov_changer.viewmodel_fov);

		const auto fov_offset = SCHEMA("CCSPlayerBase_CameraServices", "m_iFOV"_hash);
		const auto fov = g::memory.read<std::uint32_t>(camera_services + fov_offset);
		g::memory.write<std::uint32_t>(camera_services + fov_offset, target_fov);
	}

	void misc_features::draw_spectators( zdraw::draw_list& draw_list )
	{
		if ( !settings::g_misc.m_main.spectator_list )
			return;

		std::vector<std::string> specs{};

		const auto local_controller = systems::g_local.controller( );
		if ( !local_controller )
			return;

		for ( const auto& entry : systems::g_entities.by_type( systems::entities::type::player ) )
		{
			const auto controller = entry.ptr;
			if ( !controller || controller == local_controller )
				continue;

			const auto alive = g::memory.read<bool>( controller + SCHEMA( "CCSPlayerController", "m_bPawnIsAlive"_hash ) );
			if ( alive )
				continue;

			// Priority to observer pawn as that's what spectators use
			auto pawn_handle = g::memory.read<std::uint32_t>( controller + SCHEMA( "CCSPlayerController", "m_hObserverPawn"_hash ) );
			if ( !pawn_handle || pawn_handle == 0xffffffff )
				pawn_handle = g::memory.read<std::uint32_t>( controller + SCHEMA( "CCSPlayerController", "m_hPlayerPawn"_hash ) );

			if ( !pawn_handle || pawn_handle == 0xffffffff )
				continue;

			const auto pawn = systems::g_entities.lookup( pawn_handle );
			if ( !pawn )
				continue;

			const auto observer_services = g::memory.read<std::uintptr_t>( pawn + SCHEMA( "C_BasePlayerPawn", "m_pObserverServices"_hash ) );
			if ( !observer_services )
				continue;

			const auto observer_target_handle = g::memory.read<std::uint32_t>( observer_services + SCHEMA( "CPlayer_ObserverServices", "m_hObserverTarget"_hash ) );
			if ( !observer_target_handle || observer_target_handle == 0xffffffff )
				continue;

			const auto observer_target_pawn = systems::g_entities.lookup( observer_target_handle );
			if ( !observer_target_pawn || observer_target_pawn != systems::g_local.view_pawn( ) )
				continue;

			const auto name_ptr = g::memory.read<std::uintptr_t>( controller + SCHEMA( "CCSPlayerController", "m_sSanitizedPlayerName"_hash ) );
			if ( name_ptr )
			{
				const auto name = g::memory.read_string( name_ptr, 128 );
				const auto mode = g::memory.read<int>( observer_services + SCHEMA( "CPlayer_ObserverServices", "m_iObserverMode"_hash ) );
				
				if ( mode == 0 || mode == 1 )
					continue; // Mode 0 = None, Mode 1 = Deathcam

				std::string mode_str;
				switch (mode) {
					case 2: mode_str = "freezecam";   break;
					case 4: mode_str = "freecam";        break;
					case 5: mode_str = "first-person";   break;
					case 6: mode_str = "third-person";      break;
					default: mode_str = std::to_string(mode); break;
				}
				specs.push_back(std::format("{} ({})", name, mode_str));
			}
		}

		const auto display = zdraw::get_display_size( );
		const auto x = 10.0f;
		auto y = display.second / 2.0f;

		const auto& main_cfg = settings::g_misc.m_main;

		draw_list.add_text( x, y, "spectators", zdraw::get_font( ), main_cfg.spectator_list_color, zdraw::text_style::outlined );
		y += 15.0f;

		for ( const auto& spec : specs )
		{
			draw_list.add_text( x, y, spec, zdraw::get_font( ), zdraw::rgba{ 195, 200, 215, 230 }, zdraw::text_style::outlined );
			y += 15.0f;
		}
	}

	void misc_features::draw_bomb_timer( zdraw::draw_list& draw_list )
	{
		if ( !settings::g_misc.m_main.bomb_timer )
			return;

		const auto& main_cfg = settings::g_misc.m_main;

		for ( const auto& bomb : systems::g_entities.by_type( systems::entities::type::bomb ) )
		{
			const auto class_name_ptr = g::memory.read<std::uintptr_t>( bomb.ptr + 0x10 ); // Offset to class name is usually at the start of entity+0x10
			
			const auto is_planted = g::memory.read<bool>( bomb.ptr + SCHEMA( "C_PlantedC4", "m_bBombPlanted"_hash ) );
			if ( !is_planted )
				continue;

			const auto is_defused = g::memory.read<bool>( bomb.ptr + SCHEMA( "C_PlantedC4", "m_bBombDefused"_hash ) );
			if ( is_defused )
				continue;

			const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
			if ( !global_vars )
				continue;

			const auto current_time = g::memory.read<float>( global_vars + 0x30 );

			const auto blow_time = g::memory.read<float>( bomb.ptr + SCHEMA( "C_PlantedC4", "m_flC4Blow"_hash ) );
			const auto time_left = blow_time - current_time;

			if ( time_left <= 0.0f )
				continue;

			const auto site = g::memory.read<int>( bomb.ptr + SCHEMA( "C_PlantedC4", "m_nBombSite"_hash ) );
			const auto is_defusing = g::memory.read<bool>( bomb.ptr + SCHEMA( "C_PlantedC4", "m_bBeingDefused"_hash ) );
			const auto defuse_countdown = g::memory.read<float>( bomb.ptr + SCHEMA( "C_PlantedC4", "m_flDefuseCountDown"_hash ) );
			const auto defuse_time_left = defuse_countdown - current_time;

			const auto [screen_w, screen_h] = zdraw::get_display_size( );
			const auto x = screen_w * 0.5f;
			const auto y = screen_h * 0.15f;
			
			// Background
			constexpr auto bar_w{ 240.0f };
			constexpr auto bar_h{ 6.0f };
			
			draw_list.add_rect_filled( x - ( bar_w * 0.5f ) - 2.0f, y - 2.0f, bar_w + 4.0f, 35.0f, zdraw::rgba{ 12, 12, 12, 180 } );
			draw_list.add_rect( x - ( bar_w * 0.5f ) - 2.0f, y - 2.0f, bar_w + 4.0f, 35.0f, zdraw::rgba{ 45, 45, 45, 200 } );

			// Site & Time text
			const auto site_str = ( site == 0 ? "A" : "B" );
			const auto timer_str = std::format( "SITE  {} - {:.2f}s", site_str, time_left );
			
			const auto [tw, th] = zdraw::measure_text( timer_str );
			draw_list.add_text( x - ( tw * 0.5f ), y + 4.0f, timer_str, zdraw::get_font( ), zdraw::rgba{ 215, 220, 240, 255 }, zdraw::text_style::outlined );

			// Timer Bar
			const auto timer_frac = std::clamp( time_left / 40.0f, 0.0f, 1.0f );
			const auto bar_color = zui::lerp( { 220, 60, 60, 255 }, main_cfg.bomb_timer_color, timer_frac );
			
			draw_list.add_rect_filled( x - ( bar_w * 0.5f ), y + 24.0f, bar_w, bar_h, zdraw::rgba{ 30, 30, 35, 200 } );
			draw_list.add_rect_filled( x - ( bar_w * 0.5f ), y + 24.0f, bar_w * timer_frac, bar_h, bar_color );

			// Defuse Bar (if being defused)
			if ( is_defusing && defuse_time_left > 0.0f )
			{
				const auto defuse_total = g::memory.read<float>( bomb.ptr + SCHEMA( "C_PlantedC4", "m_flDefuseLength"_hash ) );
				const auto defuse_frac = std::clamp( defuse_time_left / ( defuse_total > 0.0f ? defuse_total : 5.0f ), 0.0f, 1.0f );
				
				// Optional logic: show if defuse is possible
				const auto can_defuse = defuse_time_left < time_left;
				const auto defuse_color = can_defuse ? zdraw::rgba{ 90, 220, 110, 255 } : zdraw::rgba{ 220, 180, 60, 255 };

				draw_list.add_rect_filled( x - ( bar_w * 0.5f ), y + 32.0f, bar_w, 2.0f, zdraw::rgba{ 30, 30, 35, 200 } );
				draw_list.add_rect_filled( x - ( bar_w * 0.5f ), y + 32.0f, bar_w * ( 1.0f - defuse_frac ), 2.0f, defuse_color );
				
				const auto defuse_str = std::format( "DEFUSING - {:.2f}s", defuse_time_left );
				const auto [dtw, dth] = zdraw::measure_text( defuse_str );
				draw_list.add_text( x - ( dtw * 0.5f ), y + 38.0f, defuse_str, zdraw::get_font( ), defuse_color, zdraw::text_style::outlined );
			}
		}
	}

	void misc_features::hitsounds()
	{
		const auto& main_cfg = settings::g_misc.m_main;
		if ( main_cfg.hitsound <= 0 )
			return;

		const auto local_pawn = systems::g_local.pawn();
		if (!local_pawn)
			return;

		const auto bullet_services = g::memory.read<std::uintptr_t>(
			local_pawn + SCHEMA("C_CSPlayerPawn", "m_pBulletServices"_hash)
		);
		if (!bullet_services)
			return;

		const auto total_hits = g::memory.read<int>(
			bullet_services + SCHEMA("CCSPlayer_BulletServices", "m_totalHitsOnServer"_hash)
		);

		if (total_hits > this->m_old_hits)
		{
			const auto now = std::chrono::steady_clock::now( );
			std::unique_lock lock( this->m_mutex );

			// Play sound
			switch ( main_cfg.hitsound )
			{
			case 1: ::PlaySoundA( reinterpret_cast< LPCSTR >( hit_sound ), nullptr, SND_MEMORY | SND_ASYNC ); break;
			case 2: ::PlaySoundA( "SystemAsterisk", nullptr, SND_ALIAS | SND_ASYNC ); break;
			case 3: ::PlaySoundA( "SystemHand", nullptr, SND_ALIAS | SND_ASYNC ); break;
			default: break;
			}

			// Add hit marker
			if ( settings::g_esp.m_hit_marker.enabled )
			{
				this->m_hit_markers.push_back( { now, static_cast<float>( settings::g_esp.m_hit_marker.duration ) / 1000.0f } );
			}
		}

		this->m_old_hits = total_hits;
	}

	void misc_features::draw_hit_markers( zdraw::draw_list& draw_list )
	{
		const auto& cfg = settings::g_esp.m_hit_marker;
		if ( !cfg.enabled || m_hit_markers.empty( ) )
			return;

		const auto now = std::chrono::steady_clock::now( );
		const auto [sw, sh] = zdraw::get_display_size( );
		const auto cx = sw * 0.5f;
		const auto cy = sh * 0.5f;

		// Cleanup
		std::erase_if( m_hit_markers, [ & ]( const auto& m ) { 
			return std::chrono::duration_cast<std::chrono::duration<float>>( now - m.time ).count( ) > m.max_time; 
		} );

		for ( const auto& marker : m_hit_markers )
		{
			const float elapsed = std::chrono::duration_cast<std::chrono::duration<float>>( now - marker.time ).count( );
			const auto alpha = std::clamp( 1.0f - elapsed / marker.max_time, 0.0f, 1.0f );
			const auto current_color = zdraw::rgba{ cfg.color.r, cfg.color.g, cfg.color.b, static_cast<std::uint8_t>( alpha * cfg.color.a ) };

			const auto s = static_cast<float>( cfg.size );
			const auto g = static_cast<float>( cfg.gap );

			draw_list.add_line( cx - g - s, cy - g - s, cx - g, cy - g, current_color, 1.5f);
			draw_list.add_line( cx + g + s, cy - g - s, cx + g, cy - g, current_color, 1.5f);
			draw_list.add_line( cx - g - s, cy + g + s, cx - g, cy + g, current_color, 1.5f);
			draw_list.add_line( cx + g + s, cy + g + s, cx + g, cy + g, current_color, 1.5f);
		}
	}

	void misc_features::draw_damage_indicators( zdraw::draw_list& draw_list )
	{
		const auto& cfg = settings::g_esp.m_damage_indicator;
		if ( !cfg.enabled || m_damage_indicators.empty( ) )
			return;

		const auto now = std::chrono::steady_clock::now( );
		
		// Cleanup
		std::erase_if( m_damage_indicators, [ & ]( const auto& m ) { 
			return std::chrono::duration_cast<std::chrono::duration<float>>( now - m.time ).count( ) > m.max_time; 
		} );

		zdraw::push_font( g::render.fonts( ).pretzel_24 );

		for ( const auto& indicator : m_damage_indicators )
		{
			const float elapsed = std::chrono::duration_cast<std::chrono::duration<float>>( now - indicator.time ).count( );
			const auto alpha = std::clamp( 1.0f - elapsed / indicator.max_time, 0.0f, 1.0f );
			
			auto world_pos = indicator.pos;
			world_pos.z += elapsed * cfg.floating_speed;

			const auto screen = systems::g_view.project( world_pos );
			if ( !systems::g_view.projection_valid( screen ) )
				continue;

			const auto color = indicator.is_headshot ? cfg.crit_color : cfg.color;
			const auto current_color = zdraw::rgba{ color.r, color.g, color.b, static_cast<std::uint8_t>( alpha * color.a ) };

			const auto text = std::format( "{}", static_cast<int>( indicator.damage ) );
			const auto [tw, th] = zdraw::measure_text( text );
			
			draw_list.add_text( screen.x - tw * 0.5f, screen.y - th * 0.5f, text, zdraw::get_font( ), current_color, zdraw::text_style::outlined );
		}

		zdraw::pop_font( );
	}

	void misc_features::draw_watermark( zdraw::draw_list& draw_list )
	{
		if ( !settings::g_misc.m_main.watermark )
			return;
			
		const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
		if ( !global_vars )
			return;

		const auto fps = zdraw::fps;

		const auto watermark_text = std::format( "catalyst | fps: {}", fps );
		const auto [tw, th] = zdraw::measure_text( watermark_text );

		const auto screen = zdraw::get_display_size( );
		const float x = screen.first - tw - 25.0f;
		const float y = 15.0f;

		// Background (using the same style as spectator list/bomb timer)
		draw_list.add_rect_filled( x - 10.0f, y - 5.0f, tw + 20.0f, th + 10.0f, zdraw::rgba{ 12, 12, 12, 180 } );
		draw_list.add_rect( x - 10.0f, y - 5.0f, tw + 20.0f, th + 10.0f, zdraw::rgba{ 45, 45, 45, 200 } );

		// Text
		draw_list.add_text( x, y, watermark_text, zdraw::get_font( ), zdraw::rgba{ 215, 220, 240, 255 }, zdraw::text_style::outlined );
	}

} // namespace features::misc

