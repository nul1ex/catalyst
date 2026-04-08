#include <stdafx.hpp>

namespace features::esp {

	void footsteps::tick( )
	{
		const auto& cfg = settings::g_esp.m_player.m_footsteps;
		if ( !cfg.enabled )
		{
			this->m_rings.clear( );
			return;
		}

		const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
		if ( !global_vars ) return;

		const float current_time = g::memory.read<float>( global_vars + 0x30 );
		const auto local_pawn = systems::g_local.pawn( );

		for ( const auto& player : systems::g_collector.players( ) )
		{
			if ( !player.pawn || !player.alive )
				continue;

			if ( !systems::g_local.is_enemy( player.team ) && !cfg.show_teammates )
				continue;

			if ( player.pawn == local_pawn )
				continue;

			const std::uint32_t flags = g::memory.read<std::uint32_t>( player.pawn + SCHEMA( "C_BaseEntity", "m_fFlags"_hash ) );
			const bool is_on_ground = ( flags & ( 1 << 0 ) ) != 0;

			bool was_on_ground = true;
			if ( auto it = m_prev_on_ground.find( player.pawn ); it != m_prev_on_ground.end( ) )
			{
				was_on_ground = it->second;
			}

			const float speed = player.velocity.length( );
			const math::vector3 feet_pos = player.origin;

			if ( !was_on_ground && is_on_ground && speed > 80.0f )
			{
				ring r;
				r.world_pos = feet_pos;
				float land_radius = cfg.land_max_radius * std::min( 1.0f, speed / 400.0f );
				r.max_radius = std::max( cfg.land_max_radius * 0.5f, land_radius );
				r.color = cfg.land_color;
				r.start_time = current_time;
				m_rings.push_back( r );
			}
			else if ( was_on_ground && !is_on_ground && speed > 50.0f )
			{
				ring r;
				r.world_pos = feet_pos;
				r.max_radius = cfg.jump_max_radius;
				r.color = cfg.jump_color;
				r.start_time = current_time;
				m_rings.push_back( r );
			}
			else if ( was_on_ground && is_on_ground && speed > 50.0f )
			{
				float last_time = 0;
				if ( auto it = m_last_step_time.find( player.pawn ); it != m_last_step_time.end( ) )
				{
					last_time = it->second;
				}

				if ( current_time - last_time > 0.25f )
				{
					m_last_step_time[ player.pawn ] = current_time;
					ring r;
					r.world_pos = feet_pos;
					float step_radius = cfg.footstep_max_radius * std::min( 1.0f, speed / 250.0f );
					r.max_radius = std::max( cfg.footstep_max_radius * 0.4f, step_radius );
					r.color = cfg.footstep_color;
					r.start_time = current_time;
					m_rings.push_back( r );
				}
			}

			m_prev_on_ground[ player.pawn ] = is_on_ground;
		}

		const float max_age = cfg.expand_duration + cfg.fade_duration;
		m_rings.erase(
			std::remove_if( m_rings.begin( ), m_rings.end( ),
				[ current_time, max_age ]( const ring& r ) { return ( current_time - r.start_time ) > max_age; } ),
			m_rings.end( )
		);
	}

	void footsteps::on_render( zdraw::draw_list& draw_list )
	{
		const auto& cfg = settings::g_esp.m_player.m_footsteps;
		if ( !cfg.enabled ) return;

		const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
		if ( !global_vars ) return;

		const float current_time = g::memory.read<float>( global_vars + 0x30 );

		for ( const auto& r : m_rings )
		{
			float elapsed = current_time - r.start_time;
			if ( elapsed < 0.0f || elapsed > ( cfg.expand_duration + cfg.fade_duration ) )
				continue;

			float current_radius;
			float alpha_factor;

			if ( elapsed < cfg.expand_duration )
			{
				float t = elapsed / cfg.expand_duration;
				current_radius = r.max_radius * t;
				alpha_factor = 1.0f;
			}
			else
			{
				current_radius = r.max_radius;
				float fade_t = ( elapsed - cfg.expand_duration ) / cfg.fade_duration;
				alpha_factor = 1.0f - fade_t;
			}

			if ( alpha_factor < 0.01f ) continue;

			const auto screen_pos = systems::g_view.project( r.world_pos );
			if ( !systems::g_view.projection_valid( screen_pos ) )
				continue;

			const auto edge_world = r.world_pos + math::vector3{ current_radius, 0, 0 };
			const auto edge_screen = systems::g_view.project( edge_world );

			if ( systems::g_view.projection_valid( edge_screen ) )
			{
				float screen_radius = std::abs( edge_screen.x - screen_pos.x );
				if ( screen_radius < 1.0f ) screen_radius = 1.0f;

				zdraw::rgba color = r.color;
				color.a = static_cast< std::uint8_t >( color.a * alpha_factor );

				draw_list.add_circle( screen_pos.x, screen_pos.y, screen_radius, color, cfg.segments, cfg.thickness );
			}
		}
	}

} // namespace features::esp
