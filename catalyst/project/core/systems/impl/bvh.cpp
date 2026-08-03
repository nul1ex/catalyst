#include <stdafx.hpp>

namespace systems {

	namespace detail {

		struct inner_node
		{
			float min[ 3 ];
			std::uint32_t packed0;
			float max[ 3 ];
			std::uint32_t packed1;

			[[nodiscard]] std::uint32_t type( ) const { return this->packed0 >> 30; }
			[[nodiscard]] std::uint32_t payload( ) const { return this->packed0 & 0x3fffffffu; }
		};

		struct hedge
		{
			std::uint8_t next;
			std::uint8_t twin;
			std::uint8_t vert;
			std::uint8_t face;
		};

		static bool extract_mesh( std::uintptr_t bvh_ptr, std::uintptr_t vert_ptr, std::uintptr_t tri_ptr, std::uint32_t node_count, const math::matrix3x3& rot, const float scale[ 3 ], const float pos[ 3 ], std::uintptr_t mat_arr_ptr, int mat_count, const std::vector<bvh::global_surface_entry>& global_table, const bvh::surface_info& default_surface, std::vector<bvh::triangle>& out )
		{
			if ( !bvh_ptr || !vert_ptr || !tri_ptr || node_count == 0 || node_count > 0x1000000 )
			{
				return false;
			}

			std::vector<std::uint8_t> bvh_buf( static_cast< std::size_t >( node_count ) * 32 );
			g::memory.read( bvh_ptr, bvh_buf.data( ), bvh_buf.size( ) );

			std::uint32_t min_tri = UINT32_MAX, max_tri = 0;
			std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
			std::vector<std::uint32_t> stack{ 0u };

			stack.reserve( 256 );

			while ( !stack.empty( ) )
			{
				const auto cursor = stack.back( );
				stack.pop_back( );

				if ( cursor >= node_count )
				{
					continue;
				}

				const auto node = reinterpret_cast< const inner_node* >( bvh_buf.data( ) + static_cast< std::size_t >( cursor ) * 32 );
				const auto payload = node->payload( );

				if ( node->type( ) == 3 )
				{
					if ( payload == 0 || payload >= 0x1000000 )
					{
						continue;
					}

					ranges.push_back( { node->packed1, payload } );
					min_tri = std::min( min_tri, node->packed1 );
					max_tri = std::max( max_tri, node->packed1 + payload );
					continue;
				}

				if ( payload == 0 )
				{
					continue;
				}

				if ( cursor + payload < node_count )
				{
					stack.push_back( cursor + payload );
				}

				stack.push_back( cursor + 1 );
			}

			if ( ranges.empty( ) || max_tri <= min_tri )
			{
				return false;
			}

			const auto total_tris = max_tri - min_tri;
			if ( total_tris > 0x1000000 )
			{
				return false;
			}

			std::vector<int> indices( total_tris * 3 );
			g::memory.read( tri_ptr + static_cast< std::uintptr_t >( min_tri ) * 12, indices.data( ), total_tris * 12 );

			auto max_vert{ 0 };

			for ( const auto idx : indices )
			{
				max_vert = std::max( max_vert, idx );
			}

			if ( max_vert <= 0 || max_vert > 0x1000000 )
			{
				return false;
			}

			const auto vert_count = static_cast< std::uint32_t >( max_vert + 1 );

			std::vector<float> vertices( vert_count * 3 );
			g::memory.read( vert_ptr, vertices.data( ), static_cast< std::size_t >( vert_count ) * 12 );

			std::vector<std::uint8_t> materials{};

			const auto has_materials = mat_arr_ptr > 0x10000 && mat_count > 0;
			if ( has_materials )
			{
				materials.resize( total_tris );
				g::memory.read( mat_arr_ptr + static_cast< std::uintptr_t >( min_tri ), materials.data( ), total_tris );
			}

			const auto global_count = static_cast< int >( global_table.size( ) );
			const auto before = out.size( );

			for ( const auto& [start, count] : ranges )
			{
				for ( auto i = 0u; i < count; ++i )
				{
					const auto local_idx = start - min_tri + i;
					if ( local_idx >= total_tris )
					{
						continue;
					}

					auto surf = default_surface;

					if ( has_materials && local_idx < materials.size( ) )
					{
						const auto gi = materials[ local_idx ];
						if ( gi < global_count )
						{
							const auto& gs = global_table[ gi ];
							surf.penetration = gs.penetration_mod;
							surf.surface_type = gs.surface_type;
							surf.global_index = gi;
						}
					}

					const auto base = local_idx * 3;
					const auto i0 = indices[ base ];
					const auto i1 = indices[ static_cast< std::size_t >( base ) + 1 ];
					const auto i2 = indices[ static_cast< std::size_t >( base ) + 2 ];

					if ( i0 < 0 || i1 < 0 || i2 < 0 )
					{
						continue;
					}

					if ( static_cast< std::uint32_t >( i0 ) >= vert_count || static_cast< std::uint32_t >( i1 ) >= vert_count || static_cast< std::uint32_t >( i2 ) >= vert_count )
					{
						continue;
					}

					auto xf = [ & ]( int vi ) -> math::vector3
						{
							const auto local = rot.rotate( { vertices[ vi * 3 ] * scale[ 0 ], vertices[ vi * 3 + 1 ] * scale[ 1 ], vertices[ vi * 3 + 2 ] * scale[ 2 ] } );
							return { local.x + pos[ 0 ], local.y + pos[ 1 ], local.z + pos[ 2 ] };
						};

					out.push_back( { .v0 = xf( i0 ), .v1 = xf( i1 ), .v2 = xf( i2 ), .surface = surf } );
				}
			}

			return out.size( ) > before;
		}

		static bool extract_hull( std::uintptr_t hull_data, float uniform_scale, const bvh::surface_info& surface, std::vector<bvh::triangle>& out )
		{
			if ( !hull_data )
			{
				return false;
			}

			std::uint8_t hd[ 0x100 ]{};
			g::memory.read( hull_data, hd, sizeof( hd ) );

			const auto vert_count = *reinterpret_cast< const int* >( hd + 0x88 );
			const auto vert_ptr = *reinterpret_cast< const std::uintptr_t* >( hd + 0x90 );
			const auto hedge_count = *reinterpret_cast< const int* >( hd + 0xA0 );
			const auto hedge_ptr = *reinterpret_cast< const std::uintptr_t* >( hd + 0xA8 );
			const auto face_count = *reinterpret_cast< const int* >( hd + 0xB8 );
			const auto face_ptr = *reinterpret_cast< const std::uintptr_t* >( hd + 0xC0 );
			const auto sane = [ ]( int count, std::uintptr_t ptr ) { return count > 0 && count <= 0xffff && ptr != 0; };

			if ( !sane( vert_count, vert_ptr ) || !sane( hedge_count, hedge_ptr ) || !sane( face_count, face_ptr ) )
			{
				return false;
			}

			std::vector<float> verts( vert_count * 3 );
			std::vector<hedge> hedges( hedge_count );
			std::vector<std::uint8_t> faces( face_count );

			g::memory.read( vert_ptr, verts.data( ), static_cast< std::size_t >( vert_count ) * 12 );
			g::memory.read( hedge_ptr, hedges.data( ), static_cast< std::size_t >( hedge_count ) * 4 );
			g::memory.read( face_ptr, faces.data( ), face_count );

			auto vert = [ & ]( int vi ) -> math::vector3
				{
					if ( vi < 0 || vi >= vert_count )
					{
						return {};
					}

					return { verts[ vi * 3 ] * uniform_scale, verts[ vi * 3 + 1 ] * uniform_scale, verts[ vi * 3 + 2 ] * uniform_scale };
				};

			const auto before = out.size( );

			std::vector<int> face_verts{};
			face_verts.reserve( 8 );

			for ( auto fi = 0; fi < face_count; ++fi )
			{
				const auto start_he = faces[ fi ];
				if ( start_he >= hedge_count )
				{
					continue;
				}

				face_verts.clear( );

				auto he = start_he;

				for ( auto safety = 0; he < hedge_count && safety < 64; ++safety )
				{
					face_verts.push_back( hedges[ he ].vert );
					he = hedges[ he ].next;

					if ( he == start_he )
					{
						break;
					}
				}

				if ( face_verts.size( ) < 3 )
				{
					continue;
				}

				const auto v0 = vert( face_verts[ 0 ] );

				for ( auto i = 1ull; i + 1 < face_verts.size( ); ++i )
				{
					out.push_back( { .v0 = v0, .v1 = vert( face_verts[ i ] ), .v2 = vert( face_verts[ i + 1 ] ), .surface = surface } );
				}
			}

			return out.size( ) > before;
		}

		static void process_shape( std::uintptr_t shape_body, std::uintptr_t hull_vtable, std::uintptr_t mesh_vtable, const std::vector<bvh::global_surface_entry>& global_table, std::vector<bvh::triangle>& out )
		{
			const auto interacts_as = g::memory.read<std::uint64_t>( shape_body + 0x50 );
			if ( ( interacts_as & 0xFFFFull ) == 0 || interacts_as == 0x40000008ull || interacts_as == 0x40000030ull )
			{
				return;
			}

			const auto vtable = g::memory.read<std::uintptr_t>( shape_body );
			if ( vtable == hull_vtable )
			{
				const auto hull_data = g::memory.read<std::uintptr_t>( shape_body + 0xB8 );
				if ( !hull_data )
				{
					return;
				}

				const auto raw_scale = g::memory.read<float>( shape_body + 0xB0 );
				const auto scale = ( raw_scale > 0.0f && std::isfinite( raw_scale ) ) ? raw_scale : 1.0f;

				bvh::surface_info surface{};
				surface.penetration = g::memory.read<float>( shape_body + 0x28 );

				extract_hull( hull_data, scale, surface, out );
				return;
			}

			if ( vtable != mesh_vtable )
			{
				return;
			}

			const auto mesh_data = g::memory.read<std::uintptr_t>( shape_body + 0xC0 );
			if ( !mesh_data )
			{
				return;
			}

			std::uint8_t md[ 0xA0 ]{};
			g::memory.read( mesh_data, md, sizeof( md ) );

			const auto node_count = *reinterpret_cast< const int* >( md + 0x18 );
			if ( node_count <= 0 || *reinterpret_cast< const int* >( md + 0x30 ) <= 0 || *reinterpret_cast< const int* >( md + 0x48 ) <= 0 )
			{
				return;
			}

			float scale[ 3 ]{};
			g::memory.read( shape_body + 0xB0, scale, sizeof( scale ) );

			for ( auto& s : scale )
			{
				if ( !std::isfinite( s ) )
				{
					return;
				}

				if ( s == 0.0f )
				{
					s = 1.0f;
				}
			}

			float world_pos[ 3 ]{};
			g::memory.read( shape_body + 0x100, world_pos, sizeof( world_pos ) );

			math::quaternion quat{};
			g::memory.read( shape_body + 0x130, &quat, sizeof( quat ) );

			const auto ql = quat.x * quat.x + quat.y * quat.y + quat.z * quat.z + quat.w * quat.w;
			if ( ql < 0.5f || ql > 1.5f )
			{
				quat = { 0, 0, 0, 1 };
			}

			bvh::surface_info default_surface{};
			default_surface.penetration = g::memory.read<float>( shape_body + 0x28 );

			extract_mesh( *reinterpret_cast< const std::uintptr_t* >( md + 0x20 ), *reinterpret_cast< const std::uintptr_t* >( md + 0x38 ), *reinterpret_cast< const std::uintptr_t* >( md + 0x50 ), static_cast< std::uint32_t >( node_count ), math::matrix3x3::from_quaternion( quat ), scale, world_pos, *reinterpret_cast< const std::uintptr_t* >( md + 0x98 ), *reinterpret_cast< const int* >( md + 0x90 ), global_table, default_surface, out );
		}

		static std::vector<bvh::global_surface_entry> load_global_surface_table( std::uintptr_t surface_manager )
		{
			std::vector<bvh::global_surface_entry> table;

			const auto array_base = g::memory.read<std::uintptr_t>( surface_manager + 40 );
			if ( !array_base )
			{
				return table;
			}

			auto count{ 0 };

			for ( const auto off : { 32, 36, 24, 28, 48 } )
			{
				const auto candidate = g::memory.read<int>( surface_manager + off );
				if ( candidate > 0 && candidate < 4096 )
				{
					count = candidate;
					break;
				}
			}

			if ( count <= 0 )
			{
				auto empty_run{ 0 };

				for ( auto i = 0; i < 1024; ++i )
				{
					bvh::global_surface_entry sd{};
					g::memory.read( array_base + static_cast< std::size_t >( i ) * 32, &sd, sizeof( sd ) );

					const bool empty = sd.penetration_mod == 0.0f && sd.surface_type == 0 && sd.unk_00 == 0.0f;
					if ( empty )
					{
						if ( ++empty_run > 8 )
						{
							break;
						}

						continue;
					}

					empty_run = 0;
					count = i + 1;
				}
			}

			if ( count > 0 )
			{
				table.resize( count );
				g::memory.read( array_base, table.data( ), static_cast< std::size_t >( count ) * sizeof( bvh::global_surface_entry ) );
			}

			return table;
		}

	} // namespace detail

	void bvh::parse( )
	{
		const auto physics_iface_load = g::memory.find_pattern( g::modules.client, "48 8B 0D ? ? ? ? E8 ? ? ? ? C7 87 10 1D 00 00 00 00 80 3F" );
		if ( !physics_iface_load )
		{
			return;
		}

		const auto holder = g::memory.read<std::uintptr_t>( g::memory.resolve_rip( physics_iface_load ) );
		const auto world = holder ? g::memory.read<std::uintptr_t>( holder ) : 0;

		if ( !world )
		{
			return;
		}

		const auto surface_data_fn = g::memory.find_pattern( g::modules.client, "48 63 41 ? 48 8B 0D" );
		const auto surface_manager = surface_data_fn ? g::memory.read<std::uintptr_t>( g::memory.resolve_rip( surface_data_fn + 4 ) ) : 0;

		if ( !surface_manager )
		{
			return;
		}

		const auto inner_world = g::memory.read<std::uintptr_t>( world + 0x30 );
		const auto body_array = inner_world ? g::memory.read<std::uintptr_t>( inner_world + 0x110 ) : 0;
		const auto body_count = body_array ? g::memory.read<int>( body_array + 0x268 ) : 0;

		if ( body_count <= 0 )
		{
			return;
		}

		const auto hull_vtable = g::memory.find_vtable( g::modules.vphysics2, "CRnHullShape" );
		const auto mesh_vtable = g::memory.find_vtable( g::modules.vphysics2, "CRnMeshShape" );

		if ( !hull_vtable || !mesh_vtable )
		{
			return;
		}

		const auto global_table = detail::load_global_surface_table( surface_manager );

		std::vector<triangle> fresh{};
		fresh.reserve( 262144 );

		for ( auto body_idx = 0; body_idx < body_count; ++body_idx )
		{
			const auto body = body_array + static_cast< std::uintptr_t >( body_idx ) * 88;

			if ( g::memory.read<std::uint32_t>( body + 0x40 ) != 2 )
			{
				continue;
			}

			const auto bvh_nodes_ptr = g::memory.read<std::uintptr_t>( body + 0x18 );
			if ( !bvh_nodes_ptr )
			{
				continue;
			}

			const auto bvh_root = g::memory.read<int>( body );

			if ( bvh_root < 0 )
			{
				const auto shape = g::memory.read<std::uintptr_t>( body + 0x28 );
				if ( shape )
				{
					detail::process_shape( shape, hull_vtable, mesh_vtable, global_table, fresh );
				}

				continue;
			}

			const auto outer_node_count = std::max( { static_cast< std::uint32_t >( bvh_root + 1 ),static_cast< std::uint32_t >( g::memory.read<int>( body + 0x08 ) ),static_cast< std::uint32_t >( g::memory.read<int>( body + 0x10 ) ), } );
			if ( outer_node_count == 0 || outer_node_count > 0x100000 )
			{
				continue;
			}

			std::vector<std::uint8_t> outer_buf( outer_node_count * 48 );
			g::memory.read( bvh_nodes_ptr, outer_buf.data( ), outer_buf.size( ) );

			std::unordered_set<std::uintptr_t> seen{};
			std::vector<int> stack{};

			stack.reserve( 128 );
			stack.push_back( bvh_root );

			while ( !stack.empty( ) )
			{
				const auto idx = stack.back( );
				stack.pop_back( );

				if ( idx < 0 || static_cast< std::uint32_t >( idx ) >= outer_node_count )
				{
					continue;
				}

				const auto node = outer_buf.data( ) + static_cast< std::uintptr_t >( idx ) * 48;
				const auto left = *reinterpret_cast< const int* >( node + 12 );

				if ( left == -1 )
				{
					const auto shape_ptr = *reinterpret_cast< const std::uintptr_t* >( node + 0x28 );
					if ( shape_ptr && seen.insert( shape_ptr ).second )
					{
						detail::process_shape( shape_ptr, hull_vtable, mesh_vtable, global_table, fresh );
					}

					continue;
				}

				stack.push_back( *reinterpret_cast< const int* >( node + 28 ) );
				stack.push_back( left );
			}
		}

		{
			std::unique_lock lock( this->m_mutex );
			this->m_triangles = std::move( fresh );
		}

		this->rebuild_accel( );
	}

	void bvh::clear( )
	{
		std::unique_lock lock( this->m_mutex );
		this->m_triangles.clear( );
		this->m_nodes.clear( );
		this->m_indices.clear( );
		this->m_tri_bounds.clear( );
		this->m_centroids.clear( );
	}

	bvh::trace_result bvh::trace_ray( const math::vector3& start, const math::vector3& end, int exclude_tri ) const
	{
		trace_result result{};
		result.end_pos = end;

		if ( this->m_nodes.empty( ) )
		{
			return result;
		}

		const auto dx = end.x - start.x;
		const auto dy = end.y - start.y;
		const auto dz = end.z - start.z;
		const auto max_dist = std::sqrt( dx * dx + dy * dy + dz * dz );

		if ( max_dist < 1e-8f )
		{
			return result;
		}

		const auto inv_dist = 1.0f / max_dist;
		const float dir[ 3 ]{ dx * inv_dist, dy * inv_dist, dz * inv_dist };
		const float origin[ 3 ]{ start.x, start.y, start.z };
		const float inv_dir[ 3 ]{ std::abs( dir[ 0 ] ) > 1e-8f ? 1.0f / dir[ 0 ] : ( dir[ 0 ] >= 0 ? 1e12f : -1e12f ), std::abs( dir[ 1 ] ) > 1e-8f ? 1.0f / dir[ 1 ] : ( dir[ 1 ] >= 0 ? 1e12f : -1e12f ), std::abs( dir[ 2 ] ) > 1e-8f ? 1.0f / dir[ 2 ] : ( dir[ 2 ] >= 0 ? 1e12f : -1e12f ) };

		auto closest_t = max_dist;

		int stack[ 128 ]{};
		int sp{ 0 };
		stack[ 0 ] = 0;

		while ( sp >= 0 )
		{
			const auto& node = this->m_nodes[ stack[ sp-- ] ];
			if ( !node.bounds.intersects_ray( origin, inv_dir, closest_t ) )
			{
				continue;
			}

			if ( node.left == -1 )
			{
				for ( int i = node.tri_start; i < node.tri_start + node.tri_count; ++i )
				{
					const auto ti = this->m_indices[ i ];
					if ( ti == exclude_tri )
					{
						continue;
					}

					const auto& tri = this->m_triangles[ ti ];

					const auto e1x = tri.v1.x - tri.v0.x, e1y = tri.v1.y - tri.v0.y, e1z = tri.v1.z - tri.v0.z;
					const auto e2x = tri.v2.x - tri.v0.x, e2y = tri.v2.y - tri.v0.y, e2z = tri.v2.z - tri.v0.z;

					const auto hx = dir[ 1 ] * e2z - dir[ 2 ] * e2y;
					const auto hy = dir[ 2 ] * e2x - dir[ 0 ] * e2z;
					const auto hz = dir[ 0 ] * e2y - dir[ 1 ] * e2x;
					const auto a = e1x * hx + e1y * hy + e1z * hz;

					if ( a > -1e-8f && a < 1e-8f )
					{
						continue;
					}

					const auto f = 1.0f / a;
					const auto sx = origin[ 0 ] - tri.v0.x, sy = origin[ 1 ] - tri.v0.y, sz = origin[ 2 ] - tri.v0.z;
					const auto u = f * ( sx * hx + sy * hy + sz * hz );

					if ( u < 0.0f || u > 1.0f )
					{
						continue;
					}

					const auto qx = sy * e1z - sz * e1y, qy = sz * e1x - sx * e1z, qz = sx * e1y - sy * e1x;
					const auto v = f * ( dir[ 0 ] * qx + dir[ 1 ] * qy + dir[ 2 ] * qz );

					if ( v < 0.0f || u + v > 1.0f )
					{
						continue;
					}

					const auto t = f * ( e2x * qx + e2y * qy + e2z * qz );

					if ( t > 1e-5f && t < closest_t )
					{
						closest_t = t;
						result.hit = true;
						result.fraction = t / max_dist;
						result.distance = t;
						result.triangle_index = ti;
						result.surface = tri.surface;
						result.end_pos = { origin[ 0 ] + dir[ 0 ] * t, origin[ 1 ] + dir[ 1 ] * t, origin[ 2 ] + dir[ 2 ] * t };

						const auto nx = e1y * e2z - e1z * e2y;
						const auto ny = e1z * e2x - e1x * e2z;
						const auto nz = e1x * e2y - e1y * e2x;
						const auto nl = std::sqrt( nx * nx + ny * ny + nz * nz );

						if ( nl > 1e-8f )
						{
							const auto inv_nl = 1.0f / nl;
							result.normal = { nx * inv_nl, ny * inv_nl, nz * inv_nl };
						}
					}
				}
			}
			else if ( sp + 2 < 127 )
			{
				stack[ ++sp ] = node.right;
				stack[ ++sp ] = node.left;
			}
		}

		return result;
	}

	bvh::trace_result bvh::trace_hull( const math::vector3& start, const math::vector3& end, const math::vector3& hull_mins, const math::vector3& hull_maxs, int exclude_tri ) const
	{
		const float half[ 3 ]
		{
			( hull_maxs.x - hull_mins.x ) * 0.5f,
			( hull_maxs.y - hull_mins.y ) * 0.5f,
			( hull_maxs.z - hull_mins.z ) * 0.5f
		};

		const float offset[ 3 ]
		{
			( hull_mins.x + hull_maxs.x ) * 0.5f,
			( hull_mins.y + hull_maxs.y ) * 0.5f,
			( hull_mins.z + hull_maxs.z ) * 0.5f
		};

		const math::vector3 shifted_start{ start.x + offset[ 0 ], start.y + offset[ 1 ], start.z + offset[ 2 ] };
		const math::vector3 shifted_end{ end.x + offset[ 0 ], end.y + offset[ 1 ], end.z + offset[ 2 ] };

		trace_result result{};
		result.end_pos = end;

		if ( this->m_nodes.empty( ) )
		{
			return result;
		}

		const auto dx = shifted_end.x - shifted_start.x;
		const auto dy = shifted_end.y - shifted_start.y;
		const auto dz = shifted_end.z - shifted_start.z;
		const auto max_dist = std::sqrt( dx * dx + dy * dy + dz * dz );

		if ( max_dist < 1e-8f )
		{
			return result;
		}

		const auto inv_dist = 1.0f / max_dist;
		const float dir[ 3 ]{ dx * inv_dist, dy * inv_dist, dz * inv_dist };
		const float origin[ 3 ]{ shifted_start.x, shifted_start.y, shifted_start.z };
		const float inv_dir[ 3 ]
		{
			std::abs( dir[ 0 ] ) > 1e-8f ? 1.0f / dir[ 0 ] : ( dir[ 0 ] >= 0 ? 1e12f : -1e12f ),
			std::abs( dir[ 1 ] ) > 1e-8f ? 1.0f / dir[ 1 ] : ( dir[ 1 ] >= 0 ? 1e12f : -1e12f ),
			std::abs( dir[ 2 ] ) > 1e-8f ? 1.0f / dir[ 2 ] : ( dir[ 2 ] >= 0 ? 1e12f : -1e12f )
		};

		auto closest_t = max_dist;

		int stack[ 128 ]{};
		int sp{ 0 };
		stack[ 0 ] = 0;

		while ( sp >= 0 )
		{
			const auto& node = this->m_nodes[ stack[ sp-- ] ];
			auto expanded = node.bounds;

			for ( auto i = 0; i < 3; ++i )
			{
				expanded.mins[ i ] -= half[ i ];
				expanded.maxs[ i ] += half[ i ];
			}

			if ( !expanded.intersects_ray( origin, inv_dir, closest_t ) )
			{
				continue;
			}

			if ( node.left == -1 )
			{
				for ( int i = node.tri_start; i < node.tri_start + node.tri_count; ++i )
				{
					const auto ti = this->m_indices[ i ];
					if ( ti == exclude_tri )
					{
						continue;
					}

					const auto& tri = this->m_triangles[ ti ];

					const auto e1x = tri.v1.x - tri.v0.x, e1y = tri.v1.y - tri.v0.y, e1z = tri.v1.z - tri.v0.z;
					const auto e2x = tri.v2.x - tri.v0.x, e2y = tri.v2.y - tri.v0.y, e2z = tri.v2.z - tri.v0.z;

					auto nx = e1y * e2z - e1z * e2y;
					auto ny = e1z * e2x - e1x * e2z;
					auto nz = e1x * e2y - e1y * e2x;
					const auto nl = std::sqrt( nx * nx + ny * ny + nz * nz );

					if ( nl < 1e-8f )
					{
						continue;
					}

					const auto inv_nl = 1.0f / nl;
					nx *= inv_nl;
					ny *= inv_nl;
					nz *= inv_nl;

					const auto support = half[ 0 ] * std::abs( nx ) + half[ 1 ] * std::abs( ny ) + half[ 2 ] * std::abs( nz );

					const auto push_x = nx * support;
					const auto push_y = ny * support;
					const auto push_z = nz * support;

					const auto center_to_origin_dot = ( origin[ 0 ] - tri.v0.x ) * nx + ( origin[ 1 ] - tri.v0.y ) * ny + ( origin[ 2 ] - tri.v0.z ) * nz;
					const auto sign = center_to_origin_dot >= 0.0f ? 1.0f : -1.0f;

					const auto v0x = tri.v0.x + push_x * sign, v0y = tri.v0.y + push_y * sign, v0z = tri.v0.z + push_z * sign;
					const auto fe1x = tri.v1.x + push_x * sign - v0x, fe1y = tri.v1.y + push_y * sign - v0y, fe1z = tri.v1.z + push_z * sign - v0z;
					const auto fe2x = tri.v2.x + push_x * sign - v0x, fe2y = tri.v2.y + push_y * sign - v0y, fe2z = tri.v2.z + push_z * sign - v0z;

					const auto hx = dir[ 1 ] * fe2z - dir[ 2 ] * fe2y;
					const auto hy = dir[ 2 ] * fe2x - dir[ 0 ] * fe2z;
					const auto hz = dir[ 0 ] * fe2y - dir[ 1 ] * fe2x;
					const auto a = fe1x * hx + fe1y * hy + fe1z * hz;

					if ( a > -1e-8f && a < 1e-8f )
					{
						continue;
					}

					const auto f = 1.0f / a;
					const auto sx = origin[ 0 ] - v0x, sy = origin[ 1 ] - v0y, sz = origin[ 2 ] - v0z;
					const auto u = f * ( sx * hx + sy * hy + sz * hz );

					if ( u < -0.01f || u > 1.01f )
					{
						continue;
					}

					const auto qx = sy * fe1z - sz * fe1y, qy = sz * fe1x - sx * fe1z, qz = sx * fe1y - sy * fe1x;
					const auto v = f * ( dir[ 0 ] * qx + dir[ 1 ] * qy + dir[ 2 ] * qz );

					if ( v < -0.01f || u + v > 1.02f )
					{
						continue;
					}

					const auto t = f * ( fe2x * qx + fe2y * qy + fe2z * qz );

					if ( t > 0.0f && t < closest_t )
					{
						closest_t = t;
						result.hit = true;
						result.fraction = t / max_dist;
						result.distance = t;
						result.triangle_index = ti;
						result.surface = tri.surface;
						result.normal = { nx * sign, ny * sign, nz * sign };

						result.end_pos =
						{
							start.x + dir[ 0 ] * t,
							start.y + dir[ 1 ] * t,
							start.z + dir[ 2 ] * t
						};
					}
				}
			}
			else if ( sp + 2 < 127 )
			{
				stack[ ++sp ] = node.right;
				stack[ ++sp ] = node.left;
			}
		}

		return result;
	}

	std::vector<bvh::hit_entry> bvh::trace_ray_all( const math::vector3& start, const math::vector3& end ) const
	{
		std::vector<hit_entry> hits{};

		if ( this->m_nodes.empty( ) )
		{
			return hits;
		}

		const auto dx = end.x - start.x;
		const auto dy = end.y - start.y;
		const auto dz = end.z - start.z;
		const auto max_dist = std::sqrt( dx * dx + dy * dy + dz * dz );

		if ( max_dist < 1e-8f )
		{
			return hits;
		}

		const auto inv_dist = 1.0f / max_dist;
		const float dir[ 3 ]{ dx * inv_dist, dy * inv_dist, dz * inv_dist };
		const float origin[ 3 ]{ start.x, start.y, start.z };
		const float inv_dir[ 3 ]{ std::abs( dir[ 0 ] ) > 1e-8f ? 1.0f / dir[ 0 ] : ( dir[ 0 ] >= 0 ? 1e12f : -1e12f ), std::abs( dir[ 1 ] ) > 1e-8f ? 1.0f / dir[ 1 ] : ( dir[ 1 ] >= 0 ? 1e12f : -1e12f ), std::abs( dir[ 2 ] ) > 1e-8f ? 1.0f / dir[ 2 ] : ( dir[ 2 ] >= 0 ? 1e12f : -1e12f ) };

		int stack[ 128 ]{};
		int sp{ 0 };
		stack[ 0 ] = 0;

		while ( sp >= 0 )
		{
			const auto& node = this->m_nodes[ stack[ sp-- ] ];
			if ( !node.bounds.intersects_ray( origin, inv_dir, max_dist ) )
			{
				continue;
			}

			if ( node.left == -1 )
			{
				for ( int i = node.tri_start; i < node.tri_start + node.tri_count; ++i )
				{
					const auto ti = this->m_indices[ i ];
					const auto& tri = this->m_triangles[ ti ];

					const auto e1x = tri.v1.x - tri.v0.x, e1y = tri.v1.y - tri.v0.y, e1z = tri.v1.z - tri.v0.z;
					const auto e2x = tri.v2.x - tri.v0.x, e2y = tri.v2.y - tri.v0.y, e2z = tri.v2.z - tri.v0.z;

					const auto hx = dir[ 1 ] * e2z - dir[ 2 ] * e2y;
					const auto hy = dir[ 2 ] * e2x - dir[ 0 ] * e2z;
					const auto hz = dir[ 0 ] * e2y - dir[ 1 ] * e2x;
					const auto a = e1x * hx + e1y * hy + e1z * hz;

					if ( a > -1e-8f && a < 1e-8f )
					{
						continue;
					}

					const auto f = 1.0f / a;
					const auto sx = origin[ 0 ] - tri.v0.x, sy = origin[ 1 ] - tri.v0.y, sz = origin[ 2 ] - tri.v0.z;
					const auto u = f * ( sx * hx + sy * hy + sz * hz );

					if ( u < 0.0f || u > 1.0f )
					{
						continue;
					}

					const auto qx = sy * e1z - sz * e1y, qy = sz * e1x - sx * e1z, qz = sx * e1y - sy * e1x;
					const auto v = f * ( dir[ 0 ] * qx + dir[ 1 ] * qy + dir[ 2 ] * qz );

					if ( v < 0.0f || u + v > 1.0f )
					{
						continue;
					}

					const auto t = f * ( e2x * qx + e2y * qy + e2z * qz );

					if ( t > 1e-5f && t < max_dist )
					{
						auto nx = e1y * e2z - e1z * e2y;
						auto ny = e1z * e2x - e1x * e2z;
						auto nz = e1x * e2y - e1y * e2x;
						const auto nl = std::sqrt( nx * nx + ny * ny + nz * nz );

						if ( nl > 1e-8f )
						{
							const auto inv_nl = 1.0f / nl;
							nx *= inv_nl;
							ny *= inv_nl;
							nz *= inv_nl;
						}

						const auto ndot = nx * dir[ 0 ] + ny * dir[ 1 ] + nz * dir[ 2 ];

						hit_entry hit{};
						hit.distance = t;
						hit.fraction = t / max_dist;
						hit.position = { origin[ 0 ] + dir[ 0 ] * t, origin[ 1 ] + dir[ 1 ] * t, origin[ 2 ] + dir[ 2 ] * t };
						hit.normal = { nx, ny, nz };
						hit.surface = tri.surface;
						hit.triangle_index = ti;
						hit.is_enter = ( ndot < 0.0f );

						hits.push_back( hit );
					}
				}
			}
			else if ( sp + 2 < 127 )
			{
				stack[ ++sp ] = node.right;
				stack[ ++sp ] = node.left;
			}
		}

		std::sort( hits.begin( ), hits.end( ), [ ]( const hit_entry& a, const hit_entry& b ) { return a.distance < b.distance; } );

		return hits;
	}

	std::vector<bvh::penetration_segment> bvh::build_segments( const std::vector<hit_entry>& hits, float ray_length ) const
	{
		std::vector<penetration_segment> segments;

		if ( hits.empty( ) )
		{
			return segments;
		}

		auto sorted = hits;

		for ( auto i = 1ull; i < sorted.size( ); ++i )
		{
			auto& prev = sorted[ i - 1 ];
			auto& curr = sorted[ i ];

			if ( !curr.is_enter && prev.is_enter && ( curr.fraction - prev.fraction ) * ray_length <= ( 1.0f / 512.0f ) )
			{
				std::swap( prev, curr );
			}
		}

		auto was_exit{ true };
		auto seg_enter_idx{ -1 };
		auto seg_enter_fraction{ 0.0f };

		for ( auto i = 0ull; i < sorted.size( ); ++i )
		{
			const auto& hit = sorted[ i ];
			const bool is_exit = !hit.is_enter;

			if ( is_exit != was_exit )
			{
				was_exit = is_exit;

				if ( !is_exit )
				{
					if ( seg_enter_idx >= 0 && i > 0 )
					{
						const auto& exit_hit = sorted[ i - 1 ];

						penetration_segment seg{};
						seg.enter_fraction = sorted[ seg_enter_idx ].fraction;
						seg.exit_fraction = exit_hit.fraction;
						seg.enter_distance = sorted[ seg_enter_idx ].distance;
						seg.exit_distance = exit_hit.distance;
						seg.enter_pos = sorted[ seg_enter_idx ].position;
						seg.exit_pos = exit_hit.position;
						seg.enter_surface = sorted[ seg_enter_idx ].surface;
						seg.exit_surface = exit_hit.surface;
						seg.thickness = exit_hit.distance - sorted[ seg_enter_idx ].distance;
						seg.min_pen_mod = sorted[ seg_enter_idx ].surface.penetration;

						if ( seg.thickness > 0.0f )
						{
							segments.push_back( seg );
						}
					}

					seg_enter_idx = static_cast< int >( i );
					seg_enter_fraction = hit.fraction;
				}
			}
		}

		if ( seg_enter_idx >= 0 )
		{
			const auto& enter_hit = sorted[ seg_enter_idx ];
			const auto& last_hit = sorted.back( );

			penetration_segment seg{};
			seg.enter_fraction = enter_hit.fraction;
			seg.exit_fraction = last_hit.fraction;
			seg.enter_distance = enter_hit.distance;
			seg.exit_distance = last_hit.distance;
			seg.enter_pos = enter_hit.position;
			seg.exit_pos = last_hit.position;
			seg.enter_surface = enter_hit.surface;
			seg.exit_surface = last_hit.surface;
			seg.thickness = last_hit.distance - enter_hit.distance;

			if ( seg.thickness < 1.0f )
			{
				seg.thickness = 1.0f;
			}

			seg.min_pen_mod = enter_hit.surface.penetration;

			segments.push_back( seg );
		}

		if ( segments.empty( ) && !sorted.empty( ) )
		{
			for ( auto i = 0ull; i + 1 < sorted.size( ); i += 2 )
			{
				penetration_segment seg{};
				seg.enter_fraction = sorted[ i ].fraction;
				seg.exit_fraction = sorted[ i + 1 ].fraction;
				seg.enter_distance = sorted[ i ].distance;
				seg.exit_distance = sorted[ i + 1 ].distance;
				seg.enter_pos = sorted[ i ].position;
				seg.exit_pos = sorted[ i + 1 ].position;
				seg.enter_surface = sorted[ i ].surface;
				seg.exit_surface = sorted[ i + 1 ].surface;
				seg.thickness = sorted[ i + 1 ].distance - sorted[ i ].distance;

				if ( seg.thickness < 1.0f )
				{
					seg.thickness = 1.0f;
				}

				seg.min_pen_mod = sorted[ i ].surface.penetration;
				segments.push_back( seg );
			}

			if ( sorted.size( ) % 2 == 1 )
			{
				const auto& h = sorted.back( );
				penetration_segment seg{};
				seg.enter_fraction = h.fraction;
				seg.exit_fraction = h.fraction;
				seg.enter_distance = h.distance;
				seg.exit_distance = h.distance + 1.0f;
				seg.enter_pos = h.position;
				seg.exit_pos = h.position;
				seg.enter_surface = h.surface;
				seg.exit_surface = h.surface;
				seg.thickness = 1.0f;
				seg.min_pen_mod = h.surface.penetration;
				segments.push_back( seg );
			}
		}

		return segments;
	}

	const std::vector<bvh::triangle>& bvh::triangles( ) const
	{
		return this->m_triangles;
	}

	std::size_t bvh::count( ) const
	{
		std::shared_lock lock( this->m_mutex );
		return this->m_triangles.size( );
	}

	bool bvh::valid( ) const
	{
		std::shared_lock lock( this->m_mutex );
		return !this->m_triangles.empty( );
	}

	void bvh::aabb::expand( const math::vector3& p )
	{
		if ( p.x < this->mins[ 0 ] )
		{
			this->mins[ 0 ] = p.x;
		}

		if ( p.y < this->mins[ 1 ] )
		{
			this->mins[ 1 ] = p.y;
		}

		if ( p.z < this->mins[ 2 ] )
		{
			this->mins[ 2 ] = p.z;
		}

		if ( p.x > this->maxs[ 0 ] )
		{
			this->maxs[ 0 ] = p.x;
		}

		if ( p.y > this->maxs[ 1 ] )
		{
			this->maxs[ 1 ] = p.y;
		}

		if ( p.z > this->maxs[ 2 ] )
		{
			this->maxs[ 2 ] = p.z;
		}
	}

	void bvh::aabb::expand( const aabb& o )
	{
		for ( int i = 0; i < 3; ++i )
		{
			if ( o.mins[ i ] < this->mins[ i ] )
			{
				this->mins[ i ] = o.mins[ i ];
			}

			if ( o.maxs[ i ] > this->maxs[ i ] )
			{
				this->maxs[ i ] = o.maxs[ i ];
			}
		}
	}

	int bvh::aabb::longest_axis( ) const
	{
		const auto ex = this->maxs[ 0 ] - this->mins[ 0 ];
		const auto ey = this->maxs[ 1 ] - this->mins[ 1 ];
		const auto ez = this->maxs[ 2 ] - this->mins[ 2 ];

		if ( ex >= ey && ex >= ez )
		{
			return 0;
		}

		if ( ey >= ez )
		{
			return 1;
		}

		return 2;
	}

	bool bvh::aabb::intersects_ray( const float origin[ 3 ], const float inv_dir[ 3 ], float max_t ) const
	{
		auto tmin{ 0.0f };
		auto tmax = max_t;

		for ( int i = 0; i < 3; ++i )
		{
			auto t0 = ( this->mins[ i ] - origin[ i ] ) * inv_dir[ i ];
			auto t1 = ( this->maxs[ i ] - origin[ i ] ) * inv_dir[ i ];

			if ( inv_dir[ i ] < 0.0f )
			{
				const auto tmp = t0;
				t0 = t1;
				t1 = tmp;
			}

			if ( t0 > tmin )
			{
				tmin = t0;
			}

			if ( t1 < tmax )
			{
				tmax = t1;
			}

			if ( tmax < tmin )
			{
				return false;
			}
		}

		return true;
	}

	void bvh::rebuild_accel( )
	{
		this->m_nodes.clear( );
		this->m_indices.clear( );
		this->m_tri_bounds.clear( );
		this->m_centroids.clear( );

		const auto tri_count = static_cast< int >( this->m_triangles.size( ) );
		if ( tri_count == 0 )
		{
			return;
		}

		this->m_indices.resize( tri_count );
		this->m_tri_bounds.resize( tri_count );
		this->m_centroids.resize( static_cast< std::size_t >( tri_count ) * 3 );

		for ( auto i = 0; i < tri_count; ++i )
		{
			this->m_indices[ i ] = i;

			aabb bb{};
			bb.expand( this->m_triangles[ i ].v0 );
			bb.expand( this->m_triangles[ i ].v1 );
			bb.expand( this->m_triangles[ i ].v2 );
			this->m_tri_bounds[ i ] = bb;

			const auto ci = static_cast< std::size_t >( i ) * 3;
			this->m_centroids[ ci ] = ( bb.mins[ 0 ] + bb.maxs[ 0 ] ) * 0.5f;
			this->m_centroids[ ci + 1 ] = ( bb.mins[ 1 ] + bb.maxs[ 1 ] ) * 0.5f;
			this->m_centroids[ ci + 2 ] = ( bb.mins[ 2 ] + bb.maxs[ 2 ] ) * 0.5f;
		}

		this->m_nodes.reserve( static_cast< std::size_t >( tri_count ) * 2 );
		this->build_recursive( 0, tri_count, 0 );
	}

	int bvh::build_recursive( int start, int end, int depth )
	{
		const auto node_idx = static_cast< int >( this->m_nodes.size( ) );
		this->m_nodes.push_back( {} );

		auto& node = this->m_nodes[ node_idx ];
		const auto count = end - start;

		for ( int i = start; i < end; ++i )
		{
			node.bounds.expand( this->m_tri_bounds[ this->m_indices[ i ] ] );
		}

		if ( count <= k_max_leaf_tris || depth >= k_max_depth )
		{
			node.tri_start = start;
			node.tri_count = count;
			return node_idx;
		}

		aabb centroid_bounds{};

		for ( auto i = start; i < end; ++i )
		{
			const auto ci = static_cast< std::size_t >( this->m_indices[ i ] ) * 3;
			centroid_bounds.expand( math::vector3{ this->m_centroids[ ci ], this->m_centroids[ ci + 1 ], this->m_centroids[ ci + 2 ] } );
		}

		const auto axis = centroid_bounds.longest_axis( );
		const auto mid = ( centroid_bounds.mins[ axis ] + centroid_bounds.maxs[ axis ] ) * 0.5f;

		auto partition_point = std::partition( this->m_indices.begin( ) + start, this->m_indices.begin( ) + end, [ & ]( int idx ) { return this->m_centroids[ static_cast< std::size_t >( idx ) * 3 + axis ] < mid; } );
		auto split = static_cast< int >( partition_point - this->m_indices.begin( ) );

		if ( split == start || split == end )
		{
			split = start + count / 2;
		}

		node.left = this->build_recursive( start, split, depth + 1 );
		this->m_nodes[ node_idx ].right = this->build_recursive( split, end, depth + 1 );

		return node_idx;
	}

} // namespace systems