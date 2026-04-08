#include <stdafx.hpp>
#include <fstream>
#include <sstream>

namespace features::misc {

	void nade_helper::on_render( zdraw::draw_list& draw_list )
	{
		if ( !settings::g_misc.m_nade_helper.enabled )
		{
			return;
		}

		if ( this->m_nades.empty( ) )
		{
			return;
		}

		const auto local_pawn = systems::g_local.view_pawn( );
		if ( !local_pawn )
		{
			return;
		}

		const auto weapon_vdata = systems::g_local.weapon_vdata( );
		if ( !weapon_vdata )
			return;

		const auto weapon_name_ptr = g::memory.read<std::uintptr_t>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_szName"_hash ) );
		if ( !weapon_name_ptr )
			return;

		const auto current_weapon_name = g::memory.read_string( weapon_name_ptr, 64 );
		
		auto get_current_type = [ & ]( ) -> int {
			if ( current_weapon_name.find( "hegrenade" ) != std::string::npos ) return 0;
			if ( current_weapon_name.find( "flashbang" ) != std::string::npos ) return 1;
			if ( current_weapon_name.find( "smokegrenade" ) != std::string::npos ) return 2;
			if ( current_weapon_name.find( "molotov" ) != std::string::npos ) return 3;
			if ( current_weapon_name.find( "incendiary" ) != std::string::npos ) return 4;
			if ( current_weapon_name.find( "decoy" ) != std::string::npos ) return 5;
			return -1;
		};

		const int current_nade_type = get_current_type( );
		if ( current_nade_type == -1 )
			return;

		const auto game_scene_node = g::memory.read<std::uintptr_t>( local_pawn + SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );
		if ( !game_scene_node )
		{
			return;
		}

		const auto local_origin = g::memory.read<math::vector3>( game_scene_node + SCHEMA( "CGameSceneNode", "m_vecAbsOrigin"_hash ) );
		const auto& cfg = settings::g_misc.m_nade_helper;

		for ( const auto& nade : this->m_nades )
		{
			// only show if holding proper grenade
			if ( nade.nade_type != current_nade_type )
				continue;

			const auto dist = local_origin.distance( nade.pos );

			// render stand position (circle on ground)
			const auto stand_screen = systems::g_view.project( nade.pos );
			if ( systems::g_view.projection_valid( stand_screen ) )
			{
				const auto alpha = std::clamp( 1.0f - ( dist / 1000.0f ), 0.0f, 1.0f );
				if ( alpha > 0.0f )
				{
					zdraw::rgba col = cfg.stand_pos_color;
					col.a = static_cast< std::uint8_t >( col.a * alpha );

					// draw circle
					constexpr int segments = 32;
					const float radius = cfg.stand_radius;
					
					std::vector<math::vector2> points{};
					for ( int i = 0; i <= segments; ++i )
					{
						float angle = ( static_cast< float >( i ) / segments ) * std::numbers::pi_v<float> * 2.0f;
						math::vector3 p = nade.pos + math::vector3{ std::cos( angle ) * radius, std::sin( angle ) * radius, 2.0f };
						auto s = systems::g_view.project( p );
						if ( systems::g_view.projection_valid( s ) )
						{
							points.push_back( s );
						}
					}

					if ( points.size( ) > 2 )
					{
						for ( size_t i = 0; i < points.size( ) - 1; ++i )
						{
							draw_list.add_line( points[ i ].x, points[ i ].y, points[ i + 1 ].x, points[ i + 1 ].y, col, 2.0f );
						}
					}

					if ( cfg.show_name && dist < 500.0f )
					{
						auto [tw, th] = zdraw::measure_text( nade.name.c_str( ) );
						draw_list.add_text( stand_screen.x - tw * 0.5f, stand_screen.y + 10.0f, nade.name.c_str( ), zdraw::get_font( ), cfg.text_color );
					}
				}
			}

			// if we are close to the stand position, show the aim position
			if ( dist < 15.0f )
			{
				const auto aim_screen = systems::g_view.project( nade.target_pos );
				if ( systems::g_view.projection_valid( aim_screen ) )
				{
					draw_list.add_rect_filled( aim_screen.x - cfg.aim_dot_size, aim_screen.y - cfg.aim_dot_size, cfg.aim_dot_size * 2.0f, cfg.aim_dot_size * 2.0f, cfg.aim_pos_color );
					
					std::string display_str;
					if ( cfg.show_name )
						display_str += nade.name;
					
					if ( cfg.show_type )
					{
						static const char* throw_types[ ]{ "STAND", "JUMP", "WALK", "RUN", "CROUCH", "CROUCH JUMP" };
						const char* type_str = ( nade.throw_type >= 0 && nade.throw_type < 6 ) ? throw_types[ nade.throw_type ] : "UNKNOWN";
						
						if ( !display_str.empty( ) )
							display_str += " - ";
						display_str += type_str;
					}

					if ( !display_str.empty( ) )
					{
						auto [tw, th] = zdraw::measure_text( display_str.c_str() );
						draw_list.add_text( aim_screen.x - tw * 0.5f, aim_screen.y - th - 10.0f, display_str.c_str(), zdraw::get_font( ), cfg.text_color );
					}
				}
			}
		}
	}

	void nade_helper::tick( )
	{
		const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
		if ( !global_vars )
		{
			return;
		}

		const auto map_ptr = g::memory.read<std::uintptr_t>( global_vars + 0x188 );
		const auto current_map = map_ptr ? g::memory.read_string( map_ptr ) : std::string{};

		if ( !current_map.empty( ) && current_map != "<empty>" && current_map != this->m_current_map )
		{
			this->m_current_map = current_map;
			this->load_nades( current_map );
		}
	}

	void nade_helper::load_nades( const std::string& map_name )
	{
		this->m_nades.clear( );

		std::string actual_map = map_name;
		if ( actual_map.empty( ) )
		{
			actual_map = this->m_current_map;
		}

		if ( actual_map.empty( ) )
		{
			return;
		}

		// sanitize map name
		size_t last_slash = actual_map.find_last_of( "/\\" );
		if ( last_slash != std::string::npos )
		{
			actual_map = actual_map.substr( last_slash + 1 );
		}

		if ( actual_map.ends_with( ".vpk" ) ) actual_map.erase( actual_map.size( ) - 4 );
		if ( actual_map.ends_with( ".vmap" ) ) actual_map.erase( actual_map.size( ) - 5 );

		std::filesystem::path nade_path = g::config.get_path( ) / "nades";
		if ( !std::filesystem::exists( nade_path ) )
		{
			std::filesystem::create_directories( nade_path );
		}

		std::filesystem::path file = nade_path / ( actual_map + ".txt" );
		std::ifstream in( file );
		if ( !in.is_open( ) )
		{
			return;
		}

		std::string line;
		while ( std::getline( in, line ) )
		{
			std::stringstream ss( line );
			std::string segment;
			std::vector<std::string> parts{};

			while ( std::getline( ss, segment, '|' ) )
			{
				parts.push_back( segment );
			}

			if ( parts.size( ) >= 8 )
			{
				nade_data n{};
				n.name = parts[ 0 ];
				n.pos.x = std::stof( parts[ 1 ] );
				n.pos.y = std::stof( parts[ 2 ] );
				n.pos.z = std::stof( parts[ 3 ] );
				n.target_pos.x = std::stof( parts[ 4 ] );
				n.target_pos.y = std::stof( parts[ 5 ] );
				n.target_pos.z = std::stof( parts[ 6 ] );
				n.throw_type = std::stoi( parts[ 7 ] );
				n.nade_type = ( parts.size( ) > 8 ) ? std::stoi( parts[ 8 ] ) : 0;
				this->m_nades.push_back( n );
			}
		}
		in.close( );
		g::console.print( "loaded {} nades for {}", this->m_nades.size( ), actual_map );
	}

	void nade_helper::save_nades( const std::string& map_name )
	{
		std::string actual_map = map_name;
		if ( actual_map.empty( ) )
		{
			if ( this->m_current_map.empty( ) ) {
				this->tick( );
			}
			actual_map = this->m_current_map;
		}

		if ( actual_map.empty( ) )
		{
			g::console.error( "cannot save nades: no current map detected." );
			return;
		}

		// sanitize map name
		size_t last_slash = actual_map.find_last_of( "/\\" );
		if ( last_slash != std::string::npos )
		{
			actual_map = actual_map.substr( last_slash + 1 );
		}

		if ( actual_map.ends_with( ".vpk" ) ) actual_map.erase( actual_map.size( ) - 4 );
		if ( actual_map.ends_with( ".vmap" ) ) actual_map.erase( actual_map.size( ) - 5 );

		std::filesystem::path nade_path = g::config.get_path( ) / "nades";
		if ( !std::filesystem::exists( nade_path ) )
		{
			if ( !std::filesystem::create_directories( nade_path ) ) {
				g::console.error( "failed to create nades directory." );
				return;
			}
		}

		std::filesystem::path file = nade_path / ( actual_map + ".txt" );
		std::ofstream out( file );
		if ( !out.is_open( ) )
		{
			g::console.error( "failed to open nade file for writing: {}", file.string( ) );
			return;
		}

		for ( const auto& n : this->m_nades )
		{
			out << n.name << "|" << n.pos.x << "|" << n.pos.y << "|" << n.pos.z << "|" << n.target_pos.x << "|" << n.target_pos.y << "|" << n.target_pos.z << "|" << n.throw_type << "|" << n.nade_type << "\n";
		}
		out.close( );
		g::console.success( "saved {} nades for {}", this->m_nades.size( ), actual_map );
	}

} // namespace features::misc
