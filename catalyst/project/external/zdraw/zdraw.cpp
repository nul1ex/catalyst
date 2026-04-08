#include "zdraw.hpp"

#include <d3dcompiler.h>
#include <wincodec.h>
#include <algorithm>
#include <fstream>
#include <numbers>

#include <ft2build.h>
#include <freetype/freetype.h>

#include "external/fonts/inter.hpp"
#include "external/shaders/shaders.hpp"
#include <iostream>

namespace zdraw {

	namespace detail {

		using Microsoft::WRL::ComPtr;

		struct persistent_buffer
		{
			ComPtr<ID3D11Buffer> m_buffer{};
			void* m_mapped_data{ nullptr };
			std::uint32_t m_size{ 0 };
			std::uint32_t m_write_offset{ 0 };
			std::uint32_t m_capacity{ 0 };
			bool m_is_mapped{ false };

			bool create( ID3D11Device* device, std::uint32_t initial_capacity, D3D11_BIND_FLAG bind_flags )
			{
				this->m_capacity = initial_capacity;

				D3D11_BUFFER_DESC desc{};
				desc.ByteWidth = this->m_capacity;
				desc.Usage = D3D11_USAGE_DYNAMIC;
				desc.BindFlags = bind_flags;
				desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

				return SUCCEEDED( device->CreateBuffer( &desc, nullptr, &this->m_buffer ) );
			}

			bool map_discard( ID3D11DeviceContext* context )
			{
				if ( this->m_is_mapped )
				{
					this->unmap( context );
				}

				D3D11_MAPPED_SUBRESOURCE mapped{};
				auto hr{ context->Map( this->m_buffer.Get( ), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped ) };

				if ( SUCCEEDED( hr ) )
				{
					this->m_mapped_data = mapped.pData;
					this->m_is_mapped = true;
					this->m_write_offset = 0;
					return true;
				}
				else
				{
					return false;
				}
			}

			void unmap( ID3D11DeviceContext* context ) noexcept
			{
				if ( this->m_is_mapped )
				{
					context->Unmap( this->m_buffer.Get( ), 0 );
					this->m_mapped_data = nullptr;
					this->m_is_mapped = false;
				}
			}

			[[nodiscard]] void* allocate( std::uint32_t bytes )
			{
				if ( !this->m_is_mapped )
				{
					return nullptr;
				}

				if ( this->m_write_offset + bytes > this->m_capacity ) [[unlikely]]
				{
					return nullptr;
				}

				auto result{ static_cast< char* >( this->m_mapped_data ) + this->m_write_offset };
				this->m_write_offset += bytes;
				return result;
			}

			void reset_offsets( ) noexcept
			{
				this->m_write_offset = 0;
			}

			[[nodiscard]] bool needs_resize( std::uint32_t required_size ) const noexcept
			{
				return required_size > this->m_capacity;
			}

			void resize( ID3D11Device* device, ID3D11DeviceContext* context, std::uint32_t new_capacity, D3D11_BIND_FLAG bind_flags )
			{
				this->unmap( context );
				this->m_buffer.Reset( );
				this->m_capacity = new_capacity;
				this->create( device, this->m_capacity, bind_flags );
				this->m_write_offset = 0;
			}
		};

		struct render_state_cache
		{
			ID3D11ShaderResourceView* m_last_texture{ nullptr };
			bool m_state_dirty{ true };

			bool m_has_scissor{ false };
			D3D11_RECT m_last_scissor{};

			void reset_frame( ) noexcept
			{
				this->m_last_texture = nullptr;
				this->m_state_dirty = true;
				this->m_has_scissor = false;
				this->m_last_scissor = D3D11_RECT{ 0,0,0,0 };
			}

			[[nodiscard]] bool needs_texture_bind( ID3D11ShaderResourceView* new_tex ) const noexcept
			{
				return this->m_last_texture != new_tex;
			}

			void set_texture( ID3D11ShaderResourceView* tex ) noexcept
			{
				this->m_last_texture = tex;
			}

			[[nodiscard]] bool needs_scissor( const D3D11_RECT& r ) const noexcept
			{
				if ( !this->m_has_scissor )
				{
					return true;
				}

				return this->m_last_scissor.left != r.left || this->m_last_scissor.top != r.top || this->m_last_scissor.right != r.right || this->m_last_scissor.bottom != r.bottom;
			}

			void set_scissor( const D3D11_RECT& r ) noexcept
			{
				this->m_last_scissor = r;
				this->m_has_scissor = true;
			}
		};

		struct render_data
		{
			ComPtr<ID3D11Device> m_device{};
			ComPtr<ID3D11DeviceContext> m_context{};

			persistent_buffer m_vertex_buffer{};
			persistent_buffer m_index_buffer{};

			ComPtr<ID3D11Buffer> m_constant_buffer{};
			ComPtr<ID3D11VertexShader> m_vertex_shader{};
			ComPtr<ID3D11PixelShader> m_pixel_shader{};
			ComPtr<ID3D11InputLayout> m_input_layout{};
			ComPtr<ID3D11RasterizerState> m_rasterizer_state{};
			ComPtr<ID3D11BlendState> m_blend_state{};
			ComPtr<ID3D11DepthStencilState> m_depth_stencil_state{};
			ComPtr<ID3D11SamplerState> m_sampler_state{};
			ComPtr<ID3D11Texture2D> m_white_texture{};
			ComPtr<ID3D11ShaderResourceView> m_white_texture_srv{};

			static constexpr std::uint32_t k_initial_vertex_capacity{ 65536u * static_cast< std::uint32_t >( sizeof( vertex ) ) };
			static constexpr std::uint32_t k_initial_index_capacity{ 131072u * static_cast< std::uint32_t >( sizeof( std::uint32_t ) ) };

			draw_list m_draw_lists[ 3 ]{};
			render_state_cache m_state_cache{};

			std::vector<std::unique_ptr<font>> m_fonts{};
			font* m_default_font{ nullptr };
			std::vector<font*> m_font_stack{};

			std::uint32_t m_frame_vertex_count{ 0 };
			std::uint32_t m_frame_index_count{ 0 };
			std::uint32_t m_buffer_resize_count{ 0 };

			LARGE_INTEGER m_performance_frequency{};
			LARGE_INTEGER m_last_frame_time{};
			float m_delta_time{ 0.0f };
			float m_framerate{ 0.0f };

			static constexpr float k_framerate_smoothing{ 0.1f };
		};

		struct constant_buffer_data
		{
			float m_projection[ 4 ][ 4 ];
		};

		static render_data g_render{};

		[[nodiscard]] static bool create_shaders( )
		{
			ComPtr<ID3DBlob> vs_blob{};
			ComPtr<ID3DBlob> error_blob{};

			auto hr{ D3DCompile( shaders::vertex_shader_src, std::strlen( shaders::vertex_shader_src ), nullptr, nullptr, nullptr, "main", "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &vs_blob, &error_blob ) };
			if ( FAILED( hr ) ) [[unlikely]]
			{
				return false;
			}

			hr = g_render.m_device->CreateVertexShader( vs_blob->GetBufferPointer( ), vs_blob->GetBufferSize( ), nullptr, &g_render.m_vertex_shader );
			if ( FAILED( hr ) ) [[unlikely]]
			{
				return false;
			}

			constexpr D3D11_INPUT_ELEMENT_DESC layout[ ]
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof( vertex, m_pos ),D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof( vertex, m_uv ),D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof( vertex, m_col ),D3D11_INPUT_PER_VERTEX_DATA, 0},
			};

			hr = g_render.m_device->CreateInputLayout( layout, 3, vs_blob->GetBufferPointer( ), vs_blob->GetBufferSize( ), &g_render.m_input_layout );
			if ( FAILED( hr ) ) [[unlikely]]
			{
				return false;
			}

			ComPtr<ID3DBlob> ps_blob{};
			error_blob.Reset( );
			hr = D3DCompile( shaders::pixel_shader_src, std::strlen( shaders::pixel_shader_src ), nullptr, nullptr, nullptr, "main", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &ps_blob, &error_blob );
			if ( FAILED( hr ) ) [[unlikely]]
			{
				return false;
			}

			hr = g_render.m_device->CreatePixelShader( ps_blob->GetBufferPointer( ), ps_blob->GetBufferSize( ), nullptr, &g_render.m_pixel_shader );
			return SUCCEEDED( hr );
		}

		[[nodiscard]] static bool create_render_states( )
		{
			D3D11_RASTERIZER_DESC raster_desc{};
			raster_desc.FillMode = D3D11_FILL_SOLID;
			raster_desc.CullMode = D3D11_CULL_NONE;
			raster_desc.ScissorEnable = TRUE;
			raster_desc.DepthClipEnable = TRUE;
			raster_desc.MultisampleEnable = FALSE;
			raster_desc.AntialiasedLineEnable = FALSE;

			if ( FAILED( g_render.m_device->CreateRasterizerState( &raster_desc, &g_render.m_rasterizer_state ) ) ) [[unlikely]]
			{
				return false;
			}

			D3D11_BLEND_DESC blend_desc{};
			blend_desc.RenderTarget[ 0 ].BlendEnable = TRUE;
			blend_desc.RenderTarget[ 0 ].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			blend_desc.RenderTarget[ 0 ].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blend_desc.RenderTarget[ 0 ].BlendOp = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[ 0 ].SrcBlendAlpha = D3D11_BLEND_ONE;
			blend_desc.RenderTarget[ 0 ].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
			blend_desc.RenderTarget[ 0 ].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[ 0 ].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			if ( FAILED( g_render.m_device->CreateBlendState( &blend_desc, &g_render.m_blend_state ) ) ) [[unlikely]]
			{
				return false;
			}

			D3D11_DEPTH_STENCIL_DESC depth_desc{};
			depth_desc.DepthEnable = FALSE;

			if ( FAILED( g_render.m_device->CreateDepthStencilState( &depth_desc, &g_render.m_depth_stencil_state ) ) ) [[unlikely]]
			{
				return false;
			}

			D3D11_SAMPLER_DESC sampler_desc{};
			sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampler_desc.MipLODBias = 0.0f;
			sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
			sampler_desc.MinLOD = 0.0f;
			sampler_desc.MaxLOD = 0.0f;

			return SUCCEEDED( g_render.m_device->CreateSamplerState( &sampler_desc, &g_render.m_sampler_state ) );
		}

		[[nodiscard]] static bool create_white_texture( )
		{
			D3D11_TEXTURE2D_DESC tex_desc{};
			tex_desc.Width = 1;
			tex_desc.Height = 1;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			constexpr std::uint32_t white_pixel{ 0xFFFFFFFFu };
			D3D11_SUBRESOURCE_DATA init_data{ &white_pixel, 4u, 0u };

			auto hr{ g_render.m_device->CreateTexture2D( &tex_desc, &init_data, &g_render.m_white_texture ) };
			if ( FAILED( hr ) ) [[unlikely]]
			{
				return false;
			}

			D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.Format = tex_desc.Format;
			srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = 1;

			hr = g_render.m_device->CreateShaderResourceView( g_render.m_white_texture.Get( ), &srv_desc, &g_render.m_white_texture_srv );
			return SUCCEEDED( hr );
		}

		[[nodiscard]] static bool create_constant_buffer( )
		{
			D3D11_BUFFER_DESC desc{};
			desc.ByteWidth = static_cast< UINT >( sizeof( constant_buffer_data ) );
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			return SUCCEEDED( g_render.m_device->CreateBuffer( &desc, nullptr, &g_render.m_constant_buffer ) );
		}

		[[nodiscard]] static bool create_persistent_buffers( )
		{
			return g_render.m_vertex_buffer.create( g_render.m_device.Get( ), g_render.k_initial_vertex_capacity, D3D11_BIND_VERTEX_BUFFER ) && g_render.m_index_buffer.create( g_render.m_device.Get( ), g_render.k_initial_index_capacity, D3D11_BIND_INDEX_BUFFER );
		}

		static font* create_font( std::span<const std::byte> font_data, float size_pixels, int atlas_width, int atlas_height )
		{
			FT_Library ft_library{ nullptr };
			if ( FT_Init_FreeType( &ft_library ) != 0 )
			{
				return nullptr;
			}

			FT_Face ft_face{ nullptr };
			if ( FT_New_Memory_Face( ft_library, reinterpret_cast< const FT_Byte* >( font_data.data( ) ), static_cast< FT_Long >( font_data.size( ) ), 0, &ft_face ) != 0 )
			{
				FT_Done_FreeType( ft_library );
				return nullptr;
			}

			const auto is_scalable{ FT_IS_SCALABLE( ft_face ) != 0 };
			if ( is_scalable )
			{
				FT_Size_RequestRec req{};
				req.type = FT_SIZE_REQUEST_TYPE_REAL_DIM;
				req.width = 0;
				req.height = static_cast< FT_Long >( size_pixels * 64.0f );
				req.horiResolution = 0;
				req.vertResolution = 0;
				FT_Request_Size( ft_face, &req );
			}
			else
			{
				auto best_idx{ 0 };
				auto best_diff{ INT_MAX };

				for ( int i{ 0 }; i < ft_face->num_fixed_sizes; ++i )
				{
					const auto diff{ std::abs( ft_face->available_sizes[ i ].height - static_cast< int >( size_pixels ) ) };
					if ( diff < best_diff )
					{
						best_diff = diff;
						best_idx = i;
					}
				}

				FT_Select_Size( ft_face, best_idx );
			}

			auto new_font{ std::make_unique<font>( ) };
			new_font->m_font_size = size_pixels;
			new_font->m_atlas = std::make_shared<font_atlas>( );
			new_font->m_atlas->m_width = atlas_width;
			new_font->m_atlas->m_height = atlas_height;

			if ( is_scalable )
			{
				new_font->m_ascent = static_cast< float >( FT_MulFix( ft_face->ascender, ft_face->size->metrics.y_scale ) ) / 64.0f;
				new_font->m_descent = static_cast< float >( FT_MulFix( ft_face->descender, ft_face->size->metrics.y_scale ) ) / 64.0f;
			}
			else
			{
				new_font->m_ascent = static_cast< float >( ft_face->size->metrics.ascender ) / 64.0f;
				new_font->m_descent = static_cast< float >( ft_face->size->metrics.descender ) / 64.0f;
			}

			new_font->m_line_height = static_cast< float >( ft_face->size->metrics.height ) / 64.0f;
			new_font->m_line_gap = new_font->m_line_height - ( new_font->m_ascent - new_font->m_descent );

			std::vector<std::uint8_t> rgba_bitmap( static_cast< std::size_t >( atlas_width ) * static_cast< std::size_t >( atlas_height ) * 4u, 0u );

			constexpr auto padding{ 1 };
			auto pen_x{ padding };
			auto pen_y{ padding };
			auto row_height{ 0 };

			const FT_Int32 load_flags{ is_scalable ? static_cast< FT_Int32 >( FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT ) : static_cast< FT_Int32 >( FT_LOAD_RENDER | FT_LOAD_TARGET_MONO ) };

			for ( int i{ 0 }; i < 95; ++i )
			{
				const char c{ static_cast< char >( 32 + i ) };

				if ( FT_Load_Char( ft_face, c, load_flags ) != 0 )
				{
					auto& info{ new_font->m_glyph_info[ i ] };
					info.m_advance_x = size_pixels * 0.5f;
					continue;
				}

				auto glyph{ ft_face->glyph };
				auto& bitmap{ glyph->bitmap };

				const auto glyph_width{ static_cast< int >( bitmap.width ) };
				const auto glyph_height{ static_cast< int >( bitmap.rows ) };

				if ( pen_x + glyph_width + padding > atlas_width )
				{
					pen_x = padding;
					pen_y += row_height + padding;
					row_height = 0;
				}

				if ( pen_y + glyph_height + padding > atlas_height )
				{
					break;
				}

				for ( int y{ 0 }; y < glyph_height; ++y )
				{
					for ( int x{ 0 }; x < glyph_width; ++x )
					{
						const auto atlas_x{ pen_x + x };
						const auto atlas_y{ pen_y + y };
						const auto atlas_idx{ static_cast< std::size_t >( atlas_y * atlas_width + atlas_x ) * 4u };

						std::uint8_t alpha;
						if ( bitmap.pixel_mode == FT_PIXEL_MODE_MONO )
						{
							const auto byte_idx{ static_cast< std::size_t >( y * bitmap.pitch + ( x >> 3 ) ) };
							alpha = ( bitmap.buffer[ byte_idx ] & ( 0x80 >> ( x & 7 ) ) ) ? 255u : 0u;
						}
						else
						{
							const auto byte_idx{ static_cast< std::size_t >( y * bitmap.pitch + x ) };
							alpha = bitmap.buffer[ byte_idx ];
						}

						rgba_bitmap[ atlas_idx + 0 ] = 255u;
						rgba_bitmap[ atlas_idx + 1 ] = 255u;
						rgba_bitmap[ atlas_idx + 2 ] = 255u;
						rgba_bitmap[ atlas_idx + 3 ] = alpha;
					}
				}

				auto& info{ new_font->m_glyph_info[ i ] };
				info.m_advance_x = static_cast< float >( glyph->advance.x ) / 64.0f;
				info.m_bearing_x = static_cast< float >( glyph->bitmap_left );
				info.m_bearing_y = static_cast< float >( glyph->bitmap_top );
				info.m_width = static_cast< float >( glyph_width );
				info.m_height = static_cast< float >( glyph_height );
				info.m_atlas_x = static_cast< float >( pen_x );
				info.m_atlas_y = static_cast< float >( pen_y );

				pen_x += glyph_width + padding;
				row_height = std::max( row_height, glyph_height );
			}

			FT_Done_Face( ft_face );
			FT_Done_FreeType( ft_library );

			D3D11_TEXTURE2D_DESC tex_desc{};
			tex_desc.Width = static_cast< UINT >( atlas_width );
			tex_desc.Height = static_cast< UINT >( atlas_height );
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.Usage = D3D11_USAGE_DEFAULT;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			D3D11_SUBRESOURCE_DATA init_data{};
			init_data.pSysMem = rgba_bitmap.data( );
			init_data.SysMemPitch = atlas_width * 4;

			auto hr{ g_render.m_device->CreateTexture2D( &tex_desc, &init_data, &new_font->m_atlas->m_texture ) };
			if ( FAILED( hr ) )
			{
				return nullptr;
			}

			D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.Format = tex_desc.Format;
			srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = 1;

			hr = g_render.m_device->CreateShaderResourceView( new_font->m_atlas->m_texture.Get( ), &srv_desc, &new_font->m_atlas->m_texture_srv );
			if ( FAILED( hr ) )
			{
				return nullptr;
			}

			g_render.m_fonts.push_back( std::move( new_font ) );
			return g_render.m_fonts.back( ).get( );
		}

		static void ensure_buffer_capacity( draw_list& dl )
		{
			const std::uint32_t required_vertex_bytes{ static_cast< std::uint32_t >( dl.m_vertices.size( ) ) * static_cast< std::uint32_t >( sizeof( vertex ) ) };
			const std::uint32_t required_index_bytes{ static_cast< std::uint32_t >( dl.m_indices.size( ) ) * static_cast< std::uint32_t >( sizeof( std::uint32_t ) ) };

			if ( g_render.m_vertex_buffer.needs_resize( required_vertex_bytes ) )
			{
				std::uint32_t new_capacity{ std::max( g_render.m_vertex_buffer.m_capacity * 2u, required_vertex_bytes ) };
				g_render.m_vertex_buffer.resize( g_render.m_device.Get( ), g_render.m_context.Get( ), new_capacity, D3D11_BIND_VERTEX_BUFFER );
				g_render.m_buffer_resize_count += 1u;
			}

			if ( g_render.m_index_buffer.needs_resize( required_index_bytes ) )
			{
				std::uint32_t new_capacity{ std::max( g_render.m_index_buffer.m_capacity * 2u, required_index_bytes ) };
				g_render.m_index_buffer.resize( g_render.m_device.Get( ), g_render.m_context.Get( ), new_capacity, D3D11_BIND_INDEX_BUFFER );
				g_render.m_buffer_resize_count += 1u;
			}
		}

		static void setup_projection_matrix( float width, float height )
		{
			D3D11_MAPPED_SUBRESOURCE mapped{};
			if ( SUCCEEDED( g_render.m_context->Map( g_render.m_constant_buffer.Get( ), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped ) ) )
			{
				auto cb{ static_cast< constant_buffer_data* >( mapped.pData ) };

				const auto L{ 0.0f };
				const auto R{ width };
				const auto T{ 0.0f };
				const auto B{ height };

				const float ortho_projection[ 4 ][ 4 ]
				{
					{ 2.0f / ( R - L ), 0.0f, 0.0f, 0.0f },
					{ 0.0f, 2.0f / ( T - B ), 0.0f, 0.0f },
					{ 0.0f, 0.0f, 0.5f, 0.0f },
					{ ( R + L ) / ( L - R ), ( T + B ) / ( B - T ), 0.5f, 1.0f },
				};

				std::memcpy( cb->m_projection, ortho_projection, sizeof( ortho_projection ) );
				g_render.m_context->Unmap( g_render.m_constant_buffer.Get( ), 0 );
			}
		}

		static void setup_render_state( )
		{
			auto& d{ g_render };

			d.m_context->IASetInputLayout( d.m_input_layout.Get( ) );
			d.m_context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

			d.m_context->VSSetShader( d.m_vertex_shader.Get( ), nullptr, 0 );
			d.m_context->VSSetConstantBuffers( 0, 1, d.m_constant_buffer.GetAddressOf( ) );
			d.m_context->PSSetShader( d.m_pixel_shader.Get( ), nullptr, 0 );
			d.m_context->PSSetSamplers( 0, 1, d.m_sampler_state.GetAddressOf( ) );

			d.m_context->RSSetState( d.m_rasterizer_state.Get( ) );

			constexpr float blend_factor[ 4 ]{ 0.0f, 0.0f, 0.0f, 0.0f };
			d.m_context->OMSetBlendState( d.m_blend_state.Get( ), blend_factor, 0xFFFFFFFFu );
			d.m_context->OMSetDepthStencilState( d.m_depth_stencil_state.Get( ), 0 );

			constexpr std::uint32_t stride{ static_cast< std::uint32_t >( sizeof( vertex ) ) };
			constexpr std::uint32_t offset{ 0u };

			d.m_context->IASetVertexBuffers( 0, 1, d.m_vertex_buffer.m_buffer.GetAddressOf( ), &stride, &offset );
			d.m_context->IASetIndexBuffer( d.m_index_buffer.m_buffer.Get( ), DXGI_FORMAT_R32_UINT, 0 );
		}

		static void generate_circle_vertices( float x, float y, float radius, int segments, nvec<float>& points )
		{
			points.clear( );

			const auto required_size = static_cast< std::size_t >( segments ) * 2;
			const auto angle_increment{ 2.0f * std::numbers::pi_v<float> / static_cast< float >( segments ) };
			auto data = points.allocate( required_size );

			for ( int i{ 0 }; i < segments; ++i )
			{
				const auto angle{ angle_increment * static_cast< float >( i ) };
				data[ i * 2 + 0 ] = x + std::cos( angle ) * radius;
				data[ i * 2 + 1 ] = y + std::sin( angle ) * radius;
			}
		}

		static D3D11_RECT intersect_rect( const D3D11_RECT& a, const D3D11_RECT& b )
		{
			D3D11_RECT r{};
			r.left = std::max( a.left, b.left );
			r.top = std::max( a.top, b.top );
			r.right = std::min( a.right, b.right );
			r.bottom = std::min( a.bottom, b.bottom );
			return r;
		}

	} // namespace detail

	void draw_list::push_clip_rect( float x0, float y0, float x1, float y1 )
	{
		D3D11_RECT r{};
		r.left = static_cast< LONG >( std::floor( x0 ) );
		r.top = static_cast< LONG >( std::floor( y0 ) );
		r.right = static_cast< LONG >( std::ceil( x1 ) );
		r.bottom = static_cast< LONG >( std::ceil( y1 ) );

		if ( !this->m_clip_stack.empty( ) )
		{
			const auto& parent = this->m_clip_stack.back( );
			r.left = std::max( r.left, parent.left );
			r.top = std::max( r.top, parent.top );
			r.right = std::min( r.right, parent.right );
			r.bottom = std::min( r.bottom, parent.bottom );
		}

		this->m_clip_stack.push_back( r );
	}

	void draw_list::pop_clip_rect( )
	{
		if ( !this->m_clip_stack.empty( ) )
		{
			this->m_clip_stack.pop_back( );
		}
	}

	void draw_list::ensure_draw_cmd( ID3D11ShaderResourceView* texture )
	{
		auto actual_texture{ texture != nullptr ? texture : detail::g_render.m_white_texture_srv.Get( ) };
		const auto has_clip = !this->m_clip_stack.empty( );
		D3D11_RECT clip{};

		if ( has_clip )
		{
			clip = this->m_clip_stack.back( );
		}

		auto need_new_cmd{ false };

		if ( this->m_commands.size( ) == 0 )
		{
			need_new_cmd = true;
		}
		else
		{
			const auto& last = this->m_commands.data( )[ this->m_commands.size( ) - 1 ];
			if ( last.m_texture.Get( ) != actual_texture )
			{
				need_new_cmd = true;
			}
			else if ( last.m_has_clip != has_clip )
			{
				need_new_cmd = true;
			}
			else if ( has_clip )
			{
				const auto& lr = last.m_clip_rect;
				if ( lr.left != clip.left || lr.top != clip.top || lr.right != clip.right || lr.bottom != clip.bottom )
				{
					need_new_cmd = true;
				}
			}
		}

		if ( need_new_cmd )
		{
			auto cmd{ this->m_commands.allocate( 1 ) };
			*cmd = draw_cmd{};

			cmd->m_texture = actual_texture;
			cmd->m_idx_offset = static_cast< std::uint32_t >( this->m_indices.size( ) );
			cmd->m_has_clip = has_clip;

			if ( has_clip )
			{
				cmd->m_clip_rect = clip;
			}
		}
	}

	void draw_list::add_line( float x0, float y0, float x1, float y1, rgba color, float thickness )
	{
		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };
		const auto dx{ x1 - x0 };
		const auto dy{ y1 - y0 };
		const auto length{ std::sqrt( dx * dx + dy * dy ) };

		if ( length < 0.0001f ) [[unlikely]]
		{
			return;
		}

		const auto half_thickness{ std::max( 0.0f, thickness ) * 0.5f };
		const auto perp_x{ ( -dy / length ) * half_thickness };
		const auto perp_y{ ( dx / length ) * half_thickness };

		this->push_vertex( x0 + perp_x, y0 + perp_y, 0.0f, 0.0f, color );
		this->push_vertex( x1 + perp_x, y1 + perp_y, 1.0f, 0.0f, color );
		this->push_vertex( x1 - perp_x, y1 - perp_y, 1.0f, 1.0f, color );
		this->push_vertex( x0 - perp_x, y0 - perp_y, 0.0f, 1.0f, color );

		auto idx{ this->m_indices.allocate( 6 ) };
		idx[ 0 ] = vtx_base; idx[ 1 ] = vtx_base + 1; idx[ 2 ] = vtx_base + 2;
		idx[ 3 ] = vtx_base; idx[ 4 ] = vtx_base + 2; idx[ 5 ] = vtx_base + 3;

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 6u;
	}

	void draw_list::add_rect( float x, float y, float w, float h, rgba color, float thickness )
	{
		if ( w <= 0.0f || h <= 0.0f )
		{
			return;
		}

		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };
		const auto max_th{ 0.5f * std::min( w, h ) };

		thickness = std::clamp( thickness, 0.0f, max_th );
		if ( thickness <= 0.0f )
		{
			return;
		}

		const auto inner_x{ x + thickness };
		const auto inner_y{ y + thickness };
		const auto inner_w{ std::max( 0.0f, w - thickness * 2.0f ) };
		const auto inner_h{ std::max( 0.0f, h - thickness * 2.0f ) };

		this->push_vertex( x, y, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y + h, 0.0f, 0.0f, color );
		this->push_vertex( x, y + h, 0.0f, 0.0f, color );

		this->push_vertex( inner_x, inner_y, 0.0f, 0.0f, color );
		this->push_vertex( inner_x + inner_w, inner_y, 0.0f, 0.0f, color );
		this->push_vertex( inner_x + inner_w, inner_y + inner_h, 0.0f, 0.0f, color );
		this->push_vertex( inner_x, inner_y + inner_h, 0.0f, 0.0f, color );

		auto idx{ this->m_indices.allocate( 24 ) };
		idx[ 0 ] = vtx_base; idx[ 1 ] = vtx_base + 1; idx[ 2 ] = vtx_base + 5;
		idx[ 3 ] = vtx_base; idx[ 4 ] = vtx_base + 5; idx[ 5 ] = vtx_base + 4;
		idx[ 6 ] = vtx_base + 1; idx[ 7 ] = vtx_base + 2; idx[ 8 ] = vtx_base + 6;
		idx[ 9 ] = vtx_base + 1; idx[ 10 ] = vtx_base + 6; idx[ 11 ] = vtx_base + 5;
		idx[ 12 ] = vtx_base + 2; idx[ 13 ] = vtx_base + 3; idx[ 14 ] = vtx_base + 7;
		idx[ 15 ] = vtx_base + 2; idx[ 16 ] = vtx_base + 7; idx[ 17 ] = vtx_base + 6;
		idx[ 18 ] = vtx_base + 3; idx[ 19 ] = vtx_base; idx[ 20 ] = vtx_base + 4;
		idx[ 21 ] = vtx_base + 3; idx[ 22 ] = vtx_base + 4; idx[ 23 ] = vtx_base + 7;

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 24u;
	}

	void draw_list::add_rect_cornered( float x, float y, float w, float h, rgba color, float corner_length, float thickness )
	{
		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };
		const auto max_corner{ std::min( w, h ) * 0.5f };
		const auto actual_corner_length{ std::min( corner_length, max_corner ) };
		const auto max_th{ 0.5f * std::min( w, h ) };

		thickness = std::clamp( thickness, 0.0f, max_th );
		if ( thickness <= 0.0f )
		{
			return;
		}

		this->push_vertex( x, y, 0.0f, 0.0f, color );
		this->push_vertex( x + actual_corner_length, y, 0.0f, 0.0f, color );
		this->push_vertex( x + actual_corner_length, y + thickness, 0.0f, 0.0f, color );
		this->push_vertex( x, y + thickness, 0.0f, 0.0f, color );
		this->push_vertex( x, y + thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + thickness, y + thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + thickness, y + actual_corner_length, 0.0f, 0.0f, color );
		this->push_vertex( x, y + actual_corner_length, 0.0f, 0.0f, color );

		this->push_vertex( x + w - actual_corner_length, y, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y + thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + w - actual_corner_length, y + thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + w - thickness, y + thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y + thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y + actual_corner_length, 0.0f, 0.0f, color );
		this->push_vertex( x + w - thickness, y + actual_corner_length, 0.0f, 0.0f, color );

		this->push_vertex( x + w - thickness, y + h - actual_corner_length, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y + h - actual_corner_length, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y + h - thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + w - thickness, y + h - thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + w - actual_corner_length, y + h - thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y + h - thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y + h, 0.0f, 0.0f, color );
		this->push_vertex( x + w - actual_corner_length, y + h, 0.0f, 0.0f, color );

		this->push_vertex( x, y + h - actual_corner_length, 0.0f, 0.0f, color );
		this->push_vertex( x + thickness, y + h - actual_corner_length, 0.0f, 0.0f, color );
		this->push_vertex( x + thickness, y + h - thickness, 0.0f, 0.0f, color );
		this->push_vertex( x, y + h - thickness, 0.0f, 0.0f, color );
		this->push_vertex( x, y + h - thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + actual_corner_length, y + h - thickness, 0.0f, 0.0f, color );
		this->push_vertex( x + actual_corner_length, y + h, 0.0f, 0.0f, color );
		this->push_vertex( x, y + h, 0.0f, 0.0f, color );

		auto idx{ this->m_indices.allocate( 48 ) };

		for ( int i{ 0 }; i < 8; ++i )
		{
			const auto base{ i * 6 };
			const auto vertex_base{ static_cast< std::uint32_t >( vtx_base + i * 4 ) };

			idx[ base + 0 ] = vertex_base; idx[ base + 1 ] = vertex_base + 1;
			idx[ base + 2 ] = vertex_base + 2;
			idx[ base + 3 ] = vertex_base; idx[ base + 4 ] = vertex_base + 2;
			idx[ base + 5 ] = vertex_base + 3;
		}

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 48u;
	}

	void draw_list::add_rect_filled( float x, float y, float w, float h, rgba color )
	{
		if ( w <= 0.0f || h <= 0.0f )
		{
			return;
		}

		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };

		this->push_vertex( x, y, 0.0f, 0.0f, color );
		this->push_vertex( x + w, y, 1.0f, 0.0f, color );
		this->push_vertex( x + w, y + h, 1.0f, 1.0f, color );
		this->push_vertex( x, y + h, 0.0f, 1.0f, color );

		auto idx{ this->m_indices.allocate( 6 ) };
		idx[ 0 ] = vtx_base; idx[ 1 ] = vtx_base + 1; idx[ 2 ] = vtx_base + 2;
		idx[ 3 ] = vtx_base; idx[ 4 ] = vtx_base + 2; idx[ 5 ] = vtx_base + 3;

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 6u;
	}

	void draw_list::add_rect_filled_multi_color( float x, float y, float w, float h, rgba color_tl, rgba color_tr, rgba color_br, rgba color_bl )
	{
		if ( w <= 0.0f || h <= 0.0f )
		{
			return;
		}

		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };

		this->push_vertex( x, y, 0.0f, 0.0f, color_tl );
		this->push_vertex( x + w, y, 1.0f, 0.0f, color_tr );
		this->push_vertex( x + w, y + h, 1.0f, 1.0f, color_br );
		this->push_vertex( x, y + h, 0.0f, 1.0f, color_bl );

		auto idx{ this->m_indices.allocate( 6 ) };
		idx[ 0 ] = vtx_base; idx[ 1 ] = vtx_base + 1; idx[ 2 ] = vtx_base + 2;
		idx[ 3 ] = vtx_base; idx[ 4 ] = vtx_base + 2; idx[ 5 ] = vtx_base + 3;

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 6u;
	}

	void draw_list::add_rect_textured( float x, float y, float w, float h, ID3D11ShaderResourceView* tex, float u0, float v0, float u1, float v1, rgba color )
	{
		this->ensure_draw_cmd( tex );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };

		this->push_vertex( x, y, u0, v0, color );
		this->push_vertex( x + w, y, u1, v0, color );
		this->push_vertex( x + w, y + h, u1, v1, color );
		this->push_vertex( x, y + h, u0, v1, color );

		auto idx{ this->m_indices.allocate( 6 ) };
		idx[ 0 ] = vtx_base; idx[ 1 ] = vtx_base + 1; idx[ 2 ] = vtx_base + 2;
		idx[ 3 ] = vtx_base; idx[ 4 ] = vtx_base + 2; idx[ 5 ] = vtx_base + 3;

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 6u;
	}

	void draw_list::add_convex_poly_filled( std::span<const float> points, rgba color )
	{
		const auto num_points{ static_cast< int >( points.size( ) ) / 2 };
		if ( num_points < 3 ) [[unlikely]]
		{
			return;
		}

		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };

		for ( std::size_t i{ 0 }; i < static_cast< std::size_t >( num_points ); ++i )
		{
			const auto x{ points[ i * 2 + 0 ] };
			const auto y{ points[ i * 2 + 1 ] };
			this->push_vertex( x, y, 0.5f, 0.5f, color );
		}

		const auto num_triangles{ num_points - 2 };
		auto idx{ this->m_indices.allocate( static_cast< std::size_t >( num_triangles ) * 3u ) };

		for ( int i{ 0 }; i < num_triangles; ++i )
		{
			const auto base_idx{ i * 3 };
			idx[ base_idx + 0 ] = vtx_base;
			idx[ base_idx + 1 ] = vtx_base + static_cast< std::uint32_t >( i + 1 );
			idx[ base_idx + 2 ] = vtx_base + static_cast< std::uint32_t >( i + 2 );
		}

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += static_cast< std::uint32_t >( num_triangles ) * 3u;
	}

	void draw_list::add_polyline( std::span<const float> points, rgba color, bool closed, float thickness )
	{
		const auto num_points{ static_cast< int >( points.size( ) ) / 2 };
		if ( num_points < 2 ) [[unlikely]]
		{
			return;
		}

		this->ensure_draw_cmd( nullptr );

		const auto num_segments{ closed ? num_points : ( num_points - 1 ) };
		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };
		const auto half_thickness{ std::max( 0.0f, thickness ) * 0.5f };

		const auto normals_size = static_cast< std::size_t >( num_segments );

		this->m_scratch_normals_x.clear( );
		this->m_scratch_normals_y.clear( );

		auto normals_x = this->m_scratch_normals_x.allocate( normals_size );
		auto normals_y = this->m_scratch_normals_y.allocate( normals_size );

		for ( int i{ 0 }; i < num_segments; ++i )
		{
			const auto p1_idx{ i };
			const auto p2_idx{ closed ? ( ( i + 1 ) % num_points ) : ( i + 1 ) };
			const auto dx{ points[ static_cast< std::size_t >( p2_idx * 2 + 0 ) ] - points[ static_cast< std::size_t >( p1_idx * 2 + 0 ) ] };
			const auto dy{ points[ static_cast< std::size_t >( p2_idx * 2 + 1 ) ] - points[ static_cast< std::size_t >( p1_idx * 2 + 1 ) ] };

			const auto length{ std::sqrt( dx * dx + dy * dy ) };
			if ( length > 0.0001f )
			{
				normals_x[ i ] = -dy / length;
				normals_y[ i ] = dx / length;
			}
			else
			{
				normals_x[ i ] = 0.0f;
				normals_y[ i ] = 0.0f;
			}
		}

		for ( int i{ 0 }; i < num_points; ++i )
		{
			const auto x{ points[ static_cast< std::size_t >( i ) * 2 + 0 ] };
			const auto y{ points[ static_cast< std::size_t >( i ) * 2 + 1 ] };

			auto normal_x{ 0.0f };
			auto normal_y{ 0.0f };

			if ( closed )
			{
				const auto prev_seg{ ( i - 1 + num_segments ) % num_segments };
				const auto curr_seg{ i % num_segments };
				normal_x = ( normals_x[ static_cast< std::size_t >( prev_seg ) ] + normals_x[ static_cast< std::size_t >( curr_seg ) ] ) * 0.5f;
				normal_y = ( normals_y[ static_cast< std::size_t >( prev_seg ) ] + normals_y[ static_cast< std::size_t >( curr_seg ) ] ) * 0.5f;
			}
			else
			{
				if ( i == 0 )
				{
					normal_x = normals_x[ 0 ];
					normal_y = normals_y[ 0 ];
				}
				else if ( i == num_points - 1 )
				{
					normal_x = normals_x[ static_cast< std::size_t >( num_segments - 1 ) ];
					normal_y = normals_y[ static_cast< std::size_t >( num_segments - 1 ) ];
				}
				else
				{
					normal_x = ( normals_x[ static_cast< std::size_t >( i - 1 ) ] + normals_x[ static_cast< std::size_t >( i ) ] ) * 0.5f;
					normal_y = ( normals_y[ static_cast< std::size_t >( i - 1 ) ] + normals_y[ static_cast< std::size_t >( i ) ] ) * 0.5f;
				}
			}

			const auto normal_length{ std::sqrt( normal_x * normal_x + normal_y * normal_y ) };
			if ( normal_length > 0.0001f )
			{
				normal_x /= normal_length;
				normal_y /= normal_length;
			}

			this->push_vertex( x + normal_x * half_thickness, y + normal_y * half_thickness, 0.0f, 0.0f, color );
			this->push_vertex( x - normal_x * half_thickness, y - normal_y * half_thickness, 1.0f, 1.0f, color );
		}

		auto idx{ this->m_indices.allocate( static_cast< std::size_t >( num_segments ) * 6u ) };

		for ( int i{ 0 }; i < num_segments; ++i )
		{
			const auto next_i{ closed ? ( ( i + 1 ) % num_points ) : ( i + 1 ) };
			const auto base_idx{ i * 6 };

			const auto curr_top{ vtx_base + static_cast< std::uint32_t >( i * 2 + 0 ) };
			const auto curr_bot{ vtx_base + static_cast< std::uint32_t >( i * 2 + 1 ) };
			const auto next_top{ vtx_base + static_cast< std::uint32_t >( next_i * 2 + 0 ) };
			const auto next_bot{ vtx_base + static_cast< std::uint32_t >( next_i * 2 + 1 ) };

			idx[ base_idx + 0 ] = curr_top;
			idx[ base_idx + 1 ] = curr_bot;
			idx[ base_idx + 2 ] = next_bot;
			idx[ base_idx + 3 ] = curr_top;
			idx[ base_idx + 4 ] = next_bot;
			idx[ base_idx + 5 ] = next_top;
		}

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += static_cast< std::uint32_t >( num_segments ) * 6u;
	}

	void draw_list::add_polyline_multi_color( std::span<const float> points, std::span<const rgba> colors, bool closed, float thickness )
	{
		const auto num_points{ static_cast< int >( points.size( ) ) / 2 };
		if ( num_points < 2 || static_cast< int >( colors.size( ) ) < num_points ) [[unlikely]]
		{
			return;
		}

		this->ensure_draw_cmd( nullptr );

		const auto num_segments{ closed ? num_points : ( num_points - 1 ) };
		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };
		const auto half_thickness{ std::max( 0.0f, thickness ) * 0.5f };

		const auto normals_size = static_cast< std::size_t >( num_segments );

		this->m_scratch_normals_x.clear( );
		this->m_scratch_normals_y.clear( );

		auto normals_x = this->m_scratch_normals_x.allocate( normals_size );
		auto normals_y = this->m_scratch_normals_y.allocate( normals_size );

		for ( int i{ 0 }; i < num_segments; ++i )
		{
			const auto p1_idx{ i };
			const auto p2_idx{ closed ? ( ( i + 1 ) % num_points ) : ( i + 1 ) };
			const auto dx{ points[ static_cast< std::size_t >( p2_idx * 2 + 0 ) ] - points[ static_cast< std::size_t >( p1_idx * 2 + 0 ) ] };
			const auto dy{ points[ static_cast< std::size_t >( p2_idx * 2 + 1 ) ] - points[ static_cast< std::size_t >( p1_idx * 2 + 1 ) ] };

			const auto length{ std::sqrt( dx * dx + dy * dy ) };
			if ( length > 0.0001f )
			{
				normals_x[ i ] = -dy / length;
				normals_y[ i ] = dx / length;
			}
			else
			{
				normals_x[ i ] = 0.0f;
				normals_y[ i ] = 0.0f;
			}
		}

		for ( int i{ 0 }; i < num_points; ++i )
		{
			const auto x{ points[ static_cast< std::size_t >( i ) * 2 + 0 ] };
			const auto y{ points[ static_cast< std::size_t >( i ) * 2 + 1 ] };
			const auto color{ colors[ static_cast< std::size_t >( i ) ] };

			auto normal_x{ 0.0f };
			auto normal_y{ 0.0f };

			if ( closed )
			{
				const auto prev_seg{ ( i - 1 + num_segments ) % num_segments };
				const auto curr_seg{ i % num_segments };
				normal_x = ( normals_x[ static_cast< std::size_t >( prev_seg ) ] + normals_x[ static_cast< std::size_t >( curr_seg ) ] ) * 0.5f;
				normal_y = ( normals_y[ static_cast< std::size_t >( prev_seg ) ] + normals_y[ static_cast< std::size_t >( curr_seg ) ] ) * 0.5f;
			}
			else
			{
				if ( i == 0 )
				{
					normal_x = normals_x[ 0 ];
					normal_y = normals_y[ 0 ];
				}
				else if ( i == num_points - 1 )
				{
					normal_x = normals_x[ static_cast< std::size_t >( num_segments - 1 ) ];
					normal_y = normals_y[ static_cast< std::size_t >( num_segments - 1 ) ];
				}
				else
				{
					normal_x = ( normals_x[ static_cast< std::size_t >( i - 1 ) ] + normals_x[ static_cast< std::size_t >( i ) ] ) * 0.5f;
					normal_y = ( normals_y[ static_cast< std::size_t >( i - 1 ) ] + normals_y[ static_cast< std::size_t >( i ) ] ) * 0.5f;
				}
			}

			const auto normal_length{ std::sqrt( normal_x * normal_x + normal_y * normal_y ) };
			if ( normal_length > 0.0001f )
			{
				normal_x /= normal_length;
				normal_y /= normal_length;
			}

			this->push_vertex( x + normal_x * half_thickness, y + normal_y * half_thickness, 0.0f, 0.0f, color );
			this->push_vertex( x - normal_x * half_thickness, y - normal_y * half_thickness, 1.0f, 1.0f, color );
		}

		auto idx{ this->m_indices.allocate( static_cast< std::size_t >( num_segments ) * 6u ) };

		for ( int i{ 0 }; i < num_segments; ++i )
		{
			const auto next_i{ closed ? ( ( i + 1 ) % num_points ) : ( i + 1 ) };
			const auto base_idx{ i * 6 };

			const auto curr_top{ vtx_base + static_cast< std::uint32_t >( i * 2 + 0 ) };
			const auto curr_bot{ vtx_base + static_cast< std::uint32_t >( i * 2 + 1 ) };
			const auto next_top{ vtx_base + static_cast< std::uint32_t >( next_i * 2 + 0 ) };
			const auto next_bot{ vtx_base + static_cast< std::uint32_t >( next_i * 2 + 1 ) };

			idx[ base_idx + 0 ] = curr_top;
			idx[ base_idx + 1 ] = curr_bot;
			idx[ base_idx + 2 ] = next_bot;
			idx[ base_idx + 3 ] = curr_top;
			idx[ base_idx + 4 ] = next_bot;
			idx[ base_idx + 5 ] = next_top;
		}

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += static_cast< std::uint32_t >( num_segments ) * 6u;
	}

	void draw_list::add_triangle( float x0, float y0, float x1, float y1, float x2, float y2, rgba color, float thickness )
	{
		const float points[ 6 ]{ x0, y0, x1, y1, x2, y2 };
		this->add_polyline( std::span<const float>{ points, 6 }, color, true, thickness );
	}

	void draw_list::add_triangle_filled( float x0, float y0, float x1, float y1, float x2, float y2, rgba color )
	{
		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };

		this->push_vertex( x0, y0, 0.0f, 0.0f, color );
		this->push_vertex( x1, y1, 0.0f, 0.0f, color );
		this->push_vertex( x2, y2, 0.0f, 0.0f, color );

		auto idx{ this->m_indices.allocate( 3 ) };
		idx[ 0 ] = vtx_base; idx[ 1 ] = vtx_base + 1; idx[ 2 ] = vtx_base + 2;

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 3u;
	}

	void draw_list::add_triangle_filled_multi_color( float x0, float y0, float x1, float y1, float x2, float y2, rgba color0, rgba color1, rgba color2 )
	{
		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };

		this->push_vertex( x0, y0, 0.0f, 0.0f, color0 );
		this->push_vertex( x1, y1, 0.0f, 0.0f, color1 );
		this->push_vertex( x2, y2, 0.0f, 0.0f, color2 );

		auto idx{ this->m_indices.allocate( 3 ) };
		idx[ 0 ] = vtx_base; idx[ 1 ] = vtx_base + 1; idx[ 2 ] = vtx_base + 2;

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 3u;
	}

	void draw_list::add_circle( float x, float y, float radius, rgba color, int segments, float thickness )
	{
		detail::generate_circle_vertices( x, y, radius, segments, this->m_scratch_points );
		this->add_polyline( this->m_scratch_points.span( ), color, true, thickness );
	}

	void draw_list::add_circle_filled( float x, float y, float radius, rgba color, int segments )
	{
		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };
		const auto angle_increment{ 2.0f * std::numbers::pi_v<float> / static_cast< float >( segments ) };

		this->push_vertex( x, y, 0.5f, 0.5f, color );

		for ( int i{ 0 }; i < segments; ++i )
		{
			const auto angle{ angle_increment * static_cast< float >( i ) };
			this->push_vertex( x + std::cos( angle ) * radius, y + std::sin( angle ) * radius, 0.5f, 0.5f, color );
		}

		auto idx{ this->m_indices.allocate( static_cast< std::size_t >( segments ) * 3u ) };

		for ( int i{ 0 }; i < segments; ++i )
		{
			const auto next{ ( i + 1 ) % segments };
			const auto base_idx{ i * 3 };
			idx[ base_idx + 0 ] = vtx_base;
			idx[ base_idx + 1 ] = vtx_base + 1 + static_cast< std::uint32_t >( i );
			idx[ base_idx + 2 ] = vtx_base + 1 + static_cast< std::uint32_t >( next );
		}

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += static_cast< std::uint32_t >( segments ) * 3u;
	}

	void draw_list::add_arc( float x, float y, float radius, float start_angle, float end_angle, rgba color, int segments, float thickness )
	{
		if ( segments < 3 )
		{
			segments = 3;
		}

		const auto angle_range{ end_angle - start_angle };
		const auto angle_increment{ angle_range / static_cast< float >( segments ) };

		this->m_scratch_points.clear( );
		const auto required_size = static_cast< std::size_t >( segments + 1 ) * 2;

		auto data = this->m_scratch_points.allocate( required_size );

		for ( int i{ 0 }; i <= segments; ++i )
		{
			const auto angle{ start_angle + angle_increment * static_cast< float >( i ) };
			data[ i * 2 + 0 ] = x + std::cos( angle ) * radius;
			data[ i * 2 + 1 ] = y + std::sin( angle ) * radius;
		}

		this->add_polyline( this->m_scratch_points.span( ), color, false, thickness );
	}

	void draw_list::add_arc_filled( float x, float y, float radius, float start_angle, float end_angle, rgba color, int segments )
	{
		if ( segments < 3 )
		{
			segments = 3;
		}

		this->ensure_draw_cmd( nullptr );

		const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };
		const auto angle_range{ end_angle - start_angle };
		const auto angle_increment{ angle_range / static_cast< float >( segments ) };

		this->push_vertex( x, y, 0.5f, 0.5f, color );

		for ( int i{ 0 }; i <= segments; ++i )
		{
			const auto angle{ start_angle + angle_increment * static_cast< float >( i ) };
			this->push_vertex( x + std::cos( angle ) * radius, y + std::sin( angle ) * radius, 0.5f, 0.5f, color );
		}

		auto idx{ this->m_indices.allocate( static_cast< std::size_t >( segments ) * 3u ) };

		for ( int i{ 0 }; i < segments; ++i )
		{
			const auto base_idx{ i * 3 };
			idx[ base_idx + 0 ] = vtx_base;
			idx[ base_idx + 1 ] = vtx_base + 1 + static_cast< std::uint32_t >( i );
			idx[ base_idx + 2 ] = vtx_base + 2 + static_cast< std::uint32_t >( i );
		}

		this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += static_cast< std::uint32_t >( segments ) * 3u;
	}

	void draw_list::add_text( float x, float y, std::string_view text, const font* f, rgba color, text_style style )
	{
		if ( style == text_style::outlined )
		{
			constexpr std::array< std::pair< float, float >, 4 > offsets
			{ {
				{ -1.0f,  0.0f },
				{  1.0f,  0.0f },
				{  0.0f, -1.0f },
				{  0.0f,  1.0f }
			} };

			const rgba outline_color{ 0, 0, 0, color.a };

			for ( const auto& [ox, oy] : offsets )
			{
				this->add_text( x + ox, y + oy, text, f, outline_color, text_style::normal );
			}
		}
		else if ( style == text_style::shadowed )
		{
			const rgba shadow_color{ 0, 0, 0, color.a };
			this->add_text( x + 1.0f, y + 1.0f, text, f, shadow_color, text_style::normal );
		}

		if ( f == nullptr ) [[unlikely]]
		{
			f = get_font( );
		}

		if ( f == nullptr || f->m_atlas == nullptr || f->m_atlas->m_texture_srv == nullptr ) [[unlikely]]
		{
			return;
		}

		this->ensure_draw_cmd( f->m_atlas->m_texture_srv.Get( ) );

		auto current_x{ std::floor( x ) };
		auto current_y{ std::floor( y + f->m_ascent ) };

		for ( char c : text )
		{
			if ( c == '\n' )
			{
				current_x = std::floor( x );
				current_y += f->m_line_height;
				continue;
			}

			if ( c < 32 || c > 126 )
			{
				continue;
			}

			const auto& glyph{ f->get_glyph( c ) };
			if ( !glyph.m_valid )
			{
				continue;
			}

			const auto char_x{ current_x + glyph.m_quad_x0 };
			const auto char_y{ current_y + glyph.m_quad_y0 };
			const auto char_w{ glyph.m_quad_x1 - glyph.m_quad_x0 };
			const auto char_h{ glyph.m_quad_y1 - glyph.m_quad_y0 };

			if ( char_w > 0.0f && char_h > 0.0f )
			{
				const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };

				this->push_vertex( char_x, char_y, glyph.m_uv_x0, glyph.m_uv_y0, color );
				this->push_vertex( char_x + char_w, char_y, glyph.m_uv_x1, glyph.m_uv_y0, color );
				this->push_vertex( char_x + char_w, char_y + char_h, glyph.m_uv_x1, glyph.m_uv_y1, color );
				this->push_vertex( char_x, char_y + char_h, glyph.m_uv_x0, glyph.m_uv_y1, color );

				auto idx{ this->m_indices.allocate( 6 ) };
				idx[ 0 ] = vtx_base; idx[ 1 ] = vtx_base + 1; idx[ 2 ] = vtx_base + 2;
				idx[ 3 ] = vtx_base; idx[ 4 ] = vtx_base + 2; idx[ 5 ] = vtx_base + 3;

				this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 6u;
			}

			current_x += glyph.m_advance_x;
		}
	}

	void draw_list::add_text_multi_color( float x, float y, std::string_view text, const font* f, rgba color_tl, rgba color_tr, rgba color_br, rgba color_bl )
	{
		if ( f == nullptr || f->m_atlas == nullptr || f->m_atlas->m_texture_srv == nullptr ) [[unlikely]]
		{
			return;
		}

		this->ensure_draw_cmd( f->m_atlas->m_texture_srv.Get( ) );

		auto current_x{ x };
		auto current_y{ y + f->m_ascent };

		auto text_width{ 0.0f };
		auto text_height = f->m_line_height;

		auto temp_x{ 0.0f };

		for ( char c : text )
		{
			if ( c == '\n' )
			{
				text_width = std::max( text_width, temp_x );
				text_height += f->m_line_height;
				temp_x = 0.0f;
				continue;
			}

			if ( c < 32 || c > 126 )
			{
				continue;
			}

			const auto& glyph{ f->get_glyph( c ) };
			if ( !glyph.m_valid )
			{
				continue;
			}

			temp_x += glyph.m_advance_x;
		}

		text_width = std::max( text_width, temp_x );

		if ( text_width < 0.0001f || text_height < 0.0001f )
		{
			return;
		}

		const auto min_x = x;
		const auto max_x = x + text_width;
		const auto min_y = y;
		const auto max_y = y + text_height;

		auto lerp_color = [ ]( const rgba& a, const rgba& b, float t ) -> rgba
			{
				return rgba
				{
					static_cast< std::uint8_t >( a.r + ( b.r - a.r ) * t ),
					static_cast< std::uint8_t >( a.g + ( b.g - a.g ) * t ),
					static_cast< std::uint8_t >( a.b + ( b.b - a.b ) * t ),
					static_cast< std::uint8_t >( a.a + ( b.a - a.a ) * t )
				};
			};

		auto get_color_at = [ & ]( float px, float py ) -> rgba
			{
				const auto tx = ( px - min_x ) / text_width;
				const auto ty = ( py - min_y ) / text_height;
				const auto color_top = lerp_color( color_tl, color_tr, tx );
				const auto color_bottom = lerp_color( color_bl, color_br, tx );
				return lerp_color( color_top, color_bottom, ty );
			};

		for ( char c : text )
		{
			if ( c == '\n' )
			{
				current_x = x;
				current_y += f->m_line_height;
				continue;
			}

			if ( c < 32 || c > 126 )
			{
				continue;
			}

			const auto& glyph{ f->get_glyph( c ) };
			if ( !glyph.m_valid )
			{
				continue;
			}

			const auto char_x{ current_x + glyph.m_quad_x0 };
			const auto char_y{ current_y + glyph.m_quad_y0 };
			const auto char_w{ glyph.m_quad_x1 - glyph.m_quad_x0 };
			const auto char_h{ glyph.m_quad_y1 - glyph.m_quad_y0 };

			if ( char_w > 0.0f && char_h > 0.0f )
			{
				const auto vtx_base{ static_cast< std::uint32_t >( this->m_vertices.size( ) ) };

				const auto color_char_tl = get_color_at( char_x, char_y );
				const auto color_char_tr = get_color_at( char_x + char_w, char_y );
				const auto color_char_br = get_color_at( char_x + char_w, char_y + char_h );
				const auto color_char_bl = get_color_at( char_x, char_y + char_h );

				this->push_vertex( char_x, char_y, glyph.m_uv_x0, glyph.m_uv_y0, color_char_tl );
				this->push_vertex( char_x + char_w, char_y, glyph.m_uv_x1, glyph.m_uv_y0, color_char_tr );
				this->push_vertex( char_x + char_w, char_y + char_h, glyph.m_uv_x1, glyph.m_uv_y1, color_char_br );
				this->push_vertex( char_x, char_y + char_h, glyph.m_uv_x0, glyph.m_uv_y1, color_char_bl );

				auto idx{ this->m_indices.allocate( 6 ) };
				idx[ 0 ] = vtx_base; idx[ 1 ] = vtx_base + 1; idx[ 2 ] = vtx_base + 2;
				idx[ 3 ] = vtx_base; idx[ 4 ] = vtx_base + 2; idx[ 5 ] = vtx_base + 3;

				this->m_commands.data( )[ this->m_commands.size( ) - 1 ].m_idx_count += 6u;
			}

			current_x += glyph.m_advance_x;
		}
	}

	const glyph_cache_entry& font::get_glyph( char c ) const
	{
		const auto it{ this->m_glyph_cache.find( c ) };
		if ( it != this->m_glyph_cache.end( ) )
		{
			return it->second;
		}

		auto& entry{ this->m_glyph_cache[ c ] };

		const auto char_index{ static_cast< int >( c ) - 32 };
		if ( char_index < 0 || char_index >= 95 )
		{
			entry.m_valid = false;
			return entry;
		}

		const auto& info{ this->m_glyph_info[ char_index ] };
		const float inv_width{ 1.0f / static_cast< float >( this->m_atlas->m_width ) };
		const float inv_height{ 1.0f / static_cast< float >( this->m_atlas->m_height ) };

		entry.m_advance_x = info.m_advance_x;
		entry.m_quad_x0 = info.m_bearing_x;
		entry.m_quad_y0 = -info.m_bearing_y;
		entry.m_quad_x1 = info.m_bearing_x + info.m_width;
		entry.m_quad_y1 = -info.m_bearing_y + info.m_height;
		entry.m_uv_x0 = info.m_atlas_x * inv_width;
		entry.m_uv_y0 = info.m_atlas_y * inv_height;
		entry.m_uv_x1 = ( info.m_atlas_x + info.m_width ) * inv_width;
		entry.m_uv_y1 = ( info.m_atlas_y + info.m_height ) * inv_height;

		entry.m_valid = true;

		return entry;
	}

	void font::calc_text_size( std::string_view text, float& width, float& height ) const
	{
		const auto it{ this->m_text_size_cache.find( text ) };
		if ( it != this->m_text_size_cache.end( ) )
		{
			width = it->second.first;
			height = it->second.second;
			return;
		}

		const auto scale{ 1.0f };
		const auto line_height{ this->m_line_height };

		width = 0.0f;
		height = 0.0f;
		auto line_width{ 0.0f };

		for ( char c : text )
		{
			if ( c == '\n' )
			{
				width = std::max( width, line_width );
				height += line_height;
				line_width = 0.0f;
				continue;
			}

			if ( c == '\r' )
			{
				continue;
			}

			if ( c < 32 || c > 126 )
			{
				continue;
			}

			const auto& glyph{ this->get_glyph( c ) };
			if ( glyph.m_valid )
			{
				line_width += glyph.m_advance_x * scale;
			}
		}

		width = std::max( width, line_width );
		if ( line_width > 0.0f || height == 0.0f )
		{
			height += line_height;
		}

		width = std::floor( width + 0.99999f );

		this->m_text_size_cache[ std::string( text ) ] = std::make_pair( width, height );
	}

	void font::clear_caches( ) const noexcept
	{
		this->m_glyph_cache.clear( );
		this->m_text_size_cache.clear( );
	}

	bool initialize( ID3D11Device* device, ID3D11DeviceContext* context )
	{
		if ( device == nullptr || context == nullptr ) [[unlikely]]
		{
			return false;
		}

		detail::g_render.m_device = device;
		detail::g_render.m_context = context;

		if (
			!detail::create_shaders( )
			|| !detail::create_render_states( )
			|| !detail::create_white_texture( )
			|| !detail::create_constant_buffer( )
			|| !detail::create_persistent_buffers( )
			)
		{
			return false;
		}

		{
			for ( auto& dl : detail::g_render.m_draw_lists )
			{
				dl.reserve( 5000u, 10000u, 256u );
			}

			detail::g_render.m_default_font = detail::create_font( { std::span( reinterpret_cast< const std::byte* >( fonts::inter ), sizeof( fonts::inter ) ) }, 15.0f, 512, 512 );
			detail::g_render.m_font_stack.push_back( detail::g_render.m_default_font );
		}

		{
			QueryPerformanceFrequency( &detail::g_render.m_performance_frequency );
			QueryPerformanceCounter( &detail::g_render.m_last_frame_time );
			detail::g_render.m_delta_time = 0.0f;
			detail::g_render.m_framerate = 0.0f;
		}

		return true;
	}

	void begin_frame( )
	{
		auto& d{ detail::g_render };

		LARGE_INTEGER current_time{};
		QueryPerformanceCounter( &current_time );

		const auto delta_ticks{ current_time.QuadPart - d.m_last_frame_time.QuadPart };
		d.m_delta_time = static_cast< float >( delta_ticks ) / static_cast< float >( d.m_performance_frequency.QuadPart );
		d.m_delta_time = std::min( d.m_delta_time, 0.1f );
		d.m_last_frame_time = current_time;

		if ( d.m_delta_time > 0.0f )
		{
			const auto instantaneous_fps{ 1.0f / d.m_delta_time };
			d.m_framerate = d.m_framerate * ( 1.0f - d.k_framerate_smoothing ) + instantaneous_fps * d.k_framerate_smoothing;
			fps = d.m_framerate;
		}

		for ( auto& dl : d.m_draw_lists )
		{
			dl.clear( );
		}

		d.m_state_cache.reset_frame( );

		d.m_vertex_buffer.reset_offsets( );
		d.m_index_buffer.reset_offsets( );

		d.m_frame_vertex_count = 0u;
		d.m_frame_index_count = 0u;
	}

	void end_frame( )
	{
		auto& d{ detail::g_render };

		D3D11_VIEWPORT viewport{};
		UINT num_viewports{ 1u };
		d.m_context->RSGetViewports( &num_viewports, &viewport );
		const float vp_w = num_viewports > 0 ? viewport.Width : static_cast< float >( GetSystemMetrics( SM_CXSCREEN ) );
		const float vp_h = num_viewports > 0 ? viewport.Height : static_cast< float >( GetSystemMetrics( SM_CYSCREEN ) );

		D3D11_RECT viewport_rect{};
		viewport_rect.left = 0;
		viewport_rect.top = 0;
		viewport_rect.right = static_cast< LONG >( std::ceil( vp_w ) );
		viewport_rect.bottom = static_cast< LONG >( std::ceil( vp_h ) );

		d.m_frame_vertex_count = 0u;
		d.m_frame_index_count = 0u;

		for ( auto& dl : d.m_draw_lists )
		{
			if ( dl.m_vertices.size( ) == 0 || dl.m_commands.size( ) == 0 )
			{
				continue;
			}

			detail::ensure_buffer_capacity( dl );

			const auto vertex_data_size{ static_cast< std::uint32_t >( dl.m_vertices.size( ) ) * static_cast< std::uint32_t >( sizeof( vertex ) ) };
			const auto index_data_size{ static_cast< std::uint32_t >( dl.m_indices.size( ) ) * static_cast< std::uint32_t >( sizeof( std::uint32_t ) ) };

			if ( !d.m_vertex_buffer.map_discard( d.m_context.Get( ) ) || !d.m_index_buffer.map_discard( d.m_context.Get( ) ) )
			{
				d.m_vertex_buffer.unmap( d.m_context.Get( ) );
				d.m_index_buffer.unmap( d.m_context.Get( ) );
				continue;
			}

			auto vertex_dest{ d.m_vertex_buffer.allocate( vertex_data_size ) };
			auto index_dest{ d.m_index_buffer.allocate( index_data_size ) };

			if ( vertex_dest != nullptr && index_dest != nullptr )
			{
				std::memcpy( vertex_dest, dl.m_vertices.data( ), vertex_data_size );
				std::memcpy( index_dest, dl.m_indices.data( ), index_data_size );
			}
			else
			{
				d.m_vertex_buffer.unmap( d.m_context.Get( ) );
				d.m_index_buffer.unmap( d.m_context.Get( ) );
				continue;
			}

			d.m_vertex_buffer.unmap( d.m_context.Get( ) );
			d.m_index_buffer.unmap( d.m_context.Get( ) );

			detail::setup_projection_matrix( vp_w, vp_h );
			detail::setup_render_state( );

			d.m_state_cache.reset_frame( );
			d.m_context->RSSetScissorRects( 1, &viewport_rect );
			d.m_state_cache.set_scissor( viewport_rect );

			auto& state_cache{ d.m_state_cache };
			for ( std::size_t i{ 0 }; i < dl.m_commands.size( ); ++i )
			{
				const auto& cmd{ dl.m_commands.data( )[ i ] };
				if ( cmd.m_idx_count == 0u )
				{
					continue;
				}

				D3D11_RECT scissor{ viewport_rect };
				if ( cmd.m_has_clip )
				{
					scissor = detail::intersect_rect( scissor, cmd.m_clip_rect );
					if ( scissor.right <= scissor.left || scissor.bottom <= scissor.top )
					{
						continue;
					}
				}

				if ( state_cache.needs_scissor( scissor ) )
				{
					d.m_context->RSSetScissorRects( 1, &scissor );
					state_cache.set_scissor( scissor );
				}

				if ( state_cache.needs_texture_bind( cmd.m_texture.Get( ) ) )
				{
					d.m_context->PSSetShaderResources( 0, 1, cmd.m_texture.GetAddressOf( ) );
					state_cache.set_texture( cmd.m_texture.Get( ) );
				}

				d.m_context->DrawIndexed( cmd.m_idx_count, cmd.m_idx_offset, 0 );
			}

			d.m_frame_vertex_count += static_cast< std::uint32_t >( dl.m_vertices.size( ) );
			d.m_frame_index_count += static_cast< std::uint32_t >( dl.m_indices.size( ) );
		}
	}

	draw_list& get_draw_list( draw_layer layer ) noexcept
	{
		return detail::g_render.m_draw_lists[ static_cast< int >( layer ) ];
	}

	std::pair<int, int> get_display_size( ) noexcept
	{
		static std::pair<int, int> cached_size = [ ]( ) -> std::pair<int, int>
			{
				auto& d{ detail::g_render };

				D3D11_VIEWPORT viewport{};
				UINT num_viewports{ 1u };
				if ( d.m_context )
				{
					d.m_context->RSGetViewports( &num_viewports, &viewport );
				}

				if ( num_viewports > 0 && viewport.Width > 0.0f && viewport.Height > 0.0f )
				{
					return { static_cast< int >( std::lround( viewport.Width ) ), static_cast< int >( std::lround( viewport.Height ) ) };
				}

				return { 0, 0 };
			}( );

		return cached_size;
	}

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> load_texture_from_memory( std::span<const std::byte> data, int* out_width, int* out_height )
	{
		static Microsoft::WRL::ComPtr<IWICImagingFactory> factory = [ ]
			{
				( void )CoInitializeEx( nullptr, COINIT_MULTITHREADED );
				Microsoft::WRL::ComPtr<IWICImagingFactory> f;
				( void )CoCreateInstance( CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &f ) );
				return f;
			}( );

		if ( !factory ) [[unlikely]]
		{
			return nullptr;
		}

		Microsoft::WRL::ComPtr<IWICStream> stream;
		auto hr = factory->CreateStream( &stream );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		hr = stream->InitializeFromMemory( reinterpret_cast< BYTE* >( const_cast< std::byte* >( data.data( ) ) ), static_cast< DWORD >( data.size( ) ) );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
		hr = factory->CreateDecoderFromStream( stream.Get( ), nullptr, WICDecodeMetadataCacheOnDemand, &decoder );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
		hr = decoder->GetFrame( 0, &frame );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		UINT width, height;
		frame->GetSize( &width, &height );

		Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
		hr = factory->CreateFormatConverter( &converter );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		hr = converter->Initialize( frame.Get( ), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		std::vector<BYTE> pixels( width * height * 4 );
		hr = converter->CopyPixels( nullptr, width * 4, static_cast< UINT >( pixels.size( ) ), pixels.data( ) );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		if ( out_width != nullptr ) { *out_width = static_cast< int >( width ); }
		if ( out_height != nullptr ) { *out_height = static_cast< int >( height ); }

		D3D11_TEXTURE2D_DESC tex_desc{};
		tex_desc.Width = width;
		tex_desc.Height = height;
		tex_desc.MipLevels = 0;
		tex_desc.ArraySize = 1;
		tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.Usage = D3D11_USAGE_DEFAULT;
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		hr = detail::g_render.m_device->CreateTexture2D( &tex_desc, nullptr, &texture );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		detail::g_render.m_context->UpdateSubresource( texture.Get( ), 0, nullptr, pixels.data( ), width * 4, 0 );

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
		srv_desc.Format = tex_desc.Format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = -1;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_srv;
		hr = detail::g_render.m_device->CreateShaderResourceView( texture.Get( ), &srv_desc, &texture_srv );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		detail::g_render.m_context->GenerateMips( texture_srv.Get( ) );

		return texture_srv;
	}

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> load_icon_from_memory( std::span<const std::byte> data, int* out_width, int* out_height )
	{
		static Microsoft::WRL::ComPtr<IWICImagingFactory> factory = [ ]
			{
				( void )CoInitializeEx( nullptr, COINIT_MULTITHREADED );
				Microsoft::WRL::ComPtr<IWICImagingFactory> f;
				( void )CoCreateInstance( CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &f ) );
				return f;
			}( );

		if ( !factory ) [[unlikely]]
		{
			return nullptr;
		}

		Microsoft::WRL::ComPtr<IWICStream> stream;
		auto hr = factory->CreateStream( &stream );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		hr = stream->InitializeFromMemory( reinterpret_cast< BYTE* >( const_cast< std::byte* >( data.data( ) ) ), static_cast< DWORD >( data.size( ) ) );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
		hr = factory->CreateDecoderFromStream( stream.Get( ), nullptr, WICDecodeMetadataCacheOnDemand, &decoder );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
		hr = decoder->GetFrame( 0, &frame );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		UINT width, height;
		frame->GetSize( &width, &height );

		Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
		hr = factory->CreateFormatConverter( &converter );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		hr = converter->Initialize( frame.Get( ), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		std::vector<BYTE> pixels( width * height * 4 );
		hr = converter->CopyPixels( nullptr, width * 4, static_cast< UINT >( pixels.size( ) ), pixels.data( ) );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		if ( out_width != nullptr ) { *out_width = static_cast< int >( width ); }
		if ( out_height != nullptr ) { *out_height = static_cast< int >( height ); }

		const auto pixel_count{ width * height };
		for ( UINT i{ 0 }; i < pixel_count; ++i )
		{
			const auto idx{ i * 4 };
			const auto r{ pixels[ static_cast< std::size_t >( idx ) + 0 ] };
			const auto g{ pixels[ static_cast< std::size_t >( idx ) + 1 ] };
			const auto b{ pixels[ static_cast< std::size_t >( idx ) + 2 ] };
			const auto a{ pixels[ static_cast< std::size_t >( idx ) + 3 ] };

			const auto luminance{ static_cast< std::uint8_t >( ( r * 0.299f + g * 0.587f + b * 0.114f ) ) };
			const auto computed_alpha{ static_cast< std::uint8_t >( 255 - luminance ) };
			const auto final_alpha{ a < 250 ? a : computed_alpha };

			pixels[ static_cast< std::size_t >( idx ) + 0 ] = 255;
			pixels[ static_cast< std::size_t >( idx ) + 1 ] = 255;
			pixels[ static_cast< std::size_t >( idx ) + 2 ] = 255;
			pixels[ static_cast< std::size_t >( idx ) + 3 ] = final_alpha;
		}

		D3D11_TEXTURE2D_DESC tex_desc{};
		tex_desc.Width = width;
		tex_desc.Height = height;
		tex_desc.MipLevels = 1;
		tex_desc.ArraySize = 1;
		tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA init_data{};
		init_data.pSysMem = pixels.data( );
		init_data.SysMemPitch = width * 4;
		init_data.SysMemSlicePitch = 0;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		hr = detail::g_render.m_device->CreateTexture2D( &tex_desc, &init_data, &texture );
		if ( FAILED( hr ) ) [[unlikely]]
		{
			return nullptr;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
		srv_desc.Format = tex_desc.Format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_srv;
		hr = detail::g_render.m_device->CreateShaderResourceView( texture.Get( ), &srv_desc, &texture_srv );

		return FAILED( hr ) ? nullptr : texture_srv;
	}

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> load_texture_from_file( std::string_view filepath, int* out_width, int* out_height )
	{
		std::ifstream file{ std::string( filepath ), std::ios::binary | std::ios::ate };
		if ( !file.is_open( ) ) [[unlikely]]
		{
			return nullptr;
		}

		const std::streamsize size{ file.tellg( ) };
		file.seekg( 0, std::ios::beg );

		std::vector<std::byte> buffer( static_cast< std::size_t >( size ) );
		file.read( reinterpret_cast< char* >( buffer.data( ) ), size );

		return load_texture_from_memory( buffer, out_width, out_height );
	}

	font* add_font_from_memory( std::span<const std::byte> font_data, float size_pixels, int atlas_width, int atlas_height )
	{
		return detail::create_font( font_data, size_pixels, atlas_width, atlas_height );
	}

	font* add_font_from_file( std::string_view filepath, float size_pixels, int atlas_width, int atlas_height )
	{
		std::ifstream file{ std::string( filepath ), std::ios::binary | std::ios::ate };
		if ( !file.is_open( ) ) [[unlikely]]
		{
			return nullptr;
		}

		std::streamsize size{ file.tellg( ) };
		file.seekg( 0, std::ios::beg );

		std::vector<std::byte> buffer( static_cast< std::size_t >( size ) );
		if ( !file.read( reinterpret_cast< char* >( buffer.data( ) ), size ) ) [[unlikely]]
		{
			return nullptr;
		}

		return add_font_from_memory( buffer, size_pixels, atlas_width, atlas_height );
	}

	font* get_font( ) noexcept
	{
		if ( detail::g_render.m_font_stack.empty( ) ) [[unlikely]]
		{
			return detail::g_render.m_default_font;
		}

		return detail::g_render.m_font_stack.back( );
	}

	font* get_default_font( ) noexcept
	{
		return detail::g_render.m_default_font;
	}

	float get_delta_time( ) noexcept
	{
		return detail::g_render.m_delta_time;
	}

	float get_framerate( ) noexcept
	{
		return detail::g_render.m_framerate;
	}

	void push_font( font* f )
	{
		if ( f == nullptr )
		{
			f = detail::g_render.m_default_font;
		}

		detail::g_render.m_font_stack.push_back( f );
	}

	void pop_font( )
	{
		if ( detail::g_render.m_font_stack.size( ) > 1 )
		{
			detail::g_render.m_font_stack.pop_back( );
		}
	}

	std::pair<float, float> measure_text( std::string_view text, const font* fnt )
	{
		const auto f{ fnt != nullptr ? fnt : get_font( ) };

		float w, h;
		f->calc_text_size( text, w, h );
		return { w, h };
	}

} // namespace zdraw