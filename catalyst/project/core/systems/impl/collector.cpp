#include <stdafx.hpp>

namespace systems {

	void collector::run( )
	{
		systems::g_view.update( );
		const auto raw = systems::g_entities.all( );

		this->collect_players( raw );
		this->collect_items( raw );
		this->collect_projectiles( raw );
	}

	std::vector<collector::player> collector::players( ) const
	{
		std::shared_lock lock( this->m_mutex );
		return this->m_players;
	}

	std::vector<collector::item> collector::items( ) const
	{
		std::shared_lock lock( this->m_mutex );
		return this->m_items;
	}

	std::vector<collector::projectile> collector::projectiles( ) const
	{
		std::shared_lock lock( this->m_mutex );
		return this->m_projectiles;
	}

	void collector::collect_players( const std::vector<entities::cached>& raw )
	{
		std::vector<player> fresh{};
		fresh.reserve( 64 );

		const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
		const auto current_time = global_vars ? g::memory.read<float>( global_vars + 0x30 ) : 0.0f;

		for ( const auto& entry : raw )
		{
			if ( entry.type != entities::type::player )
			{
				continue;
			}

			const auto player_pawn_handle = g::memory.read<std::uint32_t>( entry.ptr + SCHEMA( "CCSPlayerController", "m_hPlayerPawn"_hash ) );
			if ( !player_pawn_handle )
			{
				continue;
			}

			const auto player_pawn = systems::g_entities.lookup( player_pawn_handle );
			if ( !player_pawn || player_pawn == systems::g_local.view_pawn( ) )
			{
				continue;
			}

			const auto health = g::memory.read<std::int32_t>( player_pawn + SCHEMA( "C_BaseEntity", "m_iHealth"_hash ) );
			if ( health < 0 )
			{
				continue;
			}

			player p{};
			p.controller = entry.ptr;
			p.pawn = player_pawn;
			p.health = health;
			p.alive = g::memory.read<bool>( entry.ptr + SCHEMA( "CCSPlayerController", "m_bPawnIsAlive"_hash ) ) && health > 0;
			p.team = g::memory.read<std::int32_t>( player_pawn + SCHEMA( "C_BaseEntity", "m_iTeamNum"_hash ) );
			p.invulnerable = g::memory.read<bool>( player_pawn + SCHEMA( "C_CSPlayerPawn", "m_bGunGameImmunity"_hash ) );
			p.armor = g::memory.read<std::int32_t>( player_pawn + SCHEMA( "C_CSPlayerPawn", "m_ArmorValue"_hash ) );
			p.is_scoped = g::memory.read<bool>( player_pawn + SCHEMA( "C_CSPlayerPawn", "m_bIsScoped"_hash ) );
			p.is_defusing = g::memory.read<bool>( player_pawn + SCHEMA( "C_CSPlayerPawn", "m_bIsDefusing"_hash ) );
			p.is_flashed = g::memory.read<float>( player_pawn + SCHEMA( "C_CSPlayerPawnBase", "m_flFlashBangTime"_hash ) ) > 0.0f;
			p.ping = g::memory.read<std::int32_t>( entry.ptr + SCHEMA( "CCSPlayerController", "m_iPing"_hash ) );
			p.eye_angles = g::memory.read<math::vector3>( player_pawn + SCHEMA( "C_CSPlayerPawnBase", "m_angEyeAngles"_hash ) );
			p.view_offset = g::memory.read<math::vector3>( player_pawn + SCHEMA( "C_BaseModelEntity", "m_vecViewOffset"_hash ) );
			p.velocity = g::memory.read<math::vector3>( player_pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );
			const auto game_scene_node = g::memory.read<std::uintptr_t>(player_pawn + SCHEMA("C_BaseEntity", "m_pGameSceneNode"_hash));

			if ( game_scene_node )
			{
				p.game_scene_node = game_scene_node;
				p.bone_cache = g::memory.read<std::uintptr_t>( game_scene_node + SCHEMA( "CSkeletonInstance", "m_modelState"_hash ) + 0x80 );
				p.origin = g::memory.read<math::vector3>( game_scene_node + SCHEMA( "CGameSceneNode", "m_vecAbsOrigin"_hash ) );

				if ( p.bone_cache )
				{
					p.bones = systems::g_bones.get( p.bone_cache );
					const auto head = p.bones.get_position( 6 );
					p.is_visible = !systems::g_bvh.trace_ray( systems::g_view.origin( ), head ).hit;
				}

				p.hitboxes = systems::g_hitboxes.query( game_scene_node );
			}

			{
				auto& history = this->m_history[ p.pawn ];
				if ( history.last_update_time > 0.0f )
				{
					const auto delta_time = current_time - history.last_update_time;
					if ( delta_time > 0.001f && delta_time < 0.5f )
					{
						auto raw_acceleration = ( p.velocity - history.last_velocity ) / delta_time;
						
						raw_acceleration.clamp_length( 2500.0f );

						p.acceleration = ( raw_acceleration * 0.5f ) + ( history.last_acceleration * 0.5f );

						const auto ping = g::memory.read<std::int32_t>( entry.ptr + SCHEMA( "CCSPlayerController", "m_iPing"_hash ) );
						const auto latency = static_cast< float >( ping ) * 0.001f;
						const auto prediction_time = latency + 0.03125f; // Lag + 2 ticks buffer

						p.prediction_offset = ( p.velocity * prediction_time ) + ( p.acceleration * 0.5f * prediction_time * prediction_time );
					}
				}

				history.last_origin = p.origin;
				history.last_acceleration = p.acceleration;
				history.last_velocity = p.velocity;
				history.last_update_time = current_time;
			}

			const auto item_services = g::memory.read<std::uintptr_t>( player_pawn + SCHEMA( "C_BasePlayerPawn", "m_pItemServices"_hash ) );
			if ( item_services )
			{
				p.has_helmet = g::memory.read<bool>( item_services + SCHEMA( "CCSPlayer_ItemServices", "m_bHasHelmet"_hash ) );
				p.has_defuser = g::memory.read<bool>( item_services + SCHEMA( "CCSPlayer_ItemServices", "m_bHasDefuser"_hash ) );
			}

			const auto weapon_services = g::memory.read<std::uintptr_t>( player_pawn + SCHEMA( "C_BasePlayerPawn", "m_pWeaponServices"_hash ) );
			if ( weapon_services )
			{
				const auto active_weapon_handle = g::memory.read<std::uint32_t>( weapon_services + SCHEMA( "CPlayer_WeaponServices", "m_hActiveWeapon"_hash ) );
				if ( active_weapon_handle && active_weapon_handle != 0xFFFFFFFF )
				{
					p.weapon.ptr = systems::g_entities.lookup( active_weapon_handle );
					if ( p.weapon.ptr )
					{
						p.weapon.vdata = g::memory.read<std::uintptr_t>( p.weapon.ptr + SCHEMA( "C_BaseEntity", "m_nSubclassID"_hash ) + 0x8 );
						if ( p.weapon.vdata )
						{
							p.weapon.ammo = g::memory.read<std::int32_t>( p.weapon.ptr + SCHEMA( "C_BasePlayerWeapon", "m_iClip1"_hash ) );
							p.weapon.max_ammo = g::memory.read<std::int32_t>( p.weapon.vdata + SCHEMA( "CBasePlayerWeaponVData", "m_iMaxClip1"_hash ) );

							const auto weapon_name_ptr = g::memory.read<std::uintptr_t>( p.weapon.vdata + SCHEMA( "CCSWeaponBaseVData", "m_szName"_hash ) );
							if ( weapon_name_ptr )
							{
								p.weapon.name = g::memory.read_string( weapon_name_ptr, 64 );
								if ( p.weapon.name.starts_with( "weapon_" ) )
								{
									p.weapon.name.erase( 0, 7 );
								}
							}
						}
					}
				}
			}

			const auto name_ptr = g::memory.read<std::uintptr_t>( entry.ptr + SCHEMA( "CCSPlayerController", "m_sSanitizedPlayerName"_hash ) );
			if ( name_ptr )
			{
				p.display_name = g::memory.read_string( name_ptr, 128 );
				std::ranges::transform( p.display_name, p.display_name.begin( ), [ ]( unsigned char c ) { return std::tolower( c ); } );
			}

			const auto money_services = g::memory.read<std::uintptr_t>( entry.ptr + SCHEMA( "CCSPlayerController", "m_pInGameMoneyServices"_hash ) );
			if ( money_services )
			{
				p.money = g::memory.read<std::int32_t>( money_services + SCHEMA( "CCSPlayerController_InGameMoneyServices", "m_iAccount"_hash ) );
			}

			p.last_update_time = current_time;
			p.last_update_tick = std::chrono::steady_clock::now( );

			fresh.push_back( std::move( p ) );
		}

		{
			for ( auto it = this->m_history.begin( ); it != this->m_history.end( ); )
			{
				bool found = false;
				for ( const auto& p : fresh )
				{
					if ( p.pawn == it->first )
					{
						found = true;
						break;
					}
				}

				if ( !found ) it = this->m_history.erase( it );
				else ++it;
			}
		}

		{
			const auto view_origin = systems::g_view.origin( );
			std::ranges::sort( fresh, [ &view_origin ]( const player& a, const player& b ) { return view_origin.distance( a.origin ) > view_origin.distance( b.origin ); } );
		}

		std::unique_lock lock( this->m_mutex );
		this->m_players = std::move( fresh );
	}

	void collector::collect_items( const std::vector<entities::cached>& raw )
	{
		std::vector<item> fresh{};
		fresh.reserve( 64 );

		for ( const auto& entry : raw )
		{
			if ( entry.type != entities::type::item )
			{
				continue;
			}

			const auto subtype = classify_item( entry.schema_hash );
			if ( subtype == item_subtype::unknown )
			{
				continue;
			}

			const auto owner_handle = g::memory.read<std::uint32_t>( entry.ptr + SCHEMA( "C_BaseEntity", "m_hOwnerEntity"_hash ) );
			if ( owner_handle && owner_handle != 0xffffffff )
			{
				continue;
			}

			const auto game_scene_node = g::memory.read<std::uintptr_t>( entry.ptr + SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );
			if ( !game_scene_node )
			{
				continue;
			}

			item i{};
			i.entity = entry.ptr;
			i.game_scene_node = game_scene_node;
			i.subtype = subtype;
			i.origin = g::memory.read<math::vector3>( game_scene_node + SCHEMA( "CGameSceneNode", "m_vecAbsOrigin"_hash ) );
			i.ammo = g::memory.read<std::int32_t>( entry.ptr + SCHEMA( "C_BasePlayerWeapon", "m_iClip1"_hash ) );

			const auto weapon_vdata = g::memory.read<std::uintptr_t>( entry.ptr + SCHEMA( "C_BaseEntity", "m_nSubclassID"_hash ) + 0x8 );
			if ( weapon_vdata )
			{
				i.max_ammo = g::memory.read<std::int32_t>( weapon_vdata + SCHEMA( "CBasePlayerWeaponVData", "m_iMaxClip1"_hash ) );
			}

			fresh.push_back( std::move( i ) );
		}

		{
			const auto view_origin = systems::g_view.origin( );
			std::ranges::sort( fresh, [ &view_origin ]( const item& a, const item& b ) { return view_origin.distance( a.origin ) > view_origin.distance( b.origin ); } );
		}

		std::unique_lock lock( this->m_mutex );
		this->m_items = std::move( fresh );
	}

	void collector::collect_projectiles( const std::vector<entities::cached>& raw )
	{
		std::vector<projectile> fresh{};
		fresh.reserve( 32 );

		const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
		const auto current_time = global_vars ? g::memory.read<float>( global_vars + 0x30 ) : 0.0f;

		for ( const auto& entry : raw )
		{
			if ( entry.type != entities::type::projectile )
			{
				continue;
			}

			const auto subtype = classify_projectile( entry.schema_hash );
			if ( subtype == projectile_subtype::unknown )
			{
				continue;
			}

			if ( subtype == projectile_subtype::molotov_fire )
			{
				const auto fire_count = g::memory.read<int>( entry.ptr + SCHEMA( "C_Inferno", "m_fireCount"_hash ) );
				if ( fire_count <= 0 )
				{
					continue;
				}

				const auto fire_positions_base = entry.ptr + SCHEMA( "C_Inferno", "m_firePositions"_hash );
				const auto fire_active_base = entry.ptr + SCHEMA( "C_Inferno", "m_bFireIsBurning"_hash );

				std::vector<math::vector3> fire_points{};
				fire_points.reserve( std::min( fire_count, 64 ) );

				for ( int i = 0; i < std::min( fire_count, 64 ); ++i )
				{
					if ( !g::memory.read<bool>( fire_active_base + i ) )
					{
						continue;
					}

					fire_points.push_back( g::memory.read<math::vector3>( fire_positions_base + i * sizeof( math::vector3 ) ) );
				}

				if ( fire_points.empty( ) )
				{
					continue;
				}

				auto center = math::vector3{};

				for ( const auto& pt : fire_points )
				{
					center = center + pt;
				}

				center = center * ( 1.0f / static_cast< float >( fire_points.size( ) ) );

				const auto effect_tick = g::memory.read<std::int32_t>( entry.ptr + SCHEMA( "C_Inferno", "m_nFireEffectTickBegin"_hash ) );
				const auto start_time = static_cast< float >( effect_tick ) * ( 1.0f / 64.0f );
				constexpr auto inferno_duration = 7.0f;

				projectile p{};
				p.entity = entry.ptr;
				p.subtype = subtype;
				p.origin = center;
				p.fire_points = std::move( fire_points );
				p.expire_time = start_time + inferno_duration;

				fresh.push_back( std::move( p ) );
				continue;
			}

			const auto game_scene_node = g::memory.read<std::uintptr_t>( entry.ptr + SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );
			if ( !game_scene_node )
			{
				continue;
			}

			projectile p{};
			p.entity = entry.ptr;
			p.game_scene_node = game_scene_node;
			p.subtype = subtype;
			p.origin = g::memory.read<math::vector3>( game_scene_node + SCHEMA( "CGameSceneNode", "m_vecAbsOrigin"_hash ) );
			p.velocity = g::memory.read<math::vector3>( entry.ptr + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );
			p.thrower_handle = g::memory.read<std::uint32_t>( entry.ptr + SCHEMA( "C_BaseGrenade", "m_hThrower"_hash ) );
			p.bounces = g::memory.read<std::int32_t>( entry.ptr + SCHEMA( "C_BaseCSGrenadeProjectile", "m_nBounces"_hash ) );

			if ( subtype == projectile_subtype::he_grenade || subtype == projectile_subtype::flashbang )
			{
				// In CS2, some projectiles use m_nExplodeEffectTickBegin but others might use m_bExplodeEffectBegan or similar
				const auto detonate_tick = g::memory.read<std::int32_t>( entry.ptr + SCHEMA( "C_BaseCSGrenadeProjectile", "m_nExplodeEffectTickBegin"_hash ) );
				p.detonated = detonate_tick > 0;
			}
			else if ( subtype == projectile_subtype::smoke_grenade )
			{
				p.effect_tick_begin = g::memory.read<std::int32_t>( entry.ptr + SCHEMA( "C_SmokeGrenadeProjectile", "m_nSmokeEffectTickBegin"_hash ) );
				p.smoke_active = g::memory.read<bool>( entry.ptr + SCHEMA( "C_SmokeGrenadeProjectile", "m_bDidSmokeEffect"_hash ) );
			}
			else if ( subtype == projectile_subtype::decoy )
			{
				p.effect_tick_begin = g::memory.read<std::int32_t>( entry.ptr + SCHEMA( "C_DecoyProjectile", "m_nDecoyShotTick"_hash ) );
			}

			fresh.push_back( std::move( p ) );
		}

		{
			const auto view_origin = systems::g_view.origin( );
			std::ranges::sort( fresh, [ &view_origin ]( const projectile& a, const projectile& b ) { return view_origin.distance( a.origin ) > view_origin.distance( b.origin ); } );
		}

		std::unique_lock lock( this->m_mutex );
		this->m_projectiles = std::move( fresh );
	}

	collector::item_subtype collector::classify_item( std::uint32_t schema_hash )
	{
		switch ( schema_hash )
		{
		case "C_AK47"_hash:                return item_subtype::ak47;
		case "C_WeaponM4A1"_hash:          return item_subtype::m4a4;
		case "C_WeaponM4A1Silencer"_hash:  return item_subtype::m4a1s;
		case "C_WeaponAWP"_hash:           return item_subtype::awp;
		case "C_WeaponAug"_hash:           return item_subtype::aug;
		case "C_WeaponFamas"_hash:         return item_subtype::famas;
		case "C_WeaponGalilAR"_hash:       return item_subtype::galil_ar;
		case "C_WeaponSG556"_hash:         return item_subtype::sg553;
		case "C_WeaponG3SG1"_hash:         return item_subtype::g3sg1;
		case "C_WeaponSCAR20"_hash:        return item_subtype::scar20;
		case "C_WeaponSSG08"_hash:         return item_subtype::ssg08;
		case "C_WeaponMAC10"_hash:         return item_subtype::mac10;
		case "C_WeaponMP5SD"_hash:         return item_subtype::mp5sd;
		case "C_WeaponMP7"_hash:           return item_subtype::mp7;
		case "C_WeaponMP9"_hash:           return item_subtype::mp9;
		case "C_WeaponBizon"_hash:         return item_subtype::pp_bizon;
		case "C_WeaponP90"_hash:           return item_subtype::p90;
		case "C_WeaponUMP45"_hash:         return item_subtype::ump45;
		case "C_WeaponNOVA"_hash:          return item_subtype::nova;
		case "C_WeaponSawedoff"_hash:      return item_subtype::sawed_off;
		case "C_WeaponXM1014"_hash:        return item_subtype::xm1014;
		case "C_WeaponMag7"_hash:          return item_subtype::mag7;
		case "C_WeaponM249"_hash:          return item_subtype::m249;
		case "C_WeaponNegev"_hash:         return item_subtype::negev;
		case "C_DEagle"_hash:              return item_subtype::deagle;
		case "C_WeaponElite"_hash:         return item_subtype::dual_berettas;
		case "C_WeaponFiveSeven"_hash:     return item_subtype::five_seven;
		case "C_WeaponGlock"_hash:         return item_subtype::glock;
		case "C_WeaponHKP2000"_hash:       return item_subtype::p2000;
		case "C_WeaponUSPSilencer"_hash:   return item_subtype::usps;
		case "C_WeaponP250"_hash:          return item_subtype::p250;
		case "C_WeaponCZ75a"_hash:         return item_subtype::cz75;
		case "C_WeaponTec9"_hash:          return item_subtype::tec9;
		case "C_WeaponRevolver"_hash:      return item_subtype::r8_revolver;
		case "C_WeaponTaser"_hash:         return item_subtype::taser;
		case "C_Knife"_hash:               return item_subtype::knife;
		case "C_C4"_hash:                  return item_subtype::c4;
		case "C_Item_Healthshot"_hash:     return item_subtype::healthshot;
		case "C_HEGrenade"_hash:           return item_subtype::he_grenade;
		case "C_Flashbang"_hash:           return item_subtype::flashbang;
		case "C_SmokeGrenade"_hash:        return item_subtype::smoke_grenade;
		case "C_MolotovGrenade"_hash:      return item_subtype::molotov;
		case "C_IncendiaryGrenade"_hash:   return item_subtype::incendiary;
		case "C_DecoyGrenade"_hash:        return item_subtype::decoy;
		default:                           return item_subtype::unknown;
		}
	}

	collector::projectile_subtype collector::classify_projectile( std::uint32_t schema_hash )
	{
		switch ( schema_hash )
		{
		case "C_HEGrenadeProjectile"_hash:    return projectile_subtype::he_grenade;
		case "C_FlashbangProjectile"_hash:    return projectile_subtype::flashbang;
		case "C_SmokeGrenadeProjectile"_hash: return projectile_subtype::smoke_grenade;
		case "C_MolotovProjectile"_hash:      return projectile_subtype::molotov;
		case "C_IncendiaryGrenadeProjectile"_hash: return projectile_subtype::incendiary;
		case "CIncendiaryGrenadeProjectile"_hash: return projectile_subtype::incendiary;
		case "C_Inferno"_hash:                return projectile_subtype::molotov_fire;
		case "C_DecoyProjectile"_hash:        return projectile_subtype::decoy;
		default:                              return projectile_subtype::unknown;
		}
	}

} // namespace systems