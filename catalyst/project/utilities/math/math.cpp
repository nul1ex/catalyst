#include <stdafx.hpp>

namespace math {

	vector3 vector3::operator+( const vector3& v ) const noexcept { return { x + v.x, y + v.y, z + v.z }; }
	vector3 vector3::operator-( const vector3& v ) const noexcept { return { x - v.x, y - v.y, z - v.z }; }
	vector3 vector3::operator*( float scalar ) const noexcept { return { x * scalar, y * scalar, z * scalar }; }
	vector3 vector3::operator/( float scalar ) const noexcept { return { x / scalar, y / scalar, z / scalar }; }
	vector3 vector3::operator-( ) const noexcept { return { -x, -y, -z }; }

	vector3& vector3::operator*=( float scalar ) noexcept { x *= scalar; y *= scalar; z *= scalar; return *this; }
	vector3& vector3::operator/=( float scalar ) noexcept { x /= scalar; y /= scalar; z /= scalar; return *this; }
	vector3& vector3::operator+=( const vector3& v ) noexcept { x += v.x; y += v.y; z += v.z; return *this; }
	vector3& vector3::operator-=( const vector3& v ) noexcept { x -= v.x; y -= v.y; z -= v.z; return *this; }

	bool vector3::operator==( const vector3& v ) const noexcept { return x == v.x && y == v.y && z == v.z; }
	bool vector3::operator!=( const vector3& v ) const noexcept { return !( *this == v ); }

	float vector3::dot( const vector3& v ) const noexcept { return x * v.x + y * v.y + z * v.z; }
	vector3 vector3::cross( const vector3& v ) const noexcept { return { y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x }; }

	float vector3::length_sqr( ) const noexcept { return x * x + y * y + z * z; }

	float vector3::length( ) const noexcept
	{
		const auto len_sq = this->length_sqr( );
		if ( len_sq == 0.0f ) [[unlikely]]
		{
			return 0.0f;
		}

		return std::sqrtf( len_sq );
	}

	float vector3::length_2d( ) const noexcept
	{
		const auto len_sq_2d = x * x + y * y;
		if ( len_sq_2d == 0.0f ) [[unlikely]]
		{
			return 0.0f;
		}

		return std::sqrtf( len_sq_2d );
	}

	vector3& vector3::normalize( ) noexcept
	{
		const auto len = this->length( );
		if ( len > 0.0f ) [[likely]]
		{
			const float inv_len = 1.0f / len;
			*this *= inv_len;
		}

		return *this;
	}

	vector3 vector3::normalized( ) const noexcept
	{
		auto vec = *this;
		vec.normalize( );
		return vec;
	}

	float vector3::distance( const vector3& v ) const noexcept { return ( *this - v ).length( ); }
	float vector3::distance_sqr( const vector3& v ) const noexcept { return ( *this - v ).length_sqr( ); }

	vector3 vector3::to_right_vector( ) const noexcept
	{
		auto right = this->cross( vector3( 0.0f, 0.0f, 1.0f ) );

		if ( right.length_sqr( ) < 1e-6f )
		{
			right = this->cross( vector3( 0.0f, -1.0f, 0.0f ) );
		}

		return right.normalize( );
	}

	void vector3::to_directions( vector3* forward, vector3* right, vector3* up ) const noexcept
	{
		constexpr auto deg_to_rad = std::numbers::pi_v<float> / 180.0f;

		const auto sp = std::sinf( x * deg_to_rad );
		const auto cp = std::cosf( x * deg_to_rad );
		const auto sy = std::sinf( y * deg_to_rad );
		const auto cy = std::cosf( y * deg_to_rad );
		const auto sr = std::sinf( z * deg_to_rad );
		const auto cr = std::cosf( z * deg_to_rad );

		if ( forward )
		{
			forward->x = cp * cy;
			forward->y = cp * sy;
			forward->z = -sp;
		}

		if ( right )
		{
			right->x = -1.0f * sr * sp * cy + -1.0f * cr * -sy;
			right->y = -1.0f * sr * sp * sy + -1.0f * cr * cy;
			right->z = -1.0f * sr * cp;
		}

		if ( up )
		{
			up->x = cr * sp * cy + -sr * -sy;
			up->y = cr * sp * sy + -sr * cy;
			up->z = cr * cp;
		}
	}

	vector2 vector2::operator+( const vector2& v ) const noexcept { return { x + v.x, y + v.y }; }
	vector2 vector2::operator-( const vector2& v ) const noexcept { return { x - v.x, y - v.y }; }
	vector2 vector2::operator*( float scalar ) const noexcept { return { x * scalar, y * scalar }; }
	vector2 vector2::operator/( float scalar ) const noexcept { return { x / scalar, y / scalar }; }
	vector2 vector2::operator-( ) const noexcept { return { -x, -y }; }

	vector2& vector2::operator*=( float scalar ) noexcept { x *= scalar; y *= scalar; return *this; }
	vector2& vector2::operator/=( float scalar ) noexcept { x /= scalar; y /= scalar; return *this; }
	vector2& vector2::operator+=( const vector2& v ) noexcept { x += v.x; y += v.y; return *this; }
	vector2& vector2::operator-=( const vector2& v ) noexcept { x -= v.x; y -= v.y; return *this; }

	bool vector2::operator==( const vector2& v ) const noexcept { return x == v.x && y == v.y; }
	bool vector2::operator!=( const vector2& v ) const noexcept { return !( *this == v ); }

	float vector2::dot( const vector2& v ) const noexcept { return x * v.x + y * v.y; }
	float vector2::length_sqr( ) const noexcept { return x * x + y * y; }

	float vector2::length( ) const noexcept
	{
		const auto len_sq = length_sqr( );
		if ( len_sq == 0.0f ) [[unlikely]]
		{
			return 0.0f;
		}

		return std::sqrtf( len_sq );
	}

	vector2& vector2::normalize( ) noexcept
	{
		const auto len = length( );
		if ( len > 0.0f ) [[likely]]
		{
			const float inv_len = 1.0f / len;
			*this *= inv_len;
		}

		return *this;
	}

	vector2 vector2::normalized( ) const noexcept
	{
		auto vec = *this;
		vec.normalize( );
		return vec;
	}

	// -------------------------------- //

	quaternion quaternion::from_euler( const vector3& euler ) noexcept
	{
		const auto pitch = euler.x * ( std::numbers::pi_v<float> / 180.0f );
		const auto yaw = euler.y * ( std::numbers::pi_v<float> / 180.0f );
		const auto roll = euler.z * ( std::numbers::pi_v<float> / 180.0f );

		const auto cy = std::cos( yaw * 0.5f );
		const auto sy = std::sin( yaw * 0.5f );
		const auto cp = std::cos( pitch * 0.5f );
		const auto sp = std::sin( pitch * 0.5f );
		const auto cr = std::cos( roll * 0.5f );
		const auto sr = std::sin( roll * 0.5f );

		quaternion q;
		q.w = cr * cp * cy + sr * sp * sy;
		q.x = sr * cp * cy - cr * sp * sy;
		q.y = cr * sp * cy + sr * cp * sy;
		q.z = cr * cp * sy - sr * sp * cy;

		return q;
	}

	vector3 quaternion::rotate_vector( const vector3& v ) const noexcept
	{
		const auto q_vec = vector3( this->x, this->y, this->z );
		auto uv = q_vec.cross( v );
		auto uuv = q_vec.cross( uv );

		uv *= ( 2.0f * this->w );
		uuv *= 2.0f;

		return v + uv + uuv;
	}

	// -------------------------------- //

	const float* matrix3x3::operator[]( int i ) const noexcept { return mat[ i ]; }
	float* matrix3x3::operator[]( int i ) noexcept { return mat[ i ]; }

	matrix3x3 matrix3x3::from_quaternion( const quaternion& q ) noexcept
	{
		const auto xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
		const auto xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
		const auto wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

		matrix3x3 m{};
		m.mat[ 0 ][ 0 ] = 1 - 2 * ( yy + zz );
		m.mat[ 0 ][ 1 ] = 2 * ( xy + wz );
		m.mat[ 0 ][ 2 ] = 2 * ( xz - wy );
		m.mat[ 1 ][ 0 ] = 2 * ( xy - wz );
		m.mat[ 1 ][ 1 ] = 1 - 2 * ( xx + zz );
		m.mat[ 1 ][ 2 ] = 2 * ( yz + wx );
		m.mat[ 2 ][ 0 ] = 2 * ( xz + wy );
		m.mat[ 2 ][ 1 ] = 2 * ( yz - wx );
		m.mat[ 2 ][ 2 ] = 1 - 2 * ( xx + yy );
		return m;
	}

	vector3 matrix3x3::rotate( const vector3& v ) const noexcept
	{
		return
		{
			mat[ 0 ][ 0 ] * v.x + mat[ 1 ][ 0 ] * v.y + mat[ 2 ][ 0 ] * v.z,
			mat[ 0 ][ 1 ] * v.x + mat[ 1 ][ 1 ] * v.y + mat[ 2 ][ 1 ] * v.z,
			mat[ 0 ][ 2 ] * v.x + mat[ 1 ][ 2 ] * v.y + mat[ 2 ][ 2 ] * v.z
		};
	}

	const float* matrix3x4::operator[]( int i ) const noexcept
	{
		return mat[ i ];
	}

	float* matrix3x4::operator[]( int i ) noexcept
	{
		return mat[ i ];
	}

	const float* matrix4x4::operator[]( int index ) const noexcept
	{
		return mat[ index ];
	}

	float* matrix4x4::operator[]( int index ) noexcept
	{
		return mat[ index ];
	}

	// -------------------------------- //

	void helpers::angle_vectors( const vector3& angles, vector3& forward, vector3& right, vector3& up ) noexcept
	{
		const auto pitch = angles.x * ( std::numbers::pi_v<float> / 180.0f );
		const auto yaw = angles.y * ( std::numbers::pi_v<float> / 180.0f );
		const auto roll = angles.z * ( std::numbers::pi_v<float> / 180.0f );

		const auto sin_pitch = std::sinf( pitch );
		const auto cos_pitch = std::cosf( pitch );
		const auto sin_yaw = std::sinf( yaw );
		const auto cos_yaw = std::cosf( yaw );
		const auto sin_roll = std::sinf( roll );
		const auto cos_roll = std::cosf( roll );

		forward.x = cos_pitch * cos_yaw;
		forward.y = cos_pitch * sin_yaw;
		forward.z = -sin_pitch;

		right.x = -sin_roll * sin_pitch * cos_yaw + cos_roll * sin_yaw;
		right.y = -sin_roll * sin_pitch * sin_yaw - cos_roll * cos_yaw;
		right.z = -sin_roll * cos_pitch;

		up.x = cos_roll * sin_pitch * cos_yaw + sin_roll * sin_yaw;
		up.y = cos_roll * sin_pitch * sin_yaw - sin_roll * cos_yaw;
		up.z = cos_roll * cos_pitch;
	}

	void helpers::normalize_angles( vector3& angles ) noexcept
	{
		while ( angles.y > 180.0f ) { angles.y -= 360.0f; }
		while ( angles.y < -180.0f ) { angles.y += 360.0f; }
		while ( angles.x > 89.0f ) { angles.x -= 180.0f; }
		while ( angles.x < -89.0f ) { angles.x += 180.0f; }

		angles.z = 0.0f;
	}

	vector3 helpers::vector_to_angle( const vector3& forward ) noexcept
	{
		auto pitch = rad_to_deg( std::atan2( -forward.z, forward.length_2d( ) ) );
		auto yaw = rad_to_deg( std::atan2( forward.y, forward.x ) );
		return { pitch, yaw, 0.f };
	}

	vector3 helpers::calculate_angle( const vector3& src, const vector3& dst ) noexcept
	{
		const auto delta = dst - src;
		const auto length = delta.length_2d( );

		vector3 angles;
		angles.x = rad_to_deg( std::atan2f( -delta.z, length ) );
		angles.y = rad_to_deg( std::atan2f( delta.y, delta.x ) );
		angles.z = 0.0f;

		return angles;
	}

	float helpers::calculate_fov( const vector3& view_angles, const vector3& src, const vector3& dst ) noexcept
	{
		const auto angle = calculate_angle( src, dst );
		const auto delta = vector2( view_angles.x - angle.x, normalize_yaw( view_angles.y - angle.y ) );

		return delta.length( );
	}

	float helpers::deg_to_rad( float degrees ) noexcept
	{
		return degrees * ( std::numbers::pi_v<float> / 180.0f );
	}

	float helpers::rad_to_deg( float radians ) noexcept
	{
		return radians * ( 180.0f / std::numbers::pi_v<float> );
	}

	float helpers::normalize_yaw( float yaw ) noexcept
	{
		while ( yaw > 180.0f ) { yaw -= 360.0f; }
		while ( yaw < -180.0f ) { yaw += 360.0f; }
		return yaw;
	}

	math::vector3 helpers::rotate_by_quat( const math::quaternion& q, const math::vector3& v )
	{
		const auto qx = q.x, qy = q.y, qz = q.z, qw = q.w;
		const auto tx = 2.0f * ( qy * v.z - qz * v.y );
		const auto ty = 2.0f * ( qz * v.x - qx * v.z );
		const auto tz = 2.0f * ( qx * v.y - qy * v.x );

		return
		{
			v.x + qw * tx + ( qy * tz - qz * ty ),
			v.y + qw * ty + ( qz * tx - qx * tz ),
			v.z + qw * tz + ( qx * ty - qy * tx )
		};
	}

} // namespace math