#include <stdafx.hpp>

namespace features::combat {

	namespace detail {

		inline static float remap_value( float val, float a, float b, float c, float d )
		{
			if ( b == a )
			{
				return ( val - b >= 0.0f ) ? d : c;
			}

			const auto t = std::clamp( ( val - a ) / ( b - a ), 0.0f, 1.0f );
			return c + ( d - c ) * t;
		}

		inline static float normalize_angle( float a )
		{
			return a - std::floorf( a * 0.0027777778f + 0.5f ) * 360.0f;
		}

		inline static float quantize_angle( float a )
		{
			return std::floorf( normalize_angle( a ) * 2.0f ) * 0.5f;
		}

		inline static float ease_dat( float value, float curve )
		{
			auto v = std::clamp( value, 0.0f, 1.0f );
			auto c = std::max( curve, 1.1754944e-38f );
			c = std::fminf( 1.0f, c );
			return v / ( ( ( 1.0f / c - 2.0f ) * ( 1.0f - v ) ) + 1.0f );
		}

		static bool ray_hits_capsule( const math::vector3& ray_origin, const math::vector3& ray_dir, const math::vector3& capsule_start, const math::vector3& capsule_end, float radius )
		{
			const auto capsule_vec = capsule_end - capsule_start;
			const auto capsule_length = capsule_vec.length( );

			if ( capsule_length < 0.001f )
			{
				const auto to_center = capsule_start - ray_origin;
				const auto projection = to_center.dot( ray_dir );

				if ( projection < 0.0f )
				{
					return false;
				}

				const auto closest = ray_origin + ray_dir * projection;
				return ( closest - capsule_start ).length_sqr( ) <= radius * radius;
			}

			const auto capsule_dir = capsule_vec / capsule_length;
			const auto w = ray_origin - capsule_start;

			const auto a = ray_dir.dot( ray_dir );
			const auto b = ray_dir.dot( capsule_dir );
			const auto c = capsule_dir.dot( capsule_dir );
			const auto d = ray_dir.dot( w );
			const auto e = capsule_dir.dot( w );

			const auto denom = a * c - b * b;

			float s, t;

			if ( std::abs( denom ) < 0.0001f )
			{
				s = 0.0f;
				t = ( b > c ? d / b : e / c );
			}
			else
			{
				s = ( b * e - c * d ) / denom;
				t = ( a * e - b * d ) / denom;
			}

			t = std::clamp( t, 0.0f, capsule_length );
			if ( s < 0.0f )
			{
				return false;
			}

			const auto point_on_capsule = capsule_start + capsule_dir * t;
			const auto point_on_ray = ray_origin + ray_dir * s;

			return ( point_on_ray - point_on_capsule ).length_sqr( ) <= radius * radius;
		}

		static void scale_damage( int hitgroup, int armor, bool has_helmet, int team, float armor_ratio, float headshot_multiplier, float& damage )
		{
			const auto ct_head = systems::g_convars.get<float>( CONVAR( "mp_damage_scale_ct_head"_hash ) );
			const auto t_head = systems::g_convars.get<float>( CONVAR( "mp_damage_scale_t_head"_hash ) );
			const auto ct_body = systems::g_convars.get<float>( CONVAR( "mp_damage_scale_ct_body"_hash ) );
			const auto t_body = systems::g_convars.get<float>( CONVAR( "mp_damage_scale_t_body"_hash ) );

			const auto is_ct = ( team == 3 );
			const auto head_scale = is_ct ? ct_head : t_head;
			const auto body_scale = is_ct ? ct_body : t_body;

			switch ( hitgroup )
			{
			case 1:
				damage *= headshot_multiplier * head_scale;
				break;
			case 2:
			case 4:
			case 5:
			case 8:
				damage *= body_scale;
				break;
			case 3:
				damage *= 1.25f * body_scale;
				break;
			case 6:
			case 7:
				damage *= 0.75f * body_scale;
				break;
			default:
				break;
			}

			const auto is_head = ( hitgroup == 1 );
			const auto is_armored = ( hitgroup >= 1 && hitgroup <= 5 ) || ( hitgroup == 8 );

			if ( armor <= 0 || !is_armored || ( is_head && !has_helmet ) )
			{
				damage = std::floor( damage );
				return;
			}

			constexpr auto armor_bonus{ 0.5f };
			const auto armor_ratio_scaled = armor_ratio * 0.5f;

			auto damage_to_health = damage * armor_ratio_scaled;
			auto damage_to_armor = ( damage - damage_to_health ) * armor_bonus;

			if ( damage_to_armor > static_cast< float >( armor ) )
			{
				damage_to_health = damage - ( static_cast< float >( armor ) / armor_bonus );
			}

			damage = std::floor( damage_to_health );
		}

	} // namespace detail

	void shared::penetration::prepare( std::uintptr_t weapon_vdata, std::uintptr_t weapon )
	{
		if ( !weapon_vdata || !weapon )
		{
			return;
		}

		this->m_weapon_data = weapon_data
		{
			.damage = static_cast< float >( g::memory.read<int>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_nDamage"_hash ) ) ),
			.penetration = g::memory.read<float>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flPenetration"_hash ) ),
			.range_modifier = g::memory.read<float>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flRangeModifier"_hash ) ),
			.range = g::memory.read<float>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flRange"_hash ) ),
			.armor_ratio = g::memory.read<float>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flArmorRatio"_hash ) ),
			.headshot_multiplier = g::memory.read<float>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flHeadshotMultiplier"_hash ) )
		};
	}

	bool shared::penetration::run( const math::vector3& start, const math::vector3& end, const systems::collector::player& target, const systems::bones::data& bones, result& out ) const
	{
		if ( this->m_weapon_data.damage <= 0.0f )
		{
			return false;
		}

		const auto direction = ( end - start ).normalized( );
		const auto max_range = this->m_weapon_data.range;
		const auto ray_end = start + direction * max_range;

		const auto all_hits = systems::g_bvh.trace_ray_all( start, ray_end );
		const auto segments = systems::g_bvh.build_segments( all_hits, max_range );

		auto current_damage = this->m_weapon_data.damage;
		auto penetration_count{ 4 };

		auto check_target = [ & ]( const math::vector3& seg_start, float seg_start_dist, float seg_end_dist ) -> bool
			{
				int closest_hb = -1;
				float best_dist = 1e18f;
				float best_damage = 0.0f;

				for ( const auto& hb : target.hitboxes )
				{
					if ( hb.index < 0 || hb.bone < 0 || hb.radius <= 0.0f )
					{
						continue;
					}

					const auto& bone = bones.bones[ hb.bone ];
					const auto center_local = ( hb.mins + hb.maxs ) * 0.5f;
					const auto center_world = bone.position + math::helpers::rotate_by_quat( bone.rotation, center_local );

					const auto half_extent = ( hb.maxs - hb.mins ) * 0.5f;
					const auto longest = std::max( { std::abs( half_extent.x ), std::abs( half_extent.y ), std::abs( half_extent.z ) } );

					math::vector3 axis_local{};
					if ( std::abs( half_extent.x ) >= std::abs( half_extent.y ) && std::abs( half_extent.x ) >= std::abs( half_extent.z ) )
						axis_local = { longest, 0.0f, 0.0f };
					else if ( std::abs( half_extent.y ) >= std::abs( half_extent.z ) )
						axis_local = { 0.0f, longest, 0.0f };
					else
						axis_local = { 0.0f, 0.0f, longest };

					const auto axis_world = math::helpers::rotate_by_quat( bone.rotation, axis_local );
					const auto capsule_start = center_world - axis_world;
					const auto capsule_end = center_world + axis_world;

					if ( !detail::ray_hits_capsule( seg_start, direction, capsule_start, capsule_end, hb.radius ) )
					{
						continue;
					}

					const auto to_center = center_world - seg_start;
					const auto hit_dist = to_center.dot( direction );

					if ( hit_dist < 0.0f || ( seg_start_dist + hit_dist ) > seg_end_dist )
					{
						continue;
					}

					if ( hit_dist < best_dist )
					{
						const auto total_dist = seg_start_dist + hit_dist;
						auto damage = current_damage * std::pow( this->m_weapon_data.range_modifier, total_dist / 500.0f ); // Falloff is usually per 500 units in CS

						if ( damage < 1.0f )
							continue;

						const auto hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( hb.index );
						detail::scale_damage( hitgroup, target.armor, target.has_helmet, target.team, this->m_weapon_data.armor_ratio, this->m_weapon_data.headshot_multiplier, damage );

						if ( damage >= 1.0f )
						{
							best_dist = hit_dist;
							closest_hb = hb.index;
							best_damage = damage;
						}
					}
				}

				if ( closest_hb != -1 )
				{
					out.damage = best_damage;
					out.hitbox = closest_hb;
					out.penetrated = ( penetration_count < 4 );
					return true;
				}

				return false;
			};

		const auto first_wall = segments.empty( ) ? max_range : segments[ 0 ].enter_distance;
		if ( check_target( start, 0.0f, first_wall ) )
		{
			return true;
		}

		for ( std::size_t si = 0; si < segments.size( ); ++si )
		{
			const auto& seg = segments[ si ];

			auto pen_mod = seg.min_pen_mod;
			const auto enter_type = seg.enter_surface.surface_type;
			const auto exit_type = seg.exit_surface.surface_type;

			if ( enter_type != exit_type )
			{
				pen_mod = std::min( pen_mod, seg.exit_surface.penetration );
			}

			const auto thickness = seg.thickness;

			if ( seg.exit_distance > 3000.0f || pen_mod < 0.1f )
			{
				penetration_count = 0;
			}

			if ( penetration_count <= 0 )
			{
				return false;
			}

			auto damage_modifier{ 0.16f };

			if ( pen_mod >= 0.1f && enter_type == exit_type )
			{
				if ( ( ( enter_type - 85 ) & 0xfffffffd ) == 0 )
				{
					pen_mod = 3.0f;
				}
				else if ( enter_type == 76 )
				{
					pen_mod = 2.0f;
				}

				if ( thickness < 6.0f )
				{
					if ( enter_type == 71 || enter_type == 89 )
					{
						damage_modifier = 0.05f;
						pen_mod = 3.0f;
					}
				}
			}

			const auto inv_pen = 1.0f / pen_mod;
			const auto base_loss = damage_modifier * current_damage;
			const auto pen_loss = std::max( 0.0f, ( 3.0f / this->m_weapon_data.penetration ) * 1.25f ) * ( inv_pen * 3.0f );
			const auto dist_loss = ( thickness * thickness * inv_pen ) / 24.0f;

			current_damage -= ( base_loss + pen_loss + dist_loss );

			if ( current_damage < 1.0f )
			{
				return false;
			}

			--penetration_count;

			const auto next_wall = ( si + 1 < segments.size( ) ) ? segments[ si + 1 ].enter_distance : max_range;

			if ( check_target( seg.exit_pos, seg.exit_distance, next_wall ) )
			{
				return true;
			}
		}

		out = {};
		return false;
	}

	bool shared::penetration::can(const math::vector3& start, const math::vector3& direction, float& out_damage) const
	{
		out_damage = 0.0f;

		if (this->m_weapon_data.damage <= 0.0f)
		{
			return false;
		}

		const auto max_range = this->m_weapon_data.range;
		const auto ray_end = start + direction * max_range;

		const auto first_hit = systems::g_bvh.trace_ray(start, ray_end);
		if (!first_hit.hit)
		{
			return false;
		}

		const auto all_hits = systems::g_bvh.trace_ray_all(start, ray_end);
		const auto segments = systems::g_bvh.build_segments(all_hits, max_range);

		if (segments.empty())
		{
			if (first_hit.surface.penetration >= 0.1f && this->m_weapon_data.penetration > 0.0f)
			{
				out_damage = this->m_weapon_data.damage;
				return true;
			}

			return false;
		}

		const auto& seg = segments[0];

		auto pen_mod = seg.min_pen_mod;
		const auto enter_type = seg.enter_surface.surface_type;
		const auto exit_type = seg.exit_surface.surface_type;

		if (enter_type != exit_type)
		{
			pen_mod = std::min(pen_mod, seg.exit_surface.penetration);
		}

		if (seg.exit_distance > 3000.0f || pen_mod < 0.1f)
		{
			return false;
		}

		auto damage_modifier = 0.16f;

		if (pen_mod >= 0.1f && enter_type == exit_type)
		{
			if (((enter_type - 85) & 0xfffffffd) == 0)
			{
				pen_mod = 3.0f;
			}
			else if (enter_type == 76)
			{
				pen_mod = 2.0f;
			}

			if (seg.thickness < 6.0f)
			{
				if (enter_type == 71 || enter_type == 89)
				{
					damage_modifier = 0.05f;
					pen_mod = 3.0f;
				}
			}
		}

		const auto inv_pen = 1.0f / pen_mod;
		const auto base_loss = damage_modifier * this->m_weapon_data.damage;
		const auto pen_loss = std::max(0.0f, (3.0f / this->m_weapon_data.penetration) * 1.25f) * (inv_pen * 3.0f);
		const auto dist_loss = (seg.thickness * seg.thickness * inv_pen) / 24.0f;
		const auto remaining = this->m_weapon_data.damage - (base_loss + pen_loss + dist_loss);

		if (remaining < 1.0f)
		{
			return false;
		}

		out_damage = remaining;
		return true;
	}

	float shared::penetration::get_max_damage( int hitgroup, int target_armor, bool has_helmet, int target_team ) const
	{
		if ( this->m_weapon_data.damage <= 0.0f )
		{
			return 0.0f;
		}

		auto damage = this->m_weapon_data.damage;
		detail::scale_damage( hitgroup, target_armor, has_helmet, target_team, this->m_weapon_data.armor_ratio, this->m_weapon_data.headshot_multiplier, damage );
		return damage;
	}

	void shared::tick( )
	{
		context ctx{};

		const auto local_pawn = systems::g_local.pawn( );
		if ( !local_pawn )
		{
			std::unique_lock lock( this->m_ctx_mutex );
			this->m_ctx = {};
			return;
		}

		const auto weapon_services = g::memory.read<std::uintptr_t>( local_pawn + SCHEMA( "C_BasePlayerPawn", "m_pWeaponServices"_hash ) );
		if ( !weapon_services )
		{
			std::unique_lock lock( this->m_ctx_mutex );
			this->m_ctx = {};
			return;
		}

		const auto weapon_handle = g::memory.read<std::uint32_t>( weapon_services + SCHEMA( "CPlayer_WeaponServices", "m_hActiveWeapon"_hash ) );
		if ( !weapon_handle )
		{
			std::unique_lock lock( this->m_ctx_mutex );
			this->m_ctx = {};
			return;
		}

		ctx.weapon = systems::g_entities.lookup( weapon_handle );
		if ( !ctx.weapon )
		{
			std::unique_lock lock( this->m_ctx_mutex );
			this->m_ctx = {};
			return;
		}

		ctx.weapon_vdata = g::memory.read<std::uintptr_t>( ctx.weapon + SCHEMA( "C_BaseEntity", "m_nSubclassID"_hash ) + 0x8 );
		if ( !ctx.weapon_vdata )
		{
			std::unique_lock lock( this->m_ctx_mutex );
			this->m_ctx = {};
			return;
		}

		ctx.weapon_type = g::memory.read<std::uint32_t>( ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_WeaponType"_hash ) );
		ctx.item_def_idx = g::memory.read<std::uint16_t>( ctx.weapon + SCHEMA( "C_EconEntity", "m_AttributeManager"_hash ) + SCHEMA( "C_AttributeContainer", "m_Item"_hash ) + SCHEMA( "C_EconItemView", "m_iItemDefinitionIndex"_hash ) );
		ctx.num_bullets = g::memory.read<int>( ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_nNumBullets"_hash ) );
		ctx.accuracy_penalty = g::memory.read<float>( ctx.weapon + SCHEMA( "C_CSWeaponBase", "m_fAccuracyPenalty"_hash ) );

		ctx.inaccuracy = this->get_inaccuracy( local_pawn, ctx.weapon, ctx.weapon_vdata, systems::g_view.angles(), nullptr, nullptr);
		ctx.spread = this->get_spread( ctx.weapon_vdata );
		ctx.recoil_index = g::memory.read<float>( ctx.weapon + SCHEMA( "C_CSWeaponBase", "m_flRecoilIndex"_hash ) );
		ctx.is_reloading = g::memory.read<bool>( ctx.weapon + SCHEMA( "C_CSWeaponBase", "m_bInReload"_hash ) );

		const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
		if ( global_vars )
		{
			ctx.current_time = g::memory.read<float>( global_vars + 0x30 );
		}

		ctx.cycle_time = g::memory.read<float>( ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flCycleTime"_hash ) );
		ctx.last_shot_time = g::memory.read<float>( ctx.weapon + SCHEMA( "C_CSWeaponBase", "m_fLastShotTime"_hash ) );
		ctx.is_full_auto = g::memory.read<bool>( ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_bIsFullAuto"_hash ) );

		if ( ctx.weapon_type == cstypes::sniper )
		{
			ctx.weapon_ready = ( ctx.current_time - ctx.last_shot_time >= ctx.cycle_time );
			ctx.is_scoped = g::memory.read<bool>( local_pawn + SCHEMA( "C_CSPlayerPawn", "m_bIsScoped"_hash ) );
		}
		else
		{
			ctx.weapon_ready = true;
		}

		ctx.valid = true;

		this->m_pen.prepare( ctx.weapon_vdata, ctx.weapon );

		std::unique_lock lock( this->m_ctx_mutex );
		this->m_ctx = ctx;
	}

	float shared::calculate_hitchance( const math::vector3& eye_pos, const math::vector3& aim_angle, const systems::collector::player& target, const systems::bones::data& bones, const math::vector3& offset ) const
	{
		const auto& ctx = this->m_ctx;
		const auto total_spread = ctx.spread + ctx.inaccuracy;

		if ( total_spread < 0.0001f )
		{
			return 1.0f;
		}

		const auto range = this->m_pen.get_weapon_data( ).range;
		if ( range <= 0.0f )
		{
			return 0.0f;
		}

		math::vector3 forward{}, right{}, up{};
		aim_angle.to_directions( &forward, &right, &up );

		constexpr auto samples{ 1024 };
		auto hits{ 0 };

		for ( int seed = 0; seed < samples; ++seed )
		{
			const auto spread = this->calculate_spread( seed, ctx.inaccuracy, ctx.spread, ctx.recoil_index, ctx.item_def_idx, ctx.num_bullets );
			auto direction = forward + ( right * spread.x ) + ( up * spread.y );
			direction = direction.normalized( );

			for ( const auto& hb : target.hitboxes )
			{
				if ( hb.index < 0 || hb.bone < 0 )
				{
					continue;
				}

				const auto& bone = bones.bones[ hb.bone ];
				const auto center_local = ( hb.mins + hb.maxs ) * 0.5f;
				const auto center_world = bone.position + math::helpers::rotate_by_quat( bone.rotation, center_local ) + offset;

				const auto half_extent = ( hb.maxs - hb.mins ) * 0.5f;
				const auto longest = std::max( { std::abs( half_extent.x ), std::abs( half_extent.y ), std::abs( half_extent.z ) } );

				math::vector3 axis_local{};
				if ( std::abs( half_extent.x ) >= std::abs( half_extent.y ) && std::abs( half_extent.x ) >= std::abs( half_extent.z ) )
				{
					axis_local = { longest, 0.0f, 0.0f };
				}
				else if ( std::abs( half_extent.y ) >= std::abs( half_extent.z ) )
				{
					axis_local = { 0.0f, longest, 0.0f };
				}
				else
				{
					axis_local = { 0.0f, 0.0f, longest };
				}

				const auto axis_world = math::helpers::rotate_by_quat( bone.rotation, axis_local );
				const auto capsule_start = center_world - axis_world;
				const auto capsule_end = center_world + axis_world;

				if ( detail::ray_hits_capsule( eye_pos, direction, capsule_start, capsule_end, hb.radius ) )
				{
					++hits;
					break;
				}
			}

			const auto remaining = samples - ( seed + 1 );
			if ( hits + remaining < samples / 4 )
			{
				break;
			}
		}

		return static_cast< float >( hits ) / static_cast< float >( samples );
	}

	std::uint32_t shared::get_spread_seed( const math::vector3& angles, int tick ) const
	{
		struct
		{
			float pitch;
			float yaw;
			int player_render_tick;
		} buffer{};

		buffer.pitch = detail::quantize_angle( angles.x );
		buffer.yaw = detail::quantize_angle( angles.y );
		buffer.player_render_tick = tick;

		random::sha1 hash;
		hash.reset( );
		hash.update( &buffer, 12 );
		hash.final( );

		return hash.get_first_uint32( );
	}

	math::vector2 shared::calculate_spread( int seed, float inaccuracy, float spread, float recoil_index, int item_def_idx, int num_bullets ) const
	{
		constexpr std::uint16_t revolver_id{ 64 };
		constexpr std::uint16_t negev_id{ 28 };
		constexpr auto two_pi{ 2.0f * std::numbers::pi_v<float> };

		random::valve_rng rng;
		rng.seed( seed );

		auto inac_r = rng.random_float( 0.0f, 1.0f );
		auto inac_a = rng.random_float( 0.0f, two_pi );

		if ( item_def_idx == revolver_id && num_bullets == 1 )
		{
			inac_r = 1.0f - ( inac_r * inac_r );
		}
		else if ( item_def_idx == negev_id && recoil_index < 3.0f )
		{
			auto v = inac_r; auto c = 3;
			do { --c; v *= v; } while ( static_cast< float >( c ) > recoil_index );
			inac_r = 1.0f - v;
		}

		inac_r *= inaccuracy;

		auto spr_r = rng.random_float( 0.0f, 1.0f );
		auto spr_a = rng.random_float( 0.0f, two_pi );

		if ( item_def_idx == revolver_id && num_bullets == 1 )
		{
			spr_r = 1.0f - ( spr_r * spr_r );
		}
		else if ( item_def_idx == negev_id && recoil_index < 3.0f )
		{
			auto v = spr_r; auto c = 3;
			do { --c; v *= v; } while ( static_cast< float >( c ) > recoil_index );
			spr_r = 1.0f - v;
		}

		spr_r *= spread;

		return
		{
			std::cosf( spr_a ) * spr_r + std::cosf( inac_a ) * inac_r,
			std::sinf( spr_a ) * spr_r + std::sinf( inac_a ) * inac_r
		};
	}

	math::vector3 shared::extrapolate_stop( const math::vector3& pos ) const
	{
		const auto pawn = systems::g_local.pawn( );
		if ( !pawn )
		{
			return pos;
		}

		const auto movement_services = g::memory.read<std::uintptr_t>( pawn + SCHEMA( "C_BasePlayerPawn", "m_pMovementServices"_hash ) );
		if ( !movement_services )
		{
			return pos;
		}

		const auto flags = g::memory.read<std::uint32_t>( pawn + SCHEMA( "C_BaseEntity", "m_fFlags"_hash ) );
		if ( !( flags & 1 ) )
		{
			return pos;
		}

		auto velocity = g::memory.read<math::vector3>( pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );
		velocity.z = 0.0f;

		if ( velocity.length_2d( ) <= 1.0f )
		{
			return pos;
		}

		const auto surface_friction = g::memory.read<float>( movement_services + SCHEMA( "CPlayer_MovementServices_Humanoid", "m_flSurfaceFriction"_hash ) );
		const auto max_speed = g::memory.read<float>( movement_services + SCHEMA( "CPlayer_MovementServices", "m_flMaxspeed"_hash ) );
		const auto player_friction = g::memory.read<float>( pawn + SCHEMA( "C_BaseEntity", "m_flFriction"_hash ) );
		const auto current_time = this->m_ctx.current_time;
		const auto tick_complete_time = g::memory.read<float>( movement_services + SCHEMA( "CCSPlayer_MovementServices", "m_flOffsetTickCompleteTime"_hash ) );
		const auto stashed_speed = g::memory.read<float>( movement_services + SCHEMA( "CCSPlayer_MovementServices", "m_flOffsetTickStashedSpeed"_hash ) );

		const auto friction_cvar = systems::g_convars.get<float>( CONVAR( "sv_friction"_hash ) );
		const auto stopspeed_cvar = systems::g_convars.get<float>( CONVAR( "sv_stopspeed"_hash ) );
		const auto accelerate_cvar = systems::g_convars.get<float>( CONVAR( "sv_accelerate"_hash ) );
		const auto use_weapon_speed = systems::g_convars.get<bool>( CONVAR( "sv_accelerate_use_weapon_speed"_hash ) );
		const auto water_slow_cvar = systems::g_convars.get<float>( CONVAR( "sv_water_slow_amount"_hash ) );

		constexpr auto tick_interval{ 0.015625f };

		const auto buttons = g::memory.read<std::uintptr_t>( movement_services + SCHEMA( "CPlayer_MovementServices", "m_nButtons"_hash ) );
		const auto ducking_state = g::memory.read<bool>( movement_services + SCHEMA( "CCSPlayer_MovementServices", "m_bDucking"_hash ) );

		const auto is_ducking = ( flags & 2 ) || ducking_state || ( buttons & static_cast< std::uintptr_t >( cstypes::in_duck ) );
		const auto is_sprinting = !is_ducking && ( buttons & static_cast< std::uintptr_t >( cstypes::in_sprint ) );

		const auto water_level = g::memory.read<float>( pawn + SCHEMA( "C_BaseEntity", "m_flWaterLevel"_hash ) );
		const auto submerged = static_cast< std::uint32_t >( water_level * 4.0f + 1.0f ) >= 2;

		auto weapon_speed_ratio{ 1.0f };
		auto scoped_slowdown{ false };

		if ( use_weapon_speed && this->m_ctx.weapon_vdata )
		{
			const auto wpn_speed = g::memory.read<float>( this->m_ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flMaxSpeed"_hash ) );
			weapon_speed_ratio = std::fminf( 1.0f, wpn_speed / 250.0f );

			if ( this->m_ctx.weapon )
			{
				const auto zoom = g::memory.read<std::int32_t>( this->m_ctx.weapon + SCHEMA( "C_CSWeaponBaseGun", "m_zoomLevel"_hash ) );
				const auto zoom_count = g::memory.read<std::int32_t>( this->m_ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_nZoomLevels"_hash ) );

				scoped_slowdown = zoom > 0 && zoom_count > 1 && ( wpn_speed * 0.52f ) < 110.0f;
			}
		}

		const auto compute_friction = [ & ]( math::vector3& vel, bool first_tick )
			{
				const auto spd = first_tick && current_time <= tick_complete_time ? stashed_speed : vel.length( );
				if ( spd < 0.1f )
				{
					return;
				}

				const auto control = std::fmaxf( spd, stopspeed_cvar );
				const auto drop = control * friction_cvar * surface_friction * player_friction * tick_interval;
				const auto adjusted = std::fmaxf( spd - drop, 0.0f );

				if ( adjusted < spd )
				{
					vel *= adjusted / spd;
				}
			};

		const auto compute_acceleration = [ & ]( const math::vector3& vel, const math::vector3& dir, float wish_spd ) -> float
			{
				const auto base = std::fmaxf( 250.0f, wish_spd );
				auto factor{ 1.0f };
				auto cap = base;

				if ( use_weapon_speed )
				{
					cap = base * weapon_speed_ratio;

					if ( ( !is_ducking && !is_sprinting ) || scoped_slowdown )
					{
						factor = weapon_speed_ratio;
					}
				}

				auto accel_base = base;

				if ( submerged )
				{
					cap *= water_slow_cvar;
					accel_base = is_sprinting ? base : base * water_slow_cvar;
				}

				if ( is_ducking )
				{
					cap *= 0.34f;
					factor = std::fminf( 0.34f, factor );
				}

				auto final_cap = accel_base * factor;
				auto accel = accelerate_cvar;

				if ( is_sprinting && !scoped_slowdown )
				{
					final_cap *= 0.52f;

					const auto threshold = cap * 0.52f - 5.0f;
					const auto proj = std::fmaxf( 0.0f, vel.dot( dir ) );

					if ( proj > threshold )
					{
						const auto blend = ( proj - threshold ) / std::fmaxf( 0.001f, cap * 0.52f - threshold );
						accel *= std::fmaxf( 0.0f, 1.0f - std::fminf( 1.0f, blend ) );
					}
				}

				const auto gain = accel * tick_interval * final_cap * surface_friction;
				const auto current_proj = vel.dot( dir );

				return std::fminf( gain, std::fmaxf( 0.0f, -current_proj ) );
			};

		const auto stop_threshold = max_speed * 0.34f;
		auto sim_pos = pos;
		auto sim_vel = velocity;

		for ( auto tick = 0; tick < 20; ++tick )
		{
			if ( sim_vel.length_2d( ) <= stop_threshold )
			{
				break;
			}

			compute_friction( sim_vel, tick == 0 );

			if ( sim_vel.length_2d( ) <= stop_threshold )
			{
				break;
			}

			auto wish_dir = math::vector3{ -sim_vel.x, -sim_vel.y, 0.0f };
			const auto wish_len = wish_dir.length( );

			if ( wish_len > 0.0001f )
			{
				wish_dir *= 1.0f / wish_len;
			}

			const auto accel_amount = compute_acceleration( sim_vel, wish_dir, wish_len );

			sim_vel.x += wish_dir.x * accel_amount;
			sim_vel.y += wish_dir.y * accel_amount;

			sim_pos.x += sim_vel.x * tick_interval;
			sim_pos.y += sim_vel.y * tick_interval;
		}

		return sim_pos;
	}

	bool shared::is_sniper_accurate( ) const
	{
		if ( this->m_ctx.weapon_type != cstypes::sniper )
		{
			return true;
		}

		const auto pawn = systems::g_local.pawn( );
		if ( !pawn )
		{
			return false;
		}

		const auto flags = g::memory.read<std::uint32_t>( pawn + SCHEMA( "C_BaseEntity", "m_fFlags"_hash ) );
		if ( !( flags & 1 ) )
		{
			return true;
		}

		const auto camera_services = g::memory.read<std::uintptr_t>( pawn + SCHEMA( "C_BasePlayerPawn", "m_pCameraServices"_hash ) );
		if ( !camera_services )
		{
			return false;
		}

		auto scope_inaccuracy = g::memory.read<float>( camera_services + SCHEMA( "CCSPlayer_CameraServices", "m_vClientScopeInaccuracy"_hash ) );
		if ( scope_inaccuracy <= 1e-6f )
		{
			scope_inaccuracy = 0.0f;
		}

		switch ( this->m_ctx.item_def_idx )
		{
		case 40:  return scope_inaccuracy <= 0.00089f;
		case 9:   return scope_inaccuracy <= 0.0005f;
		case 38:  return scope_inaccuracy <= 0.0012f;
		case 11:  return scope_inaccuracy <= 0.0012f;
		default:  return scope_inaccuracy <= 0.00089f;
		}
	}

	float shared::get_prediction_time( ) const
	{
		const auto pawn = systems::g_local.pawn( );
		const auto controller = systems::g_local.controller( );

		if ( !pawn || !controller )
		{
			return 0.0f;
		}

		const auto ping = g::memory.read<std::int32_t>( controller + SCHEMA( "CCSPlayerController", "m_iPing"_hash ) );
		const auto latency = static_cast< float >( ping ) * 0.001f;
		const auto interp_time = g::memory.read<float>( pawn + 0x290 ); // client @ 48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 49 63 D8 48 8B F1

		return latency + interp_time;
	}

	float shared::get_spread( std::uintptr_t weapon_vdata ) const
	{
		return g::memory.read<float>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flSpread"_hash ) );
	}

	float shared::get_base_inaccuracy(std::uintptr_t weapon, std::uintptr_t weapon_vdata, std::uintptr_t pawn) const
	{
		const auto fire_mode = g::memory.read<int>(weapon + SCHEMA("C_CSWeaponBase", "m_weaponMode"_hash));
		const auto flags = g::memory.read<std::uint32_t>(pawn + SCHEMA("C_BaseEntity", "m_fFlags"_hash));
		const auto on_ground = (flags & 1) != 0;
		const auto crouching = (flags & 2) != 0;
		const auto move_type = g::memory.read<std::uint8_t>(pawn + SCHEMA("C_BaseEntity", "m_MoveType"_hash));

		const auto inaccuracy_crouch_pair = g::memory.read<std::pair<float, float>>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_flInaccuracyCrouch"_hash));
		const auto inaccuracy_stand_pair = g::memory.read<std::pair<float, float>>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_flInaccuracyStand"_hash));

		float base_inaccuracy{ 0.0f };

		if (move_type == 9)
		{
			const auto inaccuracy_ladder_pair = g::memory.read<std::pair<float, float>>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_flInaccuracyLadder"_hash));
			base_inaccuracy = (fire_mode ? inaccuracy_stand_pair.second : inaccuracy_stand_pair.first)
				+ (fire_mode ? inaccuracy_ladder_pair.second : inaccuracy_ladder_pair.first);
		}
		else
		{
			const auto recoil_index = g::memory.read<float>(weapon + SCHEMA("C_CSWeaponBase", "m_flRecoilIndex"_hash));
			const auto weapon_type = g::memory.read<int>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_WeaponType"_hash));

			float recovery_time{ 0.0f };

			if (weapon_type == 9) // sniper
			{
				recovery_time = g::memory.read<float>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_flRecoveryTimeStand"_hash));
			}
			else if (!on_ground)
			{
				recovery_time = g::memory.read<float>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_flRecoveryTimeCrouch"_hash)) * 4.0f;
			}
			else
			{
				const auto base_rec = crouching
					? g::memory.read<float>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_flRecoveryTimeCrouch"_hash))
					: g::memory.read<float>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_flRecoveryTimeStand"_hash));

				const auto final_rec = crouching
					? g::memory.read<float>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_flRecoveryTimeCrouchFinal"_hash))
					: g::memory.read<float>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_flRecoveryTimeStandFinal"_hash));

				if (final_rec == -1.0f)
				{
					recovery_time = base_rec;
				}
				else
				{
					const auto transition_start = g::memory.read<int>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_nRecoveryTransitionStartBullet"_hash));
					const auto transition_end = g::memory.read<int>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_nRecoveryTransitionEndBullet"_hash));

					if (recoil_index <= transition_start)
					{
						recovery_time = base_rec;
					}
					else if (recoil_index >= transition_end)
					{
						recovery_time = final_rec;
					}
					else
					{
						recovery_time = base_rec
							+ ((recoil_index - transition_start) / (transition_end - transition_start))
							* (final_rec - base_rec);
					}
				}
			}

			base_inaccuracy = crouching
				? (fire_mode ? inaccuracy_crouch_pair.second : inaccuracy_crouch_pair.first)
				: (fire_mode ? inaccuracy_stand_pair.second : inaccuracy_stand_pair.first);
		}

		return base_inaccuracy;
	}

	float shared::get_inaccuracy(std::uintptr_t pawn, std::uintptr_t weapon, std::uintptr_t weapon_vdata, const math::vector3& eye_angles, float* out_move_inaccuracy, float* out_air_inaccuracy) const
	{
		if (!pawn || !weapon || !weapon_vdata)
		{
			return 0.0f;
		}

		const auto forcespread = systems::g_convars.get<float>(CONVAR("weapon_accuracy_force_spread"_hash));
		if (forcespread > 0.0f)
		{
			return std::fminf(forcespread, 1.0f);
		}

		const auto nospread = systems::g_convars.get<bool>(CONVAR("weapon_accuracy_nospread"_hash));
		if (nospread)
		{
			return 0.0f;
		}

		const auto fire_mode = g::memory.read<int>(weapon + SCHEMA("C_CSWeaponBase", "m_weaponMode"_hash));
		const auto inaccuracy_penalty = g::memory.read<float>(weapon + SCHEMA("C_CSWeaponBase", "m_fAccuracyPenalty"_hash));

		const auto max_speed_pair = g::memory.read<std::pair<float, float>>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_flMaxSpeed"_hash));
		const auto max_speed = fire_mode ? max_speed_pair.second : max_speed_pair.first;

		const auto velocity = g::memory.read<math::vector3>(pawn + SCHEMA("C_BaseEntity", "m_vecVelocity"_hash));
		const auto speed = velocity.length_2d();

		const auto inaccuracy_move_pair = g::memory.read<std::pair<float, float>>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_flInaccuracyMove"_hash));
		const auto inaccuracy_move = fire_mode ? inaccuracy_move_pair.second : inaccuracy_move_pair.first;

		const auto is_walking = g::memory.read<bool>(pawn + SCHEMA("C_CSPlayerPawn", "m_bIsWalking"_hash));

		auto move_factor = features::combat::detail::remap_value(speed, max_speed * 0.34f, max_speed * 0.95f, 0.0f, 1.0f);
		if (move_factor > 0.0f)
		{
			if (!is_walking)
			{
				move_factor = std::powf(move_factor, 0.25f);
			}
		}

		const auto weapon_move_inaccuracy = move_factor * inaccuracy_move;

		if (out_move_inaccuracy)
			*out_move_inaccuracy = weapon_move_inaccuracy;

		auto total = inaccuracy_penalty + weapon_move_inaccuracy;

		const auto flags = g::memory.read<std::uint32_t>(pawn + SCHEMA("C_BaseEntity", "m_fFlags"_hash));
		const auto on_ground = (flags & 1) != 0;

		if (!on_ground)
		{
			const auto air_spread_scale = systems::g_convars.get<float>(CONVAR("weapon_air_spread_scale"_hash));
			const auto jump_initial = g::memory.read<float>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_flInaccuracyJumpInitial"_hash)) * air_spread_scale;
			const auto jump_apex = g::memory.read<float>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_flInaccuracyJumpApex"_hash)) * air_spread_scale;

			const auto impulse = systems::g_convars.get<float>(CONVAR("sv_jump_impulse"_hash));
			const auto jump_vel = std::sqrtf(std::fabsf(impulse));
			const auto cur_vel = std::sqrtf(std::fabsf(velocity.z));
			const auto lo = jump_vel * 0.25f;

			float air{ 0.0f };
			if (lo == jump_vel)  // only true when jump_vel == 0
			{
				air = (cur_vel >= jump_vel) ? jump_initial : jump_apex;
			}
			else
			{
				const auto frac = std::clamp((cur_vel - lo) / (jump_vel - lo), 0.0f, 1.0f);
				air = jump_apex + frac * (jump_initial - jump_apex);
			}

			const auto air_inaccuracy = (air >= 0.0f) ? std::fminf(jump_initial * 2.0f, air) : 0.0f;

			if (out_air_inaccuracy)
				*out_air_inaccuracy = air_inaccuracy;

			total += air_inaccuracy;
		}
		else
		{
			if (out_air_inaccuracy)
				*out_air_inaccuracy = 0.0f;
		}

		const auto num_bullets = g::memory.read<int>(weapon_vdata + SCHEMA("CCSWeaponBaseVData", "m_nNumBullets"_hash));
		const auto shotgun_patterns = systems::g_convars.get<bool>(CONVAR("weapon_accuracy_shotgun_spread_patterns"_hash));
		if (shotgun_patterns && num_bullets > 1)
		{
			total += 0.0f;
		}

		return std::fminf(1.0f, get_base_inaccuracy(weapon, weapon_vdata, pawn) + total);
	}

} // namespace features::combat