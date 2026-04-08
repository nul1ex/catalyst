#include <stdafx.hpp>
#include <iostream>
#include <cstring>

void menu::draw( )
{
	if ( GetAsyncKeyState( VK_INSERT ) & 1 )
	{
		this->m_open = !this->m_open;
	}

	if ( !this->m_open )
	{
		return;
	}

	zui::begin( );

	if ( zui::begin_window( "catalyst##main", this->m_x, this->m_y, this->m_w, this->m_h, true, 580.0f, 440.0f ) )
	{
		const auto [avail_w, avail_h] = zui::get_content_region_avail( );

		if ( zui::begin_nested_window( "##inner", avail_w, avail_h ) )
		{
			constexpr auto sidebar_w{ 120.0f };
			constexpr auto padding{ 0.0f };

			zui::set_cursor_pos( padding, padding );
			this->draw_sidebar( sidebar_w, avail_h );

			zui::set_cursor_pos( sidebar_w + padding, padding );
			this->draw_content( avail_w - sidebar_w - padding, avail_h );

			if ( const auto win = zui::detail::get_current_window( ) )
			{
				this->draw_accent_lines( win->bounds );
			}

			zui::end_nested_window( );
		}

		zui::end_window( );
	}

	zui::end( );
}

void menu::draw_sidebar( float width, float height )
{
	if ( !zui::begin_nested_window( "##sidebar", width, height ) )
	{
		return;
	}

	const auto current = zui::detail::get_current_window( );
	if ( !current )
	{
		zui::end_nested_window( );
		return;
	}

	const auto& style = zui::get_style( );
	const auto dt = zdraw::get_delta_time( );
	const auto bx = current->bounds.x;
	const auto by = current->bounds.y;
	const auto bw = current->bounds.w;
	const auto bh = current->bounds.h;

	zdraw::get_draw_list( ).add_rect_filled( bx, by, bw, bh, zdraw::rgba{ 14, 14, 14, 255 } );
	zdraw::get_draw_list( ).add_rect( bx, by, bw, bh, zdraw::rgba{ 38, 38, 38, 255 } );

	{
		constexpr auto title{ "catalyst" };
		auto [tw, th] = zdraw::measure_text( title );
		zdraw::get_draw_list( ).add_text( bx + ( bw - tw ) * 0.5f, by + 10.0f, title, nullptr, style.accent );
	}

	static constexpr std::pair<const char*, tab> tabs[ ]
	{
		{ "combat",  tab::combat  },
		{ "esp",     tab::esp     },
		{ "movement", tab::movement },
		{ "skins",   tab::skinchanger },
		{ "misc",    tab::misc    },
		{ "nades",   tab::nades   },
		{ "configs", tab::configs },
	};

	constexpr auto tab_count = static_cast< int >( std::size( tabs ) );
	constexpr auto tab_spacing{ 4.0f };
	constexpr auto tab_h{ 32.0f };

	struct tab_anim { float v{ 0.0f }; };
	static std::array<tab_anim, tab_count> anims{};

	auto cursor_y = by + 40.0f;

	for ( int i = 0; i < tab_count; ++i )
	{
		const auto& t = tabs[ i ];
		const auto is_sel = ( this->m_tab == t.second );
		auto [tw, th] = zdraw::measure_text( t.first );

		const auto tab_rect = zui::rect{ bx, cursor_y, bw, tab_h };
		const auto hovered = zui::detail::mouse_hovered( tab_rect ) && !zui::detail::overlay_blocking_input( );

		if ( hovered && zui::detail::mouse_clicked( ) )
		{
			this->m_tab = t.second;
		}

		auto& anim = anims[ i ];
		anim.v += ( ( is_sel ? 1.0f : 0.0f ) - anim.v ) * std::min( 10.0f * dt, 1.0f );

		if ( is_sel )
		{
			zdraw::get_draw_list( ).add_rect_filled( bx + 1.0f, cursor_y, 2.0f, tab_h, style.accent );
			zdraw::get_draw_list( ).add_rect_filled( bx + 1.0f, cursor_y, bw - 2.0f, tab_h, zdraw::rgba{ 255, 255, 255, 5 } );
		}

		const auto text_x = bx + 10.0f;
		const auto text_y = cursor_y + ( tab_h - th ) * 0.5f;
		const auto col = is_sel ? zui::lighten( style.accent, 1.0f + 0.1f * anim.v ) : zui::lerp( zdraw::rgba{ 110, 110, 110, 255 }, style.text, hovered ? 1.0f : 0.0f );

		zdraw::get_draw_list( ).add_text( text_x, text_y, t.first, nullptr, col );

		cursor_y += tab_h + tab_spacing;
	}

	zui::end_nested_window( );
}

void menu::draw_content( float width, float height )
{
	zui::push_style_var( zui::style_var::window_padding_x, 10.0f );
	zui::push_style_var( zui::style_var::window_padding_y, 10.0f );

	if ( !zui::begin_nested_window( "##content", width, height ) )
	{
		zui::pop_style_var( 2 );
		return;
	}

	switch ( this->m_tab )
	{
	case tab::combat:  this->draw_combat( );  break;
	case tab::esp:     this->draw_esp( );     break;
	case tab::movement: this->draw_movement( ); break;
	case tab::skinchanger: this->draw_skinchanger( ); break;
	case tab::misc:    this->draw_misc( );    break;
	case tab::nades:   this->draw_nades( );   break;
	case tab::configs: this->draw_configs( ); break;
	default: break;
	}

	zui::pop_style_var( 2 );
	zui::end_nested_window( );
}

void menu::draw_accent_lines( const zui::rect& bounds, float fade_ratio )
{
	const auto ix = bounds.x + 1.0f;
	const auto iw = bounds.w - 2.0f;
	const auto top_y = bounds.y + 1.0f;
	const auto bot_y = bounds.y + bounds.h - 2.0f;
	const auto accent = zui::get_accent_color( );
	const auto trans = zdraw::rgba{ accent.r, accent.g, accent.b, 0 };
	const auto fade_w = iw * fade_ratio;
	const auto solid_w = iw - fade_w * 2.0f;

	for ( const auto ly : { top_y, bot_y } )
	{
		zdraw::get_draw_list( ).add_rect_filled_multi_color( ix, ly, fade_w, 1.0f, trans, accent, accent, trans );
		zdraw::get_draw_list( ).add_rect_filled( ix + fade_w, ly, solid_w, 1.0f, accent );
		zdraw::get_draw_list( ).add_rect_filled_multi_color( ix + fade_w + solid_w, ly, fade_w, 1.0f, accent, trans, trans, accent );
	}
}

void menu::draw_combat( )
{
	const auto [avail_w, avail_h] = zui::get_content_region_avail( );
	const auto col_w = ( avail_w - 8.0f ) * 0.5f;
	const auto& style = zui::get_style( );

	{
		const auto win = zui::detail::get_current_window( );
		if ( win )
		{
			constexpr auto group_spacing{ 12.0f };
			constexpr auto bar_h{ 22.0f };
			auto gx = win->bounds.x + style.window_padding_x;
			const auto gy = win->bounds.y + win->cursor_y;

			for ( int i = 0; i < 6; ++i )
			{
				auto [tw, th] = zdraw::measure_text( k_weapon_groups[ i ] );
				const auto gr = zui::rect{ gx, gy, tw, bar_h };
				const auto hov = zui::detail::mouse_hovered( gr ) && !zui::detail::overlay_blocking_input( );

				if ( hov && zui::detail::mouse_clicked( ) )
				{
					this->m_weapon_group = i;
				}

				const auto sel = ( this->m_weapon_group == i );
				const auto col = sel ? zui::get_accent_color( ) : zui::lerp( zdraw::rgba{ 100, 100, 100, 255 }, style.text, hov ? 1.0f : 0.0f );

				zdraw::get_draw_list( ).add_text( gx, gy + ( bar_h - th ) * 0.5f, k_weapon_groups[ i ], nullptr, col );

				if ( sel )
				{
					const auto accent = zui::get_accent_color( );
					const auto trans = zdraw::rgba{ accent.r, accent.g, accent.b, 0 };
					const auto fade = tw * 0.3f;
					zdraw::get_draw_list( ).add_rect_filled_multi_color( gx, gy + bar_h - 2.0f, fade, 1.0f, trans, accent, accent, trans );
					zdraw::get_draw_list( ).add_rect_filled( gx + fade, gy + bar_h - 2.0f, tw - fade * 2.0f, 1.0f, accent );
					zdraw::get_draw_list( ).add_rect_filled_multi_color( gx + tw - fade, gy + bar_h - 2.0f, fade, 1.0f, accent, trans, trans, accent );
				}

				gx += tw + group_spacing;
			}

			win->cursor_y += bar_h + style.item_spacing_y;
			win->line_height = 0.0f;
		}
	}

	auto& cfg = settings::g_combat.groups[ this->m_weapon_group ];

	if ( zui::begin_group_box( "aimbot", col_w ) )
	{
		zui::checkbox( "enabled##ab", cfg.aimbot.enabled );
		zui::keybind( "key##ab", cfg.aimbot.key );

		zui::slider_int( "fov##ab", cfg.aimbot.fov, 1, 360 );
		zui::slider_int( "smoothing##ab", cfg.aimbot.smoothing, 0, 50 );

		zui::checkbox( "recoil control##ab", cfg.aimbot.rcs );

		if ( cfg.aimbot.rcs )
		{
			if ( zui::begin_popup( "##ab_rcs_popup", 200.0f ) )
			{
				zui::slider_float( "rcs factor##ab", cfg.aimbot.rcs_factor, 0.0f, 2.0f, "%.2f" );
				zui::end_popup( );
			}
		}

		zui::checkbox( "multipoint##ab", cfg.aimbot.multipoint );

		if ( cfg.aimbot.multipoint )
		{
			if ( zui::begin_popup( "##ab_mp_popup", 200.0f ) )
			{
				zui::slider_float( "multipoint scale##ab", cfg.aimbot.multipoint_scale, 0.1f, 1.0f, "%.2f" );
				zui::end_popup( );
			}
		}
		
		zui::text("hitboxes");
		if (zui::begin_popup("##hitboxpopup", 200.f))
		{
			zui::checkbox("head##ab_hg", cfg.aimbot.hitgroups.head);
			zui::checkbox("chest##ab_hg", cfg.aimbot.hitgroups.chest);
			zui::checkbox("stomach##ab_hg", cfg.aimbot.hitgroups.stomach);
			zui::checkbox("arms##ab_hg", cfg.aimbot.hitgroups.arms);
			zui::checkbox("legs##ab_hg", cfg.aimbot.hitgroups.legs);
			zui::end_popup();
		}

		zui::checkbox( "visible only##ab", cfg.aimbot.visible_only );
		zui::checkbox( "smoke check##ab", cfg.aimbot.smoke_check );

		if ( cfg.aimbot.visible_only )
		{
			zui::checkbox( "autowall##ab", cfg.aimbot.autowall );

			if ( zui::begin_popup( "##ab_aw_popup", 200.0f ) )
			{
				zui::slider_float( "min damage##ab", cfg.aimbot.min_damage, 1.0f, 100.0f, "%.0f" );
				zui::end_popup( );
			}
		}

		zui::checkbox( "silent##ab", cfg.aimbot.silent );
		zui::checkbox( "predictive##ab", cfg.aimbot.predictive );
		zui::separator( );
		zui::checkbox( "draw fov##ab", cfg.aimbot.draw_fov );

		if ( cfg.aimbot.draw_fov )
		{
			if ( zui::begin_popup( "##ab_fov_popup", 200.0f ) )
			{
				zui::color_picker( "color##ab", cfg.aimbot.fov_color );
				zui::end_popup( );
			}
		}

		zui::end_group_box( );
	}

	zui::same_line( );

	if ( zui::begin_group_box( "triggerbot", col_w ) )
	{
		zui::checkbox( "enabled##tb", cfg.triggerbot.enabled );
		zui::keybind( "key##tb", cfg.triggerbot.key );
		zui::slider_float( "hitchance##tb", cfg.triggerbot.hitchance, 0.0f, 100.0f, "%.0f%%" );
		zui::slider_int( "delay (ms)##tb", cfg.triggerbot.delay, 0, 500 );

		zui::text("hitboxes");
		if (zui::begin_popup("##hitboxpopup", 200.f))
		{
			zui::checkbox("head##ab_hg", cfg.triggerbot.hitgroups.head);
			zui::checkbox("chest##ab_hg", cfg.triggerbot.hitgroups.chest);
			zui::checkbox("stomach##ab_hg", cfg.triggerbot.hitgroups.stomach);
			zui::checkbox("arms##ab_hg", cfg.triggerbot.hitgroups.arms);
			zui::checkbox("legs##ab_hg", cfg.triggerbot.hitgroups.legs);
			zui::end_popup();
		}


		zui::checkbox( "autowall##tb", cfg.triggerbot.autowall );

		if ( zui::begin_popup( "##tb_aw_popup", 200.0f ) )
		{
			zui::slider_float( "min damage##tb", cfg.triggerbot.min_damage, 1.0f, 100.0f, "%.0f" );
			zui::end_popup( );
		}

		zui::checkbox( "autostop##tb", cfg.triggerbot.autostop );

		if ( zui::begin_popup( "##tb_as_popup", 200.0f ) )
		{
			zui::checkbox( "early autostop##tb", cfg.triggerbot.early_autostop );
			zui::end_popup( );
		}

		zui::checkbox( "pen crosshair##tb", cfg.other.penetration_crosshair );

		if ( zui::begin_popup( "##tb_pc_popup", 200.0f ) )
		{
			zui::checkbox( "show damage dealt##tb", cfg.other.penetration_damage );
			zui::color_picker( "pen color##tb", cfg.other.penetration_color_yes );
			zui::color_picker( "blocked color##tb", cfg.other.penetration_color_no );
			zui::end_popup( );
		}

		zui::checkbox("draw info##tb", cfg.aimbot.autowall_info);

		if ( zui::begin_popup( "##tb_info_popup", 200.0f ) )
		{
			zui::color_picker( "color##tb", cfg.aimbot.autowall_info_color );
			zui::end_popup( );
		}

		zui::checkbox( "predictive##tb", cfg.triggerbot.predictive );

		if ( cfg.triggerbot.predictive )
		{
			if ( zui::begin_popup( "##tb_pred_popup", 200.0f ) )
			{
				zui::checkbox( "visualize##tb", cfg.triggerbot.predictive_visualize );
				zui::slider_int( "lead ms##tb", cfg.triggerbot.predictive_ms, 0, 600, "%d ms" );
				zui::end_popup( );
			}
		}
		zui::checkbox( "seed triggerbot##tb", cfg.triggerbot.seed_triggerbot );
		zui::checkbox( "show spread##tb", cfg.triggerbot.show_spread );
		zui::checkbox( "magnet trigger##tb", cfg.triggerbot.magnet );

		if ( cfg.triggerbot.magnet )
		{
			if ( zui::begin_popup( "##tb_magnet_popup", 200.0f ) )
			{
				zui::slider_int( "smoothing##tb", cfg.triggerbot.magnet_smoothing, 1, 50, "%d" );
				zui::end_popup( );
			}
		}

		zui::end_group_box( );
	}

}

void menu::draw_movement()
{
	const auto [avail_w, avail_h] = zui::get_content_region_avail();
	const auto col_w = (avail_w - 8.0f);

	if (zui::begin_group_box("general", col_w))
	{
		zui::checkbox("bhop##mv", settings::g_movement.bhop.enabled); 
		zui::text("Type \"bind f11 +jump -jump; unbind space\" into the dev console");
		
		zui::checkbox("quickstop##mv", settings::g_movement.quickstop.enabled);
		if (settings::g_movement.quickstop.enabled)
		{
			if (zui::begin_popup("##mv_qs_popup", 200.0f))
			{
				zui::slider_float("strength##mv", settings::g_movement.quickstop.strength, 0.1f, 2.0f, "%.2f");
				zui::end_popup();
			}
		}

		zui::end_group_box();
	}
}

void menu::draw_nades( )
{
	const auto [avail_w, avail_h] = zui::get_content_region_avail( );
	const auto col_w = ( avail_w - 8.0f ) * 0.5f;
	auto& cfg = settings::g_misc.m_nade_helper;

	if ( zui::begin_group_box( "settings", col_w ) )
	{
		zui::checkbox( "enabled##nh", cfg.enabled );
		if (zui::begin_popup("##nh_popup", 200.0f))
		{
			zui::slider_float("stand radius##nh", cfg.stand_radius, 5.0f, 50.0f, "%.1f");
			zui::slider_float("aim dot size##nh", cfg.aim_dot_size, 1.0f, 10.0f, "%.1f");
			zui::separator();
			zui::color_picker("stand pos color##nh", cfg.stand_pos_color);
			zui::color_picker("aim pos color##nh", cfg.aim_pos_color);
			zui::color_picker("text color##nh", cfg.text_color);
			zui::end_popup();
		}
		zui::checkbox( "show name##nh", cfg.show_name );
		zui::checkbox( "show throw type##nh", cfg.show_type );
		zui::end_group_box( );
	}

	zui::same_line( );

	if ( zui::begin_group_box( "nade list", col_w ) )
	{
		auto& nades = features::misc::g_nade_helper.get_nades( );
		static int selected{ -1 };

		if ( zui::begin_nested_window( "##nade_list_items", col_w - 20.0f, 150.0f ) )
		{
			for ( int i = 0; i < ( int )nades.size( ); ++i )
			{
				if ( zui::button( nades[ i ].name.c_str( ), col_w - 40.0f, 20.0f ) )
				{
					selected = i;
				}
			}
			zui::end_nested_window( );
		}

		zui::separator( );

		if ( zui::button( "add current position", col_w - 12.0f, 24.0f ) )
		{
			features::misc::nade_data n;
			n.name = std::format( "nade_{}", nades.size( ) );

			const auto local_pawn = systems::g_local.view_pawn( );
			if ( local_pawn )
			{
				const auto game_scene_node = g::memory.read<std::uintptr_t>( local_pawn + SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );
				if ( game_scene_node )
					n.pos = g::memory.read<math::vector3>( game_scene_node + SCHEMA( "CGameSceneNode", "m_vecAbsOrigin"_hash ) );
			}

			const auto view_angles = systems::g_view.angles( );
			math::vector3 forward{}, right{}, up{};
			math::helpers::angle_vectors( view_angles, forward, right, up );

			const auto eye_pos = systems::g_view.origin( );
			n.target_pos = eye_pos + forward * 10000.0f;
			n.throw_type = 0;

			const auto weapon_vdata = systems::g_local.weapon_vdata( );
			n.nade_type = 0;
			if ( weapon_vdata )
			{
				const auto weapon_name_ptr = g::memory.read<std::uintptr_t>( weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_szName"_hash ) );
				if ( weapon_name_ptr )
				{
					const auto weapon_name = g::memory.read_string( weapon_name_ptr, 64 );
					if ( weapon_name.find( "hegrenade" ) != std::string::npos ) n.nade_type = 0;
					else if ( weapon_name.find( "flashbang" ) != std::string::npos ) n.nade_type = 1;
					else if ( weapon_name.find( "smokegrenade" ) != std::string::npos ) n.nade_type = 2;
					else if ( weapon_name.find( "molotov" ) != std::string::npos ) n.nade_type = 3;
					else if ( weapon_name.find( "incendiary" ) != std::string::npos ) n.nade_type = 4;
					else if ( weapon_name.find( "decoy" ) != std::string::npos ) n.nade_type = 5;
				}
			}

			nades.push_back( n );
		}

		if ( selected != -1 && selected < ( int )nades.size( ) )
		{
			auto& n = nades[ selected ];
			zui::text_input( "name##nade_edit", n.name, 32 );

			constexpr const char* throw_types[ ]{ "stand", "jump", "walk", "run", "crouch", "crouch jump" };
			zui::combo( "throw type##nade_edit", n.throw_type, throw_types, 6 );

			constexpr const char* nade_types[ ]{ "HE", "Flash", "Smoke", "Molly", "Incendiary", "Decoy" };
			zui::combo( "grenade type##nade_edit", n.nade_type, nade_types, 6 );

			if ( zui::button( "delete nade", col_w - 12.0f, 24.0f ) )
			{
				nades.erase( nades.begin( ) + selected );
				selected = -1;
			}
		}

		zui::separator( );

		if ( zui::button( "save map", ( col_w - 18.0f ) * 0.5f, 24.0f ) )
		{
			features::misc::g_nade_helper.save_nades( "" );
		}
		zui::same_line( );
		if ( zui::button( "load map", ( col_w - 18.0f ) * 0.5f, 24.0f ) )
		{
			features::misc::g_nade_helper.load_nades( "" );
		}

		zui::end_group_box( );
	}
}


void menu::draw_esp( )
{
	const auto [avail_w, avail_h] = zui::get_content_region_avail( );
	const auto col_w = ( avail_w - 8.0f ) * 0.5f;
	auto& p = settings::g_esp.m_player;

	const auto [start_x, start_y] = zui::get_cursor_pos( );

	if ( zui::begin_group_box( "players", col_w ) )
	{
		zui::checkbox( "enabled##pl", p.enabled );

		zui::checkbox( "box##bx", p.m_box.enabled );
		if ( zui::begin_popup( "##bx_popup", 200.0f ) )
		{
			constexpr const char* box_styles[ ]{ "full", "cornered" };
			zui::combo("style##bx", p.m_box.style, box_styles, 2);

			zui::checkbox( "fill##bx", p.m_box.fill );
			zui::checkbox( "outline##bx", p.m_box.outline );

			if ( p.m_box.style == (int)settings::esp::player::box::style0::cornered )
			{
				zui::slider_float( "corner length##bx", p.m_box.corner_length, 4.0f, 30.0f, "%.0f" );
			}

			zui::color_picker( "visible##bx", p.m_box.visible_color );
			zui::color_picker( "occluded##bx", p.m_box.occluded_color );
			zui::end_popup( );
		}

		zui::checkbox( "skeleton##sk", p.m_skeleton.enabled );
		if ( zui::begin_popup( "##sk_popup", 200.0f ) )
		{
			zui::checkbox( "rounded##sk", p.m_skeleton.rounded );
			zui::slider_float( "thickness##sk", p.m_skeleton.thickness, 0.5f, 4.0f, "%.1f" );
			zui::color_picker( "visible##sk", p.m_skeleton.visible_color );
			zui::color_picker( "occluded##sk", p.m_skeleton.occluded_color );
			zui::end_popup( );
		}

		zui::checkbox( "health bar##hb", p.m_health_bar.enabled );
		if ( zui::begin_popup( "##hb_popup", 200.0f ) )
		{
			constexpr const char* positions[ ]{ "left", "top", "bottom" };
			zui::combo("position##hb", p.m_health_bar.position, positions, 3);

			zui::checkbox( "outline##hb", p.m_health_bar.outline );
			zui::checkbox( "gradient##hb", p.m_health_bar.gradient );
			zui::checkbox( "show value##hb", p.m_health_bar.show_value );
			zui::color_picker( "full color##hb", p.m_health_bar.full_color );
			zui::color_picker( "low color##hb", p.m_health_bar.low_color );
			zui::end_popup( );
		}

		zui::checkbox( "ammo bar##amb", p.m_ammo_bar.enabled );
		if ( zui::begin_popup( "##amb_popup", 200.0f ) )
		{
			constexpr const char* positions[ ]{ "left", "top", "bottom" };
			zui::combo("position##amb", p.m_ammo_bar.position, positions, 3);
			zui::checkbox( "outline##amb", p.m_ammo_bar.outline );
			zui::checkbox( "gradient##amb", p.m_ammo_bar.gradient );
			zui::checkbox( "show value##amb", p.m_ammo_bar.show_value );
			zui::color_picker( "full color##amb", p.m_ammo_bar.full_color );
			zui::color_picker( "low color##amb", p.m_ammo_bar.low_color );
			zui::end_popup( );
		}

		zui::checkbox( "name##nm", p.m_name.enabled );
		if ( zui::begin_popup( "##nm_popup", 200.0f ) )
		{
			zui::color_picker( "color##nm", p.m_name.color );
			zui::end_popup( );
		}

		zui::checkbox( "weapon##wp", p.m_weapon.enabled );
		if ( zui::begin_popup( "##wp_popup", 200.0f ) )
		{
			constexpr const char* disp_types[ ]{ "text", "icon", "text + icon" };

			zui::combo("display##wp", p.m_weapon.display, disp_types, 3);

			zui::color_picker( "text color##wp", p.m_weapon.text_color );
			zui::color_picker( "icon color##wp", p.m_weapon.icon_color );
			zui::end_popup( );
		}

		zui::checkbox( "hitboxes##ht", p.m_hitboxes.enabled );
		if ( zui::begin_popup( "##ht_popup", 200.0f ) )
		{
			constexpr const char* materials[ ]{ "flat", "glow", "pulse", "wireframe" };

			zui::combo( "material mode##ht", ( int& )p.m_hitboxes.mode, materials, 4 );

			if ((int)p.m_hitboxes.mode == 3) {
				zui::color_picker("visible##ht", p.m_hitboxes.visible_color);
				zui::color_picker("occluded##ht", p.m_hitboxes.occluded_color);
			}
			else {
				zui::checkbox("fill##ht", p.m_hitboxes.fill);
				if (p.m_hitboxes.fill)
				{
					zui::color_picker("visible##ht", p.m_hitboxes.visible_color);
					zui::color_picker("occluded##ht", p.m_hitboxes.occluded_color);
				}

				zui::checkbox("outline##ht", p.m_hitboxes.outline);
				if (p.m_hitboxes.outline)
				{
					zui::color_picker("outline color##ht", p.m_hitboxes.outline_color);
				}
			}
			zui::checkbox("health indicator##ht", p.m_hitboxes.health_indicator);
			zui::end_popup( );
		}

		zui::checkbox( "info flags##if", p.m_info_flags.enabled );
		if ( zui::begin_popup( "##if_popup", 200.0f ) )
		{
			auto flag_checkbox = [ & ]( const char* label, settings::esp::player::info_flags::flag f )
				{
					bool val = p.m_info_flags.has( f );
					if ( zui::checkbox( label, val ) )
					{
						if ( val ) p.m_info_flags.flags |= f;
						else       p.m_info_flags.flags &= ~f;
					}
				};

			flag_checkbox( "money", settings::esp::player::info_flags::money );
			flag_checkbox( "armor", settings::esp::player::info_flags::armor );
			flag_checkbox( "kit", settings::esp::player::info_flags::kit );
			flag_checkbox( "scoped", settings::esp::player::info_flags::scoped );
			flag_checkbox( "defusing", settings::esp::player::info_flags::defusing );
			flag_checkbox( "flashed", settings::esp::player::info_flags::flashed );
			flag_checkbox( "ping", settings::esp::player::info_flags::ping );
			flag_checkbox( "distance", settings::esp::player::info_flags::distance );
			zui::end_popup( );
		}

		zui::end_group_box( );
	}

	if ( zui::begin_group_box( "projectiles", col_w ) )
	{
		auto& pr = settings::g_esp.m_projectile;
		zui::checkbox( "enabled##pr", pr.enabled );
		if (zui::begin_popup("##pr_popup", 200.0f))
		{
			zui::color_picker("he color##pr", pr.color_he);
			zui::color_picker("flash color##pr", pr.color_flash);
			zui::color_picker("smoke color##pr", pr.color_smoke);
			zui::color_picker("molotov color##pr", pr.color_molotov);
			zui::color_picker("decoy color##pr", pr.color_decoy);
			zui::end_popup();
		}
		zui::checkbox( "icon##pr", pr.show_icon );
		zui::checkbox( "name##pr", pr.show_name );
		zui::checkbox( "timer bar##pr", pr.show_timer_bar );
		zui::checkbox( "inferno bounds##pr", pr.show_inferno_bounds );
		zui::checkbox( "smoke voxels##pr", pr.show_smoke_voxels );

		zui::end_group_box( );
	}

	const auto [bottom_x, bottom_y] = zui::get_cursor_pos( );
	zui::set_cursor_pos( start_x + col_w + 8.0f, start_y );

	if ( zui::begin_group_box( "items", col_w ) )
	{
		auto& it = settings::g_esp.m_item;
		zui::checkbox( "enabled##it", it.enabled );
		zui::slider_float( "max distance##it", it.max_distance, 5.0f, 150.0f, "%.0fm" );

		zui::checkbox( "icon##it", it.m_icon.enabled );
		if ( zui::begin_popup( "##it_icon_popup", 200.0f ) )
		{
			zui::color_picker( "color##it", it.m_icon.color );
			zui::end_popup( );
		}

		zui::checkbox( "name##it", it.m_name.enabled );
		if ( zui::begin_popup( "##it_name_popup", 200.0f ) )
		{
			zui::color_picker( "color##it", it.m_name.color );
			zui::end_popup( );
		}

		zui::checkbox( "ammo##it", it.m_ammo.enabled );
		if ( zui::begin_popup( "##it_ammo_popup", 200.0f ) )
		{
			zui::color_picker( "color##it", it.m_ammo.color );
			zui::color_picker( "empty color##it", it.m_ammo.empty_color );
			zui::end_popup( );
		}

		zui::separator( );

		auto& f = it.m_filters;
		static bool filter_selected[ 8 ];
		static settings::esp::item::filters last_f{};

		// sync from settings if they were changed (e.g. by config load)
		if ( std::memcmp( &f, &last_f, sizeof( f ) ) != 0 )
		{
			filter_selected[ 0 ] = f.rifles;
			filter_selected[ 1 ] = f.smgs;
			filter_selected[ 2 ] = f.shotguns;
			filter_selected[ 3 ] = f.snipers;
			filter_selected[ 4 ] = f.pistols;
			filter_selected[ 5 ] = f.heavy;
			filter_selected[ 6 ] = f.grenades;
			filter_selected[ 7 ] = f.utility;

			last_f = f;
		}

		constexpr const char* filter_items[ ]{ "rifles", "smgs", "shotguns", "snipers", "pistols", "heavy", "grenades", "utility" };
		if ( zui::multicombo( "filters##f", filter_selected, filter_items, 8 ) )
		{
			f.rifles = filter_selected[ 0 ];
			f.smgs = filter_selected[ 1 ];
			f.shotguns = filter_selected[ 2 ];
			f.snipers = filter_selected[ 3 ];
			f.pistols = filter_selected[ 4 ];
			f.heavy = filter_selected[ 5 ];
			f.grenades = filter_selected[ 6 ];
			f.utility = filter_selected[ 7 ];

			last_f = f;
		}

		zui::end_group_box( );
	}

	if ( zui::begin_group_box( "other", col_w ) )
	{
		auto& mt = settings::g_esp.m_player.m_trails;
		zui::checkbox( "movement trails##mt", mt.enabled );
		if ( zui::begin_popup( "##mt_popup", 200.0f ) )
		{
			zui::checkbox( "local trail##mt", mt.local );
			zui::color_picker( "local color##mt", mt.local_color );
			zui::checkbox( "enemy trail##mt", mt.enemy );
			zui::color_picker( "enemy color##mt", mt.enemy_color );
			zui::checkbox( "team trail##mt", mt.team );
			zui::color_picker( "team color##mt", mt.team_color );
			zui::slider_float( "thickness##mt", mt.thickness, 0.5f, 5.0f, "%.1f" );
			zui::slider_float( "lifetime##mt", mt.lifetime, 0.1f, 5.0f, "%.1f" );
			zui::end_popup( );
		}

		auto& bt = settings::g_esp.m_bullet_tracers;
		zui::checkbox( "bullet tracers##bt", bt.enabled );
		if ( zui::begin_popup( "##bt_popup", 200.0f ) )
		{
			zui::color_picker( "color##bt", bt.color );
			zui::slider_float( "thickness##bt", bt.thickness, 0.5f, 5.0f, "%.1f" );
			zui::slider_int( "duration##bt", bt.duration, 500, 5000, "%dms" );
			zui::end_popup( );
		}

		auto& oof = p.m_oof_arrow;
		zui::checkbox( "out of fov arrows##oof", oof.enabled );
		if ( zui::begin_popup( "##oof_popup", 200.0f ) )
		{
			zui::slider_float( "radius##oof", oof.radius, 50.0f, 400.0f, "%.0f" );
			zui::slider_float( "size##oof", oof.size, 5.0f, 40.0f, "%.0f" );
			zui::color_picker( "color##oof", oof.color );
			zui::end_popup( );
		}

		auto& hm = settings::g_esp.m_hit_marker;
		zui::checkbox( "hit marker##hm", hm.enabled );
		if ( zui::begin_popup( "##hm_popup", 200.0f ) )
		{
			zui::slider_int( "size##hm", hm.size, 2, 20 );
			zui::slider_int( "gap##hm", hm.gap, 0, 10 );
			zui::color_picker( "color##hm", hm.color );
			zui::end_popup( );
		}

		auto& di = settings::g_esp.m_damage_indicator;
		zui::checkbox( "damage indicator##di", di.enabled );
		if ( zui::begin_popup( "##di_popup", 200.0f ) )
		{
			zui::slider_int( "duration##di", di.duration, 500, 3000, "%dms" );
			zui::slider_float( "floating speed##di", di.floating_speed, 10.0f, 100.0f, "%.1f" );
			zui::color_picker( "color##di", di.color );
			zui::color_picker( "crit color##di", di.crit_color );
			zui::end_popup( );
		}

		auto& fs = p.m_footsteps;
		zui::checkbox( "footsteps ESP##fs", fs.enabled );
		if ( zui::begin_popup( "##fs_popup", 200.0f ) )
		{
			zui::checkbox( "teammates##fs", fs.show_teammates );
			zui::slider_float( "expand duration##fs", fs.expand_duration, 0.1f, 2.0f, "%.1fs" );
			zui::slider_float( "fade duration##fs", fs.fade_duration, 0.1f, 5.0f, "%.1fs" );
			zui::slider_float( "thickness##fs", fs.thickness, 0.5f, 5.0f, "%.1f" );
			zui::slider_int( "segments##fs", fs.segments, 8, 128 );

			zui::separator( );
			zui::color_picker( "footstep color##fs", fs.footstep_color );
			zui::color_picker( "jump color##fs", fs.jump_color );
			zui::color_picker( "land color##fs", fs.land_color );
			zui::end_popup( );
		}

		zui::end_group_box( );
	}

	const auto [right_bottom_x, right_bottom_y] = zui::get_cursor_pos( );
	zui::set_cursor_pos( start_x, std::max( bottom_y, right_bottom_y ) );
}


void menu::draw_misc( )
{
	const auto [avail_w, avail_h] = zui::get_content_region_avail( );
	const auto col_w = ( avail_w - 8.0f ) * 0.5f;

	if ( zui::begin_group_box( "grenade prediction", col_w ) )
	{
		auto& gr = settings::g_misc.m_grenades;
		zui::checkbox( "enabled##gr", gr.enabled );
		if (zui::begin_popup("##gr_popup", 200.0f))
		{
			zui::checkbox("gradient line##gr", gr.line_gradient);
			zui::slider_float("line thickness##gr", gr.line_thickness, 0.5f, 5.0f, "%.1f");
			zui::color_picker("line color##gr", gr.line_color);
			zui::separator();
			zui::checkbox("show bounces##gr", gr.show_bounces);
			zui::color_picker("bounce color##gr", gr.bounce_color);
			zui::slider_float("bounce size##gr", gr.bounce_size, 1.0f, 8.0f, "%.1f");
			zui::separator();
			zui::color_picker("detonate color##gr", gr.detonate_color);
			zui::slider_float("detonate size##gr", gr.detonate_size, 1.0f, 10.0f, "%.1f");
			zui::slider_float("fade duration##gr", gr.fade_duration, 0.0f, 2.0f, "%.2f");
			zui::end_popup();
		}
		zui::checkbox( "local only##gr", gr.local_only );
		zui::checkbox( "per type colors##ptc", gr.per_type_colors );
		if ( gr.per_type_colors )
		{
			if ( zui::begin_popup( "##ptc_popup", 200.0f ) )
			{
				zui::color_picker( "he##ptc", gr.color_he );
				zui::color_picker( "flash##ptc", gr.color_flash );
				zui::color_picker( "smoke##ptc", gr.color_smoke );
				zui::color_picker( "molotov##ptc", gr.color_molotov );
				zui::color_picker( "decoy##ptc", gr.color_decoy );
				zui::end_popup( );
			}
		}

		zui::end_group_box( );
	}

	zui::same_line( );

	if ( zui::begin_group_box( "other", col_w ) )
	{
		auto& mm = settings::g_misc.m_main;
		zui::checkbox( "spectator list##mm", mm.spectator_list );
		if ( mm.spectator_list )
		{
			if ( zui::begin_popup( "##sl_popup", 200.0f ) )
			{
				zui::color_picker( "color##mm", mm.spectator_list_color );
				zui::end_popup( );
			}
		}

		zui::checkbox( "bomb timer##mm", mm.bomb_timer );
		if ( mm.bomb_timer )
		{
			if ( zui::begin_popup( "##bt_popup", 200.0f ) )
			{
				zui::color_picker( "color##mm", mm.bomb_timer_color );
				zui::end_popup( );
			}
		}

		zui::checkbox( "watermark##mm", mm.watermark );

		constexpr const char* hitsounds[ ]{ "none", "metallic", "bell", "bubble" };
		zui::combo( "hitsound##mm", mm.hitsound, hitsounds, 4 );

		zui::separator( );

		auto& fc = settings::g_misc.m_fov_changer;
		zui::checkbox( "fov changer", fc.enabled );
		if ( fc.enabled )
		{
			if ( zui::begin_popup( "##fc_popup", 200.0f ) )
			{
				zui::slider_int( "fov value", fc.fov, 30, 160 );
				zui::slider_int( "viewmodel fov", fc.viewmodel_fov, 30, 160 );
				zui::checkbox( "disable when scoped", fc.disable_when_scoped );
				zui::end_popup( );
			}
		}

		zui::end_group_box( );
	}
}


void menu::draw_configs( )
{
	const auto [avail_w, avail_h] = zui::get_content_region_avail( );
	const auto col_w = ( avail_w - 8.0f ) * 0.5f;

	if ( zui::begin_group_box( "config list", col_w ) )
	{
		static std::string config_name{};
		zui::text_input( "##cfg_name", config_name, 64 );

		if ( zui::button( "create config", col_w - 12.0f, 24.0f ) )
		{
			if ( !config_name.empty( ) )
			{
				g::config.save( config_name );
				config_name.clear( );
			}
		}

		zui::separator( );

		const auto& cfgs = g::config.get_configs( );
		static int selected{ -1 };

		if ( zui::begin_nested_window( "##cfg_list", col_w - 20.0f, 150.0f ) )
		{
			for ( int i = 0; i < ( int )cfgs.size( ); ++i )
			{
				if ( zui::button( cfgs[ i ].c_str( ), col_w - 40.0f, 20.0f ) )
				{
					selected = i;
				}
			}
			zui::end_nested_window( );
		}

		zui::separator( );

		if ( !cfgs.empty( ) && selected != -1 && selected < ( int )cfgs.size( ) )
		{
			const auto& cur = cfgs[ selected ];

			if ( zui::button( "load", ( col_w - 36.0f ) / 3.0f, 24.0f ) )
			{
				g::config.load( cur );
			}

			zui::same_line( );

			if ( zui::button( "save", ( col_w - 36.0f ) / 3.0f, 24.0f ) )
			{
				g::config.save( cur );
			}

			zui::same_line( );

			if ( zui::button( "delete", ( col_w - 36.0f ) / 3.0f, 24.0f ) )
			{
				g::config.remove( cur );
				selected = -1;
			}
		}

		zui::end_group_box( );
	}

	zui::same_line( );

	if ( zui::begin_group_box( "information", col_w ) )
	{
		zui::text( "config path:" );
		zui::text_colored( g::config.get_path( ).string( ).c_str( ), zui::get_style( ).accent );

		zui::separator( );

		if ( zui::button( "refresh list", col_w - 12.0f, 24.0f ) )
		{
			g::config.refresh_list( );
		}

		if ( zui::button( "open folder", col_w - 12.0f, 24.0f ) )
		{
			ShellExecuteA( NULL, "open", g::config.get_path( ).string( ).c_str( ), NULL, NULL, SW_SHOW );
		}

		zui::end_group_box( );
	}
}

void menu::draw_skinchanger( )
{
	const auto [avail_w, avail_h] = zui::get_content_region_avail( );
	const auto col_w = ( avail_w - 8.0f ) * 0.5f;

	if ( zui::begin_group_box( "settings", col_w ) )
	{
		zui::checkbox( "enabled##sc", settings::g_skinchanger.enabled );
		if ( zui::button( "force update##sc", col_w - 20.0f, 24.0f ) )
		{
			features::skinchanger::g_skinchanger.m_force_update = true;
		}

		zui::separator();

		auto music_kits = features::skinchanger::g_skindb.get_music_kits();
		std::vector<const char*> music_kit_names;
		for (const auto& mk : music_kits) music_kit_names.push_back(mk.name.c_str());

		static int selected_music_idx = 0;
		static bool music_init = false;
		if (!music_init) {
			for (int i = 0; i < (int)music_kits.size(); ++i) {
				if (music_kits[i].id == settings::g_skinchanger.music_kit) {
					selected_music_idx = i;
					break;
				}
			}
			music_init = true;
		}

		if (zui::combo("music kit##sc", selected_music_idx, music_kit_names.data(), (int)music_kit_names.size())) {
			settings::g_skinchanger.music_kit = music_kits[selected_music_idx].id;
		}

		zui::end_group_box( );
	}

	zui::same_line( );

	if ( zui::begin_group_box( "current weapon", col_w ) )
	{
		const auto local_pawn = systems::g_local.pawn();
		if (!local_pawn) {
			zui::text("please join a game");
			zui::end_group_box();
			return;
		}

		const auto weapon_services = g::memory.read<std::uintptr_t>(local_pawn + SCHEMA("C_BasePlayerPawn", "m_pWeaponServices"_hash));
		if (!weapon_services) {
			zui::text("could not get weapon services");
			zui::end_group_box();
			return;
		}

		const auto h_active_weapon = g::memory.read<std::uint32_t>(weapon_services + SCHEMA("CPlayer_WeaponServices", "m_hActiveWeapon"_hash));
		const auto active_weapon = systems::g_entities.lookup(h_active_weapon);
		if (!active_weapon) {
			zui::text("no weapon held");
			zui::end_group_box();
			return;
		}

		const auto item_view = active_weapon + SCHEMA("C_EconEntity", "m_AttributeManager"_hash) + SCHEMA("C_AttributeContainer", "m_Item"_hash);
		const auto def_idx = g::memory.read<std::uint16_t>(item_view + SCHEMA("C_EconItemView", "m_iItemDefinitionIndex"_hash));

		auto get_weapon_name = [](int id) -> std::string {
			switch (id) {
			case 1:  return "Deagle";
			case 2:  return "Dual Berettas";
			case 3:  return "Five-SeveN";
			case 4:  return "Glock-18";
			case 7:  return "AK-47";
			case 8:  return "AUG";
			case 9:  return "AWP";
			case 10: return "FAMAS";
			case 11: return "G3SG1";
			case 13: return "Galil AR";
			case 14: return "M249";
			case 16: return "M4A4";
			case 17: return "MAC-10";
			case 19: return "P90";
			case 23: return "MP5-SD";
			case 24: return "UMP-45";
			case 25: return "XM1014";
			case 26: return "PP-Bizon";
			case 27: return "MAG-7";
			case 28: return "Negev";
			case 29: return "Sawed-Off";
			case 30: return "Tec-9";
			case 31: return "Zeus x27";
			case 32: return "P2000";
			case 33: return "MP7";
			case 34: return "MP9";
			case 35: return "Nova";
			case 36: return "P250";
			case 38: return "SCAR-20";
			case 39: return "SG 553";
			case 40: return "SSG 08";
			case 60: return "M4A1-S";
			case 61: return "USP-S";
			case 63: return "CZ75-Auto";
			case 64: return "R8 Revolver";
			default: return std::string("weapon #") + std::to_string(id);
			}
		};

		zui::text("held: " + get_weapon_name(def_idx));
		zui::separator();

		std::unique_lock lock(settings::g_skinchanger.mutex);
		auto& cfg = settings::g_skinchanger.weapon_skins[def_idx];
		auto skins = features::skinchanger::g_skindb.get_weapon_skins((features::skinchanger::weapons_enum)def_idx);
		
		std::vector<const char*> skin_names;
		for (const auto& s : skins) skin_names.push_back(s.name.c_str());

		static int last_def_idx = -1;
		static int selected_skin_idx = 0;

		if ( last_def_idx != def_idx )
		{
			selected_skin_idx = 0;
			for ( int i = 0; i < ( int )skins.size( ); ++i )
			{
				if ( skins[ i ].paint_kit == cfg.paint_kit )
				{
					selected_skin_idx = i;
					break;
				}
			}
			last_def_idx = def_idx;
		}

		if ( zui::combo( "skin##sc", selected_skin_idx, skin_names.data( ), ( int )skin_names.size( ) ) )
		{
			cfg.paint_kit = skins[ selected_skin_idx ].paint_kit;
			cfg.uses_old_model = skins[selected_skin_idx].uses_old_model;
			g::console.print("Skin: {} is a {}", skins[selected_skin_idx].name, cfg.uses_old_model ? "Legacy skin" : "Newgen skin");
			features::skinchanger::g_skinchanger.m_force_update = true;
		}

		zui::slider_float("wear##sc", cfg.wear, 0.0f, 1.0f, "%.4f");
		zui::slider_int("seed##sc", cfg.seed, 0, 1000);
		zui::slider_int("stat-trak##sc", cfg.stat_trak, -1, 1337);

		zui::end_group_box( );
	}
}
