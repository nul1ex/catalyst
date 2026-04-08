#include <stdafx.hpp>

namespace features::misc {

	void impacts::on_render( zdraw::draw_list& draw_list )
	{
		const auto& cfg = settings::g_esp.m_bullet_tracers;
		if ( !cfg.enabled )
		{
			return;
		}

		const auto& view_matrix = systems::g_view.matrix( );

		auto get_w = [&]( const math::vector3& p ) {
			return view_matrix[ 3 ][ 0 ] * p.x + view_matrix[ 3 ][ 1 ] * p.y + view_matrix[ 3 ][ 2 ] * p.z + view_matrix[ 3 ][ 3 ];
		};

		for ( const auto& tracer : this->m_tracers )
		{
			math::vector3 p1 = tracer.start;
			math::vector3 p2 = tracer.end;

			float w1 = get_w( p1 );
			float w2 = get_w( p2 );

			// Basic Near Plane Clipping (w = 0.01)
			if ( w1 < 0.01f && w2 < 0.01f ) continue;

			if ( w1 < 0.01f ) {
				float t = ( 0.01f - w1 ) / ( w2 - w1 );
				p1 = p1 + ( p2 - p1 ) * t;
			}
			else if ( w2 < 0.01f ) {
				float t = ( 0.01f - w1 ) / ( w2 - w1 );
				p2 = p1 + ( p2 - p1 ) * t;
			}

			const auto s1 = systems::g_view.project( p1 );
			const auto s2 = systems::g_view.project( p2 );

			if ( systems::g_view.projection_valid( s1 ) && systems::g_view.projection_valid( s2 ) )
			{
				const auto alpha_frac = std::clamp( tracer.time / tracer.max_time, 0.0f, 1.0f );
				const auto alpha = static_cast< std::uint8_t >( alpha_frac * cfg.color.a );
				const auto color = zdraw::rgba{ cfg.color.r, cfg.color.g, cfg.color.b, alpha };

				// Main tracer line
				draw_list.add_line( s1.x, s1.y, s2.x, s2.y, color, cfg.thickness );
				
				// Optional: subtle glow effect by drawing a thinner line on top
				if ( alpha > 50 )
				{
					draw_list.add_line( s1.x, s1.y, s2.x, s2.y, zdraw::rgba{ 255, 255, 255, static_cast<uint8_t>( alpha / 2 ) }, cfg.thickness * 0.5f );
				}
			}
		}
	}

	void impacts::tick( )
	{
		const auto& cfg = settings::g_esp.m_bullet_tracers;
		const auto pawn = systems::g_local.pawn( );
		
		if ( !pawn )
		{
			this->m_tracers.clear( );
			this->m_old_shots = 0;
			return;
		}

		const auto shots_fired = g::memory.read<int>( pawn + SCHEMA( "C_CSPlayerPawn", "m_iShotsFired"_hash ) );

		if ( shots_fired > this->m_old_shots )
		{
			const auto view_origin = systems::g_view.origin( );
			const auto angles = systems::g_view.angles( );
			
			math::vector3 forward{}, right{}, up{};
			angles.to_directions( &forward, &right, &up );

			// Offset the start point slightly to the right and down to look like it comes from the gun muzzle (fake muzzle)
			const auto start = view_origin + ( right * 4.0f ) + ( up * -3.0f ) + ( forward * 10.0f );
			const auto trace_end = view_origin + forward * 8192.0f; 

			// Use BVH to find the actual hit location
			const auto trace = systems::g_bvh.trace_ray( view_origin, trace_end );
			const auto end = trace.hit ? trace.end_pos : trace_end;

			this->m_tracers.push_back( { start, end, static_cast<float>(cfg.duration), static_cast<float>(cfg.duration) } );
		}
		this->m_old_shots = shots_fired;

		// Stabilization: use real time instead of frames
		static auto last_time = std::chrono::steady_clock::now( );
		const auto now = std::chrono::steady_clock::now( );
		const auto dt = std::chrono::duration<float, std::milli>( now - last_time ).count( );
		last_time = now;

		for ( auto it = this->m_tracers.begin( ); it != this->m_tracers.end( ); )
		{
			it->time -= dt;
			if ( it->time <= 0.0f )
			{
				it = this->m_tracers.erase( it );
			}
			else
			{
				++it;
			}
		}
	}

} // namespace features::misc
