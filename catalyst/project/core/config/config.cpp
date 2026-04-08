#include <stdafx.hpp>
#include <fstream>
#include <sstream>
#include <shlobj.h>
#include <iomanip>

#include "config.hpp"

namespace fs = std::filesystem;

void config_system::init( )
{
	char path[ MAX_PATH ];
	if ( SUCCEEDED( SHGetFolderPathA( NULL, CSIDL_MYDOCUMENTS, NULL, 0, path ) ) )
	{
		this->m_path = fs::path( path ) / "catalyst";

		if ( !fs::exists( this->m_path ) )
		{
			fs::create_directory( this->m_path );
		}
	}

	this->refresh_list( );
	this->initialized = true;
}

void config_system::refresh_list( )
{
	this->m_configs.clear( );

	if ( !fs::exists( this->m_path ) )
	{
		return;
	}

	for ( const auto& entry : fs::directory_iterator( this->m_path ) )
	{
		if ( entry.is_regular_file( ) && entry.path( ).extension( ) == ".cfg" )
		{
			this->m_configs.push_back( entry.path( ).stem( ).string( ) );
		}
	}
}

template<typename T>
static void write( std::ofstream& out, std::string_view name, const T& value )
{
	out << name << "=" << value << "\n";
}

static void write( std::ofstream& out, std::string_view name, const zdraw::rgba& color )
{
	out << name << "=" << (int)color.r << "," << (int)color.g << "," << (int)color.b << "," << (int)color.a << "\n";
}

template<typename T> requires std::is_enum_v<T>
static void write( std::ofstream& out, std::string_view name, const T& value )
{
	out << name << "=" << static_cast<int>( value ) << "\n";
}

void config_system::save( std::string_view name )
{
	if ( name.empty( ) ) return;

	fs::path file = this->m_path / ( std::string( name ) + ".cfg" );
	std::ofstream out( file );

	if ( !out.is_open( ) ) return;

	for ( std::uint32_t i = 0; i < settings::combat::k_group_count; ++i )
	{
		auto& cfg = settings::g_combat.groups[ i ];
		std::string prefix = std::format( "combat.group_{}.", i );

		write( out, prefix + "aimbot.enabled", cfg.aimbot.enabled );
		write( out, prefix + "aimbot.key", cfg.aimbot.key );
		write( out, prefix + "aimbot.type", cfg.aimbot.type );
		write( out, prefix + "aimbot.fov", cfg.aimbot.fov );
		write( out, prefix + "aimbot.smoothing", cfg.aimbot.smoothing );
		write( out, prefix + "aimbot.autowall", cfg.aimbot.autowall );
		write( out, prefix + "aimbot.min_damage", cfg.aimbot.min_damage );
		write( out, prefix + "aimbot.visible_only", cfg.aimbot.visible_only );
		write( out, prefix + "aimbot.smoke_check", cfg.aimbot.smoke_check );
		write( out, prefix + "aimbot.draw_fov", cfg.aimbot.draw_fov );
		write( out, prefix + "aimbot.fov_color", cfg.aimbot.fov_color );
		write( out, prefix + "aimbot.autowall_info", cfg.aimbot.autowall_info );
		write( out, prefix + "aimbot.autowall_info_color", cfg.aimbot.autowall_info_color );
		write( out, prefix + "aimbot.predictive", cfg.aimbot.predictive );
		write( out, prefix + "aimbot.rcs", cfg.aimbot.rcs );
		write( out, prefix + "aimbot.silent", cfg.aimbot.silent );
		write( out, prefix + "aimbot.rcs_factor", cfg.aimbot.rcs_factor );

		write( out, prefix + "aimbot.hitgroups.head", cfg.aimbot.hitgroups.head );
		write( out, prefix + "aimbot.hitgroups.chest", cfg.aimbot.hitgroups.chest );
		write( out, prefix + "aimbot.hitgroups.stomach", cfg.aimbot.hitgroups.stomach );
		write( out, prefix + "aimbot.hitgroups.arms", cfg.aimbot.hitgroups.arms );
		write( out, prefix + "aimbot.hitgroups.legs", cfg.aimbot.hitgroups.legs );

		write( out, prefix + "aimbot.multipoint", cfg.aimbot.multipoint );
		write( out, prefix + "aimbot.multipoint_scale", cfg.aimbot.multipoint_scale );

		write( out, prefix + "triggerbot.enabled", cfg.triggerbot.enabled );
		write( out, prefix + "triggerbot.key", cfg.triggerbot.key );
		write( out, prefix + "triggerbot.seed_triggerbot", cfg.triggerbot.seed_triggerbot );
		write( out, prefix + "triggerbot.show_spread", cfg.triggerbot.show_spread );
		write( out, prefix + "triggerbot.hitchance", cfg.triggerbot.hitchance );
		write( out, prefix + "triggerbot.delay", cfg.triggerbot.delay );

		write( out, prefix + "triggerbot.hitgroups.head", cfg.triggerbot.hitgroups.head );
		write( out, prefix + "triggerbot.hitgroups.chest", cfg.triggerbot.hitgroups.chest );
		write( out, prefix + "triggerbot.hitgroups.stomach", cfg.triggerbot.hitgroups.stomach );
		write( out, prefix + "triggerbot.hitgroups.arms", cfg.triggerbot.hitgroups.arms );
		write( out, prefix + "triggerbot.hitgroups.legs", cfg.triggerbot.hitgroups.legs );

		write( out, prefix + "triggerbot.autowall", cfg.triggerbot.autowall );
		write( out, prefix + "triggerbot.min_damage", cfg.triggerbot.min_damage );
		write( out, prefix + "triggerbot.autostop", cfg.triggerbot.autostop );
		write( out, prefix + "triggerbot.early_autostop", cfg.triggerbot.early_autostop );
		write( out, prefix + "triggerbot.predictive", cfg.triggerbot.predictive );
		write( out, prefix + "triggerbot.predictive_ms", cfg.triggerbot.predictive_ms );
		write( out, prefix + "triggerbot.predictive_visualize", cfg.triggerbot.predictive_visualize );
		write( out, prefix + "triggerbot.draw_penetration_crosshair", cfg.other.penetration_crosshair );
		write( out, prefix + "triggerbot.draw_penetration_damage", cfg.other.penetration_damage );
		write( out, prefix + "triggerbot.penetration_color_yes", cfg.other.penetration_color_yes );
		write( out, prefix + "triggerbot.penetration_color_no", cfg.other.penetration_color_no );
		write( out, prefix + "triggerbot.magnet", cfg.triggerbot.magnet );
		write( out, prefix + "triggerbot.magnet_smoothing", cfg.triggerbot.magnet_smoothing );
	}

	// ESP - Player
	auto& p = settings::g_esp.m_player;
	write( out, "esp.player.enabled", p.enabled );
	write( out, "esp.player.box.enabled", p.m_box.enabled );
	write( out, "esp.player.box.style", p.m_box.style );
	write( out, "esp.player.box.fill", p.m_box.fill );
	write( out, "esp.player.box.outline", p.m_box.outline );
	write( out, "esp.player.box.corner_length", p.m_box.corner_length );
	write( out, "esp.player.box.visible_color", p.m_box.visible_color );
	write( out, "esp.player.box.occluded_color", p.m_box.occluded_color );

	write( out, "esp.player.skeleton.enabled", p.m_skeleton.enabled );
	write( out, "esp.player.skeleton.rounded", p.m_skeleton.rounded );
	write( out, "esp.player.skeleton.thickness", p.m_skeleton.thickness );
	write( out, "esp.player.skeleton.visible_color", p.m_skeleton.visible_color );
	write( out, "esp.player.skeleton.occluded_color", p.m_skeleton.occluded_color );

	write( out, "esp.player.hitboxes.enabled", p.m_hitboxes.enabled );
	write( out, "esp.player.hitboxes.mode", p.m_hitboxes.mode );
	write( out, "esp.player.hitboxes.visible_color", p.m_hitboxes.visible_color );
	write( out, "esp.player.hitboxes.occluded_color", p.m_hitboxes.occluded_color );
	write( out, "esp.player.hitboxes.outline_color", p.m_hitboxes.outline_color );
	write( out, "esp.player.hitboxes.fill", p.m_hitboxes.fill );
	write( out, "esp.player.hitboxes.outline", p.m_hitboxes.outline );
	write( out, "esp.player.hitboxes.health_indicator", p.m_hitboxes.health_indicator );

	write( out, "esp.player.health_bar.enabled", p.m_health_bar.enabled );
	write( out, "esp.player.health_bar.position", p.m_health_bar.position );
	write( out, "esp.player.health_bar.outline", p.m_health_bar.outline );
	write( out, "esp.player.health_bar.gradient", p.m_health_bar.gradient );
	write( out, "esp.player.health_bar.show_value", p.m_health_bar.show_value );
	write( out, "esp.player.health_bar.full_color", p.m_health_bar.full_color );
	write( out, "esp.player.health_bar.low_color", p.m_health_bar.low_color );

	write( out, "esp.player.ammo_bar.enabled", p.m_ammo_bar.enabled );
	write( out, "esp.player.ammo_bar.position", p.m_ammo_bar.position );
	write( out, "esp.player.ammo_bar.outline", p.m_ammo_bar.outline );
	write( out, "esp.player.ammo_bar.gradient", p.m_ammo_bar.gradient );
	write( out, "esp.player.ammo_bar.show_value", p.m_ammo_bar.show_value );
	write( out, "esp.player.ammo_bar.full_color", p.m_ammo_bar.full_color );
	write( out, "esp.player.ammo_bar.low_color", p.m_ammo_bar.low_color );

	write( out, "esp.player.info_flags.enabled", p.m_info_flags.enabled );
	write( out, "esp.player.info_flags.flags", (int)p.m_info_flags.flags );

	write( out, "esp.player.name.enabled", p.m_name.enabled );
	write( out, "esp.player.name.color", p.m_name.color );

	write( out, "esp.player.weapon.enabled", p.m_weapon.enabled );
	write( out, "esp.player.weapon.display", p.m_weapon.display );
	write( out, "esp.player.weapon.text_color", p.m_weapon.text_color );
	write( out, "esp.player.weapon.icon_color", p.m_weapon.icon_color );

	write( out, "esp.player.trails.enabled", p.m_trails.enabled );
	write( out, "esp.player.trails.local", p.m_trails.local );
	write( out, "esp.player.trails.enemy", p.m_trails.enemy );
	write( out, "esp.player.trails.team", p.m_trails.team );
	write( out, "esp.player.trails.thickness", p.m_trails.thickness );
	write( out, "esp.player.trails.max_points", p.m_trails.max_points );
	write( out, "esp.player.trails.local_color", p.m_trails.local_color );
	write( out, "esp.player.trails.enemy_color", p.m_trails.enemy_color );
	write( out, "esp.player.trails.team_color", p.m_trails.team_color );

	write( out, "esp.player.footsteps.enabled", p.m_footsteps.enabled );
	write( out, "esp.player.footsteps.show_teammates", p.m_footsteps.show_teammates );
	write( out, "esp.player.footsteps.footstep_max_radius", p.m_footsteps.footstep_max_radius );
	write( out, "esp.player.footsteps.footstep_color", p.m_footsteps.footstep_color );
	write( out, "esp.player.footsteps.jump_max_radius", p.m_footsteps.jump_max_radius );
	write( out, "esp.player.footsteps.jump_color", p.m_footsteps.jump_color );
	write( out, "esp.player.footsteps.land_max_radius", p.m_footsteps.land_max_radius );
	write( out, "esp.player.footsteps.land_color", p.m_footsteps.land_color );
	write( out, "esp.player.footsteps.expand_duration", p.m_footsteps.expand_duration );
	write( out, "esp.player.footsteps.fade_duration", p.m_footsteps.fade_duration );
	write( out, "esp.player.footsteps.segments", p.m_footsteps.segments );
	write( out, "esp.player.footsteps.thickness", p.m_footsteps.thickness );

	// ESP - Item
	auto& it = settings::g_esp.m_item;
	write( out, "esp.item.enabled", it.enabled );
	write( out, "esp.item.max_distance", it.max_distance );
	write( out, "esp.item.icon.enabled", it.m_icon.enabled );
	write( out, "esp.item.icon.color", it.m_icon.color );
	write( out, "esp.item.name.enabled", it.m_name.enabled );
	write( out, "esp.item.name.color", it.m_name.color );
	write( out, "esp.item.ammo.enabled", it.m_ammo.enabled );
	write( out, "esp.item.ammo.color", it.m_ammo.color );
	write( out, "esp.item.ammo.empty_color", it.m_ammo.empty_color );

	write( out, "esp.item.filters.rifles", it.m_filters.rifles );
	write( out, "esp.item.filters.smgs", it.m_filters.smgs );
	write( out, "esp.item.filters.shotguns", it.m_filters.shotguns );
	write( out, "esp.item.filters.snipers", it.m_filters.snipers );
	write( out, "esp.item.filters.pistols", it.m_filters.pistols );
	write( out, "esp.item.filters.heavy", it.m_filters.heavy );
	write( out, "esp.item.filters.grenades", it.m_filters.grenades );
	write( out, "esp.item.filters.utility", it.m_filters.utility );

	// ESP - Projectile
	auto& pr = settings::g_esp.m_projectile;
	write( out, "esp.projectile.enabled", pr.enabled );
	write( out, "esp.projectile.show_icon", pr.show_icon );
	write( out, "esp.projectile.show_name", pr.show_name );
	write( out, "esp.projectile.show_timer_bar", pr.show_timer_bar );
	write( out, "esp.projectile.show_inferno_bounds", pr.show_inferno_bounds );
	write( out, "esp.projectile.show_smoke_voxels", pr.show_smoke_voxels );
	write( out, "esp.projectile.color_he", pr.color_he );
	write( out, "esp.projectile.color_flash", pr.color_flash );
	write( out, "esp.projectile.color_smoke", pr.color_smoke );
	write( out, "esp.projectile.color_molotov", pr.color_molotov );
	write( out, "esp.projectile.color_decoy", pr.color_decoy );

	auto& bt = settings::g_esp.m_bullet_tracers;
	write( out, "esp.bullet_tracers.enabled", bt.enabled );
	write( out, "esp.bullet_tracers.thickness", bt.thickness );
	write( out, "esp.bullet_tracers.duration", (int)bt.duration );
	write( out, "esp.bullet_tracers.color", bt.color );

	// Misc
	auto& m = settings::g_misc.m_grenades;
	write( out, "misc.grenades.enabled", m.enabled );
	write( out, "misc.grenades.line_color", m.line_color );
	write( out, "misc.grenades.line_thickness", m.line_thickness );
	write( out, "misc.grenades.line_gradient", m.line_gradient );
	write( out, "misc.grenades.show_bounces", m.show_bounces );
	write( out, "misc.grenades.bounce_color", m.bounce_color );
	write( out, "misc.grenades.bounce_size", m.bounce_size );
	write( out, "misc.grenades.detonate_color", m.detonate_color );
	write( out, "misc.grenades.detonate_size", m.detonate_size );
	write( out, "misc.grenades.per_type_colors", m.per_type_colors );
	write( out, "misc.grenades.color_he", m.color_he );
	write( out, "misc.grenades.color_flash", m.color_flash );
	write( out, "misc.grenades.color_smoke", m.color_smoke );
	write( out, "misc.grenades.color_molotov", m.color_molotov );
	write( out, "misc.grenades.color_decoy", m.color_decoy );
	write( out, "misc.grenades.local_only", m.local_only );
	write( out, "misc.grenades.fade_duration", m.fade_duration );

	auto& nh = settings::g_misc.m_nade_helper;
	write( out, "misc.nade_helper.enabled", nh.enabled );
	write( out, "misc.nade_helper.show_name", nh.show_name );
	write( out, "misc.nade_helper.show_type", nh.show_type );
	write( out, "misc.nade_helper.stand_pos_color", nh.stand_pos_color );
	write( out, "misc.nade_helper.aim_pos_color", nh.aim_pos_color );
	write( out, "misc.nade_helper.text_color", nh.text_color );
	write( out, "misc.nade_helper.stand_radius", nh.stand_radius );
	write( out, "misc.nade_helper.aim_dot_size", nh.aim_dot_size );

	auto& mm = settings::g_misc.m_main;
	write( out, "misc.main.spectator_list", mm.spectator_list );
	write( out, "misc.main.spectator_list_color", mm.spectator_list_color );
	write( out, "misc.main.bomb_timer", mm.bomb_timer );
	write( out, "misc.main.bomb_timer_color", mm.bomb_timer_color );
	write( out, "misc.main.hitsound", mm.hitsound );
	auto& fc = settings::g_misc.m_fov_changer;
	write( out, "misc.fov_changer.enabled", fc.enabled );
	write( out, "misc.fov_changer.fov", fc.fov );
	write( out, "misc.fov_changer.disable_when_scoped", fc.disable_when_scoped );

	// Movement
	auto& mv = settings::g_movement;
	write( out, "movement.bhop.enabled", mv.bhop.enabled );
	write( out, "movement.quickstop.enabled", mv.quickstop.enabled );
	write( out, "movement.quickstop.strength", mv.quickstop.strength );

	// Skinchanger
	{
		std::shared_lock lock( settings::g_skinchanger.mutex );
		write( out, "skinchanger.enabled", settings::g_skinchanger.enabled );
		write( out, "skinchanger.music_kit", settings::g_skinchanger.music_kit );
		for ( const auto& [idx, cfg] : settings::g_skinchanger.weapon_skins )
		{
			std::string p = std::format( "skinchanger.weapon_{}.", idx );
			write( out, p + "paint_kit", cfg.paint_kit );
			write( out, p + "seed", cfg.seed );
			write( out, p + "stat_trak", cfg.stat_trak );
			write( out, p + "wear", cfg.wear );
			write( out, p + "uses_old_model", cfg.uses_old_model );
		}
	}

	out.close( );
	this->refresh_list( );
}

	static int parse_int( const std::string& s ) { try { return std::stoi( s ); } catch ( ... ) { return 0; } }
	static float parse_float( const std::string& s ) { try { return std::stof( s ); } catch ( ... ) { return 0.0f; } }
	static bool parse_bool( const std::string& s ) { return s == "1"; }
	static zdraw::rgba parse_color( const std::string& s )
	{
		std::stringstream ss( s );
		std::string r, g, b, a;
		std::getline( ss, r, ',' );
		std::getline( ss, g, ',' );
		std::getline( ss, b, ',' );
		std::getline( ss, a, ',' );
		return zdraw::rgba{ ( std::uint8_t )parse_int( r ), ( std::uint8_t )parse_int( g ), ( std::uint8_t )parse_int( b ), ( std::uint8_t )parse_int( a ) };
	}

	// Config loads are seperate functions because all the nested else ifs
	// would cause the compiler to truncate the function

	void config_system::load( std::string_view name )
	{
		if ( name.empty( ) ) return;

		fs::path file = this->m_path / ( std::string( name ) + ".cfg" );
		if ( !fs::exists( file ) ) return;

		std::ifstream in( file );
		if ( !in.is_open( ) ) return;

		std::string line;
		while ( std::getline( in, line ) )
		{
			size_t pos = line.find( '=' );
			if ( pos == std::string::npos ) continue;

			std::string key = line.substr( 0, pos );
			std::string val = line.substr( pos + 1 );

			if ( key.starts_with( "combat." ) ) this->load_combat( key, val );
			else if ( key.starts_with( "esp.player." ) ) this->load_esp_player( key, val );
			else if ( key.starts_with( "esp.item." ) ) this->load_esp_item( key, val );
			else if ( key.starts_with( "esp.projectile." ) ) this->load_esp_projectile( key, val );
			else if ( key.starts_with( "esp.bullet_tracers." ) )
			{
				auto& bt = settings::g_esp.m_bullet_tracers;
				if ( key == "esp.bullet_tracers.enabled" ) bt.enabled = parse_bool( val );
				else if ( key == "esp.bullet_tracers.thickness" ) bt.thickness = parse_float( val );
				else if ( key == "esp.bullet_tracers.duration" ) bt.duration = parse_float( val );
				else if ( key == "esp.bullet_tracers.color" ) bt.color = parse_color( val );
			}
			else if ( key.starts_with( "misc." ) ) this->load_misc( key, val );
			else if ( key.starts_with( "movement." ) ) this->load_movement( key, val );
			else if ( key.starts_with( "skinchanger." ) ) this->load_skinchanger( key, val );
		}

		in.close( );
	}

	void config_system::load_combat( const std::string& key, const std::string& val )
	{
		if ( key.starts_with( "combat.group_" ) )
		{
			int group_idx = std::stoi( key.substr( 13, 1 ) );
			if ( group_idx >= 0 && group_idx < settings::combat::k_group_count )
			{
				auto& cfg = settings::g_combat.groups[ group_idx ];
				std::string subkey = key.substr( 15 );

				if ( subkey == "aimbot.enabled" ) cfg.aimbot.enabled = parse_bool( val );
				else if ( subkey == "aimbot.key" ) cfg.aimbot.key = parse_int( val );
				else if ( subkey == "aimbot.type" ) cfg.aimbot.type = parse_int( val );
				else if ( subkey == "aimbot.fov" ) cfg.aimbot.fov = parse_int( val );
				else if ( subkey == "aimbot.smoothing" ) cfg.aimbot.smoothing = parse_int( val );
				else if ( subkey == "aimbot.autowall" ) cfg.aimbot.autowall = parse_bool( val );
				else if ( subkey == "aimbot.min_damage" ) cfg.aimbot.min_damage = parse_float( val );
				else if ( subkey == "aimbot.visible_only" ) cfg.aimbot.visible_only = parse_bool( val );
				else if ( subkey == "aimbot.smoke_check" ) cfg.aimbot.smoke_check = parse_bool( val );
				else if ( subkey == "aimbot.draw_fov" ) cfg.aimbot.draw_fov = parse_bool( val );
				else if ( subkey == "aimbot.fov_color" ) cfg.aimbot.fov_color = parse_color( val );
				else if ( subkey == "aimbot.autowall_info" ) cfg.aimbot.autowall_info = parse_bool( val );
				else if ( subkey == "aimbot.autowall_info_color" ) cfg.aimbot.autowall_info_color = parse_color( val );
				else if ( subkey == "aimbot.predictive" ) cfg.aimbot.predictive = parse_bool( val );
				else if ( subkey == "aimbot.rcs" ) cfg.aimbot.rcs = parse_bool( val );
				else if ( subkey == "aimbot.silent" ) cfg.aimbot.silent = parse_bool( val );
				else if ( subkey == "aimbot.rcs_factor" ) cfg.aimbot.rcs_factor = parse_float( val );
				else if ( subkey == "aimbot.hitgroups.head" ) cfg.aimbot.hitgroups.head = parse_bool( val );
				else if ( subkey == "aimbot.hitgroups.chest" ) cfg.aimbot.hitgroups.chest = parse_bool( val );
				else if ( subkey == "aimbot.hitgroups.stomach" ) cfg.aimbot.hitgroups.stomach = parse_bool( val );
				else if ( subkey == "aimbot.hitgroups.arms" ) cfg.aimbot.hitgroups.arms = parse_bool( val );
				else if ( subkey == "aimbot.hitgroups.legs" ) cfg.aimbot.hitgroups.legs = parse_bool( val );
				else if ( subkey == "aimbot.multipoint" ) cfg.aimbot.multipoint = parse_bool( val );
				else if ( subkey == "aimbot.multipoint_scale" ) cfg.aimbot.multipoint_scale = parse_float( val );
				else if ( subkey == "triggerbot.enabled" ) cfg.triggerbot.enabled = parse_bool( val );
				else if ( subkey == "triggerbot.key" ) cfg.triggerbot.key = parse_int( val );
				else if ( subkey == "triggerbot.seed_triggerbot" ) cfg.triggerbot.seed_triggerbot = parse_bool( val );
				else if ( subkey == "triggerbot.show_spread" ) cfg.triggerbot.show_spread = parse_bool( val );
				else if ( subkey == "triggerbot.hitchance" ) cfg.triggerbot.hitchance = parse_float( val );
				else if ( subkey == "triggerbot.delay" ) cfg.triggerbot.delay = parse_int( val );
				else if ( subkey == "triggerbot.hitgroups.head" ) cfg.triggerbot.hitgroups.head = parse_bool( val );
				else if ( subkey == "triggerbot.hitgroups.chest" ) cfg.triggerbot.hitgroups.chest = parse_bool( val );
				else if ( subkey == "triggerbot.hitgroups.stomach" ) cfg.triggerbot.hitgroups.stomach = parse_bool( val );
				else if ( subkey == "triggerbot.hitgroups.arms" ) cfg.triggerbot.hitgroups.arms = parse_bool( val );
				else if ( subkey == "triggerbot.hitgroups.legs" ) cfg.triggerbot.hitgroups.legs = parse_bool( val );
				else if ( subkey == "triggerbot.autowall" ) cfg.triggerbot.autowall = parse_bool( val );
				else if ( subkey == "triggerbot.min_damage" ) cfg.triggerbot.min_damage = parse_float( val );
				else if ( subkey == "triggerbot.autostop" ) cfg.triggerbot.autostop = parse_bool( val );
				else if ( subkey == "triggerbot.early_autostop" ) cfg.triggerbot.early_autostop = parse_bool( val );
				else if ( subkey == "triggerbot.predictive" ) cfg.triggerbot.predictive = parse_bool( val );
				else if ( subkey == "triggerbot.predictive_ms" ) cfg.triggerbot.predictive_ms = parse_int( val );
				else if ( subkey == "triggerbot.predictive_visualize" ) cfg.triggerbot.predictive_visualize = parse_bool( val );
				else if ( subkey == "triggerbot.draw_penetration_crosshair" ) cfg.other.penetration_crosshair = parse_bool( val );
				else if ( subkey == "triggerbot.draw_penetration_damage" ) cfg.other.penetration_damage = parse_bool( val );
				else if ( subkey == "triggerbot.penetration_color_yes" ) cfg.other.penetration_color_yes = parse_color( val );
				else if ( subkey == "triggerbot.penetration_color_no" ) cfg.other.penetration_color_no = parse_color( val );
				else if ( subkey == "triggerbot.magnet" ) cfg.triggerbot.magnet = parse_bool( val );
				else if ( subkey == "triggerbot.magnet_smoothing" ) cfg.triggerbot.magnet_smoothing = parse_float( val );
			}
		}
	}

	void config_system::load_esp_player( const std::string& key, const std::string& val )
	{
		auto& p = settings::g_esp.m_player;
		if ( key == "esp.player.enabled" ) p.enabled = parse_bool( val );
		else if ( key == "esp.player.box.enabled" ) p.m_box.enabled = parse_bool( val );
		else if ( key == "esp.player.box.style" ) p.m_box.style = parse_int( val );
		else if ( key == "esp.player.box.fill" ) p.m_box.fill = parse_bool( val );
		else if ( key == "esp.player.box.outline" ) p.m_box.outline = parse_bool( val );
		else if ( key == "esp.player.box.corner_length" ) p.m_box.corner_length = parse_float( val );
		else if ( key == "esp.player.box.visible_color" ) p.m_box.visible_color = parse_color( val );
		else if ( key == "esp.player.box.occluded_color" ) p.m_box.occluded_color = parse_color( val );
		else if ( key == "esp.player.skeleton.enabled" ) p.m_skeleton.enabled = parse_bool( val );
		else if ( key == "esp.player.skeleton.rounded" ) p.m_skeleton.rounded = parse_bool( val );
		else if ( key == "esp.player.skeleton.thickness" ) p.m_skeleton.thickness = parse_float( val );
		else if ( key == "esp.player.skeleton.visible_color" ) p.m_skeleton.visible_color = parse_color( val );
		else if ( key == "esp.player.skeleton.occluded_color" ) p.m_skeleton.occluded_color = parse_color( val );
		else if ( key == "esp.player.hitboxes.enabled" ) p.m_hitboxes.enabled = parse_bool( val );
		else if ( key == "esp.player.hitboxes.mode" ) p.m_hitboxes.mode = (settings::esp::player::hitboxes::material)parse_int( val );
		else if ( key == "esp.player.hitboxes.visible_color" ) p.m_hitboxes.visible_color = parse_color( val );
		else if ( key == "esp.player.hitboxes.occluded_color" ) p.m_hitboxes.occluded_color = parse_color( val );
		else if ( key == "esp.player.hitboxes.outline_color" ) p.m_hitboxes.outline_color = parse_color( val );
		else if ( key == "esp.player.hitboxes.fill" ) p.m_hitboxes.fill = parse_bool( val );
		else if ( key == "esp.player.hitboxes.outline" ) p.m_hitboxes.outline = parse_bool( val );
		else if ( key == "esp.player.hitboxes.health_indicator" ) p.m_hitboxes.health_indicator = parse_bool( val );
		else if ( key == "esp.player.health_bar.enabled" ) p.m_health_bar.enabled = parse_bool( val );
		else if ( key == "esp.player.health_bar.position" ) p.m_health_bar.position = parse_int( val );
		else if ( key == "esp.player.health_bar.outline" ) p.m_health_bar.outline = parse_bool( val );
		else if ( key == "esp.player.health_bar.gradient" ) p.m_health_bar.gradient = parse_bool( val );
		else if ( key == "esp.player.health_bar.show_value" ) p.m_health_bar.show_value = parse_bool( val );
		else if ( key == "esp.player.health_bar.full_color" ) p.m_health_bar.full_color = parse_color( val );
		else if ( key == "esp.player.health_bar.low_color" ) p.m_health_bar.low_color = parse_color( val );
		else if ( key == "esp.player.ammo_bar.enabled" ) p.m_ammo_bar.enabled = parse_bool( val );
		else if ( key == "esp.player.ammo_bar.position" ) p.m_ammo_bar.position = parse_int( val );
		else if ( key == "esp.player.ammo_bar.outline" ) p.m_ammo_bar.outline = parse_bool( val );
		else if ( key == "esp.player.ammo_bar.gradient" ) p.m_ammo_bar.gradient = parse_bool( val );
		else if ( key == "esp.player.ammo_bar.show_value" ) p.m_ammo_bar.show_value = parse_bool( val );
		else if ( key == "esp.player.ammo_bar.full_color" ) p.m_ammo_bar.full_color = parse_color( val );
		else if ( key == "esp.player.ammo_bar.low_color" ) p.m_ammo_bar.low_color = parse_color( val );
		else if ( key == "esp.player.info_flags.enabled" ) p.m_info_flags.enabled = parse_bool( val );
		else if ( key == "esp.player.info_flags.flags" ) p.m_info_flags.flags = ( std::uint8_t )parse_int( val );
		else if ( key == "esp.player.name.enabled" ) p.m_name.enabled = parse_bool( val );
		else if ( key == "esp.player.name.color" ) p.m_name.color = parse_color( val );
		else if ( key == "esp.player.weapon.enabled" ) p.m_weapon.enabled = parse_bool( val );
		else if ( key == "esp.player.weapon.display" ) p.m_weapon.display = parse_int( val );
		else if ( key == "esp.player.weapon.text_color" ) p.m_weapon.text_color = parse_color( val );
		else if ( key == "esp.player.weapon.icon_color" ) p.m_weapon.icon_color = parse_color( val );
		else if ( key == "esp.player.trails.enabled" ) p.m_trails.enabled = parse_bool( val );
		else if ( key == "esp.player.trails.local" ) p.m_trails.local = parse_bool( val );
		else if ( key == "esp.player.trails.enemy" ) p.m_trails.enemy = parse_bool( val );
		else if ( key == "esp.player.trails.team" ) p.m_trails.team = parse_bool( val );
		else if ( key == "esp.player.trails.thickness" ) p.m_trails.thickness = parse_float( val );
		else if ( key == "esp.player.trails.max_points" ) p.m_trails.max_points = parse_int( val );
		else if ( key == "esp.player.trails.local_color" ) p.m_trails.local_color = parse_color( val );
		else if ( key == "esp.player.trails.enemy_color" ) p.m_trails.enemy_color = parse_color( val );
		else if ( key == "esp.player.trails.team_color" ) p.m_trails.team_color = parse_color( val );
		else if ( key == "esp.player.footsteps.enabled" ) p.m_footsteps.enabled = parse_bool( val );
		else if ( key == "esp.player.footsteps.show_teammates" ) p.m_footsteps.show_teammates = parse_bool( val );
		else if ( key == "esp.player.footsteps.footstep_max_radius" ) p.m_footsteps.footstep_max_radius = parse_float( val );
		else if ( key == "esp.player.footsteps.footstep_color" ) p.m_footsteps.footstep_color = parse_color( val );
		else if ( key == "esp.player.footsteps.jump_max_radius" ) p.m_footsteps.jump_max_radius = parse_float( val );
		else if ( key == "esp.player.footsteps.jump_color" ) p.m_footsteps.jump_color = parse_color( val );
		else if ( key == "esp.player.footsteps.land_max_radius" ) p.m_footsteps.land_max_radius = parse_float( val );
		else if ( key == "esp.player.footsteps.land_color" ) p.m_footsteps.land_color = parse_color( val );
		else if ( key == "esp.player.footsteps.expand_duration" ) p.m_footsteps.expand_duration = parse_float( val );
		else if ( key == "esp.player.footsteps.fade_duration" ) p.m_footsteps.fade_duration = parse_float( val );
		else if ( key == "esp.player.footsteps.segments" ) p.m_footsteps.segments = parse_int( val );
		else if ( key == "esp.player.footsteps.thickness" ) p.m_footsteps.thickness = parse_float( val );
	}

	void config_system::load_esp_item( const std::string& key, const std::string& val )
	{
		auto& it = settings::g_esp.m_item;
		if ( key == "esp.item.enabled" ) it.enabled = parse_bool( val );
		else if ( key == "esp.item.max_distance" ) it.max_distance = parse_float( val );
		else if ( key == "esp.item.icon.enabled" ) it.m_icon.enabled = parse_bool( val );
		else if ( key == "esp.item.icon.color" ) it.m_icon.color = parse_color( val );
		else if ( key == "esp.item.name.enabled" ) it.m_name.enabled = parse_bool( val );
		else if ( key == "esp.item.name.color" ) it.m_name.color = parse_color( val );
		else if ( key == "esp.item.ammo.enabled" ) it.m_ammo.enabled = parse_bool( val );
		else if ( key == "esp.item.ammo.color" ) it.m_ammo.color = parse_color( val );
		else if ( key == "esp.item.ammo.empty_color" ) it.m_ammo.empty_color = parse_color( val );
		else if ( key == "esp.item.filters.rifles" ) it.m_filters.rifles = parse_bool( val );
		else if ( key == "esp.item.filters.smgs" ) it.m_filters.smgs = parse_bool( val );
		else if ( key == "esp.item.filters.shotguns" ) it.m_filters.shotguns = parse_bool( val );
		else if ( key == "esp.item.filters.snipers" ) it.m_filters.snipers = parse_bool( val );
		else if ( key == "esp.item.filters.pistols" ) it.m_filters.pistols = parse_bool( val );
		else if ( key == "esp.item.filters.heavy" ) it.m_filters.heavy = parse_bool( val );
		else if ( key == "esp.item.filters.grenades" ) it.m_filters.grenades = parse_bool( val );
		else if ( key == "esp.item.filters.utility" ) it.m_filters.utility = parse_bool( val );
	}

	void config_system::load_esp_projectile( const std::string& key, const std::string& val )
	{
		auto& pr = settings::g_esp.m_projectile;
		if ( key == "esp.projectile.enabled" ) pr.enabled = parse_bool( val );
		else if ( key == "esp.projectile.show_icon" ) pr.show_icon = parse_bool( val );
		else if ( key == "esp.projectile.show_name" ) pr.show_name = parse_bool( val );
		else if ( key == "esp.projectile.show_timer_bar" ) pr.show_timer_bar = parse_bool( val );
		else if ( key == "esp.projectile.show_inferno_bounds" ) pr.show_inferno_bounds = parse_bool( val );
		else if ( key == "esp.projectile.show_smoke_voxels" ) pr.show_smoke_voxels = parse_bool( val );
		else if ( key == "esp.projectile.color_he" ) pr.color_he = parse_color( val );
		else if ( key == "esp.projectile.color_flash" ) pr.color_flash = parse_color( val );
		else if ( key == "esp.projectile.color_smoke" ) pr.color_smoke = parse_color( val );
		else if ( key == "esp.projectile.color_molotov" ) pr.color_molotov = parse_color( val );
		else if ( key == "esp.projectile.color_decoy" ) pr.color_decoy = parse_color( val );
	}

	void config_system::load_misc( const std::string& key, const std::string& val )
	{
		auto& m = settings::g_misc.m_grenades;
		if ( key == "misc.grenades.enabled" ) m.enabled = parse_bool( val );
		else if ( key == "misc.grenades.line_color" ) m.line_color = parse_color( val );
		else if ( key == "misc.grenades.line_thickness" ) m.line_thickness = parse_float( val );
		else if ( key == "misc.grenades.line_gradient" ) m.line_gradient = parse_bool( val );
		else if ( key == "misc.grenades.show_bounces" ) m.show_bounces = parse_bool( val );
		else if ( key == "misc.grenades.bounce_color" ) m.bounce_color = parse_color( val );
		else if ( key == "misc.grenades.bounce_size" ) m.bounce_size = parse_float( val );
		else if ( key == "misc.grenades.detonate_color" ) m.detonate_color = parse_color( val );
		else if ( key == "misc.grenades.detonate_size" ) m.detonate_size = parse_float( val );
		else if ( key == "misc.grenades.per_type_colors" ) m.per_type_colors = parse_bool( val );
		else if ( key == "misc.grenades.color_he" ) m.color_he = parse_color( val );
		else if ( key == "misc.grenades.color_flash" ) m.color_flash = parse_color( val );
		else if ( key == "misc.grenades.color_smoke" ) m.color_smoke = parse_color( val );
		else if ( key == "misc.grenades.color_molotov" ) m.color_molotov = parse_color( val );
		else if ( key == "misc.grenades.color_decoy" ) m.color_decoy = parse_color( val );
		else if ( key == "misc.grenades.local_only" ) m.local_only = parse_bool( val );
		else if ( key == "misc.grenades.fade_duration" ) m.fade_duration = parse_float( val );

		auto& nh = settings::g_misc.m_nade_helper;
		if ( key == "misc.nade_helper.enabled" ) nh.enabled = parse_bool( val );
		else if ( key == "misc.nade_helper.show_name" ) nh.show_name = parse_bool( val );
		else if ( key == "misc.nade_helper.show_type" ) nh.show_type = parse_bool( val );
		else if ( key == "misc.nade_helper.stand_pos_color" ) nh.stand_pos_color = parse_color( val );
		else if ( key == "misc.nade_helper.aim_pos_color" ) nh.aim_pos_color = parse_color( val );
		else if ( key == "misc.nade_helper.text_color" ) nh.text_color = parse_color( val );
		else if ( key == "misc.nade_helper.stand_radius" ) nh.stand_radius = parse_float( val );
		else if ( key == "misc.nade_helper.aim_dot_size" ) nh.aim_dot_size = parse_float( val );

		auto& mm = settings::g_misc.m_main;
		if ( key == "misc.main.spectator_list" ) mm.spectator_list = parse_bool( val );
		else if ( key == "misc.main.spectator_list_color" ) mm.spectator_list_color = parse_color( val );
		else if ( key == "misc.main.bomb_timer" ) mm.bomb_timer = parse_bool( val );
		else if ( key == "misc.main.bomb_timer_color" ) mm.bomb_timer_color = parse_color( val );
		else if ( key == "misc.main.hitsound" ) mm.hitsound = parse_int( val );

		auto& fc = settings::g_misc.m_fov_changer;
		if ( key == "misc.fov_changer.enabled" ) fc.enabled = parse_bool( val );
		else if ( key == "misc.fov_changer.fov" ) fc.fov = parse_int( val );
		else if ( key == "misc.fov_changer.disable_when_scoped" ) fc.disable_when_scoped = parse_bool( val );
	}

	void config_system::load_movement( const std::string& key, const std::string& val )
	{
		auto& mv = settings::g_movement;
		if ( key == "movement.bhop.enabled" ) mv.bhop.enabled = parse_bool( val );
		else if ( key == "movement.quickstop.enabled" ) mv.quickstop.enabled = parse_bool( val );
		else if ( key == "movement.quickstop.strength" ) mv.quickstop.strength = parse_float( val );
	}

	void config_system::load_skinchanger( const std::string& key, const std::string& val )
	{
		if ( key == "skinchanger.enabled" )
		{
			settings::g_skinchanger.enabled = parse_bool( val );
			return;
		}
		if ( key == "skinchanger.music_kit" )
		{
			settings::g_skinchanger.music_kit = parse_int( val );
			return;
		}

		if ( key.starts_with( "skinchanger.weapon_" ) )
		{
			const auto dot = key.find( '.', 19 );
			if ( dot == std::string::npos ) return;

			const int idx = std::stoi( key.substr( 19, dot - 19 ) );
			const std::string subkey = key.substr( dot + 1 );

			std::unique_lock lock( settings::g_skinchanger.mutex );
			auto& cfg = settings::g_skinchanger.weapon_skins[ idx ];

			if ( subkey == "paint_kit" ) cfg.paint_kit = parse_int( val );
			else if ( subkey == "seed" ) cfg.seed = parse_int( val );
			else if ( subkey == "stat_trak" ) cfg.stat_trak = parse_int( val );
			else if ( subkey == "wear" ) cfg.wear = parse_float( val );
			else if ( subkey == "uses_old_model" ) cfg.uses_old_model = parse_bool( val );
		}
	}

	void config_system::remove( std::string_view name )
	{
		if ( name.empty( ) ) return;

		fs::path file = this->m_path / ( std::string( name ) + ".cfg" );
		if ( fs::exists( file ) )
		{
			fs::remove( file );
		}
		this->refresh_list( );
	}
