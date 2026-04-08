#pragma once

namespace math {

	class vector2
	{
	public:
		float x, y;

		vector2( ) noexcept;
		constexpr vector2( float x, float y ) noexcept;

		[[nodiscard]] vector2 operator+( const vector2& v ) const noexcept;
		[[nodiscard]] vector2 operator-( const vector2& v ) const noexcept;
		[[nodiscard]] vector2 operator*( float scalar ) const noexcept;
		[[nodiscard]] vector2 operator/( float scalar ) const noexcept;
		[[nodiscard]] vector2 operator-( ) const noexcept;

		vector2& operator*=( float scalar ) noexcept;
		vector2& operator/=( float scalar ) noexcept;
		vector2& operator+=( const vector2& v ) noexcept;
		vector2& operator-=( const vector2& v ) noexcept;

		[[nodiscard]] bool operator==( const vector2& v ) const noexcept;
		[[nodiscard]] bool operator!=( const vector2& v ) const noexcept;

		[[nodiscard]] float dot( const vector2& v ) const noexcept;
		[[nodiscard]] float length_sqr( ) const noexcept;
		[[nodiscard]] float length( ) const noexcept;

		vector2& normalize( ) noexcept;
		[[nodiscard]] vector2 normalized( ) const noexcept;
	};

	class vector3
	{
	public:
		float x, y, z;

		vector3( ) noexcept;
		constexpr vector3( float x, float y, float z ) noexcept;

		[[nodiscard]] vector3 operator+( const vector3& v ) const noexcept;
		[[nodiscard]] vector3 operator-( const vector3& v ) const noexcept;
		[[nodiscard]] vector3 operator*( float scalar ) const noexcept;
		[[nodiscard]] vector3 operator/( float scalar ) const noexcept;
		[[nodiscard]] vector3 operator-( ) const noexcept;

		vector3& operator*=( float scalar ) noexcept;
		vector3& operator/=( float scalar ) noexcept;
		vector3& operator+=( const vector3& v ) noexcept;
		vector3& operator-=( const vector3& v ) noexcept;

		[[nodiscard]] bool operator==( const vector3& v ) const noexcept;
		[[nodiscard]] bool operator!=( const vector3& v ) const noexcept;

		[[nodiscard]] float dot( const vector3& v ) const noexcept;
		[[nodiscard]] vector3 cross( const vector3& v ) const noexcept;

		[[nodiscard]] float length_sqr( ) const noexcept;
		[[nodiscard]] float length( ) const noexcept;
		[[nodiscard]] float length_2d( ) const noexcept;

		vector3& normalize( ) noexcept;
		[[nodiscard]] vector3 normalized( ) const noexcept;
		vector3& clamp_length( float max_len ) noexcept;
		[[nodiscard]] float distance( const vector3& v ) const noexcept;
		[[nodiscard]] float distance_sqr( const vector3& v ) const noexcept;
		[[nodiscard]] vector3 to_right_vector( ) const noexcept;
		void to_directions( vector3* forward, vector3* right, vector3* up ) const noexcept;
	};

	class quaternion
	{
	public:
		float x, y, z, w;

		quaternion( ) noexcept;
		constexpr quaternion( float x, float y, float z, float w ) noexcept;

		[[nodiscard]] static quaternion from_euler( const vector3& euler ) noexcept;
		[[nodiscard]] vector3 rotate_vector( const vector3& v ) const noexcept;
	};

	class matrix3x4
	{
	public:
		float mat[ 3 ][ 4 ];

		[[nodiscard]] const float* operator[]( int i ) const noexcept;
		[[nodiscard]] float* operator[]( int i ) noexcept;
	};

	class matrix4x4
	{
	public:
		float mat[ 4 ][ 4 ];

		[[nodiscard]] const float* operator[]( int index ) const noexcept;
		[[nodiscard]] float* operator[]( int index ) noexcept;
	};

	class helpers
	{
	public:
		static void angle_vectors( const vector3& angles, vector3& forward, vector3& right, vector3& up ) noexcept;
		static void normalize_angles( vector3& angles ) noexcept;

		[[nodiscard]] static vector3 vector_to_angle( const vector3& forward ) noexcept;
		[[nodiscard]] static vector3 calculate_angle( const vector3& src, const vector3& dst ) noexcept;
		[[nodiscard]] static float calculate_fov( const vector3& view_angles, const vector3& src, const vector3& dst ) noexcept;
		[[nodiscard]] static float deg_to_rad( float degrees ) noexcept;
		[[nodiscard]] static float rad_to_deg( float radians ) noexcept;
		[[nodiscard]] static float normalize_yaw( float yaw ) noexcept;
		[[nodiscard]] static float normalize_pitch(float pitch) noexcept;
		[[nodiscard]] static math::vector3 rotate_by_quat( const math::quaternion& q, const math::vector3& v );
		[[nodiscard]] static math::vector2 catmull_rom( const math::vector2& p0, const math::vector2& p1, const math::vector2& p2, const math::vector2& p3, float t ) noexcept;
	};

} // namespace math