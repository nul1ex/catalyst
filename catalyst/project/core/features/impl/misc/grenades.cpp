#include <stdafx.hpp>

namespace features::misc {

	void grenades::on_render( zdraw::draw_list& draw_list )
	{
		const auto& cfg = settings::g_misc.m_grenades;
		if ( !cfg.enabled )
		{
			return;
		}

		const auto now = std::chrono::steady_clock::now( );
		const auto has_bvh = systems::g_bvh.valid( );

		if ( has_bvh )
		{
			this->update_in_flight( );
		}

		std::erase_if( this->m_in_flight, [ & ]( const in_flight_grenade& g )
			{
				if ( !g.detonated )
				{
					return false;
				}

				const auto fade_elapsed = std::chrono::duration<float>( now - g.detonate_time ).count( );
				if ( fade_elapsed <= cfg.fade_duration )
				{
					return false;
				}

				const auto projectiles = systems::g_collector.projectiles( );
				for ( const auto& proj : projectiles )
				{
					if ( proj.entity == g.entity )
					{
						return false;
					}
				}

				return true;
			} );

		for ( auto& gren : this->m_in_flight )
		{
			if ( !gren.traj.valid )
			{
				continue;
			}

			if ( !gren.detonated )
			{
				const auto elapsed = std::chrono::duration<float>( now - gren.throw_time ).count( );
				const auto fuse_expired = elapsed >= gren.traj.duration;
				const auto effect_started = gren.effect_started;

				if ( fuse_expired || effect_started )
				{
					gren.detonated = true;
					gren.detonate_time = now;
				}
			}

			auto alpha{ 1.0f };

			if ( gren.detonated )
			{
				const auto elapsed = std::chrono::duration<float>( now - gren.detonate_time ).count( );
				alpha = std::clamp( 1.0f - elapsed / cfg.fade_duration, 0.0f, 1.0f );
			}

			if ( alpha > 0.0f )
			{
				this->render_trajectory( draw_list, gren.traj, alpha, gren.weapon_hash );
			}
		}

		const auto& ctx = combat::g_shared.ctx( );
		auto holding_now{ false };

		if ( ctx.valid && ctx.weapon && ctx.weapon_type == cstypes::weapon_type::grenade )
		{
			const auto pin_pulled = g::memory.read<bool>( ctx.weapon + SCHEMA( "C_BaseCSGrenade", "m_bPinPulled"_hash ) );
			holding_now = pin_pulled;
		}

		if ( this->m_was_holding && !holding_now )
		{
			this->m_last_throw_time = now;
		}

		this->m_was_holding = holding_now;

		if ( !has_bvh || !this->can_predict( ) )
		{
			return;
		}

		this->update_weapon_properties( );

		math::vector3 origin{}, velocity{};
		this->setup_throw( origin, velocity );

		trajectory traj{};
		this->simulate( origin, velocity, traj );

		if ( traj.valid )
		{
			this->render_trajectory( draw_list, traj, 1.0f, this->m_weapon_hash );
		}
	}

	bool grenades::can_predict( ) const
	{
		const auto& ctx = combat::g_shared.ctx( );
		if ( !ctx.valid || !ctx.weapon || !ctx.weapon_vdata )
		{
			return false;
		}

		if ( ctx.weapon_type != cstypes::weapon_type::grenade )
		{
			return false;
		}

		const auto pin_pulled = g::memory.read<bool>( ctx.weapon + SCHEMA( "C_BaseCSGrenade", "m_bPinPulled"_hash ) );
		if ( !pin_pulled )
		{
			const auto since_throw = std::chrono::duration<float>( std::chrono::steady_clock::now( ) - this->m_last_throw_time ).count( );
			if ( since_throw < throw_cooldown )
			{
				return false;
			}
		}

		const auto throw_time = g::memory.read<float>( ctx.weapon + SCHEMA( "C_BaseCSGrenade", "m_fThrowTime"_hash ) );
		if ( throw_time > 0.0f )
		{
			return false;
		}

		return true;
	}

	void grenades::update_weapon_properties( )
	{
		const auto& ctx = combat::g_shared.ctx( );
		if ( !ctx.weapon_vdata || ctx.weapon_vdata == this->m_weapon_vdata )
		{
			return;
		}

		this->m_weapon_vdata = ctx.weapon_vdata;
		this->m_throw_velocity = std::clamp( g::memory.read<float>( ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flThrowVelocity"_hash ) ), 1.0f, 10000.0f );

		const auto name_ptr = g::memory.read<std::uintptr_t>( ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_szName"_hash ) );
		if ( !name_ptr )
		{
			this->m_weapon_hash = 0;
			this->m_detonate_time = 1.5f;
			this->m_velocity_threshold = 0.1f;
			return;
		}

		char name[ 64 ]{};
		g::memory.read( name_ptr, name, sizeof( name ) - 1 );
		this->m_weapon_hash = fnv1a::runtime_hash( name );

		switch ( this->m_weapon_hash )
		{
		case "weapon_molotov"_hash:
		case "weapon_incgrenade"_hash:
			this->m_detonate_time = 2.0f;
			this->m_velocity_threshold = 0.0f;
			break;

		case "weapon_decoy"_hash:
			this->m_detonate_time = 2.0f;
			this->m_velocity_threshold = 0.2f;
			break;

		default:
			this->m_detonate_time = 1.5f;
			this->m_velocity_threshold = 0.1f;
			break;
		}
	}

	void grenades::setup_throw( math::vector3& origin, math::vector3& velocity )
	{
		const auto& ctx = combat::g_shared.ctx( );
		const auto pawn = systems::g_local.pawn( );

		auto strength{ 1.0f };

		const auto pin_pulled = g::memory.read<bool>( ctx.weapon + SCHEMA( "C_BaseCSGrenade", "m_bPinPulled"_hash ) );
		if ( pin_pulled )
		{
			strength = std::clamp( g::memory.read<float>( ctx.weapon + SCHEMA( "C_BaseCSGrenade", "m_flThrowStrength"_hash ) ), 0.0f, 1.0f );
			if ( std::fabsf( strength - 0.5f ) <= 0.1f )
			{
				strength = 0.5f;
			}
		}

		auto angles = systems::g_view.angles( );

		if ( angles.x > 90.0f )
		{
			angles.x -= 360.0f;
		}
		else if ( angles.x < -90.0f )
		{
			angles.x += 360.0f;
		}

		angles.x -= ( 90.0f - std::fabsf( angles.x ) ) * 10.0f / 90.0f;

		const auto player_velocity = g::memory.read<math::vector3>( pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );

		auto eye_pos = systems::g_view.origin( );
		eye_pos.z += strength * 12.0f - 12.0f;

		math::vector3 forward{}, right{}, up{};
		angles.to_directions( &forward, &right, &up );

		const auto trace = systems::g_bvh.trace_ray( eye_pos, eye_pos + forward * 22.0f );
		origin = trace.hit ? trace.end_pos - forward * 6.0f : eye_pos + forward * 16.0f;

		const auto throw_vel = std::clamp( this->m_throw_velocity * 0.9f, 15.0f, 750.0f );
		const auto throw_speed = ( strength * 0.7f + 0.3f ) * throw_vel;

		velocity = forward * throw_speed + player_velocity * 1.25f;
	}

	void grenades::update_in_flight( )
	{
		const auto& cfg = settings::g_misc.m_grenades;
		const auto projectiles = systems::g_collector.projectiles( );
		const auto controller = systems::g_local.controller( );

		if ( !controller )
		{
			return;
		}

		const auto local_pawn_handle = g::memory.read<std::uint32_t>( controller + SCHEMA( "CCSPlayerController", "m_hPlayerPawn"_hash ) );
		const auto now = std::chrono::steady_clock::now( );

		std::unordered_set<std::uintptr_t> alive{};

		for ( const auto& proj : projectiles )
		{
			if ( cfg.local_only && proj.thrower_handle != local_pawn_handle )
			{
				continue;
			}

			alive.insert( proj.entity );

			in_flight_grenade* existing{ nullptr };

			for ( auto& gren : this->m_in_flight )
			{
				if ( gren.entity == proj.entity )
				{
					existing = &gren;
					break;
				}
			}

			if ( existing )
			{
				existing->last_seen = now;

				if ( !existing->corrected )
				{
					const auto initial_pos = g::memory.read<math::vector3>( proj.entity + SCHEMA( "C_BaseCSGrenadeProjectile", "m_vInitialPosition"_hash ) );
					const auto initial_vel = g::memory.read<math::vector3>( proj.entity + SCHEMA( "C_BaseCSGrenadeProjectile", "m_vInitialVelocity"_hash ) );

					if ( initial_vel.length_sqr( ) >= 1.0f )
					{
						const auto saved = this->m_weapon_hash;
						this->m_weapon_hash = existing->weapon_hash;
						this->simulate( initial_pos, initial_vel, existing->traj );
						this->m_weapon_hash = saved;
						existing->corrected = true;
					}
				}

				if ( !existing->detonated && proj.effect_tick_begin > 0 )
				{
					existing->effect_started = true;
					existing->detonated = true;
					existing->detonate_time = now;
				}

				continue;
			}

			const auto initial_pos = g::memory.read<math::vector3>( proj.entity + SCHEMA( "C_BaseCSGrenadeProjectile", "m_vInitialPosition"_hash ) );
			const auto initial_vel = g::memory.read<math::vector3>( proj.entity + SCHEMA( "C_BaseCSGrenadeProjectile", "m_vInitialVelocity"_hash ) );

			if ( initial_vel.length_sqr( ) < 1.0f )
			{
				continue;
			}

			if ( proj.effect_tick_begin > 0 )
			{
				continue;
			}

			const auto wh = this->hash_from_projectile( proj.subtype );
			const auto saved = this->m_weapon_hash;
			this->m_weapon_hash = wh;

			in_flight_grenade gren{};
			gren.entity = proj.entity;
			gren.weapon_hash = wh;
			gren.throw_time = now;
			gren.last_seen = now;
			gren.corrected = true;
			this->simulate( initial_pos, initial_vel, gren.traj );

			this->m_weapon_hash = saved;
			this->m_in_flight.push_back( std::move( gren ) );
		}

		for ( auto& gren : this->m_in_flight )
		{
			if ( gren.detonated )
			{
				continue;
			}

			if ( alive.contains( gren.entity ) )
			{
				continue;
			}

			const auto missing_for = std::chrono::duration<float>( now - gren.last_seen ).count( );
			if ( missing_for >= missing_grace )
			{
				gren.detonated = true;
				gren.detonate_time = now;
			}
		}
	}

	std::uintptr_t grenades::hash_from_projectile( systems::collector::projectile_subtype type ) const
	{
		switch ( type )
		{
		case systems::collector::projectile_subtype::he_grenade:    return "weapon_hegrenade"_hash;
		case systems::collector::projectile_subtype::flashbang:     return "weapon_flashbang"_hash;
		case systems::collector::projectile_subtype::smoke_grenade: return "weapon_smokegrenade"_hash;
		case systems::collector::projectile_subtype::molotov:       return "weapon_molotov"_hash;
		case systems::collector::projectile_subtype::decoy:         return "weapon_decoy"_hash;
		default:                                                    return 0;
		}
	}

	zdraw::rgba grenades::color_for_type( std::uintptr_t weapon_hash ) const
	{
		const auto& cfg = settings::g_misc.m_grenades;

		if ( !cfg.per_type_colors )
		{
			return cfg.line_color;
		}

		switch ( weapon_hash )
		{
		case "weapon_hegrenade"_hash:    return cfg.color_he;
		case "weapon_flashbang"_hash:    return cfg.color_flash;
		case "weapon_smokegrenade"_hash: return cfg.color_smoke;
		case "weapon_molotov"_hash:
		case "weapon_incgrenade"_hash:   return cfg.color_molotov;
		case "weapon_decoy"_hash:        return cfg.color_decoy;
		default:                         return cfg.line_color;
		}
	}

	void grenades::simulate( const math::vector3& start, const math::vector3& velocity, trajectory& out )
	{
		this->m_sv_gravity = systems::g_convars.get<float>( CONVAR( "sv_gravity"_hash ) );

		const auto molotov_slope = systems::g_convars.get<float>( CONVAR( "weapon_molotov_maxdetonateslope"_hash ) );
		this->m_molotov_max_slope_z = std::cosf( molotov_slope * 3.14159265f / 180.0f );

		out.points.clear( );
		out.points.reserve( max_ticks / ticks_per_point );
		out.bounces.clear( );
		out.valid = false;
		out.end_tick = -1;

		auto pos = start;
		auto vel = velocity;
		auto bounce_count{ 0 };
		auto tick_timer{ 0 };

		for ( int tick = 0; tick < max_ticks; ++tick )
		{
			if ( tick_timer == 0 )
			{
				out.points.push_back( pos );
			}

			systems::bvh::trace_result trace{};
			this->step_simulation( pos, vel, trace );

			if ( trace.hit )
			{
				++bounce_count;
				out.bounces.push_back( pos );

				const auto is_molotov = this->m_weapon_hash == "weapon_molotov"_hash || this->m_weapon_hash == "weapon_incgrenade"_hash;
				if ( is_molotov && trace.normal.z >= this->m_molotov_max_slope_z )
				{
					out.end_tick = tick;
					out.end_pos = pos;
					out.duration = static_cast< float >( tick ) * tick_interval;
					break;
				}
			}

			const auto velocity_stopped = std::fabsf( vel.x ) < 20.0f && std::fabsf( vel.y ) < 20.0f && vel.length_sqr( ) < 400.0f;
			if ( this->should_detonate( vel, tick ) || bounce_count > 20 || velocity_stopped )
			{
				out.end_tick = tick;
				out.end_pos = pos;
				out.duration = static_cast< float >( tick ) * tick_interval;
				break;
			}

			if ( trace.hit || ++tick_timer >= ticks_per_point )
			{
				tick_timer = 0;
			}
		}

		if ( !out.points.empty( ) && out.end_tick >= 0 )
		{
			if ( out.points.back( ).distance_sqr( out.end_pos ) > 1.0f )
			{
				out.points.push_back( out.end_pos );
			}
		}

		out.valid = out.end_tick >= 0;
	}

	void grenades::step_simulation( math::vector3& pos, math::vector3& vel, systems::bvh::trace_result& trace )
	{
		const auto gravity = this->m_sv_gravity * gravity_scale;
		const auto new_vel_z = vel.z - gravity * tick_interval;

		const math::vector3 move
		{
			vel.x * tick_interval,
			vel.y * tick_interval,
			( vel.z + new_vel_z ) * 0.5f * tick_interval
		};

		vel.z = new_vel_z;

		trace = systems::g_bvh.trace_ray( pos, pos + move );
		pos = trace.end_pos;

		if ( trace.hit )
		{
			this->resolve_collision( trace, pos, vel );
		}
	}

	void grenades::resolve_collision( const systems::bvh::trace_result& trace, math::vector3& pos, math::vector3& vel )
	{
		const auto total_elasticity = std::clamp( elasticity, 0.0f, 0.9f );
		const auto backoff = vel.dot( trace.normal ) * 2.0f;

		auto new_vel = ( vel - trace.normal * backoff ) * total_elasticity;

		if ( trace.normal.z > 0.7f )
		{
			const auto speed_sqr = new_vel.length_sqr( );

			if ( speed_sqr > 96000.0f )
			{
				const auto l = new_vel.normalized( ).dot( trace.normal );
				if ( l > 0.5f )
				{
					new_vel = new_vel * ( 1.5f - l );
				}
			}

			if ( speed_sqr < 400.0f )
			{
				vel = {};
				return;
			}
		}

		vel = new_vel;

		const auto remaining = 1.0f - trace.fraction;
		if ( remaining > 0.0f )
		{
			const auto post_trace = systems::g_bvh.trace_ray( pos, pos + new_vel * ( remaining * tick_interval ) );
			pos = post_trace.end_pos;
		}
	}

	bool grenades::should_detonate( const math::vector3& vel, int tick ) const
	{
		switch ( this->m_weapon_hash )
		{
		case "weapon_smokegrenade"_hash:
		case "weapon_decoy"_hash:
		{
			const auto speed_2d = std::sqrtf( vel.x * vel.x + vel.y * vel.y );
			const auto check_ticks = static_cast< int >( 0.2f / tick_interval );
			return speed_2d < this->m_velocity_threshold && ( tick % check_ticks ) == 0;
		}

		case "weapon_molotov"_hash:
		case "weapon_incgrenade"_hash:
			return static_cast< float >( tick ) * tick_interval > this->m_detonate_time;

		case "weapon_flashbang"_hash:
		case "weapon_hegrenade"_hash:
			return static_cast< float >( tick - 8 ) * tick_interval > this->m_detonate_time;

		default:
			return false;
		}
	}

	void grenades::render_trajectory( zdraw::draw_list& draw_list, const trajectory& traj, float alpha, std::uintptr_t weapon_hash ) const
	{
		if ( !traj.valid || traj.points.size( ) < 2 )
		{
			return;
		}

		const auto& cfg = settings::g_misc.m_grenades;
		const auto base_color = this->color_for_type( weapon_hash );
		const auto total = traj.points.size( );

		for ( std::size_t i = 0; i + 1 < total; ++i )
		{
			const auto s0 = systems::g_view.project( traj.points[ i ] );
			const auto s1 = systems::g_view.project( traj.points[ i + 1 ] );

			if ( !systems::g_view.projection_valid( s0 ) || !systems::g_view.projection_valid( s1 ) )
			{
				continue;
			}

			const auto t = static_cast< float >( i ) / static_cast< float >( total - 1 );
			const auto seg_alpha = cfg.line_gradient ? alpha * ( 1.0f - t * 0.6f ) : alpha;
			const auto a = static_cast< std::uint8_t >( std::clamp( seg_alpha * static_cast< float >( base_color.a ), 0.0f, 255.0f ) );

			draw_list.add_line( s0.x, s0.y, s1.x, s1.y, zdraw::rgba{ base_color.r, base_color.g, base_color.b, a }, cfg.line_thickness );
		}

		if ( cfg.show_bounces )
		{
			const auto sz = cfg.bounce_size;
			const auto outline_sz = sz + 1.0f;

			for ( const auto& bounce : traj.bounces )
			{
				const auto s = systems::g_view.project( bounce );
				if ( !systems::g_view.projection_valid( s ) )
				{
					continue;
				}

				const auto a = static_cast< std::uint8_t >( alpha * static_cast< float >( cfg.bounce_color.a ) );

				draw_list.add_rect_filled( s.x - outline_sz, s.y - outline_sz, outline_sz * 2.0f, outline_sz * 2.0f, zdraw::rgba{ 0, 0, 0, a } );
				draw_list.add_rect_filled( s.x - sz, s.y - sz, sz * 2.0f, sz * 2.0f, zdraw::rgba{ cfg.bounce_color.r, cfg.bounce_color.g, cfg.bounce_color.b, a } );
			}
		}

		if ( traj.end_tick >= 0 )
		{
			const auto s = systems::g_view.project( traj.end_pos );
			if ( systems::g_view.projection_valid( s ) )
			{
				const auto sz = cfg.detonate_size;
				const auto outline_sz = sz + 1.0f;
				const auto a = static_cast< std::uint8_t >( alpha * static_cast< float >( cfg.detonate_color.a ) );

				draw_list.add_rect_filled( s.x - outline_sz, s.y - outline_sz, outline_sz * 2.0f, outline_sz * 2.0f, zdraw::rgba{ 0, 0, 0, a } );
				draw_list.add_rect_filled( s.x - sz, s.y - sz, sz * 2.0f, sz * 2.0f, zdraw::rgba{ cfg.detonate_color.r, cfg.detonate_color.g, cfg.detonate_color.b, a } );
			}
		}
	}

} // namespace features::misc