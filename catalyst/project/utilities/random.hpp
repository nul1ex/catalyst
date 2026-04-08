#pragma once

namespace random {

	class valve_rng
	{
	public:
		void seed( int s )
		{
			this->m_state = -std::abs( s );
			this->m_index = 0;
			this->m_seeded = false;
		}

		int generate( )
		{
			if ( !this->m_seeded )
			{
				auto v = -this->m_state;
				if ( v < 1 )
				{
					v = 1;
				}

				for ( int j = 39; j >= 0; --j )
				{
					v = this->lcg( v );

					if ( j < 32 )
					{
						this->m_table[ j ] = v;
					}
				}

				this->m_state = v;
				this->m_index = this->m_table[ 0 ];
				this->m_seeded = true;
			}

			this->m_state = this->lcg( this->m_state );
			const auto idx = this->m_index / 0x4000000;
			this->m_index = this->m_table[ idx ];
			this->m_table[ idx ] = this->m_state;
			return this->m_index;
		}

		float random_float( float min = 0.0f, float max = 1.0f )
		{
			const auto raw = this->generate( );
			const auto norm = std::fminf( 0.99999988f, static_cast< float >( raw ) * 4.6566129e-10f );
			return min + norm * ( max - min );
		}

		int get_state( ) const { return this->m_state; }
		int get_index( ) const { return this->m_index; }
		int get_table_entry( int i ) const { return this->m_table[ i ]; }

	private:
		static int lcg( int state )
		{
			const auto k = state / 127773;
			auto result = 16807 * ( state - k * 127773 ) - 2836 * k;

			if ( result < 0 )
			{
				result += 2147483647;
			}

			return result;
		}

		int m_state{ 0 };
		int m_index{ 0 };
		int m_table[ 32 ]{};
		bool m_seeded{ false };
	};

	class sha1
	{
	public:
		void reset( )
		{
			this->m_state[ 0 ] = 0x67452301;
			this->m_state[ 1 ] = 0xEFCDAB89;
			this->m_state[ 2 ] = 0x98BADCFE;
			this->m_state[ 3 ] = 0x10325476;
			this->m_state[ 4 ] = 0xC3D2E1F0;
			this->m_count = 0;
		}

		void update( const void* data, std::size_t len )
		{
			const auto bytes = static_cast< const std::uint8_t* >( data );
			auto index = static_cast< std::size_t >( this->m_count & 63 );

			this->m_count += len;

			std::size_t i{ 0 };

			if ( index )
			{
				auto part_len = 64 - index;

				if ( len >= part_len )
				{
					std::memcpy( this->m_buffer + index, bytes, part_len );
					this->transform( this->m_buffer );
					i = part_len;
				}
				else
				{
					std::memcpy( this->m_buffer + index, bytes, len );
					return;
				}
			}

			for ( ; i + 64 <= len; i += 64 )
			{
				this->transform( bytes + i );
			}

			if ( i < len )
			{
				std::memcpy( this->m_buffer, bytes + i, len - i );
			}
		}

		void final( )
		{
			std::uint8_t padding[ 64 ]{};
			padding[ 0 ] = 0x80;

			auto index = static_cast< std::size_t >( this->m_count & 63 );
			auto pad_len = ( index < 56 ) ? ( 56 - index ) : ( 120 - index );
			auto bit_count = this->m_count * 8;

			this->update( padding, pad_len );

			std::uint8_t bits[ 8 ]{};

			for ( int i = 0; i < 8; ++i )
			{
				bits[ 7 - i ] = static_cast< std::uint8_t >( bit_count >> ( i * 8 ) );
			}

			this->update( bits, 8 );

			for ( int i = 0; i < 5; ++i )
			{
				this->m_digest[ i * 4 + 0 ] = static_cast< std::uint8_t >( this->m_state[ i ] >> 24 );
				this->m_digest[ i * 4 + 1 ] = static_cast< std::uint8_t >( this->m_state[ i ] >> 16 );
				this->m_digest[ i * 4 + 2 ] = static_cast< std::uint8_t >( this->m_state[ i ] >> 8 );
				this->m_digest[ i * 4 + 3 ] = static_cast< std::uint8_t >( this->m_state[ i ] );
			}
		}

		std::uint32_t get_first_uint32( ) const
		{
			std::uint32_t result;
			std::memcpy( &result, this->m_digest, sizeof( result ) );
			return result;
		}

	private:
		static std::uint32_t rotl( std::uint32_t v, int n )
		{
			return ( v << n ) | ( v >> ( 32 - n ) );
		}

		void transform( const std::uint8_t* block )
		{
			std::uint32_t w[ 80 ]{};

			for ( int i = 0; i < 16; ++i )
			{
				w[ i ] = static_cast< std::uint32_t >( block[ i * 4 ] ) << 24 | static_cast< std::uint32_t >( block[ i * 4 + 1 ] ) << 16 | static_cast< std::uint32_t >( block[ i * 4 + 2 ] ) << 8 | static_cast< std::uint32_t >( block[ i * 4 + 3 ] );
			}

			for ( int i = 16; i < 80; ++i )
			{
				w[ i ] = this->rotl( w[ i - 3 ] ^ w[ i - 8 ] ^ w[ i - 14 ] ^ w[ i - 16 ], 1 );
			}

			auto a = this->m_state[ 0 ];
			auto b = this->m_state[ 1 ];
			auto c = this->m_state[ 2 ];
			auto d = this->m_state[ 3 ];
			auto e = this->m_state[ 4 ];

			for ( int i = 0; i < 80; ++i )
			{
				std::uint32_t f, k;

				if ( i < 20 )
				{
					f = ( b & c ) | ( ( ~b ) & d );
					k = 0x5a827999;
				}
				else if ( i < 40 )
				{
					f = b ^ c ^ d;
					k = 0x6ed9eba1;
				}
				else if ( i < 60 )
				{
					f = ( b & c ) | ( b & d ) | ( c & d );
					k = 0x8f1bbcdc;
				}
				else
				{
					f = b ^ c ^ d;
					k = 0xca62c1d6;
				}

				auto temp = this->rotl( a, 5 ) + f + e + k + w[ i ];
				e = d;
				d = c;
				c = this->rotl( b, 30 );
				b = a;
				a = temp;
			}

			this->m_state[ 0 ] += a;
			this->m_state[ 1 ] += b;
			this->m_state[ 2 ] += c;
			this->m_state[ 3 ] += d;
			this->m_state[ 4 ] += e;
		}

		std::uint32_t m_state[ 5 ]{};
		std::uint64_t m_count{ 0 };
		std::uint8_t m_buffer[ 64 ]{};
		std::uint8_t m_digest[ 20 ]{};
	};

} // namespace random