#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "dwmapi.lib")

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <vector>
#include <span>
#include <string_view>

#include "external/unordered_dense.hpp"

namespace zdraw {

	enum class draw_layer { background, window, topmost };

	struct font;
	struct font_atlas;

	struct rgba
	{
		union
		{
			std::uint32_t val;
			struct
			{
				std::uint8_t r;
				std::uint8_t g;
				std::uint8_t b;
				std::uint8_t a;
			};
		};

		constexpr rgba( ) : r{ 0 }, g{ 0 }, b{ 0 }, a{ 0 } {}
		constexpr rgba( std::uint32_t v ) : val{ v } {}
		constexpr rgba( std::uint8_t r_, std::uint8_t g_, std::uint8_t b_, std::uint8_t a_ ) : r{ r_ }, g{ g_ }, b{ b_ }, a{ a_ } {}

		constexpr operator std::uint32_t( ) const
		{
			return this->val;
		}

		constexpr auto to_float( ) const
		{
			return std::array
			{
				static_cast< float >( this->r ) / 255.0f,
				static_cast< float >( this->g ) / 255.0f,
				static_cast< float >( this->b ) / 255.0f,
				static_cast< float >( this->a ) / 255.0f
			};
		}
	};

	struct vertex
	{
		float m_pos[ 2 ];
		float m_uv[ 2 ];
		rgba m_col;
	};

	struct draw_cmd
	{
		std::uint32_t m_idx_offset{ 0 };
		std::uint32_t m_idx_count{ 0 };
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture;

		bool m_has_clip{ false };
		D3D11_RECT m_clip_rect{};

		draw_cmd( ) = default;
		draw_cmd( std::uint32_t idx_off, std::uint32_t count, ID3D11ShaderResourceView* tex ) : m_idx_offset{ idx_off }, m_idx_count{ count }, m_texture{ tex } {}
	};

	template<typename T>
	class nvec
	{
	private:
		std::vector<T> m_data{};
		std::size_t m_size{ 0 };

	public:
		void clear( ) noexcept
		{
			this->m_size = 0;
		}

		void reserve( std::size_t capacity )
		{
			if ( capacity > this->m_data.size( ) )
			{
				this->m_data.resize( capacity );
			}
		}

		[[nodiscard]] T* allocate( std::size_t count )
		{
			if ( this->m_size + count > this->m_data.size( ) )
			{
				this->m_data.resize( ( this->m_size + count ) * 2 );
			}

			T* result{ &this->m_data[ this->m_size ] };
			this->m_size += count;
			return result;
		}

		[[nodiscard]] std::size_t size( ) const noexcept
		{
			return this->m_size;
		}

		[[nodiscard]] const T* data( ) const noexcept
		{
			return this->m_data.data( );
		}

		[[nodiscard]] T* data( ) noexcept
		{
			return this->m_data.data( );
		}

		[[nodiscard]] std::span<const T> span( ) const noexcept
		{
			return std::span{ this->m_data.data( ), this->m_size };
		}
	};

	enum class text_style : std::uint8_t
	{
		normal,
		outlined,
		shadowed
	};

	struct draw_list
	{
		nvec<vertex> m_vertices{};
		nvec<std::uint32_t> m_indices{};
		nvec<draw_cmd> m_commands{};
		std::vector<D3D11_RECT> m_clip_stack{};

		nvec<float> m_scratch_normals_x{};
		nvec<float> m_scratch_normals_y{};
		nvec<float> m_scratch_points{};
		nvec<float> m_scratch_core_points{};
		nvec<float> m_scratch_aa_points{};

		void clear( ) noexcept
		{
			this->m_vertices.clear( );
			this->m_indices.clear( );
			this->m_commands.clear( );
			this->m_clip_stack.clear( );
		}

		void reserve( std::uint32_t vtx_count, std::uint32_t idx_count, std::uint32_t cmd_count = 0 )
		{
			this->m_vertices.reserve( vtx_count );
			this->m_indices.reserve( idx_count );
			if ( cmd_count > 0 ) { this->m_commands.reserve( cmd_count ); }
		}

		void push_vertex( float x, float y, float u, float v, rgba color )
		{
			vertex* vtx{ this->m_vertices.allocate( 1 ) };
			vtx->m_pos[ 0 ] = x;
			vtx->m_pos[ 1 ] = y;
			vtx->m_uv[ 0 ] = u;
			vtx->m_uv[ 1 ] = v;
			vtx->m_col = color;
		}

		void push_clip_rect( float x0, float y0, float x1, float y1 );
		void pop_clip_rect( );

		void ensure_draw_cmd( ID3D11ShaderResourceView* texture );

		void add_line( float x0, float y0, float x1, float y1, rgba color, float thickness = 1.0f );
		void add_rect( float x, float y, float w, float h, rgba color, float thickness = 1.0f );
		void add_rect_cornered( float x, float y, float w, float h, rgba color, float corner_length, float thickness );
		void add_rect_filled( float x, float y, float w, float h, rgba color );
		void add_rect_filled_multi_color( float x, float y, float w, float h, rgba color_tl, rgba color_tr, rgba color_br, rgba color_bl );
		void add_rect_textured( float x, float y, float w, float h, ID3D11ShaderResourceView* tex, float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f, rgba color = rgba{ 255, 255, 255, 255 } );
		void add_convex_poly_filled( std::span<const float> points, rgba color );
		void add_polyline( std::span<const float> points, rgba color, bool closed = false, float thickness = 1.0f );
		void add_polyline_multi_color( std::span<const float> points, std::span<const rgba> colors, bool closed = false, float thickness = 1.0f );
		void add_triangle( float x0, float y0, float x1, float y1, float x2, float y2, rgba color, float thickness = 1.0f );
		void add_triangle_filled( float x0, float y0, float x1, float y1, float x2, float y2, rgba color );
		void add_triangle_filled_multi_color( float x0, float y0, float x1, float y1, float x2, float y2, rgba color0, rgba color1, rgba color2 );
		void add_circle( float x, float y, float radius, rgba color, int segments = 32, float thickness = 1.0f );
		void add_circle_filled( float x, float y, float radius, rgba color, int segments = 32 );
		void add_arc( float x, float y, float radius, float start_angle, float end_angle, rgba color, int segments = 32, float thickness = 1.0f );
		void add_arc_filled( float x, float y, float radius, float start_angle, float end_angle, rgba color, int segments = 32 );
		void add_text( float x, float y, std::string_view text, const font* f, rgba color, text_style style = text_style::normal );
		void add_text_multi_color( float x, float y, std::string_view text, const font* f, rgba color_tl, rgba color_tr, rgba color_br, rgba color_bl );
	};

	struct font_atlas
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture{};
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture_srv{};
		int m_width{ 0 };
		int m_height{ 0 };
	};

	struct glyph_cache_entry
	{
		float m_advance_x{ 0.0f };
		float m_quad_x0{ 0.0f };
		float m_quad_y0{ 0.0f };
		float m_quad_x1{ 0.0f };
		float m_quad_y1{ 0.0f };
		float m_uv_x0{ 0.0f };
		float m_uv_y0{ 0.0f };
		float m_uv_x1{ 0.0f };
		float m_uv_y1{ 0.0f };
		bool m_valid{ false };
	};

	struct font
	{
		std::shared_ptr<font_atlas> m_atlas{};
		float m_font_size{ 12.0f };
		float m_ascent{ 0.0f };
		float m_descent{ 0.0f };
		float m_line_gap{ 0.0f };
		float m_line_height{ 0.0f };

		struct freetype_glyph_info
		{
			float m_advance_x{ 0.0f };
			float m_bearing_x{ 0.0f };
			float m_bearing_y{ 0.0f };
			float m_width{ 0.0f };
			float m_height{ 0.0f };
			float m_atlas_x{ 0.0f };
			float m_atlas_y{ 0.0f };
		};

		std::array<freetype_glyph_info, 95> m_glyph_info{};

		struct string_hash
		{
			using is_transparent = void;
			using hash_type = std::hash<std::string_view>;

			[[nodiscard]] std::size_t operator()( std::string_view sv ) const noexcept { return hash_type{}( sv ); }
			[[nodiscard]] std::size_t operator()( const std::string& s ) const noexcept { return hash_type{}( s ); }
		};

		mutable ankerl::unordered_dense::map<char, glyph_cache_entry> m_glyph_cache{};
		mutable ankerl::unordered_dense::map<std::string, std::pair<float, float>, string_hash, std::equal_to<>> m_text_size_cache{};

		[[nodiscard]] const glyph_cache_entry& get_glyph( char c ) const;
		void calc_text_size( std::string_view text, float& width, float& height ) const;
		void clear_caches( ) const noexcept;
	};

	[[nodiscard]] bool initialize( ID3D11Device* device, ID3D11DeviceContext* context );

	void begin_frame( );
	void end_frame( );

	[[nodiscard]] draw_list& get_draw_list( draw_layer layer = draw_layer::window ) noexcept;
	[[nodiscard]] std::pair<int, int> get_display_size( ) noexcept;

	[[nodiscard]] Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> load_texture_from_memory( std::span<const std::byte> data, int* out_width = nullptr, int* out_height = nullptr );
	[[nodiscard]] Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> load_icon_from_memory( std::span<const std::byte> data, int* out_width = nullptr, int* out_height = nullptr );
	[[nodiscard]] Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> load_texture_from_file( std::string_view filepath, int* out_width = nullptr, int* out_height = nullptr );

	[[nodiscard]] font* add_font_from_memory( std::span<const std::byte> font_data, float size_pixels, int atlas_width = 512, int atlas_height = 512 );
	[[nodiscard]] font* add_font_from_file( std::string_view filepath, float size_pixels, int atlas_width = 512, int atlas_height = 512 );
	[[nodiscard]] font* get_font( ) noexcept;
	[[nodiscard]] font* get_default_font( ) noexcept;

	[[nodiscard]] float get_delta_time( ) noexcept;
	[[nodiscard]] float get_framerate( ) noexcept;

	void push_font( font* f );
	void pop_font( );

	[[nodiscard]] std::pair<float, float> measure_text( std::string_view text, const font* fnt = nullptr );

	inline float fps = 0;


} // namespace zdraw