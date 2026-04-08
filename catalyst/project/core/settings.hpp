#pragma once
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <array>
#include <string>
#include <vector>
#include <external/zdraw/zdraw.hpp>

namespace settings {

	struct combat
	{
		struct aimbot_settings
		{
			bool enabled{ true };
			int key{ VK_XBUTTON2 };
			int type{ 0 };

			int fov{ 5 };
			int smoothing{ 5 };



			struct hitgroups
			{
				bool head{ true };
				bool chest{ true };
				bool stomach{ true };
				bool arms{ false };
				bool legs{ false };
			} hitgroups{};

			bool multipoint{ false };
			float multipoint_scale{ 0.7f };

			bool autowall{ true };
			float min_damage{ 90.0f };

			bool visible_only{ true };
			bool smoke_check{ true };

			bool draw_fov{ true };
			zdraw::rgba fov_color{ 225, 225, 225, 125 };

			bool autowall_info{ true };
			zdraw::rgba autowall_info_color{ 140, 150, 235, 255 };

			bool predictive{ true };
			bool silent{ false };
			bool rcs{ true };
			float rcs_factor{ 0.65f };
		} aimbot{};

		struct triggerbot_settings
		{
			bool enabled{ true };
			int key{ VK_XBUTTON2 };

			float hitchance{ 75.0f };
			int delay{ 10 };

			struct hitgroups
			{
				bool head{ true };
				bool chest{ true };
				bool stomach{ true };
				bool arms{ true };
				bool legs{ true };
			} hitgroups{};

			bool autowall{ true };
			float min_damage{ 90.0f };

			bool autostop{ false };
			bool early_autostop{ false };

			bool seed_triggerbot{ false };
			bool show_spread{ false };

			bool predictive{ true };
			bool predictive_visualize{ false };
			int predictive_ms{ 50 };
			
			bool magnet{ false };
			int magnet_smoothing{ 5 };
		} triggerbot{};

		struct other_settings
		{
			bool penetration_crosshair{ true };
			bool penetration_damage{ true };

			zdraw::rgba penetration_color_yes{ 50, 255, 50, 125 };
			zdraw::rgba penetration_color_no{ 255, 50, 50, 125 };
		} other{};

		struct group_config
		{
			aimbot_settings aimbot{};
			triggerbot_settings triggerbot{};
			other_settings other{};
		};

		static constexpr std::uint32_t k_group_count{ 6 };

		std::array<group_config, k_group_count> groups{};

		group_config& get( std::uint32_t weapon_type )
		{
			const auto idx = weapon_type - 1; // Assuming 1-based indexing for weapon types (pistol=1)
			return this->groups[ idx < k_group_count ? idx : 2 ];
		}
	};

	struct esp
	{
		struct player
		{
			bool enabled{ true };

			struct box
			{
				enum class style0 : std::uint8_t { full, cornered };

				bool enabled{ true };
				int style{ (int)style0::cornered };
				bool fill{ true };
				bool outline{ true };
				float corner_length{ 10.0f };

				zdraw::rgba visible_color{ 140, 150, 235, 255 };
				zdraw::rgba occluded_color{ 110, 115, 170, 180 };
			} m_box{};

			struct skeleton
			{
				bool enabled{ true };
				bool rounded{ false };
				float thickness{ 1.0f };

				zdraw::rgba visible_color{ 170, 175, 220, 255 };
				zdraw::rgba occluded_color{ 130, 135, 180, 180 };
			} m_skeleton{};

			struct hitboxes
			{
				enum class material : int { flat, glow, pulse, wireframe };

				bool enabled{ false };
				material mode{ material::flat };

				zdraw::rgba visible_color{ 150, 160, 240, 160 };
				zdraw::rgba occluded_color{ 115, 120, 185, 120 };

				zdraw::rgba outline_color{ 255, 255, 255, 255 };

				bool fill{ true };
				bool outline{ true };
				bool health_indicator{ false };
			} m_hitboxes{};

			struct health_bar
			{
			public:
				enum class position : std::uint8_t { left, top, bottom };

				bool enabled{ true };
				int position{ (int)position::left };
				bool outline{ true };
				bool gradient{ true };
				bool show_value{ true };

				zdraw::rgba full_color{ 140, 150, 235, 255 };
				zdraw::rgba low_color{ 75, 80, 180, 255 };
				zdraw::rgba background_color{ 15, 16, 22, 150 };
				zdraw::rgba outline_color{ 15, 16, 22, 255 };
				zdraw::rgba text_color{ 195, 200, 215, 255 };
			} m_health_bar{};

			struct ammo_bar
			{
				enum class position : std::uint8_t { left, top, bottom };

				bool enabled{ true };
				int position{ (int)position::bottom };
				bool outline{ true };
				bool gradient{ true };
				bool show_value{ false };

				zdraw::rgba full_color{ 140, 150, 235, 255 };
				zdraw::rgba low_color{ 75, 80, 180, 255 };
				zdraw::rgba background_color{ 15, 16, 22, 150 };
				zdraw::rgba outline_color{ 15, 16, 22, 255 };
				zdraw::rgba text_color{ 195, 200, 215, 255 };
			} m_ammo_bar{};

			struct info_flags
			{
				enum flag : std::uint8_t
				{
					none = 0,
					money = 1 << 0,
					armor = 1 << 1,
					kit = 1 << 2,
					scoped = 1 << 3,
					defusing = 1 << 4,
					flashed = 1 << 5,
					ping = 1 << 6,
					distance = 1 << 7
				};

				bool enabled{ true };
				std::uint8_t flags{ flag::money | flag::armor | flag::kit | flag::scoped | flag::defusing | flag::flashed | flag::ping };

				zdraw::rgba money_color{ 120, 230, 160, 255 };
				zdraw::rgba armor_color{ 195, 200, 215, 255 };
				zdraw::rgba kit_color{ 140, 150, 235, 255 };
				zdraw::rgba scoped_color{ 195, 200, 215, 255 };
				zdraw::rgba defusing_color{ 140, 150, 235, 255 };
				zdraw::rgba flashed_color{ 255, 210, 120, 255 };
				zdraw::rgba distance_color{ 90, 95, 130, 255 };

				[[nodiscard]] bool has( flag f ) const { return this->flags & f; }
			} m_info_flags{};

			struct name
			{
				bool enabled{ true };
				zdraw::rgba color{ 195, 200, 215, 230 };
			} m_name{};

			struct weapon
			{
				enum class display_type : std::uint8_t { text, icon, text_and_icon };

				bool enabled{ true };
				int display{ (int)display_type::icon };

				zdraw::rgba text_color{ 195, 200, 215, 210 };
				zdraw::rgba icon_color{ 195, 200, 215, 230 };
			} m_weapon{};

			struct movement_trails
			{
				bool enabled{ false };
				bool local{ true };
				bool enemy{ true };
				bool team{ false };
				float thickness{ 2.0f };
				float lifetime{ 1.5f };
				int max_points{ 500 };

				zdraw::rgba local_color{ 140, 150, 235, 255 };
				zdraw::rgba enemy_color{ 230, 140, 140, 255 };
				zdraw::rgba team_color{ 140, 235, 150, 255 };
			} m_trails{};

			struct oof_arrow
			{
				bool enabled{ true };
				float radius{ 120.0f };
				float size{ 16.0f };
				bool outline{ true };
				zdraw::rgba color{ 140, 150, 235, 255 };
			} m_oof_arrow{};

			struct footsteps
			{
				bool enabled{ false };
				bool show_teammates{ false };

				float footstep_max_radius{ 20.0f };
				zdraw::rgba footstep_color{ 140, 150, 235, 255 };

				float jump_max_radius{ 35.0f };
				zdraw::rgba jump_color{ 235, 140, 140, 255 };

				float land_max_radius{ 40.0f };
				zdraw::rgba land_color{ 140, 235, 150, 255 };

				float expand_duration{ 0.5f };
				float fade_duration{ 1.0f };
				int segments{ 24 };
				float thickness{ 2.0f };
			} m_footsteps{};

		} m_player{};

		struct item
		{
			bool enabled{ true };
			float max_distance{ 40.0f };

			struct icon
			{
				bool enabled{ true };
				zdraw::rgba color{ 195, 200, 215, 200 };
			} m_icon{};

			struct name
			{
				bool enabled{ false };
				zdraw::rgba color{ 195, 200, 215, 180 };
			} m_name{};

			struct ammo
			{
				bool enabled{ true };
				zdraw::rgba color{ 140, 150, 235, 200 };
				zdraw::rgba empty_color{ 180, 80, 80, 200 };
			} m_ammo{};

			struct filters
			{
				bool rifles{ true };
				bool smgs{ true };
				bool shotguns{ true };
				bool snipers{ true };
				bool pistols{ true };
				bool heavy{ true };
				bool grenades{ true };
				bool utility{ true };
			} m_filters{};
		} m_item{};

		struct projectile
		{
			bool enabled{ true };

			bool show_icon{ true };
			bool show_name{ true };
			bool show_timer_bar{ true };
			bool show_inferno_bounds{ true };
			bool show_smoke_voxels{ true };

			zdraw::rgba default_color{ 195, 200, 215, 200 };
			zdraw::rgba color_he{ 220, 150, 150, 220 };
			zdraw::rgba color_flash{ 230, 220, 150, 220 };
			zdraw::rgba color_smoke{ 160, 200, 180, 220 };
			zdraw::rgba color_molotov{ 220, 170, 130, 220 };
			zdraw::rgba color_decoy{ 170, 175, 200, 200 };

			zdraw::rgba timer_high_color{ 140, 150, 235, 255 };
			zdraw::rgba timer_low_color{ 220, 100, 100, 255 };
			zdraw::rgba bar_background{ 15, 16, 22, 150 };
		} m_projectile{};

		struct bullet_tracers
		{
			bool enabled{ false };
			float thickness{ 2.0f };
			int duration{ 2000 };
			zdraw::rgba color{ 140, 150, 235, 255 };
		} m_bullet_tracers{};

		struct hit_marker
		{
			bool enabled{ true };
			int size{ 8 };
			int gap{ 4 };
			int duration{ 500 }; // ms
			zdraw::rgba color{ 255, 255, 255, 255 };
		} m_hit_marker{};

		struct damage_indicator
		{
			bool enabled{ true };
			int duration{ 1500 }; // ms
			float floating_speed{ 40.0f };
			zdraw::rgba color{ 220, 220, 220, 255 };
			zdraw::rgba crit_color{ 255, 100, 100, 255 };
		} m_damage_indicator{};
	};

	struct misc
	{
		struct grenades
		{
			bool enabled{ true };

			zdraw::rgba line_color{ 170, 175, 220, 200 };
			float line_thickness{ 2.0f };
			bool line_gradient{ true };

			bool show_bounces{ true };
			zdraw::rgba bounce_color{ 195, 200, 215, 255 };
			float bounce_size{ 2.0f };

			zdraw::rgba detonate_color{ 140, 150, 235, 255 };
			float detonate_size{ 4.0f };

			bool per_type_colors{ false };
			zdraw::rgba color_he{ 190, 140, 140, 200 };
			zdraw::rgba color_flash{ 200, 195, 150, 200 };
			zdraw::rgba color_smoke{ 150, 185, 165, 200 };
			zdraw::rgba color_molotov{ 195, 155, 130, 200 };
			zdraw::rgba color_decoy{ 160, 165, 185, 200 };

			bool local_only{ true };
			float fade_duration{ 0.3f };
		} m_grenades{};

		struct nade_helper
		{
			bool enabled{ true };
			bool show_name{ true };
			bool show_type{ true };
			
			zdraw::rgba stand_pos_color{ 140, 150, 235, 180 };
			zdraw::rgba aim_pos_color{ 235, 140, 140, 255 };
			zdraw::rgba text_color{ 255, 255, 255, 255 };
			
			float stand_radius{ 22.0f };
			float aim_dot_size{ 3.5f };
		} m_nade_helper{};

		struct main
		{
			bool spectator_list{ true };
			zdraw::rgba spectator_list_color{ 140, 150, 235, 255 };

			bool bomb_timer{ true };
			zdraw::rgba bomb_timer_color{ 140, 150, 235, 255 };

			bool watermark{ true };

			int hitsound{ 0 };
		} m_main{};

		struct fov_changer
		{
			bool enabled{ false };
			bool disable_when_scoped{ false };
			int fov{ 90 };
			int viewmodel_fov{ 68 };
		} m_fov_changer{};
	};

	struct skinchanger
	{
		enum weapons_enum : std::uint16_t
		{
			none = 0,
			deagle = 1, elite = 2, fiveseven = 3, glock = 4, ak47 = 7, aug = 8, awp = 9, famas = 10, g3sg1 = 11, m249 = 14, mac10 = 17, p90 = 19, ump45 = 24, xm1014 = 25, bizon = 26, mag7 = 27, negev = 28, sawedoff = 29, tec9 = 30, zeus = 31, p2000 = 32, mp7 = 33, mp9 = 34, nova = 35, p250 = 36, scar20 = 38, sg556 = 39, ssg08 = 40, ct_knife = 42, m4a4 = 16, usps = 61, m4a1s = 60, cz75 = 63, revolver = 64, t_knife = 59, galil = 13, mp5sd = 23
		};

		bool enabled{ true };
		int music_kit{ 0 };

		struct skin_config
		{
			int paint_kit{ 0 };
			int seed{ 0 };
			int stat_trak{ -1 };
			float wear{ 0.1f };
			bool uses_old_model{ false };
			weapons_enum weapon_type;
		};

		mutable std::shared_mutex mutex{};
		std::unordered_map<int, skin_config> weapon_skins{};
	};

	struct movement
	{
		struct bhop
		{
			bool enabled{ false };
		} bhop{};

		struct quickstop
		{
			bool enabled{ false };
			float strength{ 1.0f };
		} quickstop{};
	};

	inline combat g_combat{};
	inline esp g_esp{};
	inline misc g_misc{};
	inline movement g_movement{};
	inline skinchanger g_skinchanger{};

} // namespace settings
