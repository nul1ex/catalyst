#pragma once
#include <core/settings.hpp>

namespace systems {

	class convars
	{
	public:
		[[nodiscard]] std::uintptr_t find( std::uint32_t name_hash );

		template<typename T>
		[[nodiscard]] T get( std::uintptr_t cvar_ptr );
	};

	class schemas
	{
	public:
		[[nodiscard]] std::int32_t lookup( const char* class_name, std::uint32_t field_hash );

	private:
		struct cached_entry
		{
			std::uint32_t field_hash;
			std::int32_t offset;
		};

		[[nodiscard]] std::uintptr_t find_class_binding( const char* class_name );
		[[nodiscard]] std::uint32_t murmur2( const char* str );
		[[nodiscard]] std::uint8_t bucket_index( std::uint32_t hash );

		std::uintptr_t m_schema_system{};
		std::uintptr_t m_client_scope{};
	};

	class entities
	{
	public:
		enum class type : std::uint8_t
		{
			unknown,
			player,
			item,
			projectile,
			bomb
		};

		struct cached
		{
			std::uintptr_t ptr{};
			std::uint32_t schema_hash{};
			std::int16_t index{};
			type type{ type::unknown };
		};

		void refresh( );

		[[nodiscard]] std::uintptr_t lookup( std::uint32_t handle ) const;
		[[nodiscard]] std::vector<cached> by_type( type filter ) const;
		[[nodiscard]] std::vector<cached> all( ) const;

	private:

		[[nodiscard]] std::uintptr_t get_entity_list( ) const;
		[[nodiscard]] std::uintptr_t get_by_index( std::uintptr_t entity_list, std::int32_t index ) const;
		[[nodiscard]] std::uint32_t get_schema_hash( std::uintptr_t entity ) const;
		[[nodiscard]] type classify( std::uint32_t schema_hash ) const;

		std::vector<cached> m_entities{};
		mutable std::shared_mutex m_mutex{};
	};

	class local
	{
	public:
		void update( );

		[[nodiscard]] std::uintptr_t controller( ) const { return this->m_controller.load( ); }
		[[nodiscard]] std::uintptr_t pawn( ) const { return this->m_pawn.load( ); }
		[[nodiscard]] std::int32_t team( ) const { return this->m_team.load( ); }
		[[nodiscard]] bool valid( ) const { return this->m_pawn.load( ) != 0 || this->m_observer_pawn.load( ) != 0; }
		[[nodiscard]] bool alive( ) const { return this->m_alive.load( ); }
		[[nodiscard]] std::uintptr_t view_pawn( ) const { return this->m_alive.load( ) ? this->m_pawn.load( ) : this->m_observer_pawn.load( ); }

		[[nodiscard]] std::uintptr_t weapon( ) const { return this->m_weapon.load( ); }
		[[nodiscard]] std::uintptr_t weapon_vdata( ) const { return this->m_weapon_vdata.load( ); }
		[[nodiscard]] std::uint32_t weapon_type( ) const { return this->m_weapon_type.load( ); }
		[[nodiscard]] math::vector3 eye_position( ) const { return { this->m_eye_x.load( ), this->m_eye_y.load( ), this->m_eye_z.load( ) }; }

		[[nodiscard]] bool is_enemy( std::int32_t other_team ) const
		{
			if ( !this->m_team_mode.load( ) )
			{
				return true;
			}

			return this->m_view_team.load( ) != other_team;
		}

	private:
		void reset( );

		std::atomic<std::uintptr_t> m_controller{};
		std::atomic<std::uintptr_t> m_pawn{};
		std::atomic<std::uintptr_t> m_observer_pawn{};
		std::atomic<std::int32_t> m_team{};
		std::atomic<std::int32_t> m_view_team{};
		std::atomic<bool> m_alive{};
		std::atomic<bool> m_team_mode{ true };

		std::atomic<std::uintptr_t> m_weapon{};
		std::atomic<std::uintptr_t> m_weapon_vdata{};
		std::atomic<std::uint32_t> m_weapon_type{};
		std::atomic<float> m_eye_x{}, m_eye_y{}, m_eye_z{};
	};

	class view
	{
	public:
		void update( );

		[[nodiscard]] math::vector2 project( const math::vector3& world_pos );
		[[nodiscard]] bool projection_valid( const math::vector2& screen_pos ) { return screen_pos.x != this->k_invalid && screen_pos.y != this->k_invalid; }
		[[nodiscard]] bool has_camera( ) const { return this->m_fov != this->k_invalid; }
		[[nodiscard]] math::vector3 origin( ) const { return this->m_origin; }
		[[nodiscard]] math::vector3 angles( ) const { return this->m_angles; }
		[[nodiscard]] float fov( ) const { return this->m_fov; }
		[[nodiscard]] math::matrix4x4 matrix( ) const { return this->m_matrix; }

	private:
		static constexpr auto k_invalid{ 0xdead };

		HWND m_game_hwnd{};
		math::vector2 m_screen_size{};
		math::vector2 m_screen_offset{};

		math::matrix4x4 m_matrix{};
		math::vector3 m_origin{};
		math::vector3 m_angles{};
		float m_fov{ 0.0f };
	};

	class bones
	{
	public:
		struct data
		{
			struct bone
			{
				math::vector3 position{};
				math::quaternion rotation{};
			};

			std::array<bone, static_cast< std::size_t >( 28 )> bones{};

			[[nodiscard]] bool is_valid( ) const;
			[[nodiscard]] math::vector3 get_position( std::uint32_t id ) const;
			[[nodiscard]] math::quaternion get_rotation( std::uint32_t id ) const;
		};

		[[nodiscard]] data get( std::uintptr_t bone_array ) const;
	};

	class bounds
	{
	public:
		struct data
		{
			math::vector2 min{};
			math::vector2 max{};

			[[nodiscard]] bool is_valid( ) const;
			[[nodiscard]] float width( ) const { return this->max.x - this->min.x; }
			[[nodiscard]] float height( ) const { return this->max.y - this->min.y; }
		};

		[[nodiscard]] data get( const bones::data& bone_data ) const;
	};

	class hitboxes
	{
	public:
		struct entry
		{
			int index{ -1 };
			int bone{ -1 };
			math::vector3 mins{};
			math::vector3 maxs{};
			float radius{};
		};

		struct set
		{
			std::array<entry, 20> entries{};
			int count{};

			[[nodiscard]] const entry* begin( ) const { return this->entries.data( ); }
			[[nodiscard]] const entry* end( ) const { return this->entries.data( ) + this->count; }
		};

		[[nodiscard]] set query( std::uintptr_t game_scene_node ) const;
		[[nodiscard]] int hitgroup_from_hitbox( int hitbox ) const;

	private:
		static constexpr int k_bone_map[ ]{ 6, -1, 0, 1, 2, 3, 4, 22, 25, 23, 26, 24, 27, 10, 15, 9, 8, 14, 13 };
	};

	class collector
	{
	public:
		enum class item_subtype : std::uint8_t
		{
			unknown = 0,
			ak47, m4a4, m4a1s, awp, aug, famas, galil_ar, sg553,
			g3sg1, scar20, ssg08,
			mac10, mp5sd, mp7, mp9, pp_bizon, p90, ump45,
			nova, sawed_off, xm1014, mag7,
			m249, negev,
			deagle, dual_berettas, five_seven, glock, p2000, usps, p250, cz75, tec9, r8_revolver,
			taser, knife, c4, healthshot,
			he_grenade, flashbang, smoke_grenade, molotov, incendiary, decoy
		};

		enum class projectile_subtype : std::uint8_t
		{
			unknown = 0,
			he_grenade, flashbang, smoke_grenade, molotov, incendiary, molotov_fire, decoy
		};

		struct weapon_info
		{
			std::uintptr_t ptr{};
			std::uintptr_t vdata{};
			std::string name{};
			std::int32_t ammo{};
			std::int32_t max_ammo{};
		};

		struct player
		{
			std::uintptr_t controller{};
			std::uintptr_t pawn{};
			std::uintptr_t game_scene_node{};
			std::uintptr_t bone_cache{};
			math::vector3 origin{};
			std::string display_name{};
			weapon_info weapon{};
			std::int32_t health{};
			std::int32_t team{};
			std::int32_t money{};
			std::int32_t ping{};
			std::int32_t armor{};
			bool alive{};
			bool invulnerable{};
			bool has_helmet{};
			bool has_defuser{};
			bool is_scoped{};
			bool is_defusing{};
			bool is_flashed{};
			bool is_visible{};
			hitboxes::set hitboxes{};
			bones::data bones{};
			math::vector3 eye_angles{};
			math::vector3 view_offset{};
			math::vector3 velocity{};
			math::vector3 acceleration{};
			math::vector3 prediction_offset{};
			float last_update_time{};
			std::chrono::steady_clock::time_point last_update_tick{};
		};

		struct item
		{
			std::uintptr_t entity{};
			std::uintptr_t game_scene_node{};
			math::vector3 origin{};
			item_subtype subtype{ item_subtype::unknown };
			std::int32_t ammo{};
			std::int32_t max_ammo{};
		};

		struct projectile
		{
			std::uintptr_t entity{};
			std::uintptr_t game_scene_node{};
			math::vector3 origin{};
			math::vector3 velocity{};
			projectile_subtype subtype{ projectile_subtype::unknown };
			std::uint32_t thrower_handle{};
			std::int32_t bounces{};
			std::int32_t effect_tick_begin{};
			bool detonated{};
			bool smoke_active{};
			std::vector<math::vector3> fire_points{};
			float expire_time{};
		};

		void run( );

		[[nodiscard]] std::vector<player> players( ) const;
		[[nodiscard]] std::vector<item> items( ) const;
		[[nodiscard]] std::vector<projectile> projectiles( ) const;

	private:
		void collect_players( const std::vector<entities::cached>& raw );
		void collect_items( const std::vector<entities::cached>& raw );
		void collect_projectiles( const std::vector<entities::cached>& raw );

		[[nodiscard]] static item_subtype classify_item( std::uint32_t schema_hash );
		[[nodiscard]] static projectile_subtype classify_projectile( std::uint32_t schema_hash );

		struct player_history
		{
			math::vector3 last_origin{};
			math::vector3 last_velocity{};
			math::vector3 last_acceleration{};
			float last_update_time{};
		};

		std::vector<player> m_players{};
		std::vector<item> m_items{};
		std::vector<projectile> m_projectiles{};
		std::unordered_map<std::uintptr_t, player_history> m_history{};
		mutable std::shared_mutex m_mutex{};
	};

	class bvh
	{
	public:
		struct surface_info
		{
			float penetration{};
			std::uint16_t surface_type{};
			std::uint8_t global_index{ 255 };
		};

		struct global_surface_entry
		{
			float unk_00{};
			float unk_04{};
			float penetration_mod{};
			float unk_0C{};
			float unk_10{};
			std::uint16_t surface_type{};
			std::uint16_t pad{};
			std::uint8_t pad2[ 8 ]{};
		};

		struct triangle
		{
			math::vector3 v0{};
			math::vector3 v1{};
			math::vector3 v2{};
			surface_info surface{};
		};

		struct trace_result
		{
			bool hit{};
			float fraction{};
			float distance{};
			math::vector3 end_pos{};
			math::vector3 normal{};
			surface_info surface{};
			std::int32_t triangle_index{ -1 };
		};

		struct hit_entry
		{
			float distance{};
			float fraction{};
			math::vector3 position{};
			math::vector3 normal{};
			surface_info surface{};
			std::int32_t triangle_index{ -1 };
			bool is_enter{ true };
		};

		struct penetration_segment
		{
			float enter_fraction{};
			float exit_fraction{};
			float enter_distance{};
			float exit_distance{};
			math::vector3 enter_pos{};
			math::vector3 exit_pos{};
			surface_info enter_surface{};
			surface_info exit_surface{};
			float thickness{};
			float min_pen_mod{};
		};

		void parse( );
		void clear( );

		[[nodiscard]] trace_result trace_ray( const math::vector3& start, const math::vector3& end, std::int32_t exclude_tri = -1 ) const;
		[[nodiscard]] std::vector<hit_entry> trace_ray_all( const math::vector3& start, const math::vector3& end ) const;
		[[nodiscard]] std::vector<penetration_segment> build_segments( const std::vector<hit_entry>& hits, float ray_length ) const;

		[[nodiscard]] const std::vector<triangle>& triangles( ) const;
		[[nodiscard]] std::size_t count( ) const;
		[[nodiscard]] bool valid( ) const;

	private:
		struct aabb
		{
			float mins[ 3 ]{ 1e12f, 1e12f, 1e12f };
			float maxs[ 3 ]{ -1e12f, -1e12f, -1e12f };

			void expand( const math::vector3& p );
			void expand( const aabb& o );
			[[nodiscard]] int longest_axis( ) const;
			[[nodiscard]] bool intersects_ray( const float origin[ 3 ], const float inv_dir[ 3 ], float max_t ) const;
		};

		struct bvh_node
		{
			aabb bounds{};
			std::int32_t left{ -1 };
			std::int32_t right{ -1 };
			std::int32_t tri_start{};
			std::int32_t tri_count{};
		};

		void rebuild_accel( );
		std::int32_t build_recursive( std::int32_t start, std::int32_t end, std::int32_t depth );

		std::vector<triangle> m_triangles{};
		mutable std::shared_mutex m_mutex{};

		std::vector<bvh_node> m_nodes{};
		std::vector<std::int32_t> m_indices{};
		std::vector<aabb> m_tri_bounds{};
		std::vector<float> m_centroids{};

		static constexpr auto k_max_leaf_tris{ 8 };
		static constexpr auto k_max_depth{ 48 };
	};

	struct voxel
	{
		int x, y, z;
		std::uint8_t r, g, b, a;
		math::vector3 world;
	};

	class voxels
	{
	public:
		void update( );
		[[nodiscard]] bool line_goes_through_smoke( const math::vector3& start, const math::vector3& end ) const;
		[[nodiscard]] const std::vector<voxel>& active_voxels( ) const { return this->m_active_voxels; }

	private:
		std::vector<voxel> m_active_voxels{};
	};

	inline convars g_convars{};
	inline schemas g_schemas{};
	inline entities g_entities{};
	inline local g_local{};
	inline view g_view{};
	inline bones g_bones{};
	inline bounds g_bounds{};
	inline hitboxes g_hitboxes{};
	inline collector g_collector{};
	inline bvh g_bvh{};
	inline voxels g_voxels{};

} // namespace systems

#define SCHEMA( class_name, field_hash ) \
	[]( ) -> std::int32_t { \
		static const auto val = systems::g_schemas.lookup( class_name, field_hash ); \
		return val; \
	}( )

#define CONVAR( name_hash ) \
	[]( ) -> std::uintptr_t { \
		static const auto val = systems::g_convars.find( name_hash ); \
		return val; \
	}( )