#pragma once
#include <deque>
#include <cstdint>
#include <stdafx.hpp>

namespace features {

	namespace combat {

		class legit
		{
		public:
			void on_render( zdraw::draw_list& draw_list );
			void tick( );

		private:
			struct target
			{
				const systems::collector::player* player{};
				systems::bones::data bones{};
				math::vector3 aim_point{};
				int bone{ -1 };
				math::vector3 offset{};
				int hitgroup{ -1 };
				float damage{};
				float fov{};
				bool penetrated{};
			};

			[[nodiscard]] target select_target( const math::vector3& eye_pos, const math::vector3& view_angles, const std::vector<systems::collector::player>& players, const settings::combat::group_config& cfg ) const;
			[[nodiscard]] math::vector3 get_aim_point( const math::vector3& eye_pos, const systems::collector::player& player, const systems::bones::data& bones, const settings::combat::group_config& cfg, float& out_damage, int& out_bone, math::vector3& out_offset, int& out_hitgroup, bool& out_penetrated ) const;

			[[nodiscard]] float get_fov( const math::vector3& view_angles, const math::vector3& eye_pos, const math::vector3& target_pos ) const;
			[[nodiscard]] float get_fov_radius( const math::vector3& eye_pos, const math::vector3& view_angles, float fov_degrees ) const;

			void draw_penetration_crosshair(zdraw::draw_list& draw_list, const math::vector3& eye_pos, const math::vector3& view_angles, const settings::combat::group_config& cfg);

			void draw_fov( zdraw::draw_list& draw_list, const math::vector3& eye_pos, const math::vector3& view_angles, const settings::combat::aimbot_settings& cfg );
			void aimbot(const math::vector3& eye_pos, const math::vector3& view_angles, const target& tgt, const settings::combat::aimbot_settings& cfg);

			struct trigger_result
			{
				const systems::collector::player* player{};
				systems::bones::data bones{};
				int hitbox{ -1 };
				int hitgroup{ -1 };
				float damage{};
				bool penetrated{};
				float sim_time{ 0.0f };
				math::vector3 smoothed_offset{};
			};

			[[nodiscard]] trigger_result trace_crosshair( const math::vector3& eye_pos, const math::vector3& view_angles, const std::vector<systems::collector::player>& players, const settings::combat::triggerbot_settings& cfg ) const;
			void triggerbot(const math::vector3& eye_pos, const math::vector3& view_angles, const math::vector3& camera_angles, const std::vector<systems::collector::player>& players, const settings::combat::triggerbot_settings& cfg);

			void apply_autostop( );
			void release_autostop( bool force = false );

			animation::spring m_fov_alpha{};
			random::valve_rng m_rng{};
			bool m_rng_seeded{ false };

			math::vector2 m_aim_error{};

			float m_trigger_delay_end{ 0.0f };
			bool m_trigger_waiting{ false };
			bool m_trigger_held{ false };
			float m_trigger_release_time{ 0.0f };
			math::vector3 m_last_punch{};
			bool m_autostop_active{ false };
			std::chrono::steady_clock::time_point m_autostop_start{};
			std::vector<std::uint16_t> m_autostop_keys{};
			std::vector<std::uint16_t> m_autostop_inhibited_keys{};
			std::uintptr_t s_magnet_pawn = 0;
			float m_last_trigger_damage{ 0.0f };
			float m_min_trigger_damage{ 0.0f };

			struct prediction_data
			{
				math::vector3 smoothed_offset{};
				math::vector3 last_velocity{};
			};
			mutable std::unordered_map<std::uintptr_t, prediction_data> m_prediction_history;
		};

		class shared
		{
		public:
			struct context
			{
				std::uintptr_t weapon;
				std::uintptr_t weapon_vdata;
				std::uint32_t weapon_type;
				std::uint16_t item_def_idx;
				int num_bullets;
				float accuracy_penalty;
				float inaccuracy;
				float spread;
				float recoil_index;
				bool is_reloading;
				bool is_full_auto;
				bool is_scoped;
				bool weapon_ready;
				float current_time;
				float cycle_time;
				float last_shot_time;
				bool valid;
			};

			class penetration
			{
			public:
				struct weapon_data
				{
					float damage;
					float penetration;
					float range_modifier;
					float range;
					float armor_ratio;
					float headshot_multiplier;
				};

				struct result
				{
					float damage;
					int hitbox;
					bool penetrated;
				};

				void prepare( std::uintptr_t weapon_vdata, std::uintptr_t weapon );

				[[nodiscard]] bool run( const math::vector3& start, const math::vector3& end, const systems::collector::player& target, const systems::bones::data& bones, result& out ) const;
				[[nodiscard]] bool can(const math::vector3& start, const math::vector3& direction, float& out_damage) const;
				[[nodiscard]] float get_max_damage( int hitgroup, int target_armor, bool has_helmet, int target_team ) const;
				[[nodiscard]] const weapon_data& get_weapon_data( ) const { return this->m_weapon_data; }

			private:
				weapon_data m_weapon_data{};
			};

			void tick( );

			[[nodiscard]] const context& ctx( ) const { return this->m_ctx; }
			[[nodiscard]] const penetration& pen( ) const { return this->m_pen; }

			[[nodiscard]] float calculate_hitchance( const math::vector3& eye_pos, const math::vector3& aim_angle, const systems::collector::player& target, const systems::bones::data& bones, const math::vector3& offset = {} ) const;
			[[nodiscard]] std::uint32_t get_spread_seed( const math::vector3& angles, int tick ) const;
			[[nodiscard]] math::vector2 calculate_spread( int seed, float accuracy, float spread, float recoil_index, int item_def_idx, int num_bullets ) const;
			[[nodiscard]] math::vector3 extrapolate_stop( const math::vector3& pos ) const;
			[[nodiscard]] bool is_sniper_accurate( ) const;
			[[nodiscard]] float get_prediction_time( ) const;

		private:
			[[nodiscard]] float get_spread( std::uintptr_t weapon_vdata ) const;
			float get_base_inaccuracy(std::uintptr_t weapon, std::uintptr_t weapon_vdata, std::uintptr_t pawn) const;
			float get_inaccuracy(std::uintptr_t pawn, std::uintptr_t weapon, std::uintptr_t weapon_vdata, const math::vector3& eye_angles, float* out_move_inaccuracy, float* out_air_inaccuracy) const;
			//[[nodiscard]] float get_inaccuracy(std::uintptr_t pawn, std::uintptr_t weapon, std::uintptr_t weapon_vdata, const math::vector3& eye_angles) const;

			context m_ctx{};
			penetration m_pen{};
			mutable std::shared_mutex m_ctx_mutex{};
		};

		inline legit g_legit{};
		inline shared g_shared{};

	} // namespace combat

	namespace esp {

		class player
		{
		public:
			void on_render( zdraw::draw_list& draw_list );

		private:
			struct draw_offsets
			{
				float left{ 0.0f };
				float top{ 0.0f };
				float bottom{ 0.0f };
				float right{ 0.0f };
			};

			void add_box( zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const settings::esp::player::box& cfg, bool is_visible );
			void add_skeleton( zdraw::draw_list& draw_list, const systems::bones::data& bones, const settings::esp::player::skeleton& cfg, bool is_visible );
			void add_hitboxes( zdraw::draw_list& draw_list, const systems::bones::data& bones, const systems::collector::player& player, const settings::esp::player::hitboxes& cfg, float current_time );
			void add_health_bar( zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const systems::collector::player& player, const settings::esp::player::health_bar& cfg, draw_offsets& offsets );
			void add_ammo_bar( zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const systems::collector::player& player, const settings::esp::player::ammo_bar& cfg, draw_offsets& offsets );
			void add_name( zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const systems::collector::player& player, const settings::esp::player::name& cfg, draw_offsets& offsets );
			void add_weapon( zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const systems::collector::player& player, const settings::esp::player::weapon& cfg, draw_offsets& offsets );
			void add_flags( zdraw::draw_list& draw_list, const systems::bounds::data& bounds, const systems::collector::player& player, const settings::esp::player::info_flags& cfg, draw_offsets& offsets );
			void add_oof_arrow( zdraw::draw_list& draw_list, const systems::collector::player& player, const settings::esp::player::oof_arrow& cfg );
			void draw_movement_trails( zdraw::draw_list& draw_list );

			[[nodiscard]] static std::string get_weapon_icon( const std::string& weapon_name );

			struct animation_data
			{
				animation::spring health{};
				animation::spring ammo{};
				bool initialized{ false };
				float last_hit_time{ 0.0f };
				int last_health{ 100 };
			};

			struct trail_point
			{
				math::vector3 pos{};
				float time{};
			};

			struct trail_data
			{
				std::deque<trail_point> path{};
			};


			std::unordered_map<std::uintptr_t, animation_data> m_animations{};
			std::unordered_map<std::uintptr_t, trail_data> m_trails{};

			void add_capsule( zdraw::draw_list& draw_list, const math::vector3& start, const math::vector3& end, float radius, const math::quaternion& rotation, const math::vector3& origin, const zdraw::rgba& color, int segments_max, bool red_only, float fill_line );
			void draw_capsule_outline( zdraw::draw_list& draw_list, const math::vector3& top, const math::vector3& bottom, const math::vector3& axis, const math::vector3& u, const math::vector3& v, float radius, const zdraw::rgba& color, int segments, bool red_only, float fill_line );
			void create_circle( const math::vector3& center, const math::vector3& u, const math::vector3& v, float radius, std::vector<math::vector3>& out, int segments );
			void precompute_sincos( int segments );

			std::vector<float> m_sin_cache{};
			std::vector<float> m_cos_cache{};
		};

		class item
		{
		public:
			void on_render( zdraw::draw_list& draw_list );

		private:
			enum class category : std::uint8_t { rifle, smg, shotgun, sniper, pistol, heavy, grenade, utility };

			void add_icon( zdraw::draw_list& draw_list, const math::vector2& screen, const systems::collector::item& item, const settings::esp::item::icon& cfg, float& y_offset );
			void add_name( zdraw::draw_list& draw_list, const math::vector2& screen, const systems::collector::item& item, const settings::esp::item::name& cfg, float& y_offset );
			void add_ammo( zdraw::draw_list& draw_list, const math::vector2& screen, const systems::collector::item& item, const settings::esp::item::ammo& cfg, float& y_offset );

			[[nodiscard]] bool passes_filter( systems::collector::item_subtype subtype, const settings::esp::item::filters& filters ) const;
			[[nodiscard]] category get_category( systems::collector::item_subtype subtype ) const;
			[[nodiscard]] std::string get_icon( systems::collector::item_subtype subtype ) const;
			[[nodiscard]] std::string get_display_name( systems::collector::item_subtype subtype ) const;
		};

		class projectile
		{
		public:
			void on_render( zdraw::draw_list& draw_list );

		private:
			void draw_timer( zdraw::draw_list& draw_list, const math::vector2& screen, float& y_offset, float remaining, float frac, const settings::esp::projectile& cfg ) const;
			void draw_inferno_bounds( zdraw::draw_list& draw_list, const systems::collector::projectile& proj, const settings::esp::projectile& cfg ) const;
			void draw_smoke_voxels( zdraw::draw_list& draw_list, const settings::esp::projectile& cfg ) const;
			[[nodiscard]] zdraw::rgba get_color( systems::collector::projectile_subtype type, const settings::esp::projectile& cfg ) const;
			[[nodiscard]] std::string get_icon( systems::collector::projectile_subtype type ) const;
			[[nodiscard]] std::string get_name( systems::collector::projectile_subtype type ) const;
			[[nodiscard]] zdraw::rgba lerp_color( const zdraw::rgba& a, const zdraw::rgba& b, float t ) const;
		};

		class footsteps
		{
		public:
			void on_render( zdraw::draw_list& draw_list );
			void tick( );

		private:
			struct ring
			{
				math::vector3 world_pos{};
				float max_radius{};
				zdraw::rgba color{};
				float start_time{};
			};

			std::vector<ring> m_rings{};
			std::unordered_map<std::uintptr_t, bool> m_prev_on_ground{};
			std::unordered_map<std::uintptr_t, float> m_last_step_time{};
		};

		inline player g_player{};
		inline item g_item{};
		inline projectile g_projectile{};
		inline footsteps g_footsteps{};

	} // namespace esp

	namespace misc {

		class grenades
		{
		public:
			void on_render( zdraw::draw_list& draw_list );

		private:
			struct trajectory
			{
				std::vector<math::vector3> points{};
				std::vector<math::vector3> bounces{};
				math::vector3 end_pos{};
				float duration{};
				int end_tick{ -1 };
				bool valid{ false };
			};

			struct in_flight_grenade
			{
				std::uintptr_t entity{};
				std::uintptr_t weapon_hash{};
				trajectory traj{};
				std::chrono::steady_clock::time_point throw_time{};
				std::chrono::steady_clock::time_point detonate_time{};
				std::chrono::steady_clock::time_point last_seen{};
				bool detonated{ false };
				bool effect_started{ false };
				bool corrected{ false };
			};

			[[nodiscard]] bool can_predict( ) const;
			void update_weapon_properties( );
			void setup_throw( math::vector3& origin, math::vector3& velocity );

			void update_in_flight( );
			[[nodiscard]] std::uintptr_t hash_from_projectile( systems::collector::projectile_subtype type ) const;
			[[nodiscard]] zdraw::rgba color_for_type( std::uintptr_t weapon_hash ) const;

			void simulate( const math::vector3& start, const math::vector3& velocity, trajectory& out );
			void step_simulation( math::vector3& pos, math::vector3& vel, systems::bvh::trace_result& trace );
			void resolve_collision( const systems::bvh::trace_result& trace, math::vector3& pos, math::vector3& vel );
			[[nodiscard]] bool should_detonate( const math::vector3& vel, int tick ) const;
			void render_trajectory( zdraw::draw_list& draw_list, const trajectory& traj, float alpha, std::uintptr_t weapon_hash ) const;

			std::uintptr_t m_weapon_vdata{};
			std::uintptr_t m_weapon_hash{};
			float m_throw_velocity{};
			float m_detonate_time{ 1.5f };
			float m_velocity_threshold{ 0.1f };

			std::vector<in_flight_grenade> m_in_flight{};
			std::chrono::steady_clock::time_point m_last_throw_time{};
			bool m_was_holding{ false };

			float m_sv_gravity{};
			float m_molotov_max_slope_z{};

			static constexpr auto tick_interval{ 1.0f / 64.0f };
			static constexpr auto gravity_scale{ 0.4f };
			static constexpr auto elasticity{ 0.45f };
			static constexpr auto max_ticks{ 1024 };
			static constexpr auto ticks_per_point{ 4 };
			static constexpr auto throw_cooldown{ 1.0f };
			static constexpr auto missing_grace{ 0.1f };
		};

		class misc_features
		{
		public:
			void on_render( zdraw::draw_list& draw_list );
			void tick( );
			void tick_write( );

		private:
			void draw_spectators( zdraw::draw_list& draw_list );
			void draw_bomb_timer( zdraw::draw_list& draw_list );
			void hitsounds( );
			void draw_hit_markers( zdraw::draw_list& draw_list );
			void draw_damage_indicators( zdraw::draw_list& draw_list );
			void draw_watermark( zdraw::draw_list& draw_list );
			void fov_changer( );

			struct hit_marker_t
			{
				std::chrono::steady_clock::time_point time;
				float max_time;
			};

			struct damage_indicator_t
			{
				math::vector3 pos;
				float damage;
				bool is_headshot;
				std::chrono::steady_clock::time_point time;
				float max_time;
			};

			int m_old_hits{ 0 };
			int m_old_damage{ 0 };
			std::vector<hit_marker_t> m_hit_markers{};
			std::vector<damage_indicator_t> m_damage_indicators{};

			struct health_history_t
			{
				int health{ -1 };
				math::vector3 origin{};
			};
			std::unordered_map<std::uintptr_t, health_history_t> m_health_history{};
			mutable std::shared_mutex m_mutex{};
		};

		class impacts
		{
		public:
			void on_render( zdraw::draw_list& draw_list );
			void tick( );

		private:
			struct tracer_t
			{
				math::vector3 start;
				math::vector3 end;
				float time;
				float max_time;
			};

			std::vector<tracer_t> m_tracers{};
			int m_old_shots{ 0 };
		};

		struct nade_data
		{
			std::string name;
			math::vector3 pos;
			math::vector3 target_pos;
			int throw_type; // 0: stand, 1: jump, 2: walk, 3: run, 4: crouch, 5: crouch jump
			int nade_type; // 0: HE, 1: Flash, 2: Smoke, 3: Molly, 4: Incendiary, 5: Decoy
		};

		class nade_helper
		{
		public:
			void on_render( zdraw::draw_list& draw_list );
			void tick( );

			void load_nades( const std::string& map_name );
			void save_nades( const std::string& map_name );

			[[nodiscard]] std::vector<nade_data>& get_nades( ) { return m_nades; }

		private:
			std::vector<nade_data> m_nades{};
			std::string m_current_map{};
		};

		inline grenades g_grenades{};
		inline impacts g_impacts{};
		inline misc_features g_misc{};
		inline nade_helper g_nade_helper{};

	} // namespace misc

	namespace movement {

		class movement_features
		{
		public:
			void tick( );
		};

		inline movement_features g_movement{};

	} // namespace movement



	namespace skinchanger {

		enum weapons_enum : std::uint16_t
		{
			none = 0,
			deagle = 1, elite = 2, fiveseven = 3, glock = 4, ak47 = 7, aug = 8, awp = 9, famas = 10, g3sg1 = 11, m249 = 14, mac10 = 17, p90 = 19, ump45 = 24, xm1014 = 25, bizon = 26, mag7 = 27, negev = 28, sawedoff = 29, tec9 = 30, zeus = 31, p2000 = 32, mp7 = 33, mp9 = 34, nova = 35, p250 = 36, scar20 = 38, sg556 = 39, ssg08 = 40, ct_knife = 42, m4a4 = 16, usps = 61, m4a1s = 60, cz75 = 63, revolver = 64, t_knife = 59, galil = 13, mp5sd = 23
		};

		struct skin_info
		{
			int paint_kit;
			bool uses_old_model;
			std::string name;
			weapons_enum weapon_type;
		};
 
		struct music_kit_info
		{
			int id;
			std::string name;
		};

		class skin_db
		{
		public:
			void initialize( );
			std::vector<skin_info> get_weapon_skins( weapons_enum type = weapons_enum::none );
			std::vector<music_kit_info> get_music_kits( );

		private:
			std::vector<skin_info> m_skins{};
		};

		class skinchanger
		{
		public:
			std::uintptr_t get_hud_arms(std::uintptr_t local_player);
			std::uintptr_t get_hud_weapon(std::uintptr_t local_player, std::uintptr_t weapon);
			void tick( );
			bool m_force_update{ false };
			uint64_t regen_skins{ 0 };
		private:
			std::vector<std::uintptr_t> get_weapons( std::uintptr_t local_player );
		};

		inline skin_db g_skindb{};
		inline skinchanger g_skinchanger{};

	} // namespace skinchanger

} // namespace features
