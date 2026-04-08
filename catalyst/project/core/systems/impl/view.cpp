#include <stdafx.hpp>

namespace systems {

	void view::update( )
	{
		this->m_matrix = g::memory.read<math::matrix4x4>( g::offsets.view_matrix );

		if ( this->m_game_hwnd && !::IsWindow( this->m_game_hwnd ) )
		{
			this->m_game_hwnd = nullptr;
		}

		if ( !this->m_game_hwnd )
		{
			struct data_t { std::uint32_t pid; HWND hwnd; };
			data_t data{ g::memory.process_id( ), nullptr };

			::EnumWindows( [ ]( HWND hwnd, LPARAM lp ) -> BOOL
				{
					auto d = reinterpret_cast< data_t* >( lp );
					DWORD pid = 0;
					::GetWindowThreadProcessId( hwnd, &pid );

					if ( pid == d->pid && ::GetWindow( hwnd, GW_OWNER ) == nullptr && ::IsWindowVisible( hwnd ) )
					{
						d->hwnd = hwnd;
						return FALSE;
					}

					return TRUE;
				}, reinterpret_cast< LPARAM >( &data ) );

			this->m_game_hwnd = data.hwnd;
		}

		if ( this->m_game_hwnd )
		{
			RECT client_rect{};
			::GetClientRect( this->m_game_hwnd, &client_rect );

			this->m_screen_size = { static_cast< float >( client_rect.right ), static_cast< float >( client_rect.bottom ) };

			POINT pt{ 0, 0 };
			::ClientToScreen( this->m_game_hwnd, &pt );
			this->m_screen_offset = { static_cast< float >( pt.x ), static_cast< float >( pt.y ) };
		}

		static const auto view_render = g::memory.find_vtable_instance( g::modules.client, "CViewRender" );
		if ( !view_render )
		{
			return;
		}

		const auto view = view_render + 0x10;
		if ( view )
		{
			this->m_origin = g::memory.read<math::vector3>( view + 0x0 );
			this->m_angles = g::memory.read<math::vector3>( view + 0xc );
			this->m_fov = g::memory.read<float>( view + 0x18 );
		}
		else
		{
			this->m_origin = { k_invalid, k_invalid, k_invalid };
			this->m_angles = { k_invalid, k_invalid, k_invalid };
			this->m_fov = k_invalid;
		}
	}

	math::vector2 view::project( const math::vector3& world_pos )
	{
		const auto& m = this->m_matrix;

		if ( m[ 3 ][ 3 ] == 0.0f )
		{
			return { this->k_invalid, this->k_invalid };
		}

		const auto w = m[ 3 ][ 0 ] * world_pos.x + m[ 3 ][ 1 ] * world_pos.y + m[ 3 ][ 2 ] * world_pos.z + m[ 3 ][ 3 ];

		if ( w < 0.01f )
		{
			return { this->k_invalid, this->k_invalid };
		}

		const auto x = m[ 0 ][ 0 ] * world_pos.x + m[ 0 ][ 1 ] * world_pos.y + m[ 0 ][ 2 ] * world_pos.z + m[ 0 ][ 3 ];
		const auto y = m[ 1 ][ 0 ] * world_pos.x + m[ 1 ][ 1 ] * world_pos.y + m[ 1 ][ 2 ] * world_pos.z + m[ 1 ][ 3 ];

		const auto inv_w = 1.0f / w;

		return
		{
			this->m_screen_offset.x + this->m_screen_size.x * 0.5f * ( 1.0f + x * inv_w ),
			this->m_screen_offset.y + this->m_screen_size.y * 0.5f * ( 1.0f - y * inv_w )
		};
	}

} // namespace systems