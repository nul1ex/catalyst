#include <stdafx.hpp>

namespace {

	inline std::uint32_t compact1( std::uint32_t v )
	{
		v &= 0x09249249U;
		v = ( v ^ ( v >> 2 ) ) & 0x030C30C3U;
		v = ( v ^ ( v >> 4 ) ) & 0x0300F00FU;
		v = ( v ^ ( v >> 8 ) ) & 0x00000FFFU;
		return v;
	}

	inline void morton3_decode( std::uint32_t m, int& x, int& y, int& z )
	{
		x = static_cast< int >( compact1( m ) );
		y = static_cast< int >( compact1( m >> 1 ) );
		z = static_cast< int >( compact1( m >> 2 ) );
	}

	inline bool segment_aabb_intersect( const math::vector3& p0, const math::vector3& p1, const math::vector3& bmin, const math::vector3& bmax )
	{
		math::vector3 d = { p1.x - p0.x, p1.y - p0.y, p1.z - p0.z };
		float t0 = 0.0f;
		float t1 = 1.0f;

		auto check_axis = [ & ] ( float p0a, float da, float min_a, float max_a ) -> bool
			{
				const float epsilon = 1e-6f;
				if ( std::abs( da ) < epsilon )
				{
					return p0a >= min_a && p0a <= max_a;
				}

				float inv = 1.0f / da;
				float t_near = ( min_a - p0a ) * inv;
				float t_far = ( max_a - p0a ) * inv;

				if ( t_near > t_far )
				{
					std::swap( t_near, t_far );
				}

				t0 = std::max( t0, t_near );
				t1 = std::min( t1, t_far );

				return t0 <= t1;
			};

		if ( !check_axis( p0.x, d.x, bmin.x, bmax.x ) )
		{
			return false;
		}

		if ( !check_axis( p0.y, d.y, bmin.y, bmax.y ) )
		{
			return false;
		}

		if ( !check_axis( p0.z, d.z, bmin.z, bmax.z ) )
		{
			return false;
		}

		return true;
	}

} // namespace

namespace systems {

	void voxels::update( )
	{
		this->m_active_voxels.clear( );

		const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );

		if ( !global_vars )
		{
			return;
		}

		const auto current_tick = g::memory.read<std::int32_t>( global_vars + 0x44 );
		const auto projectiles = systems::g_collector.projectiles( );

		for ( const auto& proj : projectiles )
		{
			if ( proj.subtype != collector::projectile_subtype::smoke_grenade || !proj.smoke_active )
			{
				continue;
			}

			const auto begin_tick = g::memory.read<std::int32_t>( proj.entity + SCHEMA( "C_SmokeGrenadeProjectile", "m_nSmokeEffectTickBegin"_hash ) );

			if ( std::abs( current_tick - begin_tick ) > 1400 )
			{
				continue;
			}

			constexpr std::uint64_t embedded_off = 0x14A8;
			constexpr std::uint64_t state_ptr_off = 0x70;
			constexpr std::uint64_t rgba_ptr_off = 0xE0;
			constexpr std::uint64_t center_off = 0xE8;
			constexpr std::uint64_t frame_index_off = 0x100;
			constexpr std::uint64_t bitset_base = 4104;

			const std::uintptr_t wrap = proj.entity + embedded_off;

			const auto state = g::memory.read<std::uint64_t>( wrap + state_ptr_off );
			const auto rgba = g::memory.read<std::uint64_t>( wrap + rgba_ptr_off );
			const auto center = g::memory.read<math::vector3>( wrap + center_off );
			const auto frame = g::memory.read<std::int32_t>( wrap + frame_index_off );

			if ( !state || !rgba || state < 0x10000 || rgba < 0x10000 )
			{
				continue;
			}

			if ( frame < 0 || frame > 1024 )
			{
				continue;
			}

			if ( center.x == 0.0f && center.y == 0.0f && center.z == 0.0f )
			{
				continue;
			}

			const std::uintptr_t current_bitset_base = state + bitset_base + ( 4096ULL * static_cast< std::uint64_t >( frame ) );

			std::array<std::uint64_t, 512> bitset{};

			if ( !g::memory.read( current_bitset_base, bitset.data( ), sizeof( bitset ) ) )
			{
				continue;
			}

			auto vol_data = std::make_unique<std::uint8_t[ ]>( 131072 );

			if ( !g::memory.read( rgba, vol_data.get( ), 131072 ) )
			{
				continue;
			}

			constexpr int n_vox = 32;
			constexpr float size_scale = 20.0f;
			constexpr float half_scale = 16.0f;

			for ( std::uint32_t w = 0; w < 512; ++w )
			{
				std::uint64_t bits = bitset[ w ];

				while ( bits )
				{
					const std::uint32_t bit_idx = static_cast< std::uint32_t >( std::countr_zero( bits ) );
					bits &= ( bits - 1 );

					const std::uint32_t morton = ( w << 6 ) | bit_idx;

					int x = 0;
					int y = 0;
					int z = 0;

					morton3_decode( morton, x, y, z );

					const int lin = x + n_vox * ( y + n_vox * z );
					const size_t c_idx = static_cast< size_t >( lin ) * 4;

					const std::uint8_t r = vol_data[ c_idx + 0 ];
					const std::uint8_t g_col = vol_data[ c_idx + 1 ];
					const std::uint8_t bl = vol_data[ c_idx + 2 ];
					const std::uint8_t a = vol_data[ c_idx + 3 ];

					const math::vector3 pos = {
						center.x + ( static_cast< float >( x ) - half_scale ) * size_scale,
						center.y + ( static_cast< float >( y ) - half_scale ) * size_scale,
						center.z + ( static_cast< float >( z ) - half_scale ) * size_scale
					};

					this->m_active_voxels.push_back( { x, y, z, r, g_col, bl, a, pos } );
				}
			}
		}
	}

	bool voxels::line_goes_through_smoke( const math::vector3& start, const math::vector3& end ) const
	{
		if ( this->m_active_voxels.empty( ) )
		{
			return false;
		}

		const float half_size = 11.0f; // Slightly larger to close gaps

		for ( const auto& v : this->m_active_voxels )
		{
			const math::vector3 bmin = { v.world.x - half_size, v.world.y - half_size, v.world.z - half_size };
			const math::vector3 bmax = { v.world.x + half_size, v.world.y + half_size, v.world.z + half_size };

			if ( segment_aabb_intersect( start, end, bmin, bmax ) )
			{
				return true;
			}
		}

		return false;
	}

} // namespace systems
