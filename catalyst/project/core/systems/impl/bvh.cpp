#include <stdafx.hpp>

namespace systems {

	namespace detail {

		static constexpr std::size_t k_inner_node_size{ 32 };
		static constexpr std::size_t k_outer_node_size{ 48 };

		struct inner_node_t
		{
			float min[ 3 ];
			std::uint32_t packed0;
			float max[ 3 ];
			std::uint32_t packed1;

			[[nodiscard]] std::uint32_t type( ) const { return packed0 >> 30; }
			[[nodiscard]] std::uint32_t payload( ) const { return packed0 & 0x3FFFFFFFu; }
		};

		struct hedge_t
		{
			std::uint8_t next;
			std::uint8_t twin;
			std::uint8_t vert;
			std::uint8_t face;
		};

		struct quat_t { float x, y, z, w; };
		struct mat3_t { float m[ 3 ][ 3 ]; };

		static mat3_t quat_to_matrix( const quat_t& q )
		{
			const auto xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
			const auto xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
			const auto wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

			mat3_t m{};
			m.m[ 0 ][ 0 ] = 1 - 2 * ( yy + zz );
			m.m[ 0 ][ 1 ] = 2 * ( xy + wz );
			m.m[ 0 ][ 2 ] = 2 * ( xz - wy );
			m.m[ 1 ][ 0 ] = 2 * ( xy - wz );
			m.m[ 1 ][ 1 ] = 1 - 2 * ( xx + zz );
			m.m[ 1 ][ 2 ] = 2 * ( yz + wx );
			m.m[ 2 ][ 0 ] = 2 * ( xz + wy );
			m.m[ 2 ][ 1 ] = 2 * ( yz - wx );
			m.m[ 2 ][ 2 ] = 1 - 2 * ( xx + yy );
			return m;
		}

		static math::vector3 rotate_point( const mat3_t& m, const math::vector3& v )
		{
			return
			{
				m.m[ 0 ][ 0 ] * v.x + m.m[ 1 ][ 0 ] * v.y + m.m[ 2 ][ 0 ] * v.z,
				m.m[ 0 ][ 1 ] * v.x + m.m[ 1 ][ 1 ] * v.y + m.m[ 2 ][ 1 ] * v.z,
				m.m[ 0 ][ 2 ] * v.x + m.m[ 1 ][ 2 ] * v.y + m.m[ 2 ][ 2 ] * v.z
			};
		}

		static math::vector3 transform_point( const mat3_t& rot, const float scale[ 3 ], const float pos[ 3 ], const math::vector3& local )
		{
			const auto scaled = math::vector3{ local.x * scale[ 0 ], local.y * scale[ 1 ], local.z * scale[ 2 ] };
			const auto rotated = rotate_point( rot, scaled );
			return { rotated.x + pos[ 0 ], rotated.y + pos[ 1 ], rotated.z + pos[ 2 ] };
		}

		static bool extract_mesh( std::uintptr_t bvh_ptr, std::uintptr_t vert_ptr, std::uintptr_t tri_ptr, std::uint32_t node_count, const mat3_t& rot, const float scale[ 3 ], const float pos[ 3 ], std::uintptr_t mat_arr_ptr, std::int32_t mat_count, const std::vector<bvh::global_surface_entry>& global_table, const bvh::surface_info& default_surface, std::vector<bvh::triangle>& out )
		{
			if ( !bvh_ptr || !vert_ptr || !tri_ptr || node_count == 0 || node_count > 0x1000000 )
			{
				return false;
			}

			std::vector<std::uint8_t> bvh_buf( static_cast< std::size_t >( node_count ) * k_inner_node_size );
			g::memory.read( bvh_ptr, bvh_buf.data( ), bvh_buf.size( ) );

			std::uint32_t min_tri = UINT32_MAX, max_tri = 0;
			std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
			std::vector<std::uint32_t> stack;
			stack.reserve( 256 );

			std::uint32_t cursor{ 0 };

			while ( true )
			{
				if ( cursor >= node_count )
				{
					if ( stack.empty( ) )
					{
						break;
					}

					cursor = stack.back( );
					stack.pop_back( );
					continue;
				}

				const auto node = reinterpret_cast< const inner_node_t* >( bvh_buf.data( ) + static_cast< std::size_t >( cursor ) * k_inner_node_size );
				const auto type = node->type( );
				const auto payload = node->payload( );

				if ( type == 3 )
				{
					if ( payload > 0 && payload < 0x1000000 )
					{
						ranges.push_back( { node->packed1, payload } );

						if ( node->packed1 < min_tri )
						{
							min_tri = node->packed1;
						}

						if ( node->packed1 + payload > max_tri )
						{
							max_tri = node->packed1 + payload;
						}
					}

					if ( stack.empty( ) )
					{
						break;
					}

					cursor = stack.back( );
					stack.pop_back( );
				}
				else
				{
					if ( payload == 0 )
					{
						if ( stack.empty( ) )
						{
							break;
						}

						cursor = stack.back( );
						stack.pop_back( );
						continue;
					}

					if ( cursor + payload < node_count )
					{
						stack.push_back( cursor + payload );
					}

					cursor++;
				}
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

			std::vector<std::int32_t> indices( total_tris * 3 );
			g::memory.read( tri_ptr + static_cast< std::uintptr_t >( min_tri ) * 12, indices.data( ), total_tris * 12 );

			std::int32_t max_vert{ 0 };
			for ( const auto idx : indices )
			{
				if ( idx > max_vert )
				{
					max_vert = idx;
				}
			}

			if ( max_vert <= 0 || max_vert > 0x1000000 )
			{
				return false;
			}

			const auto vert_count = static_cast< std::uint32_t >( max_vert + 1 );
			std::vector<float> vertices( vert_count * 3 );
			g::memory.read( vert_ptr, vertices.data( ), static_cast< std::size_t >( vert_count ) * 12 );

			const bool has_materials = mat_arr_ptr > 0x10000 && mat_count > 0;
			std::vector<std::uint8_t> materials;

			if ( has_materials )
			{
				materials.resize( total_tris );
				g::memory.read( mat_arr_ptr + static_cast< std::uintptr_t >( min_tri ), materials.data( ), total_tris );
			}

			const auto global_count = static_cast< int >( global_table.size( ) );
			const auto before = out.size( );

			for ( const auto& [start, count] : ranges )
			{
				for ( std::uint32_t i = 0; i < count; ++i )
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

					auto xf = [ & ]( std::int32_t vi ) -> math::vector3 { return transform_point( rot, scale, pos, { vertices[ vi * 3 ], vertices[ vi * 3 + 1 ], vertices[ vi * 3 + 2 ] } ); };

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

			const auto vert_count = *reinterpret_cast< const std::int32_t* >( hd + 0x88 );
			const auto vert_ptr = *reinterpret_cast< const std::uintptr_t* >( hd + 0x90 );
			const auto hedge_count = *reinterpret_cast< const std::int32_t* >( hd + 0xa0 );
			const auto hedge_ptr = *reinterpret_cast< const std::uintptr_t* >( hd + 0xa8 );
			const auto face_count = *reinterpret_cast< const std::int32_t* >( hd + 0xb8 );
			const auto face_ptr = *reinterpret_cast< const std::uintptr_t* >( hd + 0xc0 );

			if ( vert_count <= 0 || vert_count > 0xffff )
			{
				return false;
			}

			if ( hedge_count <= 0 || hedge_count > 0xffff )
			{
				return false;
			}

			if ( face_count <= 0 || face_count > 0xffff )
			{
				return false;
			}

			if ( !vert_ptr || !hedge_ptr || !face_ptr )
			{
				return false;
			}

			std::vector<float> verts( vert_count * 3 );
			g::memory.read( vert_ptr, verts.data( ), static_cast< std::size_t >( vert_count ) * 12 );

			std::vector<hedge_t> hedges( hedge_count );
			g::memory.read( hedge_ptr, hedges.data( ), static_cast< std::size_t >( hedge_count ) * 4 );

			std::vector<std::uint8_t> faces( face_count );
			g::memory.read( face_ptr, faces.data( ), face_count );

			const auto before = out.size( );

			for ( int fi = 0; fi < face_count; ++fi )
			{
				const auto start_he = faces[ fi ];
				if ( start_he >= hedge_count )
				{
					continue;
				}

				std::vector<int> face_verts;
				face_verts.reserve( 8 );

				auto he = start_he;
				auto safety{ 0 };

				do
				{
					if ( he >= hedge_count )
					{
						break;
					}

					face_verts.push_back( hedges[ he ].vert );
					he = hedges[ he ].next;
				} while ( he != start_he && ++safety < 64 );

				if ( face_verts.size( ) < 3 )
				{
					continue;
				}

				auto vert = [ & ]( int vi ) -> math::vector3
					{
						if ( vi < 0 || vi >= vert_count )
						{
							return {};
						}

						return
						{
							verts[ vi * 3 ] * uniform_scale,
							verts[ vi * 3 + 1 ] * uniform_scale,
							verts[ vi * 3 + 2 ] * uniform_scale
						};
					};

				const auto v0 = vert( face_verts[ 0 ] );

				for ( std::size_t i = 1; i + 1 < face_verts.size( ); ++i )
				{
					out.push_back( { .v0 = v0, .v1 = vert( face_verts[ i ] ), .v2 = vert( face_verts[ i + 1 ] ), .surface = surface } );
				}
			}

			return out.size( ) > before;
		}

		static void process_shape( std::uintptr_t shape_body, std::uintptr_t hull_vtable, std::uintptr_t mesh_vtable, const std::vector<bvh::global_surface_entry>& global_table, std::vector<bvh::triangle>& out )
		{
			const auto vtable = g::memory.read<std::uintptr_t>( shape_body );

			if ( vtable == hull_vtable )
			{
				const auto hull_data = g::memory.read<std::uintptr_t>( shape_body + 0xb8 );
				if ( hull_data > 0x10000 && hull_data < 0x7fffffffffff )
				{
					const auto scale = g::memory.read<float>( shape_body + 0xb0 );

					bvh::surface_info hull_surface{};
					hull_surface.penetration = g::memory.read<float>( shape_body + 0x28 );

					extract_hull( hull_data, ( scale > 0.0f && std::isfinite( scale ) ) ? scale : 1.0f, hull_surface, out );
				}

				return;
			}

			if ( vtable != mesh_vtable )
			{
				return;
			}

			const auto mesh_data = g::memory.read<std::uintptr_t>( shape_body + 0xc0 );
			if ( !mesh_data )
			{
				return;
			}

			bvh::surface_info default_surface{};
			default_surface.penetration = g::memory.read<float>( shape_body + 0x28 );

			const auto default_damage = g::memory.read<float>( shape_body + 0x2c );
			if ( default_damage < 0.0f )
			{
				return;
			}

			std::uint8_t md[ 0xA0 ]{};
			g::memory.read( mesh_data, md, sizeof( md ) );

			const auto mat_count = *reinterpret_cast< const std::int32_t* >( md + 0x90 );
			const auto mat_arr_ptr = *reinterpret_cast< const std::uintptr_t* >( md + 0x98 );
			const bool has_materials = mat_arr_ptr > 0x10000 && mat_count > 0;

			float scale[ 3 ]{};
			g::memory.read( shape_body + 0xB0, scale, 12 );

			if ( scale[ 0 ] == 0.0f && scale[ 1 ] == 0.0f && scale[ 2 ] == 0.0f )
			{
				return;
			}

			if ( !std::isfinite( scale[ 0 ] ) || !std::isfinite( scale[ 1 ] ) || !std::isfinite( scale[ 2 ] ) )
			{
				return;
			}

			float world_pos[ 3 ]{};
			g::memory.read( shape_body + 0x100, world_pos, 12 );

			detail::quat_t quat{};
			g::memory.read( shape_body + 0x130, &quat, sizeof( quat ) );

			const auto ql = quat.x * quat.x + quat.y * quat.y + quat.z * quat.z + quat.w * quat.w;
			if ( ql < 0.5f || ql > 1.5f )
			{
				quat = { 0, 0, 0, 1 };
			}

			const auto rot = quat_to_matrix( quat );
			const auto bvh_ptr = *reinterpret_cast< const std::uintptr_t* >( md + 0x20 );
			const auto vert_ptr = *reinterpret_cast< const std::uintptr_t* >( md + 0x38 );
			const auto tri_ptr = *reinterpret_cast< const std::uintptr_t* >( md + 0x50 );

			auto node_count{ 0u };
			for ( auto c : { *reinterpret_cast< const std::int32_t* >( md + 0x28 ), *reinterpret_cast< const std::int32_t* >( md + 0x30 ), *reinterpret_cast< const std::int32_t* >( md + 0x48 ), *reinterpret_cast< const std::int32_t* >( md + 0x58 ) } )
			{
				if ( c > 0 && c < 0x1000000 )
				{
					node_count = static_cast< std::uint32_t >( c );
					break;
				}
			}

			if ( node_count > 0 )
			{
				extract_mesh( bvh_ptr, vert_ptr, tri_ptr, node_count, rot, scale, world_pos, mat_arr_ptr, mat_count, global_table, default_surface, out );
			}
		}
		
	} // namespace detail

	void bvh::parse( )
	{
		const auto trace_against_entities_call = g::memory.find_pattern( g::modules.client, "E8 ? ? ? ? C7 87 ? ? ? ? ? ? ? ? 48 8D 54 24 ? 48 8B CF" );
		g::console.print("trace_against_entities_call: {:x}", trace_against_entities_call);
		const auto vphys2_world_global = g::memory.read<std::uintptr_t>( g::memory.resolve_rip( trace_against_entities_call - 0x1a ) );
		g::console.print("vphys2_world_global: {:x}", vphys2_world_global);
		const auto vphys2_world = g::memory.read<std::uintptr_t>( vphys2_world_global );
		g::console.print("vphys2_world: {:x}", vphys2_world);

		if ( !vphys2_world )
		{
			return;
		}

		const auto get_surface_data_from_handle_fn = g::memory.find_pattern( g::modules.client, "48 63 41 ? 48 8B 0D" );
		const auto surface_manager = g::memory.read<std::uintptr_t>( g::memory.resolve_rip( get_surface_data_from_handle_fn + 4 ) );
		g::console.print("surface_manager: {:x}", surface_manager);
		if ( !surface_manager )
		{
			return;
		}

		std::vector<global_surface_entry> global_table;
		{
			const auto array_base = g::memory.read<std::uintptr_t>( surface_manager + 40 );
			if ( array_base )
			{
				std::int32_t surface_count{ 0 };

				for ( const auto off : { 32, 36, 24, 28, 48 } )
				{
					const auto candidate = g::memory.read<std::int32_t>( surface_manager + off );
					if ( candidate > 0 )
					{
						surface_count = candidate;
						break;
					}
				}
				g::console.print("surface_count: {}", surface_count);
				if ( surface_count <= 0 )
				{
					for ( int i = 0; i < 1024; ++i )
					{
						global_surface_entry sd{};
						g::memory.read( array_base + static_cast< std::size_t >( i ) * 32, &sd, sizeof( sd ) );

						if ( sd.penetration_mod == 0.0f && sd.surface_type == 0 && sd.unk_00 == 0.0f )
						{
							if ( surface_count > 0 && i - surface_count > 8 )
							{
								break;
							}

							continue;
						}

						surface_count = i + 1;
					}
				}

				if ( surface_count )
				{
					global_table.resize( surface_count );
					g::memory.read( array_base, global_table.data( ), static_cast< std::size_t >( surface_count ) * sizeof( global_surface_entry ) );
				}
			}
		}

		const auto inner_world = g::memory.read<std::uintptr_t>( vphys2_world + 0x30 );
		if ( !inner_world )
		{
			return;
		}

		const auto body_array = g::memory.read<std::uintptr_t>( inner_world + 0x110 );
		if ( !body_array )
		{
			return;
		}

		const auto body_count = g::memory.read<std::int32_t>( body_array + 0x268 );
		if ( !body_count )
		{
			return;
		}

		const auto hull_vtable = g::memory.find_vtable( g::modules.vphysics2, "CRnHullShape" );
		const auto mesh_vtable = g::memory.find_vtable( g::modules.vphysics2, "CRnMeshShape" );

		if ( !hull_vtable || !mesh_vtable )
		{
			return;
		}

		std::vector<triangle> fresh;
		fresh.reserve( 262144 );

		for ( std::int32_t body_idx = 0; body_idx < body_count; ++body_idx )
		{
			const auto body = body_array + static_cast< std::uintptr_t >( body_idx ) * 88;
			const auto bvh_root = g::memory.read<std::int32_t>( body );
			const auto bvh_nodes_ptr = g::memory.read<std::uintptr_t>( body + 0x18 );

			if ( !bvh_nodes_ptr )
			{
				continue;
			}

			const auto femboys = g::memory.read<std::uint32_t>( body + 0x40 );
			if ( femboys != 2 )
			{
				continue;
			}

			if ( bvh_root >= 0 )
			{
				const auto count_a = static_cast< std::uint32_t >( bvh_root + 1 );
				const auto count_b = static_cast< std::uint32_t >( g::memory.read<std::int32_t>( body + 0x08 ) );
				const auto count_c = static_cast< std::uint32_t >( g::memory.read<std::int32_t>( body + 0x10 ) );
				const auto outer_node_count = std::max( { count_a, count_b, count_c } );

				if ( outer_node_count > 0x100000 )
				{
					continue;
				}

				std::vector<std::uint8_t> outer_buf( outer_node_count * detail::k_outer_node_size );
				g::memory.read( bvh_nodes_ptr, outer_buf.data( ), outer_buf.size( ) );

				std::vector<std::uintptr_t> leaves;
				leaves.reserve( 256 );

				std::vector<std::int32_t> outer_stack;
				outer_stack.reserve( 128 );
				outer_stack.push_back( bvh_root );

				while ( !outer_stack.empty( ) )
				{
					const auto idx = outer_stack.back( );
					outer_stack.pop_back( );

					if ( idx < 0 || static_cast< std::uint32_t >( idx ) >= outer_node_count )
					{
						continue;
					}

					const auto node = outer_buf.data( ) + static_cast< std::uintptr_t >( idx ) * detail::k_outer_node_size;
					const auto left = *reinterpret_cast< const std::int32_t* >( node + 12 );

					if ( left == -1 )
					{
						const auto shape_ptr = *reinterpret_cast< const std::uintptr_t* >( node + 0x28 );
						if ( shape_ptr )
						{
							leaves.push_back( shape_ptr );
						}
					}
					else
					{
						const auto right = *reinterpret_cast< const std::int32_t* >( node + 28 );
						if ( left >= 0 )
						{
							outer_stack.push_back( left );
						}

						if ( right >= 0 )
						{
							outer_stack.push_back( right );
						}
					}
				}

				std::unordered_set<std::uintptr_t> seen;

				for ( const auto shape : leaves )
				{
					if ( seen.count( shape ) )
					{
						continue;
					}

					seen.insert( shape );
					detail::process_shape( shape, hull_vtable, mesh_vtable, global_table, fresh );
				}
			}
			else
			{
				const auto shape = g::memory.read<std::uintptr_t>( body + 0x28 );
				if ( shape )
				{
					detail::process_shape( shape, hull_vtable, mesh_vtable, global_table, fresh );
				}
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

	bvh::trace_result bvh::trace_ray( const math::vector3& start, const math::vector3& end, std::int32_t exclude_tri ) const
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

		std::int32_t stack[ 128 ]{};
		std::int32_t sp{ 0 };
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
				for ( std::int32_t i = node.tri_start; i < node.tri_start + node.tri_count; ++i )
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

	std::vector<bvh::hit_entry> bvh::trace_ray_all( const math::vector3& start, const math::vector3& end ) const
	{
		std::vector<hit_entry> hits;

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

		std::int32_t stack[ 128 ]{};
		std::int32_t sp{ 0 };
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
				for ( std::int32_t i = node.tri_start; i < node.tri_start + node.tri_count; ++i )
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

		for ( std::size_t i = 1; i < sorted.size( ); ++i )
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

		for ( std::size_t i = 0; i < sorted.size( ); ++i )
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
			for ( std::size_t i = 0; i + 1 < sorted.size( ); i += 2 )
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

		const auto tri_count = static_cast< std::int32_t >( this->m_triangles.size( ) );
		if ( tri_count == 0 )
		{
			return;
		}

		this->m_indices.resize( tri_count );
		this->m_tri_bounds.resize( tri_count );
		this->m_centroids.resize( static_cast< std::size_t >( tri_count ) * 3 );

		for ( std::int32_t i = 0; i < tri_count; ++i )
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

	std::int32_t bvh::build_recursive( std::int32_t start, std::int32_t end, std::int32_t depth )
	{
		const auto node_idx = static_cast< std::int32_t >( this->m_nodes.size( ) );
		this->m_nodes.push_back( {} );

		auto& node = this->m_nodes[ node_idx ];
		const auto count = end - start;

		for ( std::int32_t i = start; i < end; ++i )
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
		for ( std::int32_t i = start; i < end; ++i )
		{
			const auto ci = static_cast< std::size_t >( this->m_indices[ i ] ) * 3;
			centroid_bounds.expand( math::vector3{ this->m_centroids[ ci ], this->m_centroids[ ci + 1 ], this->m_centroids[ ci + 2 ] } );
		}

		const auto axis = centroid_bounds.longest_axis( );
		const auto mid = ( centroid_bounds.mins[ axis ] + centroid_bounds.maxs[ axis ] ) * 0.5f;

		auto partition_point = std::partition( this->m_indices.begin( ) + start, this->m_indices.begin( ) + end, [ & ]( std::int32_t idx ) { return this->m_centroids[ static_cast< std::size_t >( idx ) * 3 + axis ] < mid; } );
		auto split = static_cast< std::int32_t >( partition_point - this->m_indices.begin( ) );

		if ( split == start || split == end )
		{
			split = start + count / 2;
		}

		node.left = this->build_recursive( start, split, depth + 1 );
		this->m_nodes[ node_idx ].right = this->build_recursive( split, end, depth + 1 );

		return node_idx;
	}

} // namespace systems