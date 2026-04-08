#include <stdafx.hpp>

namespace features::esp {

	void player::on_render( zdraw::draw_list& draw_list )
	{
		this->draw_movement_trails( draw_list );

		const auto& cfg = settings::g_esp.m_player;
		if ( !cfg.enabled )
		{
			return;
		}

		const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
		if ( !global_vars )
		{
			return;
		}

		const auto current_time = g::memory.read<float>( global_vars + 0x30 );

		for ( const auto& player : systems::g_collector.players( ) )
		{
			if ( !systems::g_local.is_enemy( player.team ) || !player.alive )
			{
				continue;
			}

			const auto& bones = player.bones;
			if ( !bones.is_valid( ) )
			{
				continue;
			}

			const auto bounds = systems::g_bounds.get( bones );
			if ( !bounds.is_valid( ) )
			{
				continue;
			}

			draw_offsets offsets{};

			if ( cfg.m_box.enabled )
			{
				this->add_box( draw_list, bounds, cfg.m_box, player.is_visible );
			}

			if ( cfg.m_skeleton.enabled )
			{
				this->add_skeleton( draw_list, bones, cfg.m_skeleton, player.is_visible );
			}

			if ( cfg.m_hitboxes.enabled )
			{
				this->add_hitboxes( draw_list, bones, player, cfg.m_hitboxes, current_time );
			}

			if ( cfg.m_health_bar.enabled )
			{
				this->add_health_bar( draw_list, bounds, player, cfg.m_health_bar, offsets );
			}

			if ( cfg.m_ammo_bar.enabled && player.weapon.max_ammo > 0 )
			{
				this->add_ammo_bar( draw_list, bounds, player, cfg.m_ammo_bar, offsets );
			}

			if ( cfg.m_name.enabled && !player.display_name.empty( ) )
			{
				this->add_name( draw_list, bounds, player, cfg.m_name, offsets );
			}

			if ( cfg.m_weapon.enabled && !player.weapon.name.empty( ) )
			{
				this->add_weapon( draw_list, bounds, player, cfg.m_weapon, offsets );
			}

			if ( cfg.m_info_flags.enabled )
			{
				this->add_flags( draw_list, bounds, player, cfg.m_info_flags, offsets );
			}

			if ( cfg.m_oof_arrow.enabled )
			{
				this->add_oof_arrow( draw_list, player, cfg.m_oof_arrow );
			}
		}
	}

	void player::add_oof_arrow( zdraw::draw_list& draw_list, const systems::collector::player& player, const settings::esp::player::oof_arrow& cfg )
	{
		const auto screen_pos = systems::g_view.project( player.origin );
		const auto [sw, sh] = zdraw::get_display_size( );

		if ( systems::g_view.projection_valid( screen_pos ) )
		{

		}

		const auto cx = sw * 0.5f;
		const auto cy = sh * 0.5f;

		const auto origin = systems::g_view.origin( );
		const auto view_angles = systems::g_view.angles( );

		// Calculate 2D direction in world space (ignoring Z for flat circular rotation)
		const auto angle_to_target = math::helpers::calculate_angle( origin, player.origin );
		
		// Get delta yaw and normalize
		const auto yaw_delta = math::helpers::normalize_yaw( view_angles.y - angle_to_target.y );
		
		const auto rad = math::helpers::deg_to_rad( yaw_delta - 90.0f );
		
		const auto radius = cfg.radius;
		const auto size = cfg.size;

		const auto arrow_center_x = cx + std::cosf( rad ) * radius;
		const auto arrow_center_y = cy + std::sinf( rad ) * radius;

		// Tip points further away from center
		const auto tip_x = arrow_center_x + std::cosf( rad ) * size;
		const auto tip_y = arrow_center_y + std::sinf( rad ) * size;

		// Sides are offset by 90 degrees
		const auto side_angle_l = rad + std::numbers::pi_v<float> * 0.5f;
		const auto side_angle_r = rad - std::numbers::pi_v<float> * 0.5f;
		const auto side_w = size * 0.5f;

		const auto left_x = arrow_center_x + std::cosf( side_angle_l ) * side_w;
		const auto left_y = arrow_center_y + std::sinf( side_angle_l ) * side_w;

		const auto right_x = arrow_center_x + std::cosf( side_angle_r ) * side_w;
		const auto right_y = arrow_center_y + std::sinf( side_angle_r ) * side_w;

		if ( cfg.outline )
		{
			draw_list.add_triangle( tip_x, tip_y, left_x, left_y, right_x, right_y, zdraw::rgba( 10, 10, 10, cfg.color.a ), 1.5f );
		}

		draw_list.add_triangle_filled( tip_x, tip_y, left_x, left_y, right_x, right_y, cfg.color );
	}

	void player::add_box( zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const settings::esp::player::box& cfg, bool is_visible )
	{
		const auto& color = is_visible ? cfg.visible_color : cfg.occluded_color;

		const auto x = std::floorf( bounds.min.x );
		const auto y = std::floorf( bounds.min.y );
		const auto w = std::floorf( bounds.max.x - bounds.min.x );
		const auto h = std::floorf( bounds.max.y - bounds.min.y );

		if ( cfg.fill )
		{
			constexpr auto edge_alpha{ 0.5f };
			constexpr auto center_alpha{ 0.08f };
			constexpr auto center_brightness{ 0.4f };
			constexpr auto desaturation{ 0.7f };

			const auto r = color.r / 255.0f;
			const auto g = color.g / 255.0f;
			const auto b = color.b / 255.0f;
			const auto avg = ( r + g + b ) / 3.0f;

			const auto edge_r = r * desaturation + avg * ( 1.0f - desaturation );
			const auto edge_g = g * desaturation + avg * ( 1.0f - desaturation );
			const auto edge_b = b * desaturation + avg * ( 1.0f - desaturation );

			const auto edge_color = zdraw::rgba( static_cast< std::uint8_t >( edge_r * 255 ), static_cast< std::uint8_t >( edge_g * 255 ), static_cast< std::uint8_t >( edge_b * 255 ), static_cast< std::uint8_t >( 255 * edge_alpha ) );
			const auto center_color = zdraw::rgba( static_cast< std::uint8_t >( edge_r * 255 * center_brightness ), static_cast< std::uint8_t >( edge_g * 255 * center_brightness ), static_cast< std::uint8_t >( edge_b * 255 * center_brightness ), static_cast< std::uint8_t >( 255 * center_alpha ) );

			const auto mid_y = y + h * 0.5f;
			draw_list.add_rect_filled_multi_color( x + 1, y + 1, w - 2, mid_y - y - 1, edge_color, edge_color, center_color, center_color );
			draw_list.add_rect_filled_multi_color( x + 1, mid_y, w - 2, y + h - mid_y - 1, center_color, center_color, edge_color, edge_color );
		}

		if ( cfg.style == (int)settings::esp::player::box::style0::full )
		{
			if ( cfg.outline )
			{
				draw_list.add_rect( x - 1, y - 1, w + 2, h + 2, zdraw::rgba( 0, 0, 0, 180 ), 1.0f );
				draw_list.add_rect( x, y, w, h, zdraw::rgba( 0, 0, 0, 200 ), 2.0f );
			}

			draw_list.add_rect( x, y, w, h, color, 1.0f );
		}
		else
		{
			const auto corner = std::min( cfg.corner_length, std::min( w, h ) * 0.4f );

			if ( cfg.outline )
			{
				draw_list.add_rect_cornered( x - 1, y - 1, w + 2, h + 2, zdraw::rgba( 0, 0, 0, 180 ), corner + 1, 1.0f );
				draw_list.add_rect_cornered( x, y, w, h, zdraw::rgba( 0, 0, 0, 200 ), corner, 2.0f );
			}

			draw_list.add_rect_cornered( x, y, w, h, color, corner, 1.0f );
		}
	}

	void player::add_skeleton( zdraw::draw_list& draw_list, const systems::bones::data& bones, const settings::esp::player::skeleton& cfg, bool is_visible )
	{
		const auto& color = is_visible ? cfg.visible_color : cfg.occluded_color;

		if ( cfg.rounded )
		{
			static const std::array<std::vector<std::uint32_t>, 5> paths
			{ {
				{ 6, 5, 4, 3, 2, 1, 0 },
				{ 4, 8, 9, 10 },
				{ 4, 13, 14, 15 },
				{ 0, 22, 23, 24 },
				{ 0, 25, 26, 27 }
			} };

			for ( const auto& path : paths )
			{
				std::vector<math::vector2> points;

				for ( const auto& bone_idx : path )
				{
					const auto screen = systems::g_view.project( bones.get_position( bone_idx ) );
					if ( !systems::g_view.projection_valid( screen ) )
					{
						continue;
					}

					points.push_back( { screen.x, screen.y } );
				}

				if ( points.size( ) < 2 )
				{
					continue;
				}

				points.insert( points.begin( ), points.front( ) );
				points.push_back( points.back( ) );

				math::vector2 last{ 0.0f, 0.0f };
				bool first = true;

				for ( std::size_t i = 0; i + 3 < points.size( ); i++ )
				{
					const auto& p0 = points[ i ];
					const auto& p1 = points[ i + 1 ];
					const auto& p2 = points[ i + 2 ];
					const auto& p3 = points[ i + 3 ];

					for ( float t = 0.0f; t <= 1.0f; t += 0.1f )
					{
						const auto pt = math::helpers::catmull_rom( p0, p1, p2, p3, t );

						if ( first )
						{
							last = pt;
							first = false;
							continue;
						}

						draw_list.add_line( last.x, last.y, pt.x, pt.y, color, cfg.thickness );
						last = pt;
					}
				}
			}
		}
		else
		{
			constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 18> connections
			{ {
				{ 6, 5 },
				{ 5, 4 },
				{ 4, 3 },
				{ 3, 2 },
				{ 2, 1 },
				{ 1, 0 },
				{ 4, 8 },
				{ 8, 9 },
				{ 9, 10 },
				{ 4, 13 },
				{ 13, 14 },
				{ 14, 15 },
				{ 0, 22 },
				{ 22, 23 },
				{ 23, 24 },
				{ 0, 25 },
				{ 25, 26 },
				{ 26, 27 },
			} };

			for ( const auto& [from, to] : connections )
			{
				const auto from_screen = systems::g_view.project( bones.get_position( from ) );
				const auto to_screen = systems::g_view.project( bones.get_position( to ) );

				if ( !systems::g_view.projection_valid( from_screen ) || !systems::g_view.projection_valid( to_screen ) )
				{
					continue;
				}

				draw_list.add_line( from_screen.x, from_screen.y, to_screen.x, to_screen.y, color, cfg.thickness );
			}
		}
	}

	void player::add_hitboxes( zdraw::draw_list& draw_list, const systems::bones::data& bones, const systems::collector::player& player, const settings::esp::player::hitboxes& cfg, float current_time )
	{
		auto& anim = this->m_animations[ player.controller ];
		if ( player.health < anim.last_health ) anim.last_hit_time = current_time;
		anim.last_health = player.health;

		const auto hp = std::clamp( static_cast< float >( player.health ) / 100.0f, 0.0f, 1.0f );
		const auto flash_t = std::clamp( ( current_time - anim.last_hit_time ) / 0.5f, 0.0f, 1.0f );
		const auto red = zdraw::rgba{ 220, 40, 40, 255 };

		auto color = player.is_visible ? cfg.visible_color : cfg.occluded_color;
		auto outline_color = cfg.outline_color;

		if ( cfg.mode == settings::esp::player::hitboxes::material::pulse )
		{
			const auto factor = ( std::sin( current_time * 5.0f ) * 0.5f + 0.5f );
			color.a = static_cast< std::uint8_t >( (float)color.a * factor );
			outline_color.a = static_cast< std::uint8_t >( (float)outline_color.a * factor );
		}

		const auto eye_pos = systems::g_view.origin( );

		if ( cfg.mode == settings::esp::player::hitboxes::material::wireframe )
		{
			float min_y{ 1e12f }, max_y{ -1e12f };
			for ( const auto& hbox : player.hitboxes )
			{
				const auto bone_id = static_cast< std::uint32_t >( hbox.bone );
				const auto position = bones.get_position( bone_id );
				const auto rotation = bones.get_rotation( bone_id );

				const auto top = rotation.rotate_vector( hbox.maxs ) + position;
				const auto bottom = rotation.rotate_vector( hbox.mins ) + position;
				
				const auto s_top = systems::g_view.project( top );
				const auto s_bottom = systems::g_view.project( bottom );
				
				if ( systems::g_view.projection_valid( s_top ) ) { min_y = std::min( min_y, s_top.y ); max_y = std::max( max_y, s_top.y ); }
				if ( systems::g_view.projection_valid( s_bottom ) ) { min_y = std::min( min_y, s_bottom.y ); max_y = std::max( max_y, s_bottom.y ); }
			}

			const auto range = max_y - min_y;
			const auto fill_line = min_y + range * hp;

			for ( const auto& hbox : player.hitboxes )
			{
				const auto bone_id = static_cast< std::uint32_t >( hbox.bone );
				const auto position = bones.get_position( bone_id );
				const auto rotation = bones.get_rotation( bone_id );

				this->add_capsule( draw_list, hbox.maxs, hbox.mins, hbox.radius, rotation, position, color, 10, false, 0.0f );
				if ( cfg.health_indicator && hp < 1.0f )
				{
					this->add_capsule( draw_list, hbox.maxs, hbox.mins, hbox.radius, rotation, position, red, 10, true, fill_line );
				}
			}
			return;
		}

		std::vector<std::vector<poly2d::point>> pills;
		for ( const auto& hb : player.hitboxes )
		{
			if ( hb.index < 0 || hb.bone < 0 || hb.radius <= 0.0f ) continue;

			const auto& bone = bones.bones[ hb.bone ];
			const auto center_local = ( hb.mins + hb.maxs ) * 0.5f;
			const auto center_world = bone.position + math::helpers::rotate_by_quat( bone.rotation, center_local );

			const auto half_extent = ( hb.maxs - hb.mins ) * 0.5f;
			const auto longest = std::max( { std::abs( half_extent.x ), std::abs( half_extent.y ), std::abs( half_extent.z ) } );

			math::vector3 axis_local{};
			if ( std::abs( half_extent.x ) >= std::abs( half_extent.y ) && std::abs( half_extent.x ) >= std::abs( half_extent.z ) ) axis_local = { longest, 0.0f, 0.0f };
			else if ( std::abs( half_extent.y ) >= std::abs( half_extent.z ) ) axis_local = { 0.0f, longest, 0.0f };
			else axis_local = { 0.0f, 0.0f, longest };

			const auto axis_world = math::helpers::rotate_by_quat( bone.rotation, axis_local );
			const auto cap_a = center_world - axis_world;
			const auto cap_b = center_world + axis_world;

			const auto sa = systems::g_view.project( cap_a );
			const auto sb = systems::g_view.project( cap_b );

			if ( !systems::g_view.projection_valid( sa ) || !systems::g_view.projection_valid( sb ) ) continue;

			const auto view_dir = ( center_world - eye_pos ).normalized( );
			auto perp = axis_world.cross( view_dir );
			const auto pl = perp.length( );

			if ( pl < 0.001f )
			{
				perp = axis_world.cross( math::vector3{ 0.0f, 0.0f, 1.0f } );
				const auto pl2 = perp.length( );
				perp = pl2 > 0.001f ? perp / pl2 : math::vector3{ 1.0f, 0.0f, 0.0f };
			}
			else perp = perp / pl;

			const auto perp_world = perp * hb.radius;
			const auto p_left = systems::g_view.project( center_world + perp_world );
			const auto p_right = systems::g_view.project( center_world - perp_world );

			if ( !systems::g_view.projection_valid( p_left ) || !systems::g_view.projection_valid( p_right ) ) continue;

			const auto screen_radius = std::sqrt( std::powf( p_left.x - p_right.x, 2.0f ) + std::powf( p_left.y - p_right.y, 2.0f ) ) * 0.5f;
			if ( screen_radius <= 0.0f || screen_radius > 800.0f ) continue;

			pills.push_back( poly2d::make_pill( { sa.x, sa.y }, { sb.x, sb.y }, screen_radius, 4 ) );
		}

		if ( pills.empty( ) ) return;
		auto merged = poly2d::union_pills( pills );

		for ( const auto& outline : merged.outlines )
		{
			if ( outline.size( ) < 3 ) continue;

			float min_y = outline[ 0 ].y, max_y = outline[ 0 ].y;
			for ( const auto& p : outline ) { min_y = std::min( min_y, p.y ); max_y = std::max( max_y, p.y ); }

			const auto range = max_y - min_y;
			const auto fill_line = min_y + range * hp;

			if ( cfg.fill )
			{
				const auto tris = poly2d::triangulate( outline );
				for ( std::size_t i = 0; i + 5 < tris.size( ); i += 6 )
				{
					draw_list.add_triangle_filled( tris[ i ], tris[ i + 1 ], tris[ i + 2 ], tris[ i + 3 ], tris[ i + 4 ], tris[ i + 5 ], color );
				}

				if ( cfg.health_indicator && hp < 1.0f )
				{
					std::vector<poly2d::point> clipped;
					for ( std::size_t i = 0; i < outline.size( ); ++i )
					{
						const auto& p1 = outline[ i ];
						const auto& p2 = outline[ ( i + 1 ) % outline.size( ) ];
						const bool p1_in = ( p1.y >= fill_line );
						const bool p2_in = ( p2.y >= fill_line );
						if ( p1_in ) clipped.push_back( p1 );
						if ( p1_in != p2_in )
						{
							const float t = ( fill_line - p1.y ) / ( p2.y - p1.y );
							clipped.push_back( { p1.x + t * ( p2.x - p1.x ), fill_line } );
						}
					}
					
					if ( clipped.size( ) >= 3 )
					{
						const auto clipped_tris = poly2d::triangulate( clipped );
						for ( std::size_t i = 0; i + 5 < clipped_tris.size( ); i += 6 )
						{
							draw_list.add_triangle_filled( clipped_tris[ i ], clipped_tris[ i + 1 ], clipped_tris[ i + 2 ], clipped_tris[ i + 3 ], clipped_tris[ i + 4 ], clipped_tris[ i + 5 ], red );
						}
					}
				}
			}

			if ( cfg.outline )
			{
				std::vector<float> flat;
				flat.reserve( outline.size( ) * 2 );
				for ( const auto& p : outline ) { flat.push_back( p.x ); flat.push_back( p.y ); }

				if ( cfg.mode == settings::esp::player::hitboxes::material::glow )
				{
					for ( int i = 1; i <= 6; ++i )
					{
						const auto glow_alpha = static_cast< std::uint8_t >( ( 1.0f - ( float )i / 7.0f ) * 120.0f );
						const auto alpha = static_cast< std::uint8_t >( ( float)glow_alpha * ( (float)outline_color.a / 255.0f ) );
						draw_list.add_polyline( std::span<const float>( flat.data( ), flat.size( ) ), zdraw::rgba{ outline_color.r, outline_color.g, outline_color.b, alpha }, true, 1.0f + ( float )i * 2.0f );
					}
				}

				draw_list.add_polyline( std::span<const float>( flat.data( ), flat.size( ) ), outline_color, true, 2.0f );

				if ( cfg.health_indicator && hp < 1.0f )
				{
					auto draw_clipped_outline = [&]( float y_threshold, const zdraw::rgba& col, float thickness )
						{
							for ( std::size_t i = 0; i < outline.size( ); ++i )
							{
								const auto& p1 = outline[ i ];
								const auto& p2 = outline[ ( i + 1 ) % outline.size( ) ];

								if ( p1.y < y_threshold && p2.y < y_threshold ) continue;
								if ( p1.y >= y_threshold && p2.y >= y_threshold ) draw_list.add_line( p1.x, p1.y, p2.x, p2.y, col, thickness );
								else
								{
									const float t = ( y_threshold - p1.y ) / ( p2.y - p1.y );
									const math::vector2 intersection{ p1.x + t * ( p2.x - p1.x ), y_threshold };
									if ( p1.y >= y_threshold ) draw_list.add_line( p1.x, p1.y, intersection.x, intersection.y, col, thickness );
									else draw_list.add_line( intersection.x, intersection.y, p2.x, p2.y, col, thickness );
								}
							}
						};

					if ( cfg.mode == settings::esp::player::hitboxes::material::glow )
					{
						for ( int i = 1; i <= 4; ++i )
						{
							const auto alpha = static_cast< std::uint8_t >( ( 1.0f - ( float )i / 5.0f ) * 160.0f );
							draw_clipped_outline( fill_line, zdraw::rgba{ red.r, red.g, red.b, alpha }, 1.0f + ( float )i * 2.5f );
						}
					}
					draw_clipped_outline( fill_line, red, 2.0f );
				}
			}
		}
	}

	void player::add_health_bar( zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const systems::collector::player& player, const settings::esp::player::health_bar& cfg, draw_offsets& offsets )
	{
		auto& anim = this->m_animations[ player.controller ];

		constexpr auto bar_size{ 3.5f };
		constexpr auto padding{ 4.0f };
		constexpr auto outline_size{ 1.0f };

		const auto clamped_health = std::clamp( player.health, 0, 100 );
		const auto target_fraction = static_cast< float >( clamped_health ) / 100.0f;

		if ( !anim.initialized || ( target_fraction - anim.health.value( ) > 0.5f ) )
		{
			anim.health.snap( target_fraction );
			anim.initialized = true;
		}
		else
		{
			anim.health.set_target( target_fraction );
			anim.health.update( );
		}

		const auto fraction = anim.health.value( );
		const auto vertical = ( cfg.position == (int)settings::esp::player::health_bar::position::left );

		const auto bar_w = vertical ? bar_size : std::floorf( bounds.width( ) );
		const auto bar_h = vertical ? std::floorf( bounds.height( ) ) : bar_size;
		const auto filled = std::floorf( ( vertical ? bar_h : bar_w ) * fraction );

		float x{}, y{};

		switch ( cfg.position )
		{
		case (int)settings::esp::player::health_bar::position::left:
			x = std::floorf( bounds.min.x - bar_size - padding - offsets.left - outline_size );
			y = std::floorf( bounds.min.y );
			break;
		case (int)settings::esp::player::health_bar::position::top:
			x = std::floorf( bounds.min.x );
			y = std::floorf( bounds.min.y - bar_size - padding - offsets.top - outline_size );
			break;
		case (int)settings::esp::player::health_bar::position::bottom:
			x = std::floorf( bounds.min.x );
			y = std::floorf( bounds.max.y + padding + offsets.bottom + outline_size );
			break;
		}

		switch ( cfg.position )
		{
		case (int)settings::esp::player::health_bar::position::left: offsets.left += bar_size + padding + ( outline_size * 2.0f ); break;
		case (int)settings::esp::player::health_bar::position::top: offsets.top += bar_size + padding + ( outline_size * 2.0f ); break;
		case (int)settings::esp::player::health_bar::position::bottom: offsets.bottom += bar_size + padding + ( outline_size * 2.0f ); break;
		}

		if ( cfg.outline )
		{
			draw_list.add_rect_filled( x - outline_size, y - outline_size, bar_w + outline_size * 2.0f, bar_h + outline_size * 2.0f, cfg.outline_color );
		}

		draw_list.add_rect_filled( x, y, bar_w, bar_h, cfg.background_color );

		if ( filled > 0 )
		{
			if ( cfg.gradient )
			{
				if ( vertical )
					draw_list.add_rect_filled_multi_color( x, y + bar_h - filled, bar_w, filled, cfg.full_color, cfg.full_color, cfg.low_color, cfg.low_color );
				else
					draw_list.add_rect_filled_multi_color( x, y, filled, bar_h, cfg.low_color, cfg.full_color, cfg.full_color, cfg.low_color );
			}
			else
			{
				if ( vertical )
					draw_list.add_rect_filled( x, y + bar_h - filled, bar_w, filled, cfg.full_color );
				else
					draw_list.add_rect_filled( x, y, filled, bar_h, cfg.full_color );
			}
		}

		if ( cfg.show_value && clamped_health < 100 )
		{
			const auto text = std::to_string( clamped_health );
			zdraw::push_font( g::render.fonts( ).pixel7_10 );
			const auto [ tw, th ] = zdraw::measure_text( text );

			const auto text_x = vertical
				? x + ( bar_w - tw ) * 0.5f
				: x + filled - tw * 0.5f;
			const auto text_y = vertical
				? y + bar_h - filled - th * 0.5f
				: y + ( bar_h - th ) * 0.5f;

			draw_list.add_text( text_x, text_y, text, zdraw::get_font( ), cfg.text_color, zdraw::text_style::outlined );
			zdraw::pop_font( );
		}
	}

	void player::add_ammo_bar( zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const systems::collector::player& player, const settings::esp::player::ammo_bar& cfg, draw_offsets& offsets )
	{
		auto& anim = this->m_animations[ player.controller ];

		constexpr auto bar_size{ 3.5f };
		constexpr auto padding{ 4.0f };

		const auto clamped_ammo = std::clamp( player.weapon.ammo, 0, player.weapon.max_ammo );
		const auto target_fraction = static_cast< float >( clamped_ammo ) / player.weapon.max_ammo;

		if ( !anim.initialized || ( target_fraction - anim.ammo.value( ) > 0.5f ) )
		{
			anim.ammo.snap( target_fraction );
			anim.initialized = true;
		}
		else
		{
			anim.ammo.set_target( target_fraction );
			anim.ammo.update( );
		}

		const auto fraction = anim.ammo.value( );
		const auto outline_size = cfg.outline ? 1.0f : 0.0f;
		const auto vertical = cfg.position == (int)settings::esp::player::ammo_bar::position::left;

		const auto bar_w = vertical ? bar_size : std::floorf( bounds.width( ) );
		const auto bar_h = vertical ? std::floorf( bounds.height( ) ) : bar_size;
		const auto filled = std::floorf( ( vertical ? bar_h : bar_w ) * fraction );

		const auto x = [ & ]( )
			{
				if ( cfg.position == (int)settings::esp::player::ammo_bar::position::left )
				{
					return std::floorf( bounds.min.x - bar_size - padding - offsets.left - outline_size );
				}

				return std::floorf( bounds.min.x );
			}( );

		const auto y = [ & ]( )
			{
				switch ( cfg.position )
				{
				case (int)settings::esp::player::ammo_bar::position::left: return std::floorf( bounds.min.y );
				case (int)settings::esp::player::ammo_bar::position::top: return std::floorf( bounds.min.y - bar_size - padding - offsets.top - outline_size );
				case (int)settings::esp::player::ammo_bar::position::bottom: return std::floorf( bounds.max.y + padding + offsets.bottom + outline_size );
				}
				return 0.0f;
			}( );

		switch ( cfg.position )
		{
		case (int)settings::esp::player::ammo_bar::position::left: offsets.left += bar_size + padding + ( outline_size * 2.0f ); break;
		case (int)settings::esp::player::ammo_bar::position::top: offsets.top += bar_size + padding + ( outline_size * 2.0f ); break;
		case (int)settings::esp::player::ammo_bar::position::bottom: offsets.bottom += bar_size + padding + ( outline_size * 2.0f ); break;
		}

		if ( cfg.outline )
		{
			draw_list.add_rect_filled( x - 1, y - 1, bar_w + 2, bar_h + 2, cfg.outline_color );
		}

		draw_list.add_rect_filled( x, y, bar_w, bar_h, cfg.background_color );

		if ( filled > 0 )
		{
			if ( cfg.gradient )
			{
				if ( vertical )
				{
					draw_list.add_rect_filled_multi_color( x, y + bar_h - filled, bar_w, filled, cfg.full_color, cfg.full_color, cfg.low_color, cfg.low_color );
				}
				else
				{
					draw_list.add_rect_filled_multi_color( x, y, filled, bar_h, cfg.low_color, cfg.full_color, cfg.full_color, cfg.low_color );
				}
			}
			else
			{
				if ( vertical )
				{
					draw_list.add_rect_filled( x, y + bar_h - filled, bar_w, filled, cfg.full_color );
				}
				else
				{
					draw_list.add_rect_filled( x, y, filled, bar_h, cfg.full_color );
				}
			}
		}

		if ( cfg.show_value )
		{
			zdraw::push_font( g::render.fonts( ).pixel7_10 );

			const auto text = std::format( "{}/{}", clamped_ammo, player.weapon.max_ammo );
			const auto [text_w, text_h] = zdraw::measure_text( text );
			const auto text_x = std::floorf( x + ( bar_w * 0.5f ) - ( text_w * 0.5f ) );
			const auto text_y = vertical ? std::floorf( y + bar_h - filled - text_h - 2.0f ) : std::floorf( y + bar_h + 2.0f );

			draw_list.add_text( text_x, text_y, text, zdraw::get_font( ), cfg.text_color, zdraw::text_style::outlined );
			zdraw::pop_font( );
		}
	}

	void player::add_name( zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const systems::collector::player& player, const settings::esp::player::name& cfg, draw_offsets& offsets )
	{
		zdraw::push_font( g::render.fonts( ).pretzel_12 );

		const auto [text_w, text_h] = zdraw::measure_text( player.display_name );
		const auto text_x = std::floorf( bounds.min.x + ( bounds.width( ) * 0.5f ) - ( text_w * 0.5f ) );
		const auto text_y = std::floorf( bounds.min.y - text_h - 2.0f - offsets.top );

		draw_list.add_text( text_x, text_y, player.display_name, zdraw::get_font( ), cfg.color, zdraw::text_style::outlined );
		zdraw::pop_font( );

		offsets.top += text_h + 2.0f;
	}

	void player::add_weapon( zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const systems::collector::player& player, const settings::esp::player::weapon& cfg, draw_offsets& offsets )
	{
		zdraw::push_font( g::render.fonts( ).mochi_12 );

		auto total_height{ 0.0f };

		if ( cfg.display == (int)settings::esp::player::weapon::display_type::icon || cfg.display == (int)settings::esp::player::weapon::display_type::text_and_icon )
		{
			zdraw::push_font( g::render.fonts( ).weapons_15 );

			const auto icon = this->get_weapon_icon( player.weapon.name );
			const auto [icon_w, icon_h] = zdraw::measure_text( icon );
			const auto icon_x = std::floorf( bounds.min.x + ( bounds.width( ) * 0.5f ) - ( icon_w * 0.5f ) );
			const auto icon_y = std::floorf( bounds.max.y + 2.0f + offsets.bottom + total_height );

			draw_list.add_text( icon_x, icon_y, icon, zdraw::get_font( ), cfg.icon_color, zdraw::text_style::outlined );
			zdraw::pop_font( );

			total_height += icon_h + 2.0f;
		}

		if ( cfg.display == (int)settings::esp::player::weapon::display_type::text || cfg.display == (int)settings::esp::player::weapon::display_type::text_and_icon )
		{
			const auto [text_w, text_h] = zdraw::measure_text( player.weapon.name );
			const auto text_x = std::floorf( bounds.min.x + ( bounds.width( ) * 0.5f ) - ( text_w * 0.5f ) );
			const auto text_y = std::floorf( bounds.max.y + 2.0f + offsets.bottom + total_height );

			draw_list.add_text( text_x, text_y, player.weapon.name, zdraw::get_font( ), cfg.text_color, zdraw::text_style::outlined );

			total_height += text_h + 2.0f;
		}

		zdraw::pop_font( );

		offsets.bottom += total_height;
	}

	void player::add_flags( zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const systems::collector::player& player, const settings::esp::player::info_flags& cfg, draw_offsets& offsets )
	{
		zdraw::push_font( g::render.fonts( ).pixel7_10 );

		const auto x = std::floorf( bounds.max.x + 4.0f + offsets.right );
		auto y = std::floorf( bounds.min.y );
		auto max_w{ 0.0f };

		const auto draw_flag = [ & ]( const std::string& text, const zdraw::rgba& color )
			{
				draw_list.add_text( x, y, text, zdraw::get_font( ), color, zdraw::text_style::outlined );
				const auto [text_w, text_h] = zdraw::measure_text( text );
				y += text_h;
				max_w = std::max( max_w, text_w );
			};

		if ( cfg.has( settings::esp::player::info_flags::flag::money ) )
		{
			draw_flag( std::format( "${}", player.money ), cfg.money_color );
		}

		if ( cfg.has( settings::esp::player::info_flags::flag::armor ) && player.armor > 0 )
		{
			draw_flag( player.has_helmet ? "hk" : "k", cfg.armor_color );
		}

		if ( cfg.has( settings::esp::player::info_flags::flag::kit ) && player.has_defuser )
		{
			draw_flag( "kit", cfg.kit_color );
		}

		if ( cfg.has( settings::esp::player::info_flags::flag::scoped ) && player.is_scoped )
		{
			draw_flag( "zoom", cfg.scoped_color );
		}

		if ( cfg.has( settings::esp::player::info_flags::flag::defusing ) && player.is_defusing )
		{
			draw_flag( "defusing", cfg.defusing_color );
		}

		if ( cfg.has( settings::esp::player::info_flags::flag::flashed ) && player.is_flashed )
		{
			draw_flag( "flashed", cfg.flashed_color );
		}

		if ( cfg.has( settings::esp::player::info_flags::flag::ping ) )
		{
			const auto ping_color = [ & ]( ) -> zdraw::rgba
				{
					if ( player.ping <= 20 ) return { 98, 217, 109, 255 };
					if ( player.ping <= 50 ) return { 230, 206, 137, 255 };
					if ( player.ping <= 70 ) return { 230, 156, 110, 255 };
					return { 222, 59, 59, 255 };
				}( );

			draw_flag( std::format( "{}ms", player.ping ), ping_color );
		}

		if ( cfg.has( settings::esp::player::info_flags::flag::distance ) )
		{
			const auto distance = systems::g_view.origin( ).distance( player.origin ) * 0.01905f;
			draw_flag( std::format( "{:.0f}m", distance ), cfg.distance_color );
		}

		zdraw::pop_font( );

		offsets.right += max_w + 4.0f;
	}

	std::string player::get_weapon_icon( const std::string& weapon_name )
	{
		static const std::unordered_map<std::string, std::string> icons
		{
			{ "knife_ct", "]" }, { "knife_t", "[" }, { "knife", "]" },
			{ "deagle", "A" }, { "elite", "B" }, { "fiveseven", "C" },
			{ "glock", "D" }, { "revolver", "J" }, { "hkp2000", "E" },
			{ "p250", "F" }, { "usp_silencer", "G" }, { "tec9", "H" },
			{ "cz75a", "I" }, { "mac10", "K" }, { "ump45", "L" },
			{ "bizon", "M" }, { "mp7", "N" }, { "mp9", "R" },
			{ "p90", "O" }, { "mp5sd", "N" }, { "galilar", "Q" },
			{ "famas", "R" }, { "m4a1_silencer", "T" }, { "m4a1", "S" },
			{ "aug", "U" }, { "sg556", "V" }, { "ak47", "W" },
			{ "g3sg1", "X" }, { "scar20", "Y" }, { "awp", "Z" },
			{ "ssg08", "a" }, { "xm1014", "b" }, { "sawedoff", "c" },
			{ "mag7", "d" }, { "nova", "e" }, { "negev", "f" },
			{ "m249", "g" }, { "taser", "h" }, { "flashbang", "i" },
			{ "hegrenade", "j" }, { "smokegrenade", "k" }, { "molotov", "l" },
			{ "decoy", "m" }, { "incgrenade", "n" }, { "c4", "o" },
		};

		const auto it = icons.find( weapon_name );
		return it != icons.end( ) ? it->second : "?";
	}

	void player::draw_movement_trails( zdraw::draw_list& draw_list )
	{
		const auto& cfg = settings::g_esp.m_player.m_trails;
		if ( !cfg.enabled )
		{
			this->m_trails.clear( );
			return;
		}

		const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
		if ( !global_vars ) return;

		const auto current_time = g::memory.read<float>( global_vars + 0x30 );

		const auto local_pawn = systems::g_local.pawn( );

		// Update trails for active players
		std::unordered_set<std::uintptr_t> active_pawns;

		auto update_trail = [&]( std::uintptr_t pawn, const math::vector3& origin, bool should_track )
		{
			if ( !should_track )
			{
				this->m_trails.erase( pawn );
				return;
			}

			active_pawns.insert( pawn );
			auto& trail = this->m_trails[ pawn ];

			// Remove expired points
			while ( !trail.path.empty( ) && ( current_time - trail.path.front( ).time ) > cfg.lifetime )
			{
				trail.path.pop_front( );
			}

			if ( trail.path.empty( ) || ( trail.path.back( ).pos - origin ).length_sqr( ) > 1.0f )
			{
				trail.path.push_back( { origin, current_time } );
				if ( static_cast< int >( trail.path.size( ) ) > cfg.max_points )
				{
					trail.path.pop_front( );
				}
			}
		};

		// Track local player
		if ( local_pawn && cfg.local )
		{
			const auto gsn = g::memory.read<std::uintptr_t>( local_pawn + SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );
			if ( gsn )
			{
				const auto origin = g::memory.read<math::vector3>( gsn + SCHEMA( "CGameSceneNode", "m_vecAbsOrigin"_hash ) );
				update_trail( local_pawn, origin, true );
			}
		}

		// Track others
		for ( const auto& player : systems::g_collector.players( ) )
		{
			if ( !player.pawn || player.pawn == local_pawn )
				continue;

			const bool is_enemy = systems::g_local.is_enemy( player.team );
			const bool is_team = !is_enemy;

			bool should_track = false;
			if ( is_enemy && cfg.enemy ) should_track = true;
			else if ( is_team && cfg.team ) should_track = true;

			update_trail( player.pawn, player.origin, should_track );
		}

		// Cleanup trails for inactive players
		for ( auto it = this->m_trails.begin( ); it != this->m_trails.end( ); )
		{
			if ( active_pawns.find( it->first ) == active_pawns.end( ) )
			{
				it = this->m_trails.erase( it );
			}
			else
			{
				++it;
			}
		}

		// Draw trails
		for ( const auto& [ pawn, data ] : this->m_trails )
		{
			if ( data.path.size( ) < 2 )
				continue;

			zdraw::rgba color;
			if ( pawn == local_pawn ) color = cfg.local_color;
			else
			{
				bool found_team = false;
				for ( const auto& p : systems::g_collector.players( ) )
				{
					if ( p.pawn == pawn )
					{
						color = systems::g_local.is_enemy( p.team ) ? cfg.enemy_color : cfg.team_color;
						found_team = true;
						break;
					}
				}
				if ( !found_team ) continue;
			}

			for ( size_t i = 1; i < data.path.size( ); ++i )
			{
				const auto& p1_data = data.path[ i - 1 ];
				const auto& p2_data = data.path[ i ];

				const auto p1_screen = systems::g_view.project( p1_data.pos );
				const auto p2_screen = systems::g_view.project( p2_data.pos );

				if ( systems::g_view.projection_valid( p1_screen ) && systems::g_view.projection_valid( p2_screen ) )
				{
					const float age1 = current_time - p1_data.time;
					const float age2 = current_time - p2_data.time;

					// Average age alpha or segment based alpha
					const float fraction = 1.0f - ( age2 / cfg.lifetime );
					const auto alpha = static_cast< std::uint8_t >( std::clamp( fraction, 0.0f, 1.0f ) * color.a );
					
					if ( alpha > 0 )
					{
						draw_list.add_line( p1_screen.x, p1_screen.y, p2_screen.x, p2_screen.y, zdraw::rgba{ color.r, color.g, color.b, alpha }, cfg.thickness );
					}
				}
			}
		}
	}

	void player::add_capsule( zdraw::draw_list& draw_list, const math::vector3& start, const math::vector3& end, float radius, const math::quaternion& rotation, const math::vector3& origin, const zdraw::rgba& color, int segments_max, bool red_only, float fill_line )
	{
		const auto top = rotation.rotate_vector( start ) + origin;
		const auto bottom = rotation.rotate_vector( end ) + origin;

		const auto axis = ( bottom - top ).normalized( );
		const auto arbitrary = std::abs( axis.x ) < 0.99f ? math::vector3( 1, 0, 0 ) : math::vector3( 0, 1, 0 );
		const auto u = axis.cross( arbitrary ).normalized( );
		const auto v = axis.cross( u );

		const auto capsule_mid_point = ( top + bottom ) * 0.5f;
		const auto distance = capsule_mid_point.distance( systems::g_view.origin( ) );

		const auto start_reduction_distance = 800.0f;
		const auto end_reduction_distance = 3000.0f;

		int min_segments = 4;
		int current_segments;

		if ( distance <= start_reduction_distance )
		{
			current_segments = segments_max;
		}
		else if ( distance >= end_reduction_distance )
		{
			current_segments = min_segments;
		}
		else
		{
			auto normalized_distance = ( distance - start_reduction_distance ) / ( end_reduction_distance - start_reduction_distance );
			normalized_distance = std::clamp( normalized_distance, 0.0f, 1.0f );
			current_segments = static_cast< int >( std::lerp( static_cast< float >( segments_max ), static_cast< float >( min_segments ), normalized_distance ) );
			current_segments = std::max( current_segments, min_segments );
		}

		this->precompute_sincos( current_segments );
		this->draw_capsule_outline( draw_list, top, bottom, axis, u, v, radius, color, current_segments, red_only, fill_line );
	}

	void player::draw_capsule_outline( zdraw::draw_list& draw_list, const math::vector3& top, const math::vector3& bottom, const math::vector3& axis, const math::vector3& u, const math::vector3& v, float radius, const zdraw::rgba& color, int segments, bool red_only, float fill_line )
	{
		std::vector<math::vector3> top_circle, bottom_circle;
		this->create_circle( top, u, v, radius, top_circle, segments );
		this->create_circle( bottom, u, v, radius, bottom_circle, segments );

		const auto hemisphere_segments = std::max( 3, segments / 3 );

		std::vector<math::vector2> wtop( segments + 1 ), wbottom( segments + 1 );
		for ( int i = 0; i <= segments; ++i )
		{
			wtop[ i ] = systems::g_view.project( top_circle[ i ] );
			wbottom[ i ] = systems::g_view.project( bottom_circle[ i ] );
		}

		const auto thickness = 1.25f;
		const auto draw_clipped_line = [ & ]( const math::vector2& p1, const math::vector2& p2, const zdraw::rgba& col, float thick )
			{
				if ( !systems::g_view.projection_valid( p1 ) || !systems::g_view.projection_valid( p2 ) ) return;

				if ( red_only )
				{
					if ( p1.y < fill_line && p2.y < fill_line ) return;
					if ( p1.y >= fill_line && p2.y >= fill_line )
					{
						draw_list.add_line( p1.x, p1.y, p2.x, p2.y, col, thick );
					}
					else
					{
						const float t = ( fill_line - p1.y ) / ( p2.y - p1.y );
						const math::vector2 intersection{ p1.x + t * ( p2.x - p1.x ), fill_line };
						if ( p1.y >= fill_line ) draw_list.add_line( p1.x, p1.y, intersection.x, intersection.y, col, thick );
						else draw_list.add_line( intersection.x, intersection.y, p2.x, p2.y, col, thick );
					}
				}
				else
				{
					draw_list.add_line( p1.x, p1.y, p2.x, p2.y, col, thick );
				}
			};

		for ( int i = 0; i < segments; ++i )
		{
			draw_clipped_line( wtop[ i ], wtop[ i + 1 ], color, thickness );
			draw_clipped_line( wbottom[ i ], wbottom[ i + 1 ], color, thickness );
		}

		for ( int h = 0; h < hemisphere_segments; ++h )
		{
			const auto phi = ( std::numbers::pi_v<float> / 2.0f ) * ( static_cast< float >( h + 1 ) / hemisphere_segments );
			const auto ring_radius = radius * std::cos( phi );
			const auto ring_height = radius * std::sin( phi );

			std::vector<math::vector3> top_arc, bottom_arc;
			const auto top_ring_center = top - axis * ring_height;
			const auto bottom_ring_center = bottom + axis * ring_height;

			this->create_circle( top_ring_center, u, v, ring_radius, top_arc, segments );
			this->create_circle( bottom_ring_center, u, v, ring_radius, bottom_arc, segments );

			for ( int i = 0; i < segments; ++i )
			{
				draw_clipped_line( systems::g_view.project( top_arc[ i ] ), systems::g_view.project( top_arc[ i + 1 ] ), color, thickness * 0.7f );
				draw_clipped_line( systems::g_view.project( bottom_arc[ i ] ), systems::g_view.project( bottom_arc[ i + 1 ] ), color, thickness * 0.7f );
			}
		}

		const auto half = segments / 2;
		const auto quarter = segments / 4;
		const auto three_quarter = ( 3 * segments ) / 4;

		draw_clipped_line( wtop[ 0 ], wbottom[ 0 ], color, thickness );
		draw_clipped_line( wtop[ half ], wbottom[ half ], color, thickness );
		draw_clipped_line( wtop[ quarter ], wbottom[ quarter ], color, thickness * 0.8f );
		draw_clipped_line( wtop[ three_quarter ], wbottom[ three_quarter ], color, thickness * 0.8f );
	}

	void player::precompute_sincos( int segments )
	{
		this->m_sin_cache.resize( segments + 1 );
		this->m_cos_cache.resize( segments + 1 );

		const auto angle_step = 2.0f * std::numbers::pi_v<float> / segments;
		for ( int i = 0; i <= segments; ++i )
		{
			const auto angle = angle_step * i;
			this->m_sin_cache[ i ] = std::sin( angle );
			this->m_cos_cache[ i ] = std::cos( angle );
		}
	}

	void player::create_circle( const math::vector3& center, const math::vector3& u, const math::vector3& v, float radius, std::vector<math::vector3>& out, int segments )
	{
		out.clear( );
		out.reserve( segments + 1 );

		for ( int i = 0; i <= segments; ++i )
		{
			out.push_back( center + ( u * this->m_cos_cache[ i ] + v * this->m_sin_cache[ i ] ) * radius );
		}
	}


} // namespace features::esp
