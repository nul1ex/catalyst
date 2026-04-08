#include "zui.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <unordered_map>

namespace
{
	using widget_id = std::uintptr_t;
	constexpr widget_id invalid_id{ 0 };

	struct input_state
	{
		float mouse_x{ 0.0f };
		float mouse_y{ 0.0f };
		bool mouse_down{ false };
		bool mouse_clicked{ false };
		bool mouse_released{ false };

		bool right_mouse_down{ false };
		bool right_mouse_clicked{ false };
		bool right_mouse_released{ false };

		float scroll_delta{ 0.0f };

		static constexpr std::size_t max_queue_size{ 32 };

		std::array<wchar_t, max_queue_size> char_buffer{};
		std::size_t char_count{ 0 };

		std::array<int, max_queue_size> key_press_buffer{};
		std::size_t key_press_count{ 0 };

		std::array<int, max_queue_size> key_release_buffer{};
		std::size_t key_release_count{ 0 };

		std::unordered_map<int, bool> key_down_map{};

		[[nodiscard]] bool in_rect( const zui::rect& r ) const noexcept
		{
			return r.contains( this->mouse_x, this->mouse_y );
		}

		void push_char( wchar_t c )
		{
			if ( this->char_count < max_queue_size )
			{
				this->char_buffer[ this->char_count++ ] = c;
			}
		}

		void push_key_press( int vk )
		{
			if ( this->key_press_count < max_queue_size )
			{
				this->key_press_buffer[ this->key_press_count++ ] = vk;
			}
		}

		void push_key_release( int vk )
		{
			if ( this->key_release_count < max_queue_size )
			{
				this->key_release_buffer[ this->key_release_count++ ] = vk;
			}
		}

		[[nodiscard]] std::span<const wchar_t> chars( ) const noexcept
		{
			return std::span( this->char_buffer.data( ), this->char_count );
		}

		[[nodiscard]] std::span<const int> key_presses( ) const noexcept
		{
			return std::span( this->key_press_buffer.data( ), this->key_press_count );
		}

		[[nodiscard]] std::span<const int> key_releases( ) const noexcept
		{
			return std::span( this->key_release_buffer.data( ), this->key_release_count );
		}

		void clear_queues( )
		{
			this->char_count = 0;
			this->key_press_count = 0;
			this->key_release_count = 0;
		}
	};

	class input_manager
	{
	public:
		void set_hwnd( HWND hwnd ) noexcept
		{
			this->m_hwnd = hwnd;
		}

		void update( ) noexcept
		{
			this->m_current.scroll_delta = this->m_pending_scroll_delta;
			this->m_pending_scroll_delta = 0.0f;
		}

		void clear_frame_events( ) noexcept
		{
			this->m_current.mouse_clicked = false;
			this->m_current.mouse_released = false;
			this->m_current.right_mouse_clicked = false;
			this->m_current.right_mouse_released = false;

			this->m_current.clear_queues( );

			this->m_prev = this->m_current;
		}

		bool process_wndproc_message( UINT msg, WPARAM wparam, LPARAM lparam ) noexcept
		{
			switch ( msg )
			{
			case WM_MOUSEMOVE:
			{
				const auto x = static_cast< short >( LOWORD( lparam ) );
				const auto y = static_cast< short >( HIWORD( lparam ) );
				this->m_current.mouse_x = static_cast< float >( x );
				this->m_current.mouse_y = static_cast< float >( y );
				return true;
			}

			case WM_LBUTTONDOWN:
			{
				this->m_current.mouse_down = true;
				this->m_current.mouse_clicked = true;

				const auto vk = VK_LBUTTON;
				if ( !this->m_current.key_down_map[ vk ] )
				{
					this->m_current.push_key_press( vk );
					this->m_current.key_down_map[ vk ] = true;
				}

				return true;
			}

			case WM_LBUTTONUP:
			{
				this->m_current.mouse_down = false;
				this->m_current.mouse_released = true;

				const auto vk = VK_LBUTTON;
				this->m_current.key_down_map[ vk ] = false;
				this->m_current.push_key_release( vk );

				return true;
			}

			case WM_RBUTTONDOWN:
			{
				this->m_current.right_mouse_down = true;
				this->m_current.right_mouse_clicked = true;

				const auto vk = VK_RBUTTON;
				if ( !this->m_current.key_down_map[ vk ] )
				{
					this->m_current.push_key_press( vk );
					this->m_current.key_down_map[ vk ] = true;
				}

				return true;
			}

			case WM_RBUTTONUP:
			{
				this->m_current.right_mouse_down = false;
				this->m_current.right_mouse_released = true;

				const auto vk = VK_RBUTTON;
				this->m_current.key_down_map[ vk ] = false;
				this->m_current.push_key_release( vk );

				return true;
			}

			case WM_MBUTTONDOWN:
			{
				const auto vk = VK_MBUTTON;
				if ( !this->m_current.key_down_map[ vk ] )
				{
					this->m_current.push_key_press( vk );
					this->m_current.key_down_map[ vk ] = true;
				}
				return true;
			}

			case WM_MBUTTONUP:
			{
				const auto vk = VK_MBUTTON;
				this->m_current.key_down_map[ vk ] = false;
				this->m_current.push_key_release( vk );
				return true;
			}

			case WM_MOUSEWHEEL:
			{
				const auto delta = GET_WHEEL_DELTA_WPARAM( wparam ) / static_cast< float >( WHEEL_DELTA );
				this->m_pending_scroll_delta += delta;
				return true;
			}

			case WM_XBUTTONDOWN:
			{
				const auto vk = ( GET_XBUTTON_WPARAM( wparam ) == XBUTTON1 ) ? VK_XBUTTON1 : VK_XBUTTON2;
				if ( !this->m_current.key_down_map[ vk ] )
				{
					this->m_current.push_key_press( vk );
					this->m_current.key_down_map[ vk ] = true;
				}
				return true;
			}

			case WM_XBUTTONUP:
			{
				const auto vk = ( GET_XBUTTON_WPARAM( wparam ) == XBUTTON1 ) ? VK_XBUTTON1 : VK_XBUTTON2;
				this->m_current.key_down_map[ vk ] = false;
				this->m_current.push_key_release( vk );
				return true;
			}

			case WM_CHAR:
			{
				const auto c = static_cast< wchar_t >( wparam );
				if ( c >= 32 && c != 127 )
				{
					this->m_current.push_char( c );
				}

				return true;
			}

			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
			{
				const auto vk = static_cast< int >( wparam );
				if ( !this->m_current.key_down_map[ vk ] )
				{
					this->m_current.push_key_press( vk );
					this->m_current.key_down_map[ vk ] = true;
				}

				return true;
			}

			case WM_KEYUP:
			case WM_SYSKEYUP:
			{
				const auto vk = static_cast< int >( wparam );
				this->m_current.push_key_release( vk );
				this->m_current.key_down_map[ vk ] = false;
				return true;
			}

			default:
				return false;
			}
		}

		void add_scroll_delta( float delta ) noexcept
		{
			this->m_pending_scroll_delta += delta;
		}

		[[nodiscard]] const input_state& current( ) const noexcept { return this->m_current; }
		[[nodiscard]] const input_state& previous( ) const noexcept { return this->m_prev; }

		[[nodiscard]] float mouse_x( ) const noexcept { return this->m_current.mouse_x; }
		[[nodiscard]] float mouse_y( ) const noexcept { return this->m_current.mouse_y; }
		[[nodiscard]] bool mouse_down( ) const noexcept { return this->m_current.mouse_down; }
		[[nodiscard]] bool mouse_clicked( ) const noexcept { return this->m_current.mouse_clicked; }
		[[nodiscard]] bool mouse_released( ) const noexcept { return this->m_current.mouse_released; }

		[[nodiscard]] float mouse_delta_x( ) const noexcept { return this->m_current.mouse_x - this->m_prev.mouse_x; }
		[[nodiscard]] float mouse_delta_y( ) const noexcept { return this->m_current.mouse_y - this->m_prev.mouse_y; }

		[[nodiscard]] bool right_mouse_down( ) const noexcept { return this->m_current.right_mouse_down; }
		[[nodiscard]] bool right_mouse_clicked( ) const noexcept { return this->m_current.right_mouse_clicked; }
		[[nodiscard]] bool right_mouse_released( ) const noexcept { return this->m_current.right_mouse_released; }

		[[nodiscard]] float scroll_delta( ) const noexcept { return this->m_current.scroll_delta; }

		[[nodiscard]] bool hovered( const zui::rect& r ) const noexcept
		{
			return this->m_current.in_rect( r );
		}
 
		[[nodiscard]] bool is_key_down( int vk ) const noexcept
		{
			const auto it = this->m_current.key_down_map.find( vk );
			return it != this->m_current.key_down_map.end( ) && it->second;
		}

	private:
		HWND m_hwnd{ nullptr };
		input_state m_current{};
		input_state m_prev{};
		float m_pending_scroll_delta{ 0.0f };
	};

	class string_interner
	{
	public:
		[[nodiscard]] std::string_view intern( std::string_view sv )
		{
			if ( this->is_interned( sv ) )
			{
				return sv;
			}

			const auto it = this->m_pool.find( sv.data( ) );
			if ( it != this->m_pool.end( ) )
			{
				return std::string_view( *it );
			}

			const auto [inserted_it, success] = this->m_pool.insert( std::string( sv ) );
			return std::string_view( *inserted_it );
		}

		[[nodiscard]] bool is_interned( std::string_view sv ) const noexcept
		{
			for ( const auto& str : this->m_pool )
			{
				const auto pool_begin = str.data( );
				const auto pool_end = pool_begin + str.size( );

				if ( sv.data( ) >= pool_begin && sv.data( ) < pool_end )
				{
					return true;
				}
			}

			return false;
		}

		void clear( )
		{
			this->m_pool.clear( );
		}

	private:
		ankerl::unordered_dense::set<std::string> m_pool{};
	};

	class animation_manager
	{
	public:
		[[nodiscard]] float& get( widget_id id, float initial = 0.0f )
		{
			auto it = this->m_states.find( id );
			if ( it == this->m_states.end( ) )
			{
				it = this->m_states.emplace( id, initial ).first;
			}

			return it->second;
		}

		[[nodiscard]] float get_value( widget_id id, float initial = 0.0f ) const
		{
			const auto it = this->m_states.find( id );
			return ( it != this->m_states.end( ) ) ? it->second : initial;
		}

		void set( widget_id id, float value )
		{
			this->m_states[ id ] = value;
		}

		void remove( widget_id id )
		{
			this->m_states.erase( id );
		}

		void clear( )
		{
			this->m_states.clear( );
		}

		float animate( widget_id id, float target, float speed, float initial = 0.0f )
		{
			auto& value = this->get( id, initial );
			const auto dt = zdraw::get_delta_time( );
			value = value + ( target - value ) * std::min( speed * dt, 1.0f );
			return value;
		}

		float animate_smooth( widget_id id, float target, float speed, float initial = 0.0f )
		{
			const auto raw = this->animate( id, target, speed, initial );
			return raw * raw * ( 3.0f - 2.0f * raw );
		}

	private:
		std::unordered_map<widget_id, float> m_states{};
	};

	class overlay
	{
	public:
		virtual ~overlay( ) = default;

		[[nodiscard]] widget_id id( ) const noexcept { return this->m_id; }
		[[nodiscard]] virtual bool hit_test( float x, float y ) const = 0;

		virtual bool process_input( const input_state& input ) = 0;
		virtual void render( const zui::style& style, const input_state& input ) = 0;

		[[nodiscard]] virtual bool should_close( ) const = 0;
		[[nodiscard]] virtual bool is_closed( ) const = 0;
		[[nodiscard]] const zui::rect& anchor_rect( ) const noexcept { return this->m_anchor; }

		void update_anchor( const zui::rect& new_anchor ) noexcept
		{
			this->m_anchor = new_anchor;
		}

	protected:
		explicit overlay( widget_id id, const zui::rect& anchor ) : m_id{ id }, m_anchor{ anchor } {}

		[[nodiscard]] const zui::rect& get_current_anchor( ) const noexcept
		{
			return this->m_anchor;
		}

		widget_id m_id{ invalid_id };
		zui::rect m_anchor{};
	};

	class overlay_manager
	{
	public:
		template<typename T, typename... args_>
		T* add( args_&&... args )
		{
			auto ptr = std::make_unique<T>( std::forward<args_>( args )... );
			auto* raw = ptr.get( );
			this->m_overlays.push_back( std::move( ptr ) );
			return raw;
		}

		void close( widget_id id )
		{
			for ( auto& o : this->m_overlays )
			{
				if ( o && o->id( ) == id )
				{
					this->m_closing_ids.push_back( id );
					break;
				}
			}
		}

		[[nodiscard]] bool is_open( widget_id id ) const
		{
			for ( const auto& o : this->m_overlays )
			{
				if ( o && o->id( ) == id && !o->should_close( ) )
				{
					return true;
				}
			}

			return false;
		}

		[[nodiscard]] overlay* find( widget_id id )
		{
			for ( auto& o : this->m_overlays )
			{
				if ( o && o->id( ) == id )
				{
					return o.get( );
				}
			}

			return nullptr;
		}

		[[nodiscard]] bool wants_input( float x, float y ) const
		{
			for ( const auto& o : this->m_overlays )
			{
				if ( o && o->hit_test( x, y ) )
				{
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] bool has_active_overlay( ) const
		{
			return !this->m_overlays.empty( );
		}

		[[nodiscard]] bool has_active_overlay_except( widget_id exclude_id ) const
		{
			for ( const auto& o : this->m_overlays )
			{
				if ( o && o->id( ) != exclude_id && !o->is_closed( ) )
				{
					return true;
				}
			}

			return false;
		}

		bool process_input( const input_state& input )
		{
			for ( auto it = this->m_overlays.rbegin( ); it != this->m_overlays.rend( ); ++it )
			{
				if ( *it && ( *it )->process_input( input ) )
				{
					return true;
				}
			}

			return false;
		}

		void render( const zui::style& style, const input_state& input )
		{
			for ( auto& o : this->m_overlays )
			{
				if ( o )
				{
					o->render( style, input );
				}
			}
		}

		void cleanup( )
		{
			this->m_overlays.erase( std::remove_if( this->m_overlays.begin( ), this->m_overlays.end( ), [ ]( const std::unique_ptr<overlay>& o ) { return !o || o->is_closed( ); } ), this->m_overlays.end( ) );
			this->m_closing_ids.clear( );
		}

		void clear( )
		{
			this->m_overlays.clear( );
			this->m_closing_ids.clear( );
		}

	private:
		std::vector<std::unique_ptr<overlay>> m_overlays{};
		std::vector<widget_id> m_closing_ids{};
	};

	class combo_overlay : public overlay
	{
	public:
		combo_overlay( widget_id id, const zui::rect& anchor, float width, const std::vector<std::string>& items, int* current_item, std::function<void( )> on_change = nullptr ) : overlay{ id, anchor }, m_width{ width }, m_items{ items }, m_current_item{ current_item }, m_on_change{ std::move( on_change ) }
		{
			this->m_item_anims.resize( items.size( ), 0.0f );
			this->m_hover_anims.resize( items.size( ), 0.0f );
			this->m_selected_anims.resize( items.size( ), 0.0f );

			if ( current_item && *current_item >= 0 && *current_item < static_cast< int >( items.size( ) ) )
			{
				this->m_selected_anims[ *current_item ] = 1.0f;
			}
		}

		[[nodiscard]] bool hit_test( float x, float y ) const override
		{
			const auto anchor = this->get_current_anchor( );
			return this->get_dropdown_rect( ).contains( x, y ) || anchor.contains( x, y );
		}

		bool process_input( const input_state& input ) override
		{
			if ( this->m_closing )
			{
				return false;
			}

			const auto dropdown = this->get_dropdown_rect( );
			
			if ( dropdown.contains( input.mouse_x, input.mouse_y ) )
			{
				const auto item_height = 20.0f;
				const auto padding_top = 6.0f;
				const auto padding_bottom = 6.0f;
				const auto total_h = static_cast< float >( this->m_items.size( ) ) * item_height + padding_top + padding_bottom;
				const auto max_scroll = std::max( 0.0f, total_h - dropdown.h );
				this->m_target_scroll_y = std::clamp( this->m_target_scroll_y - input.scroll_delta * 40.0f, 0.0f, max_scroll );
			}

			const auto anchor = this->get_current_anchor( );

			if ( input.mouse_clicked && !dropdown.contains( input.mouse_x, input.mouse_y ) && !anchor.contains( input.mouse_x, input.mouse_y ) )
			{
				this->m_closing = true;
				return true;
			}

			if ( input.mouse_clicked && dropdown.contains( input.mouse_x, input.mouse_y ) )
			{
				const auto padding_top = 6.0f;
				const auto item_height = 20.0f;

				for ( int i = 0; i < static_cast< int >( this->m_items.size( ) ); ++i )
				{
					const auto item_y = dropdown.y + padding_top + i * item_height - this->m_scroll_y;
					
					if ( item_y + item_height < dropdown.y || item_y > dropdown.bottom( ) )
						continue;

					const auto item_rect = zui::rect{ dropdown.x + 6.0f, item_y, dropdown.w - 12.0f, item_height };

					if ( item_rect.contains( input.mouse_x, input.mouse_y ) )
					{
						if ( this->m_current_item )
						{
							*this->m_current_item = i;
						}

						if ( this->m_on_change )
						{
							this->m_on_change( );
						}

						this->m_changed = true;
						this->m_closing = true;
						return true;
					}
				}
			}

			return dropdown.contains( input.mouse_x, input.mouse_y );
		}

		void render( const zui::style& style, const input_state& input ) override
		{
			const auto dt = zdraw::get_delta_time( );
			const auto dropdown = this->get_dropdown_rect( );
			auto& dl = zdraw::get_draw_list( zdraw::draw_layer::topmost );
			
			this->m_scroll_y += ( this->m_target_scroll_y - this->m_scroll_y ) * std::min( 15.0f * dt, 1.0f );

			const auto anim_speed = this->m_closing ? 16.0f : 14.0f;
			const auto target = this->m_closing ? 0.0f : 1.0f;
			this->m_open_anim = this->m_open_anim + ( target - this->m_open_anim ) * std::min( anim_speed * dt, 1.0f );

			if ( this->m_open_anim < 0.01f && this->m_closing )
			{
				this->m_fully_closed = true;
				return;
			}

			const auto ease_t = zui::ease::out_cubic( this->m_open_anim );
			const auto animated_h = dropdown.h * ease_t;
			const auto alpha_mult = ease_t;

			auto bg_col = style.combo_popup_bg;
			bg_col.a = static_cast< std::uint8_t >( bg_col.a * alpha_mult );

			auto border_col = zui::lighten( style.combo_popup_border, 1.1f );
			border_col.a = static_cast< std::uint8_t >( border_col.a * alpha_mult );

			dl.add_rect_filled( dropdown.x, dropdown.y, dropdown.w, animated_h, bg_col );
			dl.add_rect( dropdown.x, dropdown.y, dropdown.w, animated_h, border_col );

			dl.push_clip_rect( dropdown.x, dropdown.y, dropdown.x + dropdown.w, dropdown.y + animated_h );

			const auto padding_top{ 6.0f };
			const auto item_height = style.combo_item_height;
			const auto item_padding{ 6.0f };

			for ( int i = 0; i < static_cast< int >( this->m_items.size( ) ); ++i )
			{
				const auto item_y = dropdown.y + padding_top + i * item_height - this->m_scroll_y;
				
				if ( item_y + item_height < dropdown.y || item_y > dropdown.bottom( ) )
					continue;

				const auto item_rect = zui::rect{ dropdown.x + item_padding, item_y, dropdown.w - item_padding * 2.0f, item_height };

				const auto item_delay = i * 0.02f;
				const auto item_progress = std::clamp( this->m_open_anim - item_delay, 0.0f, 1.0f ) / ( 1.0f - std::min( item_delay, 0.5f ) );

				auto& item_anim = this->m_item_anims[ i ];
				item_anim = std::min( item_anim + 18.0f * dt, item_progress );

				const auto item_ease = zui::ease::out_cubic( item_anim );
				const auto item_alpha = item_ease * alpha_mult;
				const auto slide_offset = ( 1.0f - item_ease ) * 8.0f;

				const auto is_selected = this->m_current_item && ( i == *this->m_current_item );
				const auto is_hovered = item_rect.contains( input.mouse_x, input.mouse_y );

				auto& hover_anim = this->m_hover_anims[ i ];
				const auto hover_target = ( is_hovered && !this->m_closing ) ? 1.0f : 0.0f;
				hover_anim += ( hover_target - hover_anim ) * std::min( 15.0f * dt, 1.0f );

				auto& selected_anim = this->m_selected_anims[ i ];
				const auto selected_target = is_selected ? 1.0f : 0.0f;
				selected_anim += ( selected_target - selected_anim ) * std::min( 12.0f * dt, 1.0f );
				const auto selected_ease = zui::ease::out_quad( selected_anim );

				if ( selected_ease > 0.01f )
				{
					auto border_left = style.accent;
					border_left.a = static_cast< std::uint8_t >( 220 * item_alpha * selected_ease );
					dl.add_rect_filled( item_rect.x, item_rect.y + slide_offset, 2.5f, item_rect.h, border_left );

					auto gradient_left = style.combo_item_selected;
					gradient_left.a = static_cast< std::uint8_t >( std::min( gradient_left.a * 3.5f, 255.0f ) * item_alpha * selected_ease );
					auto gradient_right = gradient_left;
					gradient_right.a = 0;

					dl.add_rect_filled_multi_color(
						item_rect.x + 2.5f,
						item_rect.y + slide_offset,
						item_rect.w - 2.5f,
						item_rect.h,
						gradient_left, gradient_right, gradient_right, gradient_left
					);

					const auto dot_size = 4.0f;
					const auto dot_x = item_rect.x + item_rect.w - dot_size - 8.0f;
					const auto dot_y = item_rect.y + ( item_height - dot_size ) * 0.5f + slide_offset;

					auto dot_col = style.accent;
					dot_col.a = static_cast< std::uint8_t >( 160 * item_alpha * selected_ease );
					dl.add_rect_filled( dot_x, dot_y, dot_size, dot_size, dot_col );
				}

				if ( hover_anim > 0.01f )
				{
					auto hover_left = style.combo_item_hovered;
					hover_left.a = static_cast< std::uint8_t >( std::min( hover_left.a * 2.5f, 255.0f ) * item_alpha * hover_anim );
					auto hover_right = hover_left;
					hover_right.a = 0;

					dl.add_rect_filled_multi_color(
						item_rect.x,
						item_rect.y + slide_offset,
						item_rect.w,
						item_rect.h,
						hover_left, hover_right, hover_right, hover_left
					);
				}

				auto [text_w, text_h] = zdraw::measure_text( this->m_items[ i ] );
				const auto text_x = item_rect.x + 10.0f;
				const auto text_y = item_rect.y + ( item_height - text_h ) * 0.5f + slide_offset;

				auto text_col = style.text;
				auto selected_text_col = zui::lerp( style.text, style.accent, 0.4f );
				text_col = zui::lerp( text_col, selected_text_col, selected_ease );
				text_col = zui::lerp( text_col, zui::lighten( text_col, 1.3f ), hover_anim );

				text_col.a = static_cast< std::uint8_t >( text_col.a * item_alpha );

				dl.add_text( text_x, text_y, this->m_items[ i ].c_str( ), zdraw::get_font( ), text_col );
			}

			dl.pop_clip_rect( );
		}

		[[nodiscard]] bool should_close( ) const override { return this->m_closing; }
		[[nodiscard]] bool is_closed( ) const override { return this->m_fully_closed; }
		[[nodiscard]] bool was_changed( ) const { return this->m_changed; }
		void clear_changed( ) { this->m_changed = false; }

	private:
		[[nodiscard]] zui::rect get_dropdown_rect( ) const
		{
			const auto anchor = this->get_current_anchor( );
			const auto padding_top{ 6.0f };
			const auto padding_bottom{ 6.0f };
			const auto item_height{ 20.0f };
			const auto total_h = static_cast< float >( this->m_items.size( ) ) * item_height + padding_top + padding_bottom;
			const auto dropdown_h = std::min( total_h, 300.0f );

			return zui::rect
			{
				anchor.x,
				anchor.bottom( ) + 4.0f,
				this->m_width,
				dropdown_h
			};
		}

		float m_width{ 0.0f };
		std::vector<std::string> m_items{};
		int* m_current_item{ nullptr };
		std::function<void( )> m_on_change{};

		float m_scroll_y{ 0.0f };
		float m_target_scroll_y{ 0.0f };

		float m_open_anim{ 0.0f };
		std::vector<float> m_item_anims{};
		std::vector<float> m_hover_anims{};
		std::vector<float> m_selected_anims{};
		bool m_closing{ false };
		bool m_fully_closed{ false };
		bool m_changed{ false };
	};

	class multicombo_overlay : public overlay
	{
	public:
		multicombo_overlay( widget_id id, const zui::rect& anchor, float width, const char* const* items, int items_count, bool* selected_items, std::function<void( )> on_change = nullptr ) : overlay{ id, anchor }, m_width{ width }, m_items_count{ items_count }, m_selected_items{ selected_items }, m_on_change{ std::move( on_change ) }
		{
			this->m_items.reserve( items_count );
			for ( int i = 0; i < items_count; ++i )
			{
				this->m_items.emplace_back( items[ i ] );
			}

			this->m_item_anims.resize( items_count, 0.0f );
			this->m_check_anims.resize( items_count, 0.0f );
			this->m_hover_anims.resize( items_count, 0.0f );

			if ( selected_items )
			{
				for ( int i = 0; i < items_count; ++i )
				{
					this->m_check_anims[ i ] = selected_items[ i ] ? 1.0f : 0.0f;
				}
			}
		}

		[[nodiscard]] bool hit_test( float x, float y ) const override
		{
			const auto anchor = this->get_current_anchor( );
			return this->get_dropdown_rect( ).contains( x, y ) || anchor.contains( x, y );
		}

		bool process_input( const input_state& input ) override
		{
			if ( this->m_closing )
			{
				return false;
			}

			const auto anchor = this->get_current_anchor( );
			const auto dropdown = this->get_dropdown_rect( );

			if ( input.mouse_clicked && !dropdown.contains( input.mouse_x, input.mouse_y ) && !anchor.contains( input.mouse_x, input.mouse_y ) )
			{
				this->m_closing = true;
				return true;
			}

			if ( input.mouse_clicked && dropdown.contains( input.mouse_x, input.mouse_y ) )
			{
				const auto padding_top = 6.0f;
				const auto item_height = 20.0f;

				for ( int i = 0; i < this->m_items_count; ++i )
				{
					const auto item_y = dropdown.y + padding_top + i * item_height;
					const auto item_rect = zui::rect{ dropdown.x + 6.0f, item_y, dropdown.w - 12.0f, item_height };

					if ( item_rect.contains( input.mouse_x, input.mouse_y ) )
					{
						if ( this->m_selected_items )
						{
							this->m_selected_items[ i ] = !this->m_selected_items[ i ];

							if ( this->m_on_change )
							{
								this->m_on_change( );
							}

							this->m_changed = true;
							this->m_display_text_dirty = true;
						}
						return true;
					}
				}
			}

			return dropdown.contains( input.mouse_x, input.mouse_y );
		}

		void render( const zui::style& style, const input_state& input ) override
		{
			const auto dt = zdraw::get_delta_time( );
			const auto dropdown = this->get_dropdown_rect( );
			auto& dl = zdraw::get_draw_list( zdraw::draw_layer::topmost );

			const auto anim_speed = this->m_closing ? 16.0f : 14.0f;
			const auto target = this->m_closing ? 0.0f : 1.0f;
			this->m_open_anim = this->m_open_anim + ( target - this->m_open_anim ) * std::min( anim_speed * dt, 1.0f );

			if ( this->m_open_anim < 0.01f && this->m_closing )
			{
				this->m_fully_closed = true;
				return;
			}

			const auto ease_t = zui::ease::out_cubic( this->m_open_anim );
			const auto animated_h = dropdown.h * ease_t;
			const auto alpha_mult = ease_t;

			auto bg_col = style.combo_popup_bg;
			bg_col.a = static_cast< std::uint8_t >( bg_col.a * alpha_mult );

			auto border_col = zui::lighten( style.combo_popup_border, 1.1f );
			border_col.a = static_cast< std::uint8_t >( border_col.a * alpha_mult );

			dl.add_rect_filled( dropdown.x, dropdown.y, dropdown.w, animated_h, bg_col );
			dl.add_rect( dropdown.x, dropdown.y, dropdown.w, animated_h, border_col );

			dl.push_clip_rect( dropdown.x - 2.0f, dropdown.y, dropdown.x + dropdown.w + 2.0f, dropdown.y + animated_h );

			const auto padding_top{ 6.0f };
			const auto item_height = style.combo_item_height;
			const auto item_padding{ 6.0f };

			for ( int i = 0; i < this->m_items_count; ++i )
			{
				const auto item_y = dropdown.y + padding_top + i * item_height;
				const auto item_rect = zui::rect{ dropdown.x + item_padding, item_y, dropdown.w - item_padding * 2.0f, item_height };

				const auto item_delay = i * 0.08f;
				const auto item_progress = std::clamp( this->m_open_anim - item_delay, 0.0f, 1.0f ) / ( 1.0f - std::min( item_delay, 0.5f ) );

				auto& item_anim = this->m_item_anims[ i ];
				item_anim = std::min( item_anim + 18.0f * dt, item_progress );

				const auto item_ease = zui::ease::out_cubic( item_anim );
				const auto item_alpha = item_ease * alpha_mult;
				const auto slide_offset = ( 1.0f - item_ease ) * 8.0f;

				const auto is_selected = this->m_selected_items && this->m_selected_items[ i ];
				const auto is_hovered = item_rect.contains( input.mouse_x, input.mouse_y ) && !this->m_closing;

				auto& check_anim = this->m_check_anims[ i ];
				const auto check_target = is_selected ? 1.0f : 0.0f;
				check_anim += ( check_target - check_anim ) * std::min( 12.0f * dt, 1.0f );
				const auto check_ease = zui::ease::smoothstep( check_anim );

				auto& hover_anim = this->m_hover_anims[ i ];
				const auto hover_target = is_hovered ? 1.0f : 0.0f;
				hover_anim += ( hover_target - hover_anim ) * std::min( 18.0f * dt, 1.0f );
				const auto hover_ease = zui::ease::out_quad( hover_anim );

				if ( hover_ease > 0.01f )
				{
					auto hover_left = style.combo_item_hovered;
					hover_left.a = static_cast< std::uint8_t >( std::min( hover_left.a * 2.5f, 255.0f ) * item_alpha * hover_ease );
					auto hover_right = hover_left;
					hover_right.a = 0;

					dl.add_rect_filled_multi_color(
						item_rect.x,
						item_rect.y + slide_offset,
						item_rect.w,
						item_rect.h,
						hover_left, hover_right, hover_right, hover_left
					);
				}

				constexpr auto check_size = 12.0f;
				const auto check_x = item_rect.x + 4.0f;
				const auto check_y = item_rect.y + ( item_height - check_size ) * 0.5f + slide_offset;

				auto check_bg = style.checkbox_bg;
				check_bg.a = static_cast< std::uint8_t >( check_bg.a * item_alpha );
				dl.add_rect_filled( check_x, check_y, check_size, check_size, check_bg );

				auto check_border = style.checkbox_border;
				if ( check_ease > 0.01f )
				{
					check_border = zui::lerp( check_border, style.checkbox_check, check_ease * 0.5f );
				}

				check_border.a = static_cast< std::uint8_t >( check_border.a * item_alpha );
				dl.add_rect( check_x, check_y, check_size, check_size, check_border );

				if ( check_ease > 0.01f )
				{
					constexpr auto pad{ 2.0f };
					const auto inner_size = check_size - pad * 2.0f;
					const auto scale = 0.6f + check_ease * 0.4f;
					const auto scaled_size = inner_size * scale;
					const auto fill_x = check_x + pad + ( inner_size - scaled_size ) * 0.5f;
					const auto fill_y = check_y + pad + ( inner_size - scaled_size ) * 0.5f;

					auto fill_col = style.checkbox_check;
					fill_col.a = static_cast< std::uint8_t >( fill_col.a * check_ease * item_alpha );
					dl.add_rect_filled( fill_x, fill_y, scaled_size, scaled_size, fill_col );
				}

				auto [text_w, text_h] = zdraw::measure_text( this->m_items[ i ] );
				const auto text_x = item_rect.x + check_size + 10.0f;
				const auto text_y = item_rect.y + ( item_height - text_h ) * 0.5f + slide_offset;

				auto text_col = style.text;
				if ( check_ease > 0.5f )
				{
					text_col = zui::lerp( text_col, style.checkbox_check, ( check_ease - 0.5f ) * 0.6f );
				}
				text_col = zui::lerp( text_col, zui::lighten( text_col, 1.2f ), hover_ease );

				text_col.a = static_cast< std::uint8_t >( text_col.a * item_alpha );

				dl.add_text( text_x, text_y, this->m_items[ i ].c_str( ), zdraw::get_font( ), text_col );
			}

			dl.pop_clip_rect( );
		}

		[[nodiscard]] bool should_close( ) const override { return this->m_closing; }
		[[nodiscard]] bool is_closed( ) const override { return this->m_fully_closed; }
		[[nodiscard]] bool was_changed( ) const { return this->m_changed; }
		void clear_changed( ) { this->m_changed = false; }

		[[nodiscard]] const std::string& get_display_text( ) const
		{
			if ( this->m_display_text_dirty )
			{
				this->m_cached_display_text.clear( );

				for ( int i = 0; i < this->m_items_count; ++i )
				{
					if ( this->m_selected_items && this->m_selected_items[ i ] )
					{
						if ( !this->m_cached_display_text.empty( ) )
						{
							this->m_cached_display_text += ", ";
						}
						this->m_cached_display_text += this->m_items[ i ];
					}
				}

				if ( this->m_cached_display_text.empty( ) )
				{
					this->m_cached_display_text = "none";
				}

				this->m_display_text_dirty = false;
			}

			return this->m_cached_display_text;
		}

	private:
		[[nodiscard]] zui::rect get_dropdown_rect( ) const
		{
			const auto anchor = this->get_current_anchor( );
			const auto padding_top{ 6.0f };
			const auto padding_bottom{ 6.0f };
			const auto item_height{ 20.0f };
			const auto dropdown_h = static_cast< float >( this->m_items_count ) * item_height + padding_top + padding_bottom;

			return zui::rect
			{
				anchor.x,
				anchor.bottom( ) + 4.0f,
				this->m_width,
				dropdown_h
			};
		}

		float m_width{ 0.0f };
		std::vector<std::string> m_items{};
		int m_items_count{ 0 };
		bool* m_selected_items{ nullptr };
		std::function<void( )> m_on_change{};

		float m_open_anim{ 0.0f };
		std::vector<float> m_item_anims{};
		std::vector<float> m_check_anims{};
		std::vector<float> m_hover_anims{};
		bool m_closing{ false };
		bool m_fully_closed{ false };
		bool m_changed{ false };

		mutable std::string m_cached_display_text{};
		mutable bool m_display_text_dirty{ true };
	};

	struct multicombo_scroll_state
	{
		float scroll_offset{ 0.0f };
		float hover_time{ 0.0f };
		bool was_hovered{ false };
	};

	std::unordered_map<widget_id, multicombo_scroll_state>& get_multicombo_states( )
	{
		static std::unordered_map<widget_id, multicombo_scroll_state> states{};
		return states;
	}

	std::optional<zdraw::rgba>& get_color_clipboard( )
	{
		static std::optional<zdraw::rgba> clipboard{};
		return clipboard;
	}

	class color_picker_context_menu : public overlay
	{
	public:
		color_picker_context_menu( widget_id id, const zui::rect& anchor, zdraw::rgba* color_ptr ) : overlay{ id, anchor }, m_color_ptr{ color_ptr } {}

		[[nodiscard]] bool hit_test( float x, float y ) const override
		{
			return this->get_popup_rect( ).contains( x, y );
		}

		bool process_input( const input_state& input ) override
		{
			if ( this->m_closing )
			{
				return false;
			}

			const auto popup = this->get_popup_rect( );

			if ( input.mouse_clicked && !popup.contains( input.mouse_x, input.mouse_y ) )
			{
				this->m_closing = true;
				return true;
			}

			if ( input.mouse_clicked && popup.contains( input.mouse_x, input.mouse_y ) )
			{
				const auto button_h{ 22.0f };
				const auto padding{ 4.0f };

				const auto copy_rect = zui::rect{ popup.x + padding, popup.y + padding, popup.w - padding * 2.0f, button_h };
				const auto paste_rect = zui::rect{ popup.x + padding, copy_rect.bottom( ) + 2.0f, popup.w - padding * 2.0f, button_h };

				if ( copy_rect.contains( input.mouse_x, input.mouse_y ) )
				{
					if ( this->m_color_ptr )
					{
						get_color_clipboard( ) = *this->m_color_ptr;
					}

					this->m_closing = true;
					return true;
				}

				if ( paste_rect.contains( input.mouse_x, input.mouse_y ) )
				{
					auto& clipboard = get_color_clipboard( );
					if ( clipboard.has_value( ) && this->m_color_ptr )
					{
						*this->m_color_ptr = clipboard.value( );
						this->m_changed = true;
					}

					this->m_closing = true;
					return true;
				}
			}

			return popup.contains( input.mouse_x, input.mouse_y );
		}

		void render( const zui::style& style, const input_state& input ) override
		{
			const auto dt = zdraw::get_delta_time( );
			const auto popup = this->get_popup_rect( );
			auto& dl = zdraw::get_draw_list( zdraw::draw_layer::topmost );

			const auto anim_speed = this->m_closing ? 18.0f : 16.0f;
			const auto target = this->m_closing ? 0.0f : 1.0f;
			this->m_open_anim = this->m_open_anim + ( target - this->m_open_anim ) * std::min( anim_speed * dt, 1.0f );

			if ( this->m_open_anim < 0.01f && this->m_closing )
			{
				this->m_fully_closed = true;
				return;
			}

			const auto ease_t = zui::ease::out_cubic( this->m_open_anim );
			const auto alpha_mult = ease_t;
			const auto scale = 0.9f + ease_t * 0.1f;

			const auto scaled_w = popup.w * scale;
			const auto scaled_h = popup.h * scale;
			const auto scaled_x = popup.x + ( popup.w - scaled_w ) * 0.5f;
			const auto scaled_y = popup.y + ( popup.h - scaled_h ) * 0.5f;

			auto bg_col = style.combo_popup_bg;
			bg_col.a = static_cast< std::uint8_t >( bg_col.a * alpha_mult );

			auto border_col = zui::lighten( style.combo_popup_border, 1.1f );
			border_col.a = static_cast< std::uint8_t >( border_col.a * alpha_mult );

			dl.add_rect_filled( scaled_x, scaled_y, scaled_w, scaled_h, bg_col );
			dl.add_rect( scaled_x, scaled_y, scaled_w, scaled_h, border_col );

			if ( ease_t < 0.3f )
			{
				return;
			}

			const auto content_alpha = std::clamp( ( ease_t - 0.3f ) / 0.7f, 0.0f, 1.0f );

			const auto button_h{ 22.0f };
			const auto padding{ 4.0f };

			const auto copy_rect = zui::rect{ popup.x + padding, popup.y + padding, popup.w - padding * 2.0f, button_h };
			const auto paste_rect = zui::rect{ popup.x + padding, copy_rect.bottom( ) + 2.0f, popup.w - padding * 2.0f, button_h };

			const auto copy_hovered = copy_rect.contains( input.mouse_x, input.mouse_y );
			const auto paste_hovered = paste_rect.contains( input.mouse_x, input.mouse_y );
			const auto has_clipboard = get_color_clipboard( ).has_value( );

			{
				auto btn_bg = copy_hovered ? style.combo_item_hovered : zdraw::rgba{ 0, 0, 0, 0 };
				btn_bg.a = static_cast< std::uint8_t >( std::min( btn_bg.a * 2.0f, 255.0f ) * content_alpha );

				if ( copy_hovered )
				{
					dl.add_rect_filled( copy_rect.x, copy_rect.y, copy_rect.w, copy_rect.h, btn_bg );
				}

				auto text_col = style.text;
				if ( copy_hovered )
				{
					text_col = zui::lighten( text_col, 1.2f );
				}

				text_col.a = static_cast< std::uint8_t >( text_col.a * content_alpha );

				auto [text_w, text_h] = zdraw::measure_text( "copy" );
				const auto text_x = copy_rect.x + ( copy_rect.w - text_w ) * 0.5f;
				const auto text_y = copy_rect.y + ( copy_rect.h - text_h ) * 0.5f;
				dl.add_text( text_x, text_y, "copy", zdraw::get_font( ), text_col );
			}

			{
				auto btn_bg = paste_hovered && has_clipboard ? style.combo_item_hovered : zdraw::rgba{ 0, 0, 0, 0 };
				btn_bg.a = static_cast< std::uint8_t >( std::min( btn_bg.a * 2.0f, 255.0f ) * content_alpha );

				if ( paste_hovered && has_clipboard )
				{
					dl.add_rect_filled( paste_rect.x, paste_rect.y, paste_rect.w, paste_rect.h, btn_bg );
				}

				auto text_col = style.text;
				if ( !has_clipboard )
				{
					text_col.a = static_cast< std::uint8_t >( text_col.a * 0.4f );
				}
				else if ( paste_hovered )
				{
					text_col = zui::lighten( text_col, 1.2f );
				}

				text_col.a = static_cast< std::uint8_t >( text_col.a * content_alpha );

				auto [text_w, text_h] = zdraw::measure_text( "paste" );
				const auto text_x = paste_rect.x + ( paste_rect.w - text_w ) * 0.5f;
				const auto text_y = paste_rect.y + ( paste_rect.h - text_h ) * 0.5f;
				dl.add_text( text_x, text_y, "paste", zdraw::get_font( ), text_col );
			}
		}

		[[nodiscard]] bool should_close( ) const override { return this->m_closing; }
		[[nodiscard]] bool is_closed( ) const override { return this->m_fully_closed; }
		[[nodiscard]] bool was_changed( ) const { return this->m_changed; }

	private:
		[[nodiscard]] zui::rect get_popup_rect( ) const
		{
			const auto anchor = this->get_current_anchor( );
			constexpr auto padding{ 4.0f };
			constexpr auto button_h{ 22.0f };
			constexpr auto button_gap{ 2.0f };

			constexpr auto popup_w{ 70.0f };
			constexpr auto popup_h = padding + button_h + button_gap + button_h + padding;

			return zui::rect
			{
				anchor.x,
				anchor.y,
				popup_w,
				popup_h
			};
		}

		zdraw::rgba* m_color_ptr{ nullptr };
		float m_open_anim{ 0.0f };
		bool m_closing{ false };
		bool m_fully_closed{ false };
		bool m_changed{ false };
	};

	class color_picker_overlay : public overlay
	{
	public:
		color_picker_overlay( widget_id id, const zui::rect& anchor, zdraw::rgba* color_ptr, bool show_alpha ) : overlay{ id, anchor }, m_color_ptr{ color_ptr }, m_show_alpha{ show_alpha }
		{
			if ( color_ptr )
			{
				const auto hsv_color = zui::rgb_to_hsv( *color_ptr );
				this->m_hue = hsv_color.h / 360.0f;
				this->m_saturation = hsv_color.s;
				this->m_value = hsv_color.v;
			}
		}

		[[nodiscard]] bool hit_test( float x, float y ) const override
		{
			const auto anchor = this->get_current_anchor( );
			return this->get_popup_rect( ).contains( x, y ) || anchor.contains( x, y );
		}

		bool process_input( const input_state& input ) override
		{
			if ( this->m_closing )
			{
				return false;
			}

			const auto anchor = this->get_current_anchor( );
			const auto popup = this->get_popup_rect( );

			if ( input.mouse_clicked && !popup.contains( input.mouse_x, input.mouse_y ) && !anchor.contains( input.mouse_x, input.mouse_y ) )
			{
				this->m_closing = true;
				return true;
			}

			const auto pad{ 8.0f };
			const auto alpha_bar_w{ 14.0f };
			const auto sv_size = this->m_show_alpha ? ( popup.w - pad * 3.0f - alpha_bar_w ) : ( popup.w - pad * 2.0f );
			const auto hue_h{ 14.0f };

			const auto sv_rect = zui::rect{ popup.x + pad, popup.y + pad, sv_size, sv_size };
			const auto hue_rect = zui::rect{ popup.x + pad, sv_rect.bottom( ) + pad, sv_size, hue_h };
			const auto alpha_rect = zui::rect{ sv_rect.right( ) + pad, popup.y + pad, alpha_bar_w, sv_size + pad + hue_h };

			if ( input.mouse_clicked )
			{
				if ( sv_rect.contains( input.mouse_x, input.mouse_y ) )
				{
					this->m_active_component = 1;
				}
				else if ( hue_rect.contains( input.mouse_x, input.mouse_y ) )
				{
					this->m_active_component = 2;
				}
				else if ( this->m_show_alpha && alpha_rect.contains( input.mouse_x, input.mouse_y ) )
				{
					this->m_active_component = 3;
				}
			}

			if ( input.mouse_released )
			{
				this->m_active_component = 0;
			}

			if ( input.mouse_down && this->m_active_component != 0 )
			{
				if ( this->m_active_component == 1 )
				{
					this->m_saturation = std::clamp( ( input.mouse_x - sv_rect.x ) / sv_rect.w, 0.0f, 1.0f );
					this->m_value = 1.0f - std::clamp( ( input.mouse_y - sv_rect.y ) / sv_rect.h, 0.0f, 1.0f );
				}
				else if ( this->m_active_component == 2 )
				{
					this->m_hue = std::clamp( ( input.mouse_x - hue_rect.x ) / hue_rect.w, 0.0f, 1.0f );
				}
				else if ( this->m_active_component == 3 )
				{
					const auto alpha_norm = 1.0f - std::clamp( ( input.mouse_y - alpha_rect.y ) / alpha_rect.h, 0.0f, 1.0f );
					if ( this->m_color_ptr )
					{
						this->m_color_ptr->a = static_cast< std::uint8_t >( alpha_norm * 255.0f );
					}
				}

				if ( this->m_color_ptr && this->m_active_component != 3 )
				{
					const auto new_color = zui::hsv_to_rgb( this->m_hue * 360.0f, this->m_saturation, this->m_value );
					this->m_color_ptr->r = new_color.r;
					this->m_color_ptr->g = new_color.g;
					this->m_color_ptr->b = new_color.b;
				}

				this->m_changed = true;
			}

			return popup.contains( input.mouse_x, input.mouse_y );
		}

		void render( const zui::style& style, const input_state& input ) override
		{
			( void )input;
			const auto dt = zdraw::get_delta_time( );
			const auto popup = this->get_popup_rect( );
			auto& dl = zdraw::get_draw_list( zdraw::draw_layer::topmost );

			const auto anim_speed = this->m_closing ? 18.0f : 16.0f;
			const auto target = this->m_closing ? 0.0f : 1.0f;
			this->m_open_anim = this->m_open_anim + ( target - this->m_open_anim ) * std::min( anim_speed * dt, 1.0f );

			if ( this->m_open_anim < 0.01f && this->m_closing )
			{
				this->m_fully_closed = true;
				return;
			}

			const auto ease_t = zui::ease::out_cubic( this->m_open_anim );
			const auto alpha_mult = ease_t;
			const auto scale = 0.95f + ease_t * 0.05f;

			const auto scaled_w = popup.w * scale;
			const auto scaled_h = popup.h * scale;
			const auto scaled_x = popup.x + ( popup.w - scaled_w ) * 0.5f;
			const auto scaled_y = popup.y + ( popup.h - scaled_h ) * 0.5f;

			auto bg_top = style.combo_popup_bg;
			bg_top.a = static_cast< std::uint8_t >( bg_top.a * alpha_mult );
			auto bg_bottom = zui::darken( bg_top, 0.9f );

			auto border_col = zui::lighten( style.combo_popup_border, 1.1f );
			border_col.a = static_cast< std::uint8_t >( border_col.a * alpha_mult );

			dl.add_rect_filled_multi_color( scaled_x, scaled_y, scaled_w, scaled_h, bg_top, bg_top, bg_bottom, bg_bottom );
			dl.add_rect( scaled_x, scaled_y, scaled_w, scaled_h, border_col );

			if ( ease_t < 0.15f )
			{
				return;
			}

			const auto content_alpha = std::clamp( ( ease_t - 0.15f ) / 0.85f, 0.0f, 1.0f );

			dl.push_clip_rect( scaled_x, scaled_y, scaled_x + scaled_w, scaled_y + scaled_h );

			const auto pad{ 8.0f };
			const auto alpha_bar_w{ 14.0f };
			const auto sv_size = this->m_show_alpha ? ( scaled_w - pad * 3.0f - alpha_bar_w ) : ( scaled_w - pad * 2.0f );
			const auto hue_h{ 14.0f };

			const auto sv_rect = zui::rect{ scaled_x + pad, scaled_y + pad, sv_size, sv_size };
			const auto hue_rect = zui::rect{ scaled_x + pad, sv_rect.bottom( ) + pad, sv_size, hue_h };
			const auto alpha_rect = zui::rect{ sv_rect.right( ) + pad, scaled_y + pad, alpha_bar_w, sv_size + pad + hue_h };

			this->render_sv_square( sv_rect, content_alpha );
			this->render_hue_bar( hue_rect, content_alpha );

			if ( this->m_show_alpha )
			{
				this->render_alpha_bar( alpha_rect, content_alpha );
			}

			this->render_cursors( sv_rect, hue_rect, alpha_rect, content_alpha );

			dl.pop_clip_rect( );
		}

		[[nodiscard]] bool should_close( ) const override { return this->m_closing; }
		[[nodiscard]] bool is_closed( ) const override { return this->m_fully_closed; }
		[[nodiscard]] bool was_changed( ) const { return this->m_changed; }
		void clear_changed( ) { this->m_changed = false; }

	private:
		[[nodiscard]] zui::rect get_popup_rect( ) const
		{
			const auto anchor = this->get_current_anchor( );
			const auto pad{ 8.0f };
			const auto alpha_bar_w{ 14.0f };
			const auto width = this->m_show_alpha ? 194.0f : ( 194.0f - alpha_bar_w - pad );

			return zui::rect
			{
				anchor.x,
				anchor.bottom( ) + 2.0f,
				width,
				194.0f
			};
		}

		void render_sv_square( const zui::rect& r, float alpha ) const
		{
			auto& dl = zdraw::get_draw_list( zdraw::draw_layer::topmost );

			for ( int y = 0; y < static_cast< int >( r.h ); ++y )
			{
				for ( int x = 0; x < static_cast< int >( r.w ); ++x )
				{
					const auto s = static_cast< float >( x ) / r.w;
					const auto v = 1.0f - static_cast< float >( y ) / r.h;

					auto color = zui::hsv_to_rgb( this->m_hue * 360.0f, s, v );
					color.a = static_cast< std::uint8_t >( color.a * alpha );

					dl.add_rect_filled( r.x + x, r.y + y, 1.0f, 1.0f, color );
				}
			}
		}

		void render_hue_bar( const zui::rect& r, float alpha ) const
		{
			auto& dl = zdraw::get_draw_list( zdraw::draw_layer::topmost );

			for ( int x = 0; x < static_cast< int >( r.w ); ++x )
			{
				const auto h = ( static_cast< float >( x ) / r.w ) * 360.0f;

				auto color = zui::hsv_to_rgb( h, 1.0f, 1.0f );
				color.a = static_cast< std::uint8_t >( color.a * alpha );

				dl.add_rect_filled( r.x + x, r.y, 1.0f, r.h, color );
			}
		}

		void render_alpha_bar( const zui::rect& r, float alpha ) const
		{
			auto& dl = zdraw::get_draw_list( zdraw::draw_layer::topmost );

			for ( int x = 0; x < static_cast< int >( r.w ); x += 6 )
			{
				for ( int y = 0; y < static_cast< int >( r.h ); y += 6 )
				{
					const auto is_dark = ( ( x / 6 ) + ( y / 6 ) ) % 2 == 0;
					auto checker_col = is_dark ? zdraw::rgba{ 180, 180, 180, 255 } : zdraw::rgba{ 220, 220, 220, 255 };

					checker_col.a = static_cast< std::uint8_t >( checker_col.a * alpha );
					dl.add_rect_filled( r.x + x, r.y + y, std::min( 6.0f, r.w - x ), std::min( 6.0f, r.h - y ), checker_col );
				}
			}

			if ( this->m_color_ptr )
			{
				for ( int y = 0; y < static_cast< int >( r.h ); ++y )
				{
					const auto a = static_cast< std::uint8_t >( ( 1.0f - static_cast< float >( y ) / r.h ) * 255.0f * alpha );
					const auto color = zdraw::rgba{ this->m_color_ptr->r, this->m_color_ptr->g, this->m_color_ptr->b, a };
					dl.add_rect_filled( r.x, r.y + y, r.w, 1.0f, color );
				}
			}
		}

		void render_cursors( const zui::rect& sv, const zui::rect& hue, const zui::rect& alpha, float content_alpha ) const
		{
			auto& dl = zdraw::get_draw_list( zdraw::draw_layer::topmost );

			const auto sv_x = sv.x + this->m_saturation * sv.w;
			const auto sv_y = sv.y + ( 1.0f - this->m_value ) * sv.h;
			auto white_col = zdraw::rgba{ 255, 255, 255, static_cast< std::uint8_t >( 255 * content_alpha ) };
			auto black_col = zdraw::rgba{ 0, 0, 0, static_cast< std::uint8_t >( 255 * content_alpha ) };
			dl.add_rect( sv_x - 4.0f, sv_y - 4.0f, 8.0f, 8.0f, white_col, 2.0f );
			dl.add_rect( sv_x - 3.0f, sv_y - 3.0f, 6.0f, 6.0f, black_col, 3.0f );

			const auto hue_x = hue.x + this->m_hue * hue.w;
			dl.add_rect( hue_x - 2.0f, hue.y - 2.0f, 4.0f, hue.h + 4.0f, white_col, 2.0f );
			dl.add_rect( hue_x - 1.0f, hue.y - 1.0f, 2.0f, hue.h + 2.0f, black_col, 1.0f );

			if ( this->m_show_alpha && this->m_color_ptr )
			{
				const auto alpha_y = alpha.y + ( 1.0f - this->m_color_ptr->a / 255.0f ) * alpha.h;
				dl.add_rect( alpha.x - 2.0f, alpha_y - 2.0f, alpha.w + 4.0f, 4.0f, white_col, 2.0f );
				dl.add_rect( alpha.x - 1.0f, alpha_y - 1.0f, alpha.w + 2.0f, 2.0f, black_col, 1.0f );
			}
		}

		zdraw::rgba* m_color_ptr{ nullptr };
		float m_hue{ 0.0f };
		float m_saturation{ 0.0f };
		float m_value{ 1.0f };
		int m_active_component{ 0 };
		bool m_closing{ false };
		bool m_changed{ false };
		float m_open_anim{ 0.0f };
		bool m_fully_closed{ false };
		bool m_show_alpha{ true };
	};

	struct text_input_state
	{
		std::size_t cursor_pos{ 0 };
		std::size_t selection_start{ 0 };
		std::size_t selection_end{ 0 };
		float cursor_blink_timer{ 0.0f };
		float scroll_offset{ 0.0f };
		float cursor_anim_x{ 0.0f };
		float cursor_anim_start_x{ 0.0f };
		float cursor_anim_target_x{ 0.0f };
		float cursor_anim_progress{ 1.0f };
		bool cursor_anim_initialized{ false };
		std::unordered_map<int, float> key_repeat_timers{};
		std::unordered_map<int, bool> key_was_down{};
	};

	std::unordered_map<widget_id, text_input_state>& get_text_input_states( )
	{
		static std::unordered_map<widget_id, text_input_state> states{};
		return states;
	}
 
	std::unordered_map<widget_id, std::string>& get_slider_input_buffers( )
	{
		static std::unordered_map<widget_id, std::string> buffers{};
		return buffers;
	}

	bool is_printable_char( int vk )
	{
		return ( vk >= 0x20 && vk <= 0x7E );
	}

	char vk_to_char( int vk, bool shift_held )
	{
		if ( vk >= 'A' && vk <= 'Z' )
		{
			const auto caps_on = ( GetKeyState( VK_CAPITAL ) & 0x0001 ) != 0;
			const auto upper = shift_held != caps_on;
			return upper ? static_cast< char >( vk ) : static_cast< char >( vk + 32 );
		}

		if ( vk >= '0' && vk <= '9' )
		{
			if ( shift_held )
			{
				constexpr const char* shift_nums = ")!@#$%^&*(";
				return shift_nums[ vk - '0' ];
			}

			return static_cast< char >( vk );
		}

		if ( !shift_held )
		{
			switch ( vk )
			{
			case VK_SPACE: return ' ';
			case VK_OEM_1: return ';';
			case VK_OEM_PLUS: return '=';
			case VK_OEM_COMMA: return ',';
			case VK_OEM_MINUS: return '-';
			case VK_OEM_PERIOD: return '.';
			case VK_OEM_2: return '/';
			case VK_OEM_3: return '`';
			case VK_OEM_4: return '[';
			case VK_OEM_5: return '\\';
			case VK_OEM_6: return ']';
			case VK_OEM_7: return '\'';
			default: break;
			}
		}
		else
		{
			switch ( vk )
			{
			case VK_SPACE: return ' ';
			case VK_OEM_1: return ':';
			case VK_OEM_PLUS: return '+';
			case VK_OEM_COMMA: return '<';
			case VK_OEM_MINUS: return '_';
			case VK_OEM_PERIOD: return '>';
			case VK_OEM_2: return '?';
			case VK_OEM_3: return '~';
			case VK_OEM_4: return '{';
			case VK_OEM_5: return '|';
			case VK_OEM_6: return '}';
			case VK_OEM_7: return '"';
			default: break;
			}
		}

		return '\0';
	}

	class popup_overlay : public overlay
	{
	public:
		popup_overlay( widget_id id, const zui::rect& anchor, float width )
			: overlay{ id, anchor }, m_width{ width } {
		}

		[[nodiscard]] bool hit_test( float x, float y ) const override
		{
			const auto anchor = this->get_current_anchor( );
			return this->get_popup_rect( ).contains( x, y ) || anchor.contains( x, y );
		}

		bool process_input( const input_state& input ) override
		{
			if ( this->m_closing )
			{
				return false;
			}

			const auto anchor = this->get_current_anchor( );
			const auto popup = this->get_popup_rect( );

			if ( input.mouse_clicked && !popup.contains( input.mouse_x, input.mouse_y ) &&
				!anchor.contains( input.mouse_x, input.mouse_y ) )
			{
				this->m_closing = true;
				return true;
			}

			return popup.contains( input.mouse_x, input.mouse_y );
		}

		void render( const zui::style&, const input_state& ) override {}

		void tick_animation( )
		{
			const auto dt = zdraw::get_delta_time( );

			const auto anim_speed = this->m_closing ? 16.0f : 14.0f;
			const auto target = this->m_closing ? 0.0f : 1.0f;
			this->m_open_anim = this->m_open_anim + ( target - this->m_open_anim ) * std::min( anim_speed * dt, 1.0f );

			if ( this->m_open_anim < 0.01f && this->m_closing )
			{
				this->m_fully_closed = true;
			}
		}

		[[nodiscard]] bool is_closing( ) const noexcept { return this->m_closing; }
		[[nodiscard]] bool should_close( ) const override { return this->m_closing; }
		[[nodiscard]] bool is_closed( ) const override { return this->m_fully_closed; }

		void set_content_height( float h ) { this->m_content_height = h; }
		[[nodiscard]] float get_open_anim( ) const noexcept { return this->m_open_anim; }

		[[nodiscard]] zui::rect get_popup_rect( ) const
		{
			const auto anchor = this->get_current_anchor( );
			const auto clamped_h = std::min( this->m_content_height, 400.0f );

			return zui::rect
			{
				anchor.x,
				anchor.bottom( ) + 4.0f,
				this->m_width,
				clamped_h
			};
		}

	private:
		float m_width{ 200.0f };
		float m_content_height{ 100.0f };
		float m_open_anim{ 0.0f };
		bool m_closing{ false };
		bool m_fully_closed{ false };
	};

	struct style_var_backup
	{
		zui::style_var var{};
		float prev{};
	};

	struct style_color_backup
	{
		zui::style_color idx{};
		zdraw::rgba prev{};
	};

	class context
	{
	public:
		bool initialize( HWND hwnd )
		{
			this->m_input.set_hwnd( hwnd );
			return hwnd != nullptr;
		}

		void begin_frame( )
		{
			this->m_input.update( );
			this->m_windows.clear( );
			this->m_id_stack.clear( );
		}

		void end_frame( )
		{
			if ( this->m_overlays.has_active_overlay( ) )
			{
				this->m_overlays.process_input( this->m_input.current( ) );
			}

			this->m_overlays.render( this->m_style, this->m_input.current( ) );
			this->m_overlays.cleanup( );

			if ( this->m_input.mouse_released( ) )
			{
				this->m_active_window_id = invalid_id;
				this->m_active_resize_id = invalid_id;
				this->m_active_slider_id = invalid_id;
			}

			this->m_input.clear_frame_events( );
		}

		[[nodiscard]] input_manager& input( ) noexcept { return this->m_input; }
		[[nodiscard]] const input_manager& input( ) const noexcept { return this->m_input; }

		[[nodiscard]] zui::style& get_style( ) noexcept { return this->m_style; }
		[[nodiscard]] const zui::style& get_style( ) const noexcept { return this->m_style; }

		[[nodiscard]] animation_manager& anims( ) noexcept { return this->m_anims; }

		[[nodiscard]] overlay_manager& overlays( ) noexcept { return this->m_overlays; }
		[[nodiscard]] const overlay_manager& overlays( ) const noexcept { return this->m_overlays; }

		[[nodiscard]] string_interner& strings( ) noexcept { return this->m_string_interner; }
		[[nodiscard]] std::string& truncation_buffer( ) noexcept { return this->m_truncation_scratch; }

		[[nodiscard]] bool overlay_blocking_input( ) const
		{
			if ( this->m_inside_popup )
			{
				return this->m_overlays.has_active_overlay_except( this->m_active_popup_id );
			}

			return this->m_overlays.has_active_overlay( );
		}

		void push_window( zui::window_state&& state ) { this->m_windows.push_back( std::move( state ) ); }
		void pop_window( ) { if ( !this->m_windows.empty( ) ) { this->m_windows.pop_back( ); } }

		[[nodiscard]] zui::window_state* current_window( )
		{
			return this->m_windows.empty( ) ? nullptr : &this->m_windows.back( );
		}

		[[nodiscard]] const zui::window_state* current_window( ) const
		{
			return this->m_windows.empty( ) ? nullptr : &this->m_windows.back( );
		}

		void push_id( widget_id id )
		{
			this->m_id_stack.push_back( id );
		}

		void pop_id( )
		{
			if ( !this->m_id_stack.empty( ) )
			{
				this->m_id_stack.pop_back( );
			}
		}

		[[nodiscard]] static std::string_view get_display_label( std::string_view label )
		{
			const auto pos = label.find( "##" );
			if ( pos != std::string_view::npos )
			{
				return label.substr( 0, pos );
			}

			return label;
		}

		[[nodiscard]] widget_id generate_id( std::string_view label )
		{
			const auto interned = this->m_string_interner.intern( label );
			auto h = fnv1a64( interned.data( ), interned.size( ) );

			for ( const auto& parent_id : this->m_id_stack )
			{
				h ^= parent_id;
				h *= 1099511628211ull;
			}

			return h;
		}

		widget_id m_active_window_id{ invalid_id };
		widget_id m_active_resize_id{ invalid_id };
		widget_id m_active_slider_id{ invalid_id };
		widget_id m_active_keybind_id{ invalid_id };
		widget_id m_active_text_input_id{ invalid_id };

		widget_id m_active_popup_id{ invalid_id };
		std::optional<zdraw::draw_layer> m_draw_layer_override{};
		bool m_inside_popup{ false };

		std::vector<style_var_backup> m_style_var_stack{};
		std::vector<style_color_backup> m_style_color_stack{};
		std::unordered_map<widget_id, float> m_scroll_states{};
		std::unordered_map<widget_id, float> m_group_box_heights{};

	private:
		static std::uintptr_t fnv1a64( const void* p, std::size_t n )
		{
			const auto* b = static_cast< const unsigned char* >( p );
			auto h{ 14695981039346656037ull };

			while ( n-- )
			{
				h ^= *b++;
				h *= 1099511628211ull;
			}

			return h;
		}

		input_manager m_input{};
		zui::style m_style{};
		animation_manager m_anims{};
		overlay_manager m_overlays{};
		string_interner m_string_interner{};
		std::string m_truncation_scratch{};

		std::vector<zui::window_state> m_windows{};
		std::vector<widget_id> m_id_stack{};
	};

	context& ctx( )
	{
		static context instance{};
		return instance;
	}

	zdraw::draw_list& active_draw_list( )
	{
		if ( ctx( ).m_draw_layer_override.has_value( ) )
		{
			return zdraw::get_draw_list( *ctx( ).m_draw_layer_override );
		}

		return zdraw::get_draw_list( );
	}
}

namespace zui {

	namespace detail {

		float mouse_x( ) { return ctx( ).input( ).mouse_x( ); }
		float mouse_y( ) { return ctx( ).input( ).mouse_y( ); }
		bool mouse_down( ) { return ctx( ).input( ).mouse_down( ); }
		bool mouse_clicked( ) { return ctx( ).input( ).mouse_clicked( ); }
		bool mouse_released( ) { return ctx( ).input( ).mouse_released( ); }
		bool mouse_hovered( const rect& r ) { return ctx( ).input( ).hovered( r ); }
		window_state* get_current_window( ) { return ctx( ).current_window( ); }
		bool overlay_blocking_input( ) { return ctx( ).overlay_blocking_input( ); }

	} // namespace detail

	zdraw::rgba lighten( const zdraw::rgba& color, float factor ) noexcept
	{
		return zdraw::rgba
		{
			static_cast< std::uint8_t >( std::min( color.r * factor, 255.0f ) ),
			static_cast< std::uint8_t >( std::min( color.g * factor, 255.0f ) ),
			static_cast< std::uint8_t >( std::min( color.b * factor, 255.0f ) ),
			color.a
		};
	}

	zdraw::rgba darken( const zdraw::rgba& color, float factor ) noexcept
	{
		return zdraw::rgba
		{
			static_cast< std::uint8_t >( color.r * factor ),
			static_cast< std::uint8_t >( color.g * factor ),
			static_cast< std::uint8_t >( color.b * factor ),
			color.a
		};
	}

	zdraw::rgba alpha( const zdraw::rgba& color, float a ) noexcept
	{
		return zdraw::rgba{ color.r, color.g, color.b, static_cast< std::uint8_t >( a * 255.0f ) };
	}

	zdraw::rgba alpha( const zdraw::rgba& color, std::uint8_t a ) noexcept
	{
		return zdraw::rgba{ color.r, color.g, color.b, a };
	}

	zdraw::rgba lerp( const zdraw::rgba& a, const zdraw::rgba& b, float t ) noexcept
	{
		return zdraw::rgba
		{
			static_cast< std::uint8_t >( a.r + ( b.r - a.r ) * t ),
			static_cast< std::uint8_t >( a.g + ( b.g - a.g ) * t ),
			static_cast< std::uint8_t >( a.b + ( b.b - a.b ) * t ),
			static_cast< std::uint8_t >( a.a + ( b.a - a.a ) * t )
		};
	}

	static std::string_view maybe_truncate_text( std::string_view text, float max_width, std::string& scratch_buffer )
	{
		const auto [text_w, text_h] = zdraw::measure_text( text );
		if ( text_w <= max_width )
		{
			return text;
		}

		const auto [ellipsis_w, ellipsis_h] = zdraw::measure_text( "..." );
		if ( max_width < ellipsis_w )
		{
			return "...";
		}

		const auto available_for_text = max_width - ellipsis_w;

		std::size_t left{ 0 };
		std::size_t right = text.size( );
		std::size_t best_fit{ 0 };

		while ( left <= right && right != static_cast< std::size_t >( -1 ) )
		{
			const auto mid = left + ( right - left ) / 2;
			const auto candidate = text.substr( 0, mid );
			const auto [cand_w, cand_h] = zdraw::measure_text( candidate );

			if ( cand_w <= available_for_text )
			{
				best_fit = mid;
				left = mid + 1;
			}
			else
			{
				right = mid - 1;
			}
		}

		scratch_buffer.clear( );
		scratch_buffer.reserve( best_fit + 3 );
		scratch_buffer.assign( text.data( ), best_fit );
		scratch_buffer.append( "..." );

		return scratch_buffer;
	}

	hsv rgb_to_hsv( const zdraw::rgba& color ) noexcept
	{
		const auto r = color.r / 255.0f;
		const auto g = color.g / 255.0f;
		const auto b = color.b / 255.0f;

		const auto max_c = std::max( { r, g, b } );
		const auto min_c = std::min( { r, g, b } );
		const auto delta = max_c - min_c;

		hsv result{};
		result.v = max_c;
		result.s = ( max_c != 0.0f ) ? ( delta / max_c ) : 0.0f;
		result.a = color.a / 255.0f;

		if ( delta != 0.0f )
		{
			if ( max_c == r )
			{
				result.h = 60.0f * std::fmod( ( g - b ) / delta, 6.0f );
			}
			else if ( max_c == g )
			{
				result.h = 60.0f * ( 2.0f + ( b - r ) / delta );
			}
			else
			{
				result.h = 60.0f * ( 4.0f + ( r - g ) / delta );
			}
		}

		if ( result.h < 0.0f )
		{
			result.h += 360.0f;
		}

		return result;
	}

	zdraw::rgba hsv_to_rgb( const hsv& color ) noexcept
	{
		const auto c = color.v * color.s;
		const auto h_prime = color.h / 60.0f;
		const auto x = c * ( 1.0f - std::abs( std::fmod( h_prime, 2.0f ) - 1.0f ) );
		const auto m = color.v - c;

		float r1, g1, b1;

		if ( h_prime < 1.0f ) { r1 = c; g1 = x; b1 = 0.0f; }
		else if ( h_prime < 2.0f ) { r1 = x; g1 = c; b1 = 0.0f; }
		else if ( h_prime < 3.0f ) { r1 = 0.0f; g1 = c; b1 = x; }
		else if ( h_prime < 4.0f ) { r1 = 0.0f; g1 = x; b1 = c; }
		else if ( h_prime < 5.0f ) { r1 = x; g1 = 0.0f; b1 = c; }
		else { r1 = c; g1 = 0.0f; b1 = x; }

		return zdraw::rgba
		{
			static_cast< std::uint8_t >( ( r1 + m ) * 255.0f ),
			static_cast< std::uint8_t >( ( g1 + m ) * 255.0f ),
			static_cast< std::uint8_t >( ( b1 + m ) * 255.0f ),
			static_cast< std::uint8_t >( color.a * 255.0f )
		};
	}

	zdraw::rgba hsv_to_rgb( float h, float s, float v, float a ) noexcept
	{
		return hsv_to_rgb( hsv{ h, s, v, a } );
	}

	namespace ease {

		float linear( float t ) noexcept
		{
			return t;
		}

		float in_quad( float t ) noexcept
		{
			return t * t;
		}

		float out_quad( float t ) noexcept
		{
			return t * ( 2.0f - t );
		}

		float in_out_quad( float t ) noexcept
		{
			return t < 0.5f ? 2.0f * t * t : -1.0f + ( 4.0f - 2.0f * t ) * t;
		}

		float in_cubic( float t ) noexcept
		{
			return t * t * t;
		}

		float out_cubic( float t ) noexcept
		{
			const auto f = t - 1.0f;
			return f * f * f + 1.0f;
		}

		float in_out_cubic( float t ) noexcept
		{
			return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow( -2.0f * t + 2.0f, 3.0f ) / 2.0f;
		}

		float smoothstep( float t ) noexcept
		{
			return t * t * ( 3.0f - 2.0f * t );
		}

	} // namespace ease

	bool initialize( HWND hwnd )
	{
		return ctx( ).initialize( hwnd );
	}

	void begin( )
	{
		ctx( ).begin_frame( );
	}

	void end( )
	{
		ctx( ).end_frame( );
	}

	bool process_wndproc_message( UINT msg, WPARAM wparam, LPARAM lparam )
	{
		return ctx( ).input( ).process_wndproc_message( msg, wparam, lparam );
	}

	style& get_style( )
	{
		return ctx( ).get_style( );
	}

	zdraw::rgba get_accent_color( )
	{
		return ctx( ).get_style( ).accent;
	}

	void push_style_var( style_var var, float value )
	{
		float* ptr{ nullptr };
		auto& s = ctx( ).get_style( );

		switch ( var )
		{
		case style_var::window_padding_x: ptr = &s.window_padding_x; break;
		case style_var::window_padding_y: ptr = &s.window_padding_y; break;
		case style_var::item_spacing_x: ptr = &s.item_spacing_x; break;
		case style_var::item_spacing_y: ptr = &s.item_spacing_y; break;
		case style_var::frame_padding_x: ptr = &s.frame_padding_x; break;
		case style_var::frame_padding_y: ptr = &s.frame_padding_y; break;
		case style_var::border_thickness: ptr = &s.border_thickness; break;
		case style_var::group_box_title_height: ptr = &s.group_box_title_height; break;
		case style_var::checkbox_size: ptr = &s.checkbox_size; break;
		case style_var::slider_height: ptr = &s.slider_height; break;
		case style_var::keybind_height: ptr = &s.keybind_height; break;
		case style_var::combo_height: ptr = &s.combo_height; break;
		case style_var::combo_item_height: ptr = &s.combo_item_height; break;
		case style_var::color_picker_swatch_width: ptr = &s.color_picker_swatch_width; break;
		case style_var::color_picker_swatch_height: ptr = &s.color_picker_swatch_height; break;
		default: return;
		}

		if ( ptr )
		{
			ctx( ).m_style_var_stack.push_back( { var, *ptr } );
			*ptr = value;
		}
	}

	void pop_style_var( int count )
	{
		auto& s = ctx( ).get_style( );
		auto& stack = ctx( ).m_style_var_stack;

		for ( int i = 0; i < count && !stack.empty( ); ++i )
		{
			const auto& backup{ stack.back( ) };
			float* ptr{ nullptr };

			switch ( backup.var )
			{
			case style_var::window_padding_x: ptr = &s.window_padding_x; break;
			case style_var::window_padding_y: ptr = &s.window_padding_y; break;
			case style_var::item_spacing_x: ptr = &s.item_spacing_x; break;
			case style_var::item_spacing_y: ptr = &s.item_spacing_y; break;
			case style_var::frame_padding_x: ptr = &s.frame_padding_x; break;
			case style_var::frame_padding_y: ptr = &s.frame_padding_y; break;
			case style_var::border_thickness: ptr = &s.border_thickness; break;
			case style_var::group_box_title_height: ptr = &s.group_box_title_height; break;
			case style_var::checkbox_size: ptr = &s.checkbox_size; break;
			case style_var::slider_height: ptr = &s.slider_height; break;
			case style_var::keybind_height: ptr = &s.keybind_height; break;
			case style_var::combo_height: ptr = &s.combo_height; break;
			case style_var::combo_item_height: ptr = &s.combo_item_height; break;
			case style_var::color_picker_swatch_width: ptr = &s.color_picker_swatch_width; break;
			case style_var::color_picker_swatch_height: ptr = &s.color_picker_swatch_height; break;
			default: break;
			}

			if ( ptr )
			{
				*ptr = backup.prev;
			}

			stack.pop_back( );
		}
	}

	void push_style_color( style_color idx, const zdraw::rgba& col )
	{
		zdraw::rgba* ptr{ nullptr };
		auto& s = ctx( ).get_style( );

		switch ( idx )
		{
		case style_color::window_bg: ptr = &s.window_bg; break;
		case style_color::window_border: ptr = &s.window_border; break;
		case style_color::nested_bg: ptr = &s.nested_bg; break;
		case style_color::nested_border: ptr = &s.nested_border; break;
		case style_color::group_box_bg: ptr = &s.group_box_bg; break;
		case style_color::group_box_border: ptr = &s.group_box_border; break;
		case style_color::group_box_title_text: ptr = &s.group_box_title_text; break;
		case style_color::checkbox_bg: ptr = &s.checkbox_bg; break;
		case style_color::checkbox_border: ptr = &s.checkbox_border; break;
		case style_color::checkbox_check: ptr = &s.checkbox_check; break;
		case style_color::slider_bg: ptr = &s.slider_bg; break;
		case style_color::slider_border: ptr = &s.slider_border; break;
		case style_color::slider_fill: ptr = &s.slider_fill; break;
		case style_color::slider_grab: ptr = &s.slider_grab; break;
		case style_color::slider_grab_active: ptr = &s.slider_grab_active; break;
		case style_color::button_bg: ptr = &s.button_bg; break;
		case style_color::button_border: ptr = &s.button_border; break;
		case style_color::button_hovered: ptr = &s.button_hovered; break;
		case style_color::button_active: ptr = &s.button_active; break;
		case style_color::keybind_bg: ptr = &s.keybind_bg; break;
		case style_color::keybind_border: ptr = &s.keybind_border; break;
		case style_color::keybind_waiting: ptr = &s.keybind_waiting; break;
		case style_color::combo_bg: ptr = &s.combo_bg; break;
		case style_color::combo_border: ptr = &s.combo_border; break;
		case style_color::combo_arrow: ptr = &s.combo_arrow; break;
		case style_color::combo_hovered: ptr = &s.combo_hovered; break;
		case style_color::combo_popup_bg: ptr = &s.combo_popup_bg; break;
		case style_color::combo_popup_border: ptr = &s.combo_popup_border; break;
		case style_color::combo_item_hovered: ptr = &s.combo_item_hovered; break;
		case style_color::combo_item_selected: ptr = &s.combo_item_selected; break;
		case style_color::color_picker_bg: ptr = &s.color_picker_bg; break;
		case style_color::color_picker_border: ptr = &s.color_picker_border; break;
		case style_color::text_input_bg: ptr = &s.text_input_bg; break;
		case style_color::text_input_border: ptr = &s.text_input_border; break;
		case style_color::popup_bg: ptr = &s.popup_bg; break;
		case style_color::popup_border: ptr = &s.popup_border; break;
		case style_color::text: ptr = &s.text; break;
		case style_color::accent: ptr = &s.accent; break;
		default: return;
		}

		if ( ptr )
		{
			ctx( ).m_style_color_stack.push_back( { idx, *ptr } );
			*ptr = col;
		}
	}

	void pop_style_color( int count )
	{
		auto& s = ctx( ).get_style( );
		auto& stack = ctx( ).m_style_color_stack;

		for ( int i = 0; i < count && !stack.empty( ); ++i )
		{
			const auto& backup{ stack.back( ) };
			zdraw::rgba* ptr{ nullptr };

			switch ( backup.idx )
			{
			case style_color::window_bg: ptr = &s.window_bg; break;
			case style_color::window_border: ptr = &s.window_border; break;
			case style_color::nested_bg: ptr = &s.nested_bg; break;
			case style_color::nested_border: ptr = &s.nested_border; break;
			case style_color::group_box_bg: ptr = &s.group_box_bg; break;
			case style_color::group_box_border: ptr = &s.group_box_border; break;
			case style_color::group_box_title_text: ptr = &s.group_box_title_text; break;
			case style_color::checkbox_bg: ptr = &s.checkbox_bg; break;
			case style_color::checkbox_border: ptr = &s.checkbox_border; break;
			case style_color::checkbox_check: ptr = &s.checkbox_check; break;
			case style_color::slider_bg: ptr = &s.slider_bg; break;
			case style_color::slider_border: ptr = &s.slider_border; break;
			case style_color::slider_fill: ptr = &s.slider_fill; break;
			case style_color::slider_grab: ptr = &s.slider_grab; break;
			case style_color::slider_grab_active: ptr = &s.slider_grab_active; break;
			case style_color::button_bg: ptr = &s.button_bg; break;
			case style_color::button_border: ptr = &s.button_border; break;
			case style_color::button_hovered: ptr = &s.button_hovered; break;
			case style_color::button_active: ptr = &s.button_active; break;
			case style_color::keybind_bg: ptr = &s.keybind_bg; break;
			case style_color::keybind_border: ptr = &s.keybind_border; break;
			case style_color::keybind_waiting: ptr = &s.keybind_waiting; break;
			case style_color::combo_bg: ptr = &s.combo_bg; break;
			case style_color::combo_border: ptr = &s.combo_border; break;
			case style_color::combo_arrow: ptr = &s.combo_arrow; break;
			case style_color::combo_hovered: ptr = &s.combo_hovered; break;
			case style_color::combo_popup_bg: ptr = &s.combo_popup_bg; break;
			case style_color::combo_popup_border: ptr = &s.combo_popup_border; break;
			case style_color::combo_item_hovered: ptr = &s.combo_item_hovered; break;
			case style_color::combo_item_selected: ptr = &s.combo_item_selected; break;
			case style_color::color_picker_bg: ptr = &s.color_picker_bg; break;
			case style_color::color_picker_border: ptr = &s.color_picker_border; break;
			case style_color::text_input_bg: ptr = &s.text_input_bg; break;
			case style_color::text_input_border: ptr = &s.text_input_border; break;
			case style_color::popup_bg: ptr = &s.popup_bg; break;
			case style_color::popup_border: ptr = &s.popup_border; break;
			case style_color::text: ptr = &s.text; break;
			case style_color::accent: ptr = &s.accent; break;
			default: break;
			}

			if ( ptr )
			{
				*ptr = backup.prev;
			}

			stack.pop_back( );
		}
	}

	namespace
	{
		rect item_add( float w, float h )
		{
			const auto win = ctx( ).current_window( );
			if ( !win )
			{
				return { 0, 0, 0, 0 };
			}

			if ( win->line_height > 0.0f )
			{
				win->cursor_y += win->line_height + ctx( ).get_style( ).item_spacing_y;
			}

			rect r{ win->cursor_x, win->cursor_y, w, h };
			win->last_item = r;
			win->line_height = h;

			const auto item_bottom = win->cursor_y + h;
			win->content_height = std::max( win->content_height, item_bottom );

			return r;
		}

		[[nodiscard]] rect to_absolute( const rect& local )
		{
			const auto win = ctx( ).current_window( );
			if ( !win )
			{
				return local;
			}

			const auto scroll_offset = win->scroll_y;
			return rect{ local.x + win->bounds.x, local.y + win->bounds.y - scroll_offset, local.w, local.h };
		}

		void process_scroll_wheel( const rect& bounds, float content_height, float visible_start_y, float& scroll_y )
		{
			const auto& style = ctx( ).get_style( );
			const auto& input = ctx( ).input( );

			const auto visible_height = bounds.h - visible_start_y;
			const auto total_content_height = content_height + style.window_padding_y;
			const auto max_scroll = std::max( 0.0f, total_content_height - visible_height );

			if ( input.hovered( bounds ) && !ctx( ).overlay_blocking_input( ) )
			{
				const auto scroll_delta = input.scroll_delta( );
				if ( scroll_delta != 0.0f )
				{
					scroll_y -= scroll_delta * 30.0f;
				}
			}

			scroll_y = std::clamp( scroll_y, 0.0f, max_scroll );
		}
	}

	bool begin_window( std::string_view title, float& x, float& y, float& w, float& h, bool resizable, float min_w, float min_h )
	{
		const auto id = ctx( ).generate_id( title );
		auto abs = rect{ x, y, w, h };

		const auto& style = ctx( ).get_style( );
		const auto& input = ctx( ).input( );

		constexpr auto grip_size{ 16.0f };
		const auto grip_rect = rect{ abs.right( ) - grip_size, abs.bottom( ) - grip_size, grip_size, grip_size };

		const auto grip_hovered = resizable && input.hovered( grip_rect );
		const auto window_hovered = input.hovered( abs );

		if ( grip_hovered && input.mouse_clicked( ) && ctx( ).m_active_resize_id == invalid_id )
		{
			ctx( ).m_active_resize_id = id;
		}

		if ( ctx( ).m_active_resize_id == id && input.mouse_down( ) )
		{
			w = std::max( min_w, w + input.mouse_delta_x( ) );
			h = std::max( min_h, h + input.mouse_delta_y( ) );
			abs.w = w;
			abs.h = h;
		}
		else if ( window_hovered && !grip_hovered && input.mouse_clicked( ) && ctx( ).m_active_window_id == invalid_id && ctx( ).m_active_slider_id == invalid_id && ctx( ).m_active_resize_id == invalid_id && ctx( ).m_active_text_input_id == invalid_id && !ctx( ).overlay_blocking_input( ) )
		{
			ctx( ).m_active_window_id = id;
		}

		if ( ctx( ).m_active_window_id == id && input.mouse_down( ) && ctx( ).m_active_slider_id == invalid_id && ctx( ).m_active_resize_id == invalid_id && ctx( ).m_active_text_input_id == invalid_id )
		{
			x += input.mouse_delta_x( );
			y += input.mouse_delta_y( );
			abs.x = x;
			abs.y = y;
		}

		auto& scroll_states = ctx( ).m_scroll_states;
		if ( scroll_states.find( id ) == scroll_states.end( ) )
		{
			scroll_states[ id ] = 0.0f;
		}

		window_state state{};
		state.title.assign( title.begin( ), title.end( ) );
		state.bounds = abs;
		state.cursor_x = style.window_padding_x;
		state.cursor_y = style.window_padding_y;
		state.line_height = 0.0f;
		state.is_child = false;
		state.scroll_y = scroll_states[ id ];
		state.content_height = style.window_padding_y;
		state.visible_start_y = 0.0f;
		state.scroll_id = id;

		ctx( ).push_window( std::move( state ) );

		const auto base_color = style.window_bg;
		const auto col_tl = lighten( base_color, 1.15f );
		const auto col_br = darken( base_color, 0.85f );

		active_draw_list( ).add_rect_filled_multi_color( abs.x, abs.y, abs.w, abs.h, col_tl, base_color, col_br, base_color );

		if ( style.border_thickness > 0.0f )
		{
			active_draw_list( ).add_rect( abs.x, abs.y, abs.w, abs.h, style.window_border, style.border_thickness );
		}

		active_draw_list( ).push_clip_rect( abs.x, abs.y, abs.right( ), abs.bottom( ) );
		ctx( ).push_id( id );

		return true;
	}

	void end_window( )
	{
		const auto win = ctx( ).current_window( );
		active_draw_list( ).pop_clip_rect( );

		if ( win && win->scroll_id != invalid_id )
		{
			auto& scroll_y = ctx( ).m_scroll_states[ win->scroll_id ];
			scroll_y = win->scroll_y;

			process_scroll_wheel( win->bounds, win->content_height, win->visible_start_y, scroll_y );

			ctx( ).m_scroll_states[ win->scroll_id ] = scroll_y;
			win->scroll_y = scroll_y;
		}

		ctx( ).pop_window( );
		ctx( ).pop_id( );
	}

	bool begin_nested_window( std::string_view title, float w, float h )
	{
		const auto parent = ctx( ).current_window( );
		if ( !parent )
		{
			return false;
		}

		const auto id = ctx( ).generate_id( title );
		const auto& style = ctx( ).get_style( );

		const auto local = item_add( w, h );
		const auto abs = rect{ parent->bounds.x + local.x, parent->bounds.y + local.y - parent->scroll_y, w, h };

		auto& scroll_states = ctx( ).m_scroll_states;
		if ( scroll_states.find( id ) == scroll_states.end( ) )
		{
			scroll_states[ id ] = 0.0f;
		}

		window_state state{};
		state.title.assign( title.begin( ), title.end( ) );
		state.bounds = abs;
		state.cursor_x = style.window_padding_x;
		state.cursor_y = style.window_padding_y;
		state.line_height = 0.0f;
		state.is_child = true;
		state.scroll_y = scroll_states[ id ];
		state.content_height = style.window_padding_y;
		state.visible_start_y = 0.0f;
		state.scroll_id = id;

		ctx( ).push_window( std::move( state ) );

		const auto base_color = style.nested_bg;
		const auto col_tl = lighten( base_color, 1.15f );
		const auto col_br = darken( base_color, 0.85f );

		active_draw_list( ).add_rect_filled_multi_color( abs.x, abs.y, abs.w, abs.h, col_tl, base_color, col_br, base_color );

		if ( style.border_thickness > 0.0f )
		{
			active_draw_list( ).add_rect( abs.x, abs.y, abs.w, abs.h, style.nested_border, style.border_thickness );
		}

		active_draw_list( ).push_clip_rect( abs.x, abs.y, abs.right( ), abs.bottom( ) );
		ctx( ).push_id( id );

		return true;
	}

	void end_nested_window( )
	{
		end_window( );
	}

	bool begin_group_box( std::string_view title, float w, float h )
	{
		const auto parent = ctx( ).current_window( );
		if ( !parent )
		{
			return false;
		}

		const auto id = ctx( ).generate_id( title );
		const auto& style = ctx( ).get_style( );
		const auto title_h = style.group_box_title_height;
		auto actual_h = h;

		if ( h <= 0.0f )
		{
			auto& height_cache = ctx( ).m_group_box_heights;
			if ( height_cache.find( id ) != height_cache.end( ) )
			{
				actual_h = height_cache[ id ];
			}
			else
			{
				actual_h = 100.0f;
			}
		}

		const auto max_available_h = parent->bounds.h - ( parent->cursor_y + parent->scroll_y ) - style.window_padding_y;
		if ( max_available_h > 0.0f )
		{
			actual_h = std::min( actual_h, max_available_h );
		}

		const auto local = item_add( w, actual_h );
		const auto abs = rect{ parent->bounds.x + local.x, parent->bounds.y + local.y - parent->scroll_y, w, actual_h };

		const auto border_y = abs.y + title_h * 0.5f;
		const auto box_height = abs.h - title_h * 0.5f;

		active_draw_list( ).add_rect_filled( abs.x, border_y, abs.w, box_height, style.group_box_bg );
		active_draw_list( ).add_rect( abs.x, border_y, abs.w, box_height, style.group_box_border, style.border_thickness );

		if ( !title.empty( ) )
		{
			const auto text_x = abs.x + style.window_padding_x;
			const auto pad{ 4.0f };
			const auto max_title_width = abs.w - style.window_padding_x * 2.0f - pad * 2.0f;

			std::string title_str( title.begin( ), title.end( ) );
			auto [title_w, title_h_measured] = zdraw::measure_text( title_str );

			if ( title_w > max_title_width )
			{
				const auto [ellipsis_w, ellipsis_h] = zdraw::measure_text( "..." );
				const auto available_for_text = max_title_width - ellipsis_w;

				std::string truncated{};
				for ( char c : title_str )
				{
					std::string test = truncated + c;
					const auto [test_w, test_h] = zdraw::measure_text( test );

					if ( test_w > available_for_text )
					{
						break;
					}

					truncated = test;
				}

				title_str = truncated + "...";
				title_w = zdraw::measure_text( title_str ).first;
			}

			const auto gap_start = text_x - pad;
			const auto gap_end = text_x + title_w + pad;

			active_draw_list( ).add_rect_filled( gap_start, abs.y, gap_end - gap_start, title_h, style.group_box_bg );
			active_draw_list( ).add_rect( gap_start, abs.y, gap_end - gap_start, title_h, style.group_box_border, style.border_thickness );

			const auto text_y = abs.y + ( title_h - title_h_measured ) * 0.5f;
			active_draw_list( ).add_text( text_x, text_y, title_str, zdraw::get_font( ), style.group_box_title_text );
		}

		auto& scroll_states = ctx( ).m_scroll_states;
		if ( scroll_states.find( id ) == scroll_states.end( ) )
		{
			scroll_states[ id ] = 0.0f;
		}

		window_state state{};
		state.title.assign( title.begin( ), title.end( ) );
		state.bounds = abs;
		state.cursor_x = style.window_padding_x;
		state.cursor_y = title_h + style.window_padding_y;
		state.line_height = 0.0f;
		state.is_child = true;
		state.scroll_y = scroll_states[ id ];
		state.content_height = title_h + style.window_padding_y;
		state.visible_start_y = title_h * 0.5f;
		state.scroll_id = id;

		ctx( ).push_window( std::move( state ) );
		active_draw_list( ).push_clip_rect( abs.x, abs.y + title_h, abs.right( ), abs.bottom( ) );
		ctx( ).push_id( id );

		return true;
	}

	void end_group_box( )
	{
		const auto win = ctx( ).current_window( );
		if ( win && win->scroll_id != invalid_id )
		{
			const auto& style = ctx( ).get_style( );
			const auto final_height = win->content_height + style.window_padding_y;
			ctx( ).m_group_box_heights[ win->scroll_id ] = final_height;
		}

		end_window( );
	}

	void same_line( float offset_x )
	{
		const auto win = ctx( ).current_window( );
		if ( !win || win->line_height <= 0.0f )
		{
			return;
		}

		const auto spacing = ( offset_x == 0.0f ) ? ctx( ).get_style( ).item_spacing_x : offset_x;
		win->cursor_x = win->last_item.x + win->last_item.w + spacing;
		win->cursor_y = win->last_item.y;
		win->line_height = 0.0f;
	}

	void new_line( )
	{
		const auto win = ctx( ).current_window( );
		if ( !win )
		{
			return;
		}

		if ( win->line_height > 0.0f )
		{
			win->cursor_y += win->line_height + ctx( ).get_style( ).item_spacing_y;
		}

		win->cursor_x = ctx( ).get_style( ).window_padding_x;
		win->line_height = 0.0f;
	}

	void spacing( float amount )
	{
		const auto win = ctx( ).current_window( );
		if ( win )
		{
			if ( amount <= 0.0f )
			{
				amount = ctx( ).get_style( ).item_spacing_y;
			}

			win->cursor_y += amount;
		}
	}

	void indent( float amount )
	{
		if ( const auto win = ctx( ).current_window( ) )
		{
			if ( amount <= 0.0f )
			{
				amount = ctx( ).get_style( ).window_padding_x;
			}

			win->cursor_x += amount;
		}
	}

	void unindent( float amount )
	{
		if ( const auto win = ctx( ).current_window( ) )
		{
			if ( amount <= 0.0f )
			{
				amount = ctx( ).get_style( ).window_padding_x;
			}

			win->cursor_x = std::max( ctx( ).get_style( ).window_padding_x, win->cursor_x - amount );
		}
	}

	void separator( )
	{
		const auto win = ctx( ).current_window( );
		if ( !win )
		{
			return;
		}

		const auto& style = ctx( ).get_style( );
		const auto sep_h{ 1.0f };
		const auto sep_pad = style.item_spacing_y * 0.5f;

		const auto work_max_x = win->bounds.w - style.window_padding_x;
		const auto next_x = win->cursor_x;

		if ( win->line_height > 0.0f )
		{
			win->cursor_y += win->line_height + style.item_spacing_y;
			win->line_height = 0.0f;
		}

		const auto avail_w = std::max( 0.0f, work_max_x - next_x );
		const auto local = item_add( avail_w, sep_h + sep_pad * 2.0f );
		const auto abs = to_absolute( local );

		auto sep_col = style.nested_border;
		sep_col.a = static_cast< std::uint8_t >( sep_col.a * 0.5f );

		active_draw_list( ).add_line( abs.x, abs.y + sep_pad, abs.x + avail_w, abs.y + sep_pad, sep_col, sep_h );
	}

	std::pair<float, float> get_content_region_avail( )
	{
		const auto win = ctx( ).current_window( );
		if ( !win )
		{
			return { 0.0f, 0.0f };
		}

		const auto& style = ctx( ).get_style( );
		const auto work_max_x = win->bounds.w - style.window_padding_x;
		const auto work_max_y = win->bounds.h - style.window_padding_y;

		auto next_x = win->cursor_x;
		auto next_y = win->cursor_y;

		if ( win->line_height > 0.0f )
		{
			next_y += win->line_height + style.item_spacing_y;
		}

		const auto avail_w = std::max( 0.0f, work_max_x - next_x );
		const auto avail_h = std::max( 0.0f, work_max_y - next_y );
		return { avail_w, avail_h };
	}

	float calc_item_width( int count )
	{
		const auto [avail_w, avail_h] = get_content_region_avail( );

		if ( count <= 0 )
		{
			return avail_w;
		}

		const auto spacing = ctx( ).get_style( ).item_spacing_x;
		return ( avail_w - spacing * ( count - 1 ) ) / static_cast< float >( count );
	}

	void set_cursor_pos( float x, float y )
	{
		if ( const auto win = ctx( ).current_window( ) )
		{
			win->cursor_x = x;
			win->cursor_y = y;
			win->line_height = 0.0f;
		}
	}

	std::pair<float, float> get_cursor_pos( )
	{
		const auto win = ctx( ).current_window( );
		return win ? std::make_pair( win->cursor_x, win->cursor_y ) : std::make_pair( 0.0f, 0.0f );
	}

	void text( std::string_view label )
	{
		const auto win = ctx( ).current_window( );
		if ( !win )
		{
			return;
		}

		const auto [label_w, label_h] = zdraw::measure_text( label );
		const auto local = item_add( label_w, label_h );
		const auto abs = to_absolute( local );

		active_draw_list( ).add_text( abs.x, abs.y, label, zdraw::get_font( ), ctx( ).get_style( ).text );
	}

	void text_colored( std::string_view label, const zdraw::rgba& color )
	{
		const auto win = ctx( ).current_window( );
		if ( !win )
		{
			return;
		}

		const auto [label_w, label_h] = zdraw::measure_text( label );
		const auto local = item_add( label_w, label_h );
		const auto abs = to_absolute( local );

		active_draw_list( ).add_text( abs.x, abs.y, label, zdraw::get_font( ), color );
	}

	void text_gradient( std::string_view label, const zdraw::rgba& color_left, const zdraw::rgba& color_right )
	{
		const auto win = ctx( ).current_window( );
		if ( !win )
		{
			return;
		}

		const auto [label_w, label_h] = zdraw::measure_text( label );
		const auto local = item_add( label_w, label_h );
		const auto abs = to_absolute( local );

		active_draw_list( ).add_text_multi_color( abs.x, abs.y, label, zdraw::get_font( ), color_left, color_right, color_right, color_left );
	}

	void text_gradient_vertical( std::string_view label, const zdraw::rgba& color_top, const zdraw::rgba& color_bottom )
	{
		const auto win = ctx( ).current_window( );
		if ( !win )
		{
			return;
		}

		const auto [label_w, label_h] = zdraw::measure_text( label );
		const auto local = item_add( label_w, label_h );
		const auto abs = to_absolute( local );

		active_draw_list( ).add_text_multi_color( abs.x, abs.y, label, zdraw::get_font( ), color_top, color_top, color_bottom, color_bottom );
	}

	void text_gradient_four( std::string_view label, const zdraw::rgba& color_tl, const zdraw::rgba& color_tr, const zdraw::rgba& color_br, const zdraw::rgba& color_bl )
	{
		const auto win = ctx( ).current_window( );
		if ( !win )
		{
			return;
		}

		const auto [label_w, label_h] = zdraw::measure_text( label );
		const auto local = item_add( label_w, label_h );
		const auto abs = to_absolute( local );

		active_draw_list( ).add_text_multi_color( abs.x, abs.y, label, zdraw::get_font( ), color_tl, color_tr, color_br, color_bl );
	}

	bool button( std::string_view label, float w, float h )
	{
		const auto win = ctx( ).current_window( );
		if ( !win )
		{
			return false;
		}

		const auto id = ctx( ).generate_id( label );
		const auto& style = ctx( ).get_style( );
		const auto& input = ctx( ).input( );
		auto& anims = ctx( ).anims( );

		const auto local = item_add( w, h );
		const auto abs = to_absolute( local );

		const auto can_interact = !ctx( ).overlay_blocking_input( );
		const auto hovered = can_interact && input.hovered( abs );
		const auto held = hovered && input.mouse_down( );
		const auto pressed = hovered && input.mouse_clicked( );

		const auto hover_anim = anims.animate( id, hovered ? 1.0f : 0.0f, 12.0f );
		const auto active_anim = anims.animate( id + 1, held ? 1.0f : 0.0f, 15.0f );

		auto bg_col = lerp( style.button_bg, style.button_hovered, hover_anim );
		bg_col = lerp( bg_col, style.button_active, active_anim );

		const auto border_col = lerp( style.button_border, lighten( style.button_border, 1.2f ), hover_anim );
		const auto col_top = bg_col;
		const auto col_bottom = darken( bg_col, 0.85f );

		active_draw_list( ).add_rect_filled_multi_color( abs.x, abs.y, abs.w, abs.h, col_top, col_top, col_bottom, col_bottom );
		active_draw_list( ).add_rect( abs.x, abs.y, abs.w, abs.h, border_col );

		const auto display_label = context::get_display_label( label );
		if ( !display_label.empty( ) )
		{
			const auto available_w = abs.w - style.frame_padding_x * 2.0f;
			const auto label_text = maybe_truncate_text( display_label, available_w, ctx( ).truncation_buffer( ) );
			auto [label_w, label_h] = zdraw::measure_text( label_text );
			const auto text_x = abs.x + ( abs.w - label_w ) * 0.5f;
			const auto text_y = abs.y + ( abs.h - label_h ) * 0.5f;

			active_draw_list( ).add_text( text_x, text_y, label_text, zdraw::get_font( ), style.text );
		}

		return pressed;
	}

	bool checkbox( std::string_view label, bool& v )
	{
		const auto win = ctx( ).current_window( );
		if ( !win )
		{
			return false;
		}

		const auto id = ctx( ).generate_id( label );
		const auto display_label = context::get_display_label( label );
		const auto& style = ctx( ).get_style( );
		const auto& input = ctx( ).input( );
		auto& anims = ctx( ).anims( );

		const auto check_size = style.checkbox_size;
		const auto local = item_add( check_size, check_size );
		const auto abs = to_absolute( local );

		const auto [label_w, label_h] = zdraw::measure_text( display_label );
		const auto full_width = !label.empty( ) ? ( abs.w + style.item_spacing_x + label_w ) : abs.w;
		const auto extended = rect{ abs.x, abs.y, full_width, abs.h };

		const auto can_interact = !ctx( ).overlay_blocking_input( );
		const auto hovered = can_interact && input.hovered( extended );

		auto changed{ false };

		if ( hovered && input.mouse_clicked( ) )
		{
			v = !v;
			changed = true;
		}

		const auto check_anim = anims.animate( id, v ? 1.0f : 0.0f, 8.0f );
		const auto hover_anim = anims.animate( id + 1, hovered ? 1.0f : 0.0f, 10.0f );
		const auto ease_t = ease::smoothstep( check_anim );

		auto border_col = style.checkbox_border;
		if ( hover_anim > 0.01f )
		{
			border_col = lerp( border_col, style.checkbox_check, hover_anim * 0.3f );
		}

		active_draw_list( ).add_rect_filled( abs.x, abs.y, abs.w, abs.h, style.checkbox_bg );
		active_draw_list( ).add_rect( abs.x, abs.y, abs.w, abs.h, border_col );

		if ( ease_t > 0.01f )
		{
			constexpr auto pad{ 2.0f };
			const auto inner_w = abs.w - pad * 2.0f;
			const auto inner_h = abs.h - pad * 2.0f;

			const auto scale = 0.6f + ease_t * 0.4f;
			const auto scaled_w = inner_w * scale;
			const auto scaled_h = inner_h * scale;
			const auto fill_x = abs.x + pad + ( inner_w - scaled_w ) * 0.5f;
			const auto fill_y = abs.y + pad + ( inner_h - scaled_h ) * 0.5f;

			const auto check_col = style.checkbox_check;
			const auto col_top = zdraw::rgba
			{
				static_cast< std::uint8_t >( std::min( check_col.r * ( 1.1f + ease_t * 0.15f ), 255.0f ) ),
				static_cast< std::uint8_t >( std::min( check_col.g * ( 1.1f + ease_t * 0.15f ), 255.0f ) ),
				static_cast< std::uint8_t >( std::min( check_col.b * ( 1.1f + ease_t * 0.15f ), 255.0f ) ),
				static_cast< std::uint8_t >( check_col.a * ease_t )
			};

			const auto col_bottom = zdraw::rgba
			{
				static_cast< std::uint8_t >( check_col.r * 0.75f ),
				static_cast< std::uint8_t >( check_col.g * 0.75f ),
				static_cast< std::uint8_t >( check_col.b * 0.75f ),
				static_cast< std::uint8_t >( check_col.a * ease_t )
			};

			active_draw_list( ).add_rect_filled_multi_color( fill_x, fill_y, scaled_w, scaled_h, col_top, col_top, col_bottom, col_bottom );

			if ( ease_t < 0.4f && ease_t > 0.0f )
			{
				const auto pulse_t = ease_t / 0.4f;
				const auto ring_expand = 3.0f * pulse_t;
				const auto ring_alpha = static_cast< std::uint8_t >( 60 * ( 1.0f - pulse_t ) * ease_t );

				auto ring_col = check_col;
				ring_col.a = ring_alpha;

				active_draw_list( ).add_rect( abs.x - ring_expand, abs.y - ring_expand, abs.w + ring_expand * 2.0f, abs.h + ring_expand * 2.0f, ring_col, 1.5f );
			}
		}

		if ( !display_label.empty( ) )
		{
			const auto text_x = abs.x + abs.w + style.item_spacing_x;
			const auto text_y = abs.y + ( check_size - label_h ) * 0.5f;

			const auto available_w = win->bounds.right( ) - text_x - style.window_padding_x;
			const auto label_text = maybe_truncate_text( display_label, available_w, ctx( ).truncation_buffer( ) );

			active_draw_list( ).add_text( text_x, text_y, label_text, zdraw::get_font( ), style.text );
		}

		return changed;
	}

	namespace
	{
		template<typename T>
		bool slider_impl( std::string_view label, T& v, T v_min, T v_max, std::string_view format )
		{
			const auto win = ctx( ).current_window( );
			if ( !win )
			{
				return false;
			}

			const auto id = ctx( ).generate_id( label );
			const auto display_label = context::get_display_label( label );
			const auto& style = ctx( ).get_style( );
			const auto& input = ctx( ).input( );
			auto& anims = ctx( ).anims( );

			const auto [avail_w, avail_h] = get_content_region_avail( );
			const auto slider_width = avail_w;

			char value_buf[ 64 ]{};
			if constexpr ( std::is_floating_point_v<T> )
			{
				std::snprintf( value_buf, sizeof( value_buf ), format.data( ), v );
			}
			else
			{
				std::snprintf( value_buf, sizeof( value_buf ), format.data( ), static_cast< int >( v ) );
			}

			const auto [label_w, label_h] = zdraw::measure_text( display_label );
			const auto [value_w, value_h] = zdraw::measure_text( value_buf );

			const auto text_height = std::max( label_h, value_h );
			const auto track_height = style.slider_height;
			const auto knob_width{ 10.0f };
			const auto knob_height = track_height + 6.0f;
			const auto spacing = style.item_spacing_y * 0.25f;
			const auto total_height = text_height + spacing + knob_height;

			const auto local = item_add( slider_width, total_height );
			const auto abs = to_absolute( local );

			const auto track_y = abs.y + text_height + spacing + ( knob_height - track_height ) * 0.5f;
			const auto track_rect = rect{ abs.x, track_y, slider_width, track_height };

			const auto knob_min_x = abs.x;
			const auto knob_max_x = abs.x + slider_width - knob_width;
			const auto knob_y = abs.y + text_height + spacing;

			const auto hit_rect = rect{ abs.x, knob_y, slider_width, knob_height };
			const auto can_interact = !ctx( ).overlay_blocking_input( );
			const auto hovered = can_interact && input.hovered( hit_rect );

			if ( hovered && input.mouse_clicked( ) && ctx( ).m_active_slider_id == invalid_id )
			{
				if ( input.is_key_down( VK_CONTROL ) )
				{
					ctx( ).m_active_text_input_id = id;
					auto& buffer = get_slider_input_buffers( )[ id ];
					char buf[ 64 ]{};
					if constexpr ( std::is_floating_point_v<T> )
						std::snprintf( buf, sizeof( buf ), format.data( ), v );
					else
						std::snprintf( buf, sizeof( buf ), format.data( ), static_cast< int >( v ) );
					buffer = buf;
				}
				else
				{
					ctx( ).m_active_slider_id = id;
				}
			}

			const auto is_active = ctx( ).m_active_slider_id == id;
			const auto is_manual_input = ctx( ).m_active_text_input_id == id;
			auto changed{ false };

			if ( is_manual_input )
			{
				auto& buffer = get_slider_input_buffers( )[ id ];
				
				// Get or create cursor position state for this slider
				static std::unordered_map<widget_id, std::size_t> cursor_positions;
				auto& cursor_pos = cursor_positions[ id ];
				
				// Handle text input inline within the slider area
				const auto& input_state = input.current( );
				const auto dt = zdraw::get_delta_time( );
				
				// Check for Enter or Escape to finish input
				bool finish_input = false;
				for ( const auto vk : input_state.key_presses( ) )
				{
					if ( vk == VK_RETURN )
					{
						finish_input = true;
						break;
					}
					if ( vk == VK_ESCAPE )
					{
						finish_input = true;
						break;
					}
				}
				
				// Check if clicked outside
				if ( input.mouse_clicked( ) && !hit_rect.contains( input.mouse_x( ), input.mouse_y( ) ) )
				{
					finish_input = true;
				}
				
				// Handle arrow keys for cursor movement
				for ( const auto vk : input_state.key_presses( ) )
				{
					if ( vk == VK_LEFT )
					{
						if ( cursor_pos > 0 )
							cursor_pos--;
						break;
					}
					if ( vk == VK_RIGHT )
					{
						if ( cursor_pos < buffer.size( ) )
							cursor_pos++;
						break;
					}
				}
				
				// Handle backspace
				for ( const auto vk : input_state.key_presses( ) )
				{
					if ( vk == VK_BACK )
					{
						if ( cursor_pos > 0 )
						{
							buffer.erase( cursor_pos - 1, 1 );
							cursor_pos--;
							changed = true;
						}
						break;
					}
				}
				
				// Handle delete
				for ( const auto vk : input_state.key_presses( ) )
				{
					if ( vk == VK_DELETE )
					{
						if ( cursor_pos < buffer.size( ) )
						{
							buffer.erase( cursor_pos, 1 );
							changed = true;
						}
						break;
					}
				}
				
				// Process character input
				for ( const auto wch : input_state.chars( ) )
				{
					const auto ch = static_cast< char >( wch );
					// Allow digits, minus, period
					if ( ( ch >= '0' && ch <= '9' ) || ch == '-' || ch == '.' )
					{
						if ( buffer.size( ) < 16 )
						{
							buffer.insert( cursor_pos, 1, ch );
							cursor_pos++;
							changed = true;
						}
					}
				}
				
				// Clamp cursor position
				cursor_pos = std::min( cursor_pos, buffer.size( ) );
				
				if ( finish_input )
				{
					try {
						if constexpr ( std::is_floating_point_v<T> )
							v = std::clamp( std::stof( buffer ), static_cast<float>(v_min), static_cast<float>(v_max) );
						else
							v = std::clamp( std::stoi( buffer ), static_cast<int>(v_min), static_cast<int>(v_max) );
						changed = true;
					} catch ( ... ) { }
					ctx( ).m_active_text_input_id = invalid_id;
					cursor_positions.erase( id );
					get_slider_input_buffers( ).erase( id );
					return changed;
				}
				
				// Render the text input in place of the value text
				auto [input_w, input_h] = zdraw::measure_text( buffer );
				const auto value_col = style.accent;
				const auto text_x = abs.x + slider_width - input_w;
				active_draw_list( ).add_text( text_x, abs.y, buffer, zdraw::get_font( ), value_col );
				
				// Draw cursor blink at the correct position
				static float cursor_blink = 0.0f;
				cursor_blink += dt;
				if ( std::fmod( cursor_blink, 1.0f ) < 0.5f )
				{
					// Calculate cursor X position based on cursor_pos
					auto [cursor_offset_w, cursor_offset_h] = cursor_pos == 0 ? std::pair{ 0.0f, 0.0f } : zdraw::measure_text( std::string_view{ buffer.data( ), cursor_pos } );
					const auto cursor_x = text_x + cursor_offset_w;
					active_draw_list( ).add_rect_filled( cursor_x, abs.y, 1.0f, input_h, style.text );
				}
				
				return changed;
			}

			if ( is_active && input.mouse_down( ) && can_interact )
			{
				const auto mouse_normalized = std::clamp( ( input.mouse_x( ) - knob_min_x - knob_width * 0.5f ) / ( knob_max_x - knob_min_x ), 0.0f, 1.0f );

				if constexpr ( std::is_integral_v<T> )
				{
					v = static_cast< T >( v_min + mouse_normalized * ( v_max - v_min ) );
				}
				else
				{
					v = v_min + mouse_normalized * ( v_max - v_min );
				}

				changed = true;
			}

			if ( hovered && can_interact )
			{
				const auto& input_state = input.current( );
				const auto key_presses = input_state.key_presses( );

				for ( const auto vk : key_presses )
				{
					if ( vk == VK_LEFT )
					{
						if constexpr ( std::is_integral_v<T> )
						{
							v = std::max( v_min, v - 1 );
						}
						else
						{
							const auto step = ( v_max - v_min ) * 0.01f;
							v = std::max( v_min, v - step );
						}

						changed = true;
					}
					else if ( vk == VK_RIGHT )
					{
						if constexpr ( std::is_integral_v<T> )
						{
							v = std::min( v_max, v + 1 );
						}
						else
						{
							const auto step = ( v_max - v_min ) * 0.01f;
							v = std::min( v_max, v + step );
						}

						changed = true;
					}
				}
			}

			auto& value_anim = anims.get( id );
			value_anim = value_anim + ( static_cast< float >( v ) - value_anim ) * std::min( 20.0f * zdraw::get_delta_time( ), 1.0f );

			auto& hover_anim = anims.get( id + 1000000 );
			const auto hover_target = ( hovered || is_active ) ? 1.0f : 0.0f;
			hover_anim = hover_anim + ( hover_target - hover_anim ) * std::min( 12.0f * zdraw::get_delta_time( ), 1.0f );

			auto& active_anim = anims.get( id + 2000000 );
			const auto active_target = is_active ? 1.0f : 0.0f;
			active_anim = active_anim + ( active_target - active_anim ) * std::min( 15.0f * zdraw::get_delta_time( ), 1.0f );

			const auto range = static_cast< float >( v_max - v_min );
			const auto normalized_pos = ( value_anim - static_cast< float >( v_min ) ) / range;
			const auto knob_x = knob_min_x + ( knob_max_x - knob_min_x ) * normalized_pos;

			if ( !display_label.empty( ) )
			{
				const auto available_label_w = slider_width - value_w - style.item_spacing_x;
				const auto label_text = maybe_truncate_text( display_label, available_label_w, ctx( ).truncation_buffer( ) );
				active_draw_list( ).add_text( abs.x, abs.y, label_text, zdraw::get_font( ), style.text );
			}

			const auto value_col = lerp( style.text, lighten( style.slider_fill, 1.2f ), hover_anim * 0.35f );
			active_draw_list( ).add_text( abs.x + slider_width - value_w, abs.y, value_buf, zdraw::get_font( ), value_col );

			const auto track_bg = darken( style.slider_bg, 0.7f );
			active_draw_list( ).add_rect_filled( track_rect.x, track_rect.y, track_rect.w, track_rect.h, track_bg );

			const auto shadow_col = zdraw::rgba{ 0, 0, 0, static_cast< std::uint8_t >( 40 + hover_anim * 10 ) };
			active_draw_list( ).add_rect_filled_multi_color( track_rect.x + 1.0f, track_rect.y + 1.0f, track_rect.w - 2.0f, track_rect.h * 0.4f, shadow_col, shadow_col, alpha( shadow_col, 0.0f ), alpha( shadow_col, 0.0f ) );

			const auto border_col = lerp( style.slider_border, lighten( style.slider_fill, 0.6f ), hover_anim * 0.25f );
			active_draw_list( ).add_rect( track_rect.x, track_rect.y, track_rect.w, track_rect.h, border_col );

			constexpr auto fill_pad{ 2.0f };
			const auto fill_w = ( knob_x + knob_width * 0.5f ) - track_rect.x - fill_pad;

			if ( fill_w > 0.5f )
			{
				const auto fill_x = track_rect.x + fill_pad;
				const auto fill_y = track_rect.y + fill_pad;
				const auto fill_h = track_rect.h - fill_pad * 2.0f;

				const auto fill_left = lighten( style.slider_fill, 1.1f + hover_anim * 0.1f );
				const auto fill_right = darken( style.slider_fill, 0.85f );
				active_draw_list( ).add_rect_filled_multi_color( fill_x, fill_y, fill_w, fill_h, fill_left, fill_right, fill_right, fill_left );

				const auto shine = zdraw::rgba{ 255, 255, 255, static_cast< std::uint8_t >( 20 + hover_anim * 15 ) };
				active_draw_list( ).add_rect_filled_multi_color( fill_x, fill_y, fill_w, fill_h * 0.4f, shine, shine, alpha( shine, 0.0f ), alpha( shine, 0.0f ) );
			}

			const auto knob_shadow_offset = 1.0f + active_anim * 0.5f;
			const auto knob_shadow = zdraw::rgba{ 0, 0, 0, static_cast< std::uint8_t >( 50 + active_anim * 20 ) };
			active_draw_list( ).add_rect_filled( knob_x + 1.0f, knob_y + knob_shadow_offset, knob_width, knob_height, knob_shadow );

			const auto knob_bg = lerp( style.slider_bg, lighten( style.slider_bg, 1.2f ), hover_anim * 0.4f );
			active_draw_list( ).add_rect_filled( knob_x, knob_y, knob_width, knob_height, knob_bg );

			const auto knob_highlight = zdraw::rgba{ 255, 255, 255, static_cast< std::uint8_t >( 15 + hover_anim * 20 ) };
			active_draw_list( ).add_rect_filled_multi_color( knob_x + 1.0f, knob_y + 1.0f, knob_width - 2.0f, knob_height * 0.35f, knob_highlight, knob_highlight, alpha( knob_highlight, 0.0f ), alpha( knob_highlight, 0.0f ) );

			const auto knob_border = lerp( style.slider_border, lighten( style.slider_fill, 0.8f ), hover_anim * 0.5f );
			active_draw_list( ).add_rect( knob_x, knob_y, knob_width, knob_height, knob_border );

			return changed;
		}
	}

	bool slider_float( std::string_view label, float& v, float v_min, float v_max, std::string_view format )
	{
		return slider_impl( label, v, v_min, v_max, format );
	}

	bool slider_int( std::string_view label, int& v, int v_min, int v_max, std::string_view format )
	{
		return slider_impl( label, v, v_min, v_max, format );
	}

	bool keybind( std::string_view label, int& key )
	{
		const auto win = ctx( ).current_window( );
		if ( !win )
		{
			return false;
		}

		const auto id = ctx( ).generate_id( label );
		const auto display_label = context::get_display_label( label );
		const auto& style = ctx( ).get_style( );
		const auto& input = ctx( ).input( );
		auto& anims = ctx( ).anims( );

		const auto is_waiting = ( ctx( ).m_active_keybind_id == id );

		char button_text[ 64 ]{};
		if ( is_waiting )
		{
			std::snprintf( button_text, sizeof( button_text ), "..." );
		}
		else if ( key == 0 )
		{
			std::snprintf( button_text, sizeof( button_text ), "none" );
		}
		else
		{
			if ( key >= 0x41 && key <= 0x5A )
			{
				std::snprintf( button_text, sizeof( button_text ), "%c", key );
			}
			else if ( key >= 0x30 && key <= 0x39 )
			{
				std::snprintf( button_text, sizeof( button_text ), "%c", key );
			}
			else
			{
				auto key_name = "unknown";

				switch ( key )
				{
				case VK_LBUTTON: key_name = "lmb"; break;
				case VK_RBUTTON: key_name = "rmb"; break;
				case VK_MBUTTON: key_name = "mmb"; break;
				case VK_XBUTTON1: key_name = "mb4"; break;
				case VK_XBUTTON2: key_name = "mb5"; break;
				case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: key_name = "shift"; break;
				case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: key_name = "ctrl"; break;
				case VK_MENU: case VK_LMENU: case VK_RMENU: key_name = "alt"; break;
				case VK_SPACE: key_name = "space"; break;
				case VK_RETURN: key_name = "enter"; break;
				case VK_ESCAPE: key_name = "esc"; break;
				case VK_TAB: key_name = "tab"; break;
				case VK_CAPITAL: key_name = "caps"; break;
				case VK_INSERT: key_name = "insert"; break;
				case VK_DELETE: key_name = "delete"; break;
				case VK_HOME: key_name = "home"; break;
				case VK_END: key_name = "end"; break;
				case VK_PRIOR: key_name = "pgup"; break;
				case VK_NEXT: key_name = "pgdn"; break;
				case VK_LEFT: key_name = "left"; break;
				case VK_RIGHT: key_name = "right"; break;
				case VK_UP: key_name = "up"; break;
				case VK_DOWN: key_name = "down"; break;
				default: break;
				}

				std::snprintf( button_text, sizeof( button_text ), "%s", key_name );
			}
		}

		auto [label_w, label_h] = zdraw::measure_text( display_label );
		auto [button_text_w, button_text_h] = zdraw::measure_text( button_text );

		const auto button_width = style.keybind_width;
		const auto button_height = style.keybind_height;

		const auto total_w = !display_label.empty( ) ? ( button_width + style.item_spacing_x + label_w ) : button_width;
		const auto total_h = std::max( button_height, label_h );

		const auto local = item_add( total_w, total_h );
		const auto abs = to_absolute( local );

		const auto button_rect = zui::rect{ abs.x, abs.y, button_width, button_height };
		const auto can_interact = !ctx( ).overlay_blocking_input( );
		const auto hovered = can_interact && input.hovered( button_rect );
		const auto pressed = hovered && input.mouse_clicked( );

		if ( pressed )
		{
			ctx( ).m_active_keybind_id = id;
		}

		const auto dt = zdraw::get_delta_time( );
		const auto hover_anim = anims.animate( id, hovered ? 1.0f : 0.0f, 12.0f );
		const auto wait_anim = anims.animate( id + 1, is_waiting ? 1.0f : 0.0f, 12.0f );
		auto& pulse_anim = anims.get( id + 2 );

		if ( is_waiting )
		{
			pulse_anim += dt * 3.0f;
			if ( pulse_anim > 6.28318f )
			{
				pulse_anim -= 6.28318f;
			}
		}

		auto bg_col = lerp( style.keybind_bg, lighten( style.keybind_bg, 1.05f ), hover_anim );
		auto border_col = lerp( style.keybind_border, lighten( style.keybind_border, 1.3f ), hover_anim );

		active_draw_list( ).add_rect_filled( button_rect.x, button_rect.y, button_rect.w, button_rect.h, bg_col );

		if ( wait_anim > 0.01f )
		{
			constexpr auto pad{ 2.0f };
			const auto fill_x = button_rect.x + pad;
			const auto fill_y = button_rect.y + pad;
			const auto fill_w = button_rect.w - pad * 2.0f;
			const auto fill_h = button_rect.h - pad * 2.0f;

			const auto pulse_intensity = ( std::sin( pulse_anim ) * 0.5f + 0.5f ) * 0.4f + 0.6f;
			const auto shift = std::sin( pulse_anim * 0.5f ) * 0.5f + 0.5f;

			auto col_left = lighten( style.keybind_waiting, 1.0f + pulse_intensity * 0.3f );
			auto col_right = darken( style.keybind_waiting, 0.7f + pulse_intensity * 0.2f );

			col_left.a = static_cast< std::uint8_t >( col_left.a * wait_anim * 0.4f );
			col_right.a = static_cast< std::uint8_t >( col_right.a * wait_anim * 0.4f );

			if ( shift > 0.5f )
			{
				active_draw_list( ).add_rect_filled_multi_color( fill_x, fill_y, fill_w, fill_h, col_right, col_left, col_left, col_right );
			}
			else
			{
				active_draw_list( ).add_rect_filled_multi_color( fill_x, fill_y, fill_w, fill_h, col_left, col_right, col_right, col_left );
			}

			const auto waiting_border = lighten( style.keybind_waiting, 1.0f + pulse_intensity * 0.2f );
			border_col = lerp( border_col, waiting_border, wait_anim );
		}

		active_draw_list( ).add_rect( button_rect.x, button_rect.y, button_rect.w, button_rect.h, border_col );

		const auto text_x = button_rect.x + ( button_rect.w - button_text_w ) * 0.5f;
		const auto text_y = button_rect.y + ( button_rect.h - button_text_h ) * 0.5f;
		active_draw_list( ).add_text( text_x, text_y, button_text, zdraw::get_font( ), style.text );

		if ( !display_label.empty( ) )
		{
			const auto label_x = button_rect.x + button_width + style.item_spacing_x;
			const auto label_y = abs.y + ( total_h - label_h ) * 0.5f;

			const auto available_w = win->bounds.right( ) - label_x - style.window_padding_x;
			const auto label_text = maybe_truncate_text( display_label, available_w, ctx( ).truncation_buffer( ) );

			active_draw_list( ).add_text( label_x, label_y, label_text, zdraw::get_font( ), style.text );
		}

		if ( is_waiting )
		{
			if ( input.mouse_clicked( ) && !hovered )
			{
				key = VK_LBUTTON;
				ctx( ).m_active_keybind_id = invalid_id;
				return true;
			}

			if ( input.right_mouse_clicked( ) )
			{
				key = VK_RBUTTON;
				ctx( ).m_active_keybind_id = invalid_id;
				return true;
			}

			const auto& input_state = input.current( );
			const auto key_presses = input_state.key_presses( );

			if ( !key_presses.empty( ) )
			{
				const auto vk = key_presses.front( );
				if ( vk == VK_ESCAPE )
				{
					key = 0;
				}
				else
				{
					key = vk;
				}

				ctx( ).m_active_keybind_id = invalid_id;
				return true;
			}
		}

		return false;
	}

	bool combo( std::string_view label, int& current_item, const char* const items[ ], int items_count, float width )
	{
		const auto win = ctx( ).current_window( );
		if ( !win || items_count == 0 )
		{
			return false;
		}

		const auto id = ctx( ).generate_id( label );
		const auto display_label = context::get_display_label( label );
		const auto& style = ctx( ).get_style( );
		const auto& input = ctx( ).input( );
		auto& anims = ctx( ).anims( );
		auto& overlays = ctx( ).overlays( );

		const auto is_open = overlays.is_open( id );

		auto changed{ false };

		if ( auto* popup = dynamic_cast< combo_overlay* >( overlays.find( id ) ) )
		{
			changed = popup->was_changed( );
			popup->clear_changed( );
		}

		const auto [label_w, label_h] = zdraw::measure_text( display_label );

		if ( width <= 0.0f )
		{
			width = calc_item_width( 0 );
		}

		const auto combo_height = style.combo_height;
		const auto spacing = style.item_spacing_y * 0.25f;
		const auto total_height = label_h + spacing + combo_height;

		const auto local = item_add( width, total_height );
		const auto abs = to_absolute( local );

		if ( !display_label.empty( ) )
		{
			const auto label_text = maybe_truncate_text( display_label, width, ctx( ).truncation_buffer( ) );
			active_draw_list( ).add_text( abs.x, abs.y, label_text, zdraw::get_font( ), style.text );
		}

		const auto button_y = abs.y + label_h + spacing;
		const auto button_rect = zui::rect{ abs.x, button_y, width, combo_height };
		const auto hovered = input.hovered( button_rect );

		if ( auto* popup = dynamic_cast< combo_overlay* >( overlays.find( id ) ) )
		{
			popup->update_anchor( button_rect );
		}

		if ( hovered && input.mouse_clicked( ) && !ctx( ).overlay_blocking_input( ) )
		{
			if ( is_open )
			{
				overlays.close( id );
			}
			else
			{
				std::vector<std::string> item_strings{};
				item_strings.reserve( items_count );

				for ( int i = 0; i < items_count; ++i )
				{
					item_strings.emplace_back( items[ i ] );
				}

				overlays.add<combo_overlay>( id, button_rect, width, item_strings, &current_item );
			}
		}

		const auto hover_anim = anims.animate( id, ( hovered || is_open ) ? 1.0f : 0.0f, 15.0f );
		const auto bg_col = lerp( style.combo_bg, style.combo_hovered, hover_anim );
		const auto border_col = is_open ? lighten( style.combo_border, 1.3f ) : ( hovered ? lighten( style.combo_border, 1.15f ) : style.combo_border );

		active_draw_list( ).add_rect_filled( button_rect.x, button_rect.y, button_rect.w, button_rect.h, bg_col );
		active_draw_list( ).add_rect( button_rect.x, button_rect.y, button_rect.w, button_rect.h, border_col );

		const auto current_text = ( current_item >= 0 && current_item < items_count ) ? items[ current_item ] : "";
		const auto [text_w, text_h] = zdraw::measure_text( current_text );
		const auto text_x = button_rect.x + style.frame_padding_x;
		const auto text_y = button_rect.y + ( combo_height - text_h ) * 0.5f;
		active_draw_list( ).add_text( text_x, text_y, current_text, zdraw::get_font( ), style.text );

		constexpr auto arrow_size{ 6.0f };
		constexpr auto arrow_height{ 3.5f };
		const auto arrow_x = button_rect.right( ) - arrow_size - style.frame_padding_x - 4.0f;
		const auto arrow_y = button_rect.y + ( combo_height - arrow_height ) * 0.5f;
		const auto arrow_col = lerp( style.combo_arrow, lighten( style.combo_arrow, 1.3f ), hover_anim );

		if ( is_open )
		{
			const float points[ ] = { arrow_x, arrow_y + arrow_height, arrow_x + arrow_size * 0.5f, arrow_y, arrow_x + arrow_size, arrow_y + arrow_height };
			active_draw_list( ).add_polyline( points, arrow_col, false, 1.5f );
		}
		else
		{
			const float points[ ] = { arrow_x, arrow_y, arrow_x + arrow_size * 0.5f, arrow_y + arrow_height, arrow_x + arrow_size, arrow_y };
			active_draw_list( ).add_polyline( points, arrow_col, false, 1.5f );
		}

		return changed;
	}

	bool multicombo( std::string_view label, bool* selected_items, const char* const items[ ], int items_count, float width )
	{
		const auto win = ctx( ).current_window( );
		if ( !win || items_count == 0 || !selected_items )
		{
			return false;
		}

		const auto id = ctx( ).generate_id( label );
		const auto display_label = context::get_display_label( label );
		const auto& style = ctx( ).get_style( );
		const auto& input = ctx( ).input( );
		auto& anims = ctx( ).anims( );
		auto& overlays = ctx( ).overlays( );

		const auto is_open = overlays.is_open( id );
		auto changed{ false };

		if ( auto* popup = dynamic_cast< multicombo_overlay* >( overlays.find( id ) ) )
		{
			changed = popup->was_changed( );
			popup->clear_changed( );
			popup->clear_changed( );
		}

		const auto [label_w, label_h] = zdraw::measure_text( display_label );

		if ( width <= 0.0f )
		{
			width = calc_item_width( 0 );
		}

		const auto combo_height = style.combo_height;
		const auto spacing = style.item_spacing_y * 0.25f;
		const auto total_height = label_h + spacing + combo_height;

		const auto local = item_add( width, total_height );
		const auto abs = to_absolute( local );

		const auto win_clip_top = win->bounds.y + win->visible_start_y;
		const auto win_clip_bottom = win->bounds.bottom( );
		const auto win_clip_left = win->bounds.x;
		const auto win_clip_right = win->bounds.right( );

		if ( !display_label.empty( ) )
		{
			const auto label_text = maybe_truncate_text( display_label, width, ctx( ).truncation_buffer( ) );
			active_draw_list( ).add_text( abs.x, abs.y, label_text, zdraw::get_font( ), style.text );
		}

		const auto button_y = abs.y + label_h + spacing;
		const auto button_rect = zui::rect{ abs.x, button_y, width, combo_height };
		const auto hovered = input.hovered( button_rect );

		if ( auto* popup = dynamic_cast< multicombo_overlay* >( overlays.find( id ) ) )
		{
			popup->update_anchor( button_rect );
		}

		if ( hovered && input.mouse_clicked( ) && !ctx( ).overlay_blocking_input( ) )
		{
			if ( is_open )
			{
				overlays.close( id );
			}
			else
			{
				overlays.add<multicombo_overlay>( id, button_rect, width, items, items_count, selected_items );
			}
		}

		const auto hover_anim = anims.animate( id, ( hovered || is_open ) ? 1.0f : 0.0f, 15.0f );
		const auto bg_col = lerp( style.combo_bg, style.combo_hovered, hover_anim );
		const auto border_col = is_open ? lighten( style.combo_border, 1.3f ) : ( hovered ? lighten( style.combo_border, 1.15f ) : style.combo_border );

		active_draw_list( ).add_rect_filled( button_rect.x, button_rect.y, button_rect.w, button_rect.h, bg_col );
		active_draw_list( ).add_rect( button_rect.x, button_rect.y, button_rect.w, button_rect.h, border_col );

		std::string_view display_text{ "none" };
		std::string local_display_text;

		if ( auto* popup = dynamic_cast< multicombo_overlay* >( overlays.find( id ) ) )
		{
			display_text = popup->get_display_text( );
		}
		else
		{
			for ( int i = 0; i < items_count; ++i )
			{
				if ( selected_items[ i ] )
				{
					if ( !local_display_text.empty( ) )
					{
						local_display_text += ", ";
					}

					local_display_text += items[ i ];
				}
			}

			display_text = local_display_text.empty( ) ? std::string_view{ "none" } : std::string_view{ local_display_text };
		}

		const auto arrow_size{ 6.0f };
		const auto arrow_padding = style.frame_padding_x + 4.0f + arrow_size + 8.0f;
		const auto max_text_width = width - style.frame_padding_x - arrow_padding;

		auto [full_text_w, text_h] = zdraw::measure_text( display_text );
		const auto text_x = button_rect.x + style.frame_padding_x;
		const auto text_y = button_rect.y + ( combo_height - text_h ) * 0.5f;

		auto& scroll_states = get_multicombo_states( );
		auto& scroll_state = scroll_states[ id ];

		const auto dt = zdraw::get_delta_time( );
		const auto needs_scroll = full_text_w > max_text_width;

		if ( hovered && needs_scroll && !is_open )
		{
			if ( !scroll_state.was_hovered )
			{
				scroll_state.was_hovered = true;
				scroll_state.hover_time = 0.0f;
				scroll_state.scroll_offset = 0.0f;
			}

			scroll_state.hover_time += dt;

			constexpr auto scroll_delay{ 0.5f };
			constexpr auto scroll_speed{ 40.0f };
			constexpr auto pause_at_end{ 1.0f };

			if ( scroll_state.hover_time > scroll_delay )
			{
				const auto scroll_time = scroll_state.hover_time - scroll_delay;
				const auto gap{ 30.0f };
				const auto total_scroll_width = full_text_w + gap;
				const auto scroll_duration = total_scroll_width / scroll_speed;
				const auto cycle_duration = scroll_duration + pause_at_end;

				const auto cycle_time = std::fmod( scroll_time, cycle_duration );

				if ( cycle_time < scroll_duration )
				{
					scroll_state.scroll_offset = cycle_time * scroll_speed;
				}
				else
				{
					scroll_state.scroll_offset = 0.0f;
				}
			}
		}
		else
		{
			scroll_state.was_hovered = false;
			scroll_state.hover_time = 0.0f;
			scroll_state.scroll_offset = std::max( 0.0f, scroll_state.scroll_offset - dt * 100.0f );
		}

		const auto text_clip_right = button_rect.x + width - arrow_padding + 4.0f;
		const auto clip_left = std::max( text_x, win_clip_left );
		const auto clip_top = std::max( button_rect.y, win_clip_top );
		const auto clip_right = std::min( text_clip_right, win_clip_right );
		const auto clip_bottom = std::min( button_rect.bottom( ), win_clip_bottom );

		active_draw_list( ).push_clip_rect( clip_left, clip_top, clip_right, clip_bottom );

		if ( needs_scroll && scroll_state.scroll_offset > 0.01f )
		{
			const auto gap{ 30.0f };
			const auto total_width = full_text_w + gap;

			const auto offset1 = -scroll_state.scroll_offset;
			active_draw_list( ).add_text( text_x + offset1, text_y, display_text, zdraw::get_font( ), style.text );

			const auto offset2 = offset1 + total_width;
			if ( offset2 < max_text_width )
			{
				active_draw_list( ).add_text( text_x + offset2, text_y, display_text, zdraw::get_font( ), style.text );
			}
		}
		else if ( needs_scroll )
		{
			const auto [ellipsis_w, ellipsis_h] = zdraw::measure_text( "..." );

			std::string truncated{};
			auto truncated_w{ 0.0f };
			const auto available_for_text = max_text_width - ellipsis_w;

			for ( char c : display_text )
			{
				std::string test = truncated + c;
				const auto [test_w, test_h] = zdraw::measure_text( test );

				if ( test_w > available_for_text )
				{
					break;
				}

				truncated = test;
				truncated_w = test_w;
			}

			truncated += "...";
			active_draw_list( ).add_text( text_x, text_y, truncated, zdraw::get_font( ), style.text );
		}
		else
		{
			active_draw_list( ).add_text( text_x, text_y, display_text, zdraw::get_font( ), style.text );
		}

		active_draw_list( ).pop_clip_rect( );

		constexpr auto arrow_height{ 3.5f };
		const auto arrow_x = button_rect.right( ) - arrow_size - style.frame_padding_x - 4.0f;
		const auto arrow_y = button_rect.y + ( combo_height - arrow_height ) * 0.5f;
		const auto arrow_col = lerp( style.combo_arrow, lighten( style.combo_arrow, 1.3f ), hover_anim );

		if ( is_open )
		{
			const float points[ ] = { arrow_x, arrow_y + arrow_height, arrow_x + arrow_size * 0.5f, arrow_y, arrow_x + arrow_size, arrow_y + arrow_height };
			active_draw_list( ).add_polyline( points, arrow_col, false, 1.5f );
		}
		else
		{
			const float points[ ] = { arrow_x, arrow_y, arrow_x + arrow_size * 0.5f, arrow_y + arrow_height, arrow_x + arrow_size, arrow_y };
			active_draw_list( ).add_polyline( points, arrow_col, false, 1.5f );
		}

		return changed;
	}

	bool color_picker( std::string_view label, zdraw::rgba& color, float width, bool show_alpha )
	{
		const auto win = ctx( ).current_window( );
		if ( !win )
		{
			return false;
		}

		const auto id = ctx( ).generate_id( label );
		const auto display_label = context::get_display_label( label );
		const auto context_menu_id = id + 1;
		const auto& style = ctx( ).get_style( );
		const auto& input = ctx( ).input( );
		auto& anims = ctx( ).anims( );
		auto& overlays = ctx( ).overlays( );

		const auto is_open = overlays.is_open( id );
		const auto context_menu_open = overlays.is_open( context_menu_id );
		auto changed{ false };

		if ( auto* popup = dynamic_cast< color_picker_overlay* >( overlays.find( id ) ) )
		{
			changed = popup->was_changed( );
			popup->clear_changed( );
			popup->clear_changed( );
		}

		if ( auto* ctx_menu = dynamic_cast< color_picker_context_menu* >( overlays.find( context_menu_id ) ) )
		{
			if ( ctx_menu->was_changed( ) )
			{
				changed = true;
			}
		}

		const auto swatch_width = style.color_picker_swatch_width;
		const auto swatch_height = style.color_picker_swatch_height;
		const auto has_label = !display_label.empty( );

		auto label_w{ 0.0f };
		auto label_h{ 0.0f };

		if ( has_label )
		{
			auto [w, h] = zdraw::measure_text( display_label );
			label_w = w;
			label_h = h;
		}

		const auto total_w = has_label ? ( swatch_width + style.item_spacing_x + label_w ) : swatch_width;
		const auto total_h = has_label ? std::max( swatch_height, label_h ) : swatch_height;

		const auto local = item_add( total_w, total_h );
		const auto abs = to_absolute( local );
		auto x_offset{ 0.0f };

		if ( width > 0.0f )
		{
			x_offset = width - total_w;
		}

		const auto swatch_y = has_label ? ( abs.y + ( total_h - swatch_height ) * 0.5f ) : abs.y;
		const auto swatch_rect = zui::rect{ abs.x + x_offset, swatch_y, swatch_width, swatch_height };
		const auto can_interact = !ctx( ).overlay_blocking_input( );
		const auto hovered = can_interact && input.hovered( swatch_rect );

		const auto hover_anim = anims.animate( id + 2, hovered ? 1.0f : 0.0f, 12.0f );
		const auto open_anim = anims.animate( id + 3, is_open ? 1.0f : 0.0f, 12.0f );

		if ( auto* popup = dynamic_cast< color_picker_overlay* >( overlays.find( id ) ) )
		{
			popup->update_anchor( swatch_rect );
		}

		if ( hovered && input.mouse_clicked( ) && can_interact )
		{
			if ( is_open )
			{
				overlays.close( id );
			}
			else
			{
				overlays.add<color_picker_overlay>( id, swatch_rect, &color, show_alpha );
			}
		}

		if ( hovered && input.right_mouse_clicked( ) && can_interact )
		{
			if ( !context_menu_open )
			{
				const auto menu_anchor = zui::rect{ input.mouse_x( ), input.mouse_y( ), 0.0f, 0.0f };
				overlays.add<color_picker_context_menu>( context_menu_id, menu_anchor, &color );
			}
		}

		auto border_col = lerp( style.color_picker_border, lighten( style.color_picker_border, 1.15f ), hover_anim );
		border_col = lerp( border_col, lighten( style.color_picker_border, 1.3f ), open_anim );

		active_draw_list( ).add_rect_filled( swatch_rect.x, swatch_rect.y, swatch_rect.w, swatch_rect.h, style.color_picker_bg );

		constexpr auto pad{ 2.0f };
		active_draw_list( ).add_rect_filled( swatch_rect.x + pad, swatch_rect.y + pad, swatch_rect.w - pad * 2.0f, swatch_rect.h - pad * 2.0f, color );
		active_draw_list( ).add_rect( swatch_rect.x, swatch_rect.y, swatch_rect.w, swatch_rect.h, border_col );

		if ( has_label )
		{
			const auto text_x = swatch_rect.right( ) + style.item_spacing_x;
			const auto text_y = abs.y + ( total_h - label_h ) * 0.5f;

			const auto available_w = win->bounds.right( ) - text_x - style.window_padding_x;
			const auto label_text = maybe_truncate_text( display_label, available_w, ctx( ).truncation_buffer( ) );

			active_draw_list( ).add_text( text_x, text_y, label_text, zdraw::get_font( ), style.text );
		}

		return changed;
	}

	bool text_input( std::string_view label, std::string& text, std::size_t max_length, std::string_view hint )
	{
		const auto win = ctx( ).current_window( );
		if ( !win )
		{
			return false;
		}

		const auto id = ctx( ).generate_id( label );
		const auto display_label = context::get_display_label( label );
		const auto& style = ctx( ).get_style( );
		const auto& input = ctx( ).input( );
		auto& anims = ctx( ).anims( );

		auto& states = get_text_input_states( );
		auto& state = states[ id ];

		const auto is_active = ( ctx( ).m_active_text_input_id == id );
		const auto [avail_w, avail_h] = get_content_region_avail( );
		const auto input_height = style.text_input_height;

		const auto local = item_add( avail_w, input_height );
		const auto abs = to_absolute( local );

		const auto frame_rect = zui::rect{ abs.x, abs.y, avail_w, input_height };

		const auto can_interact = !ctx( ).overlay_blocking_input( );
		const auto hovered = can_interact && input.hovered( frame_rect );
		const auto clicked = hovered && input.mouse_clicked( );

		if ( clicked )
		{
			ctx( ).m_active_text_input_id = id;

			state.key_was_down.clear( );
			state.key_repeat_timers.clear( );

			const auto text_start_x = frame_rect.x + style.frame_padding_x - state.scroll_offset;
			const auto click_x = input.mouse_x( );

			std::size_t best_pos{ 0 };
			auto best_dist = std::abs( click_x - text_start_x );

			for ( std::size_t i = 1; i <= text.size( ); ++i )
			{
				auto [tw, th] = zdraw::measure_text( std::string_view{ text.data( ), i } );
				const auto char_x = text_start_x + tw;
				const auto dist = std::abs( click_x - char_x );
				if ( dist < best_dist )
				{
					best_dist = dist;
					best_pos = i;
				}
			}

			state.cursor_pos = best_pos;
			state.selection_start = best_pos;
			state.selection_end = best_pos;
			state.cursor_blink_timer = 0.0f;
		}

		if ( is_active && input.mouse_clicked( ) && !hovered )
		{
			ctx( ).m_active_text_input_id = invalid_id;
		}

		if ( is_active && hovered && input.mouse_down( ) )
		{
			const auto text_start_x = frame_rect.x + style.frame_padding_x - state.scroll_offset;
			const auto mouse_x_pos = input.mouse_x( );

			std::size_t best_pos{ 0 };
			auto best_dist = std::abs( mouse_x_pos - text_start_x );

			for ( std::size_t i = 1; i <= text.size( ); ++i )
			{
				auto [tw, th] = zdraw::measure_text( std::string_view{ text.data( ), i } );
				const auto char_x = text_start_x + tw;
				const auto dist = std::abs( mouse_x_pos - char_x );
				if ( dist < best_dist )
				{
					best_dist = dist;
					best_pos = i;
				}
			}

			state.selection_end = best_pos;
			state.cursor_pos = best_pos;
			state.cursor_blink_timer = 0.0f;
		}

		auto changed{ false };

		if ( is_active )
		{
			const auto& input_state = input.current( );
			const auto dt = zdraw::get_delta_time( );

			const auto shift_held = ( input_state.key_down_map.count( VK_SHIFT ) && input_state.key_down_map.at( VK_SHIFT ) ) || ( input_state.key_down_map.count( VK_LSHIFT ) && input_state.key_down_map.at( VK_LSHIFT ) ) || ( input_state.key_down_map.count( VK_RSHIFT ) && input_state.key_down_map.at( VK_RSHIFT ) );
			const auto ctrl_held = ( input_state.key_down_map.count( VK_CONTROL ) && input_state.key_down_map.at( VK_CONTROL ) ) || ( input_state.key_down_map.count( VK_LCONTROL ) && input_state.key_down_map.at( VK_LCONTROL ) ) || ( input_state.key_down_map.count( VK_RCONTROL ) && input_state.key_down_map.at( VK_RCONTROL ) );

			auto process_key = [ &state, &input_state, dt ]( int vk ) -> bool
				{
					for ( const auto pressed_key : input_state.key_presses( ) )
					{
						if ( pressed_key == vk )
						{
							return true;
						}
					}

					const bool is_down = input_state.key_down_map.count( vk ) && input_state.key_down_map.at( vk );
					const auto was_down = state.key_was_down[ vk ];
					state.key_was_down[ vk ] = is_down;

					if ( !is_down )
					{
						state.key_repeat_timers[ vk ] = 0.0f;
						return false;
					}

					state.key_repeat_timers[ vk ] += dt;

					constexpr auto initial_delay{ 0.4f };
					constexpr auto repeat_rate{ 0.03f };

					if ( state.key_repeat_timers[ vk ] >= initial_delay )
					{
						const auto excess = state.key_repeat_timers[ vk ] - initial_delay;
						const auto repeats = static_cast< int >( excess / repeat_rate );
						const auto prev_repeats = static_cast< int >( ( excess - dt ) / repeat_rate );

						if ( repeats > prev_repeats )
						{
							return true;
						}
					}

					return false;
				};

			const auto has_selection = state.selection_start != state.selection_end;
			const auto sel_min = std::min( state.selection_start, state.selection_end );
			const auto sel_max = std::max( state.selection_start, state.selection_end );

			if ( process_key( VK_LEFT ) )
			{
				state.cursor_blink_timer = 0.0f;
				if ( ctrl_held )
				{
					while ( state.cursor_pos > 0 && text[ state.cursor_pos - 1 ] == ' ' )
					{
						state.cursor_pos--;
					}
					while ( state.cursor_pos > 0 && text[ state.cursor_pos - 1 ] != ' ' )
					{
						state.cursor_pos--;
					}
				}
				else if ( has_selection && !shift_held )
				{
					state.cursor_pos = sel_min;
				}
				else if ( state.cursor_pos > 0 )
				{
					state.cursor_pos--;
				}

				if ( shift_held )
				{
					state.selection_end = state.cursor_pos;
				}
				else
				{
					state.selection_start = state.cursor_pos;
					state.selection_end = state.cursor_pos;
				}
			}

			if ( process_key( VK_RIGHT ) )
			{
				state.cursor_blink_timer = 0.0f;
				if ( ctrl_held )
				{
					while ( state.cursor_pos < text.size( ) && text[ state.cursor_pos ] != ' ' )
					{
						state.cursor_pos++;
					}
					while ( state.cursor_pos < text.size( ) && text[ state.cursor_pos ] == ' ' )
					{
						state.cursor_pos++;
					}
				}
				else if ( has_selection && !shift_held )
				{
					state.cursor_pos = sel_max;
				}
				else if ( state.cursor_pos < text.size( ) )
				{
					state.cursor_pos++;
				}

				if ( shift_held )
				{
					state.selection_end = state.cursor_pos;
				}
				else
				{
					state.selection_start = state.cursor_pos;
					state.selection_end = state.cursor_pos;
				}
			}

			if ( process_key( VK_HOME ) )
			{
				state.cursor_blink_timer = 0.0f;
				state.cursor_pos = 0;

				if ( shift_held )
				{
					state.selection_end = state.cursor_pos;
				}
				else
				{
					state.selection_start = state.cursor_pos;
					state.selection_end = state.cursor_pos;
				}
			}

			if ( process_key( VK_END ) )
			{
				state.cursor_blink_timer = 0.0f;
				state.cursor_pos = text.size( );
				if ( shift_held )
				{
					state.selection_end = state.cursor_pos;
				}
				else
				{
					state.selection_start = state.cursor_pos;
					state.selection_end = state.cursor_pos;
				}
			}

			if ( ctrl_held && process_key( 'A' ) )
			{
				state.selection_start = 0;
				state.selection_end = text.size( );
				state.cursor_pos = text.size( );
			}

			if ( process_key( VK_BACK ) )
			{
				state.cursor_blink_timer = 0.0f;
				if ( has_selection )
				{
					text.erase( sel_min, sel_max - sel_min );
					state.cursor_pos = sel_min;
					state.selection_start = sel_min;
					state.selection_end = sel_min;
					changed = true;
				}
				else if ( state.cursor_pos > 0 )
				{
					if ( ctrl_held )
					{
						auto erase_start = state.cursor_pos;
						while ( erase_start > 0 && text[ erase_start - 1 ] == ' ' )
						{
							erase_start--;
						}
						while ( erase_start > 0 && text[ erase_start - 1 ] != ' ' )
						{
							erase_start--;
						}
						text.erase( erase_start, state.cursor_pos - erase_start );
						state.cursor_pos = erase_start;
					}
					else
					{
						text.erase( state.cursor_pos - 1, 1 );
						state.cursor_pos--;
					}

					state.selection_start = state.cursor_pos;
					state.selection_end = state.cursor_pos;
					changed = true;
				}
			}

			if ( process_key( VK_DELETE ) )
			{
				state.cursor_blink_timer = 0.0f;
				if ( has_selection )
				{
					text.erase( sel_min, sel_max - sel_min );
					state.cursor_pos = sel_min;
					state.selection_start = sel_min;
					state.selection_end = sel_min;
					changed = true;
				}
				else if ( state.cursor_pos < text.size( ) )
				{
					if ( ctrl_held )
					{
						auto erase_end = state.cursor_pos;
						while ( erase_end < text.size( ) && text[ erase_end ] != ' ' )
						{
							erase_end++;
						}
						while ( erase_end < text.size( ) && text[ erase_end ] == ' ' )
						{
							erase_end++;
						}

						text.erase( state.cursor_pos, erase_end - state.cursor_pos );
					}
					else
					{
						text.erase( state.cursor_pos, 1 );
					}

					state.selection_start = state.cursor_pos;
					state.selection_end = state.cursor_pos;
					changed = true;
				}
			}

			if ( process_key( VK_ESCAPE ) || process_key( VK_RETURN ) )
			{
				ctx( ).m_active_text_input_id = invalid_id;
			}

			for ( const auto wch : input_state.chars( ) )
			{
				if ( text.size( ) < max_length )
				{
					state.cursor_blink_timer = 0.0f;

					if ( has_selection )
					{
						text.erase( sel_min, sel_max - sel_min );
						state.cursor_pos = sel_min;
					}

					const auto ch = static_cast< char >( wch );
					text.insert( state.cursor_pos, 1, ch );

					state.cursor_pos++;
					state.selection_start = state.cursor_pos;
					state.selection_end = state.cursor_pos;
					changed = true;
				}
			}

			state.cursor_pos = std::min( state.cursor_pos, text.size( ) );
			state.selection_start = std::min( state.selection_start, text.size( ) );
			state.selection_end = std::min( state.selection_end, text.size( ) );
		}

		const auto dt = zdraw::get_delta_time( );
		const auto hover_anim = anims.animate( id, hovered ? 1.0f : 0.0f, 12.0f );
		const auto active_anim = anims.animate( id + 1, is_active ? 1.0f : 0.0f, 12.0f );
		const auto has_selection = is_active && ( state.selection_start != state.selection_end );
		const auto selection_anim = anims.animate( id + 2, has_selection ? 1.0f : 0.0f, 15.0f );

		if ( is_active )
		{
			state.cursor_blink_timer += dt;
			if ( state.cursor_blink_timer > 1.0f )
			{
				state.cursor_blink_timer -= 1.0f;
			}
		}

		auto border_col = lerp( style.text_input_border, lighten( style.text_input_border, 1.3f ), hover_anim );
		border_col = lerp( border_col, style.accent, active_anim );

		auto bg_col = lerp( style.text_input_bg, lighten( style.text_input_bg, 1.05f ), hover_anim );
		bg_col = lerp( bg_col, lighten( style.text_input_bg, 1.08f ), active_anim );

		active_draw_list( ).add_rect_filled( frame_rect.x, frame_rect.y, frame_rect.w, frame_rect.h, bg_col );
		active_draw_list( ).add_rect( frame_rect.x, frame_rect.y, frame_rect.w, frame_rect.h, border_col );

		const auto text_padding_x = style.frame_padding_x;
		const auto text_area_w = frame_rect.w - text_padding_x * 2.0f;
		auto [text_w, text_h] = text.empty( ) ? zdraw::measure_text( "A" ) : zdraw::measure_text( text );

		if ( text.empty( ) )
		{
			text_w = 0.0f;
		}

		if ( is_active )
		{
			const auto [cursor_offset_w, cursor_offset_h] = state.cursor_pos == 0 ? std::pair{ 0.0f, 0.0f } : zdraw::measure_text( std::string_view{ text.data( ), state.cursor_pos } );
			const auto cursor_x_in_text = cursor_offset_w;
			const auto visible_start = state.scroll_offset;
			const auto visible_end = state.scroll_offset + text_area_w;

			if ( cursor_x_in_text < visible_start )
			{
				state.scroll_offset = cursor_x_in_text;
			}
			else if ( cursor_x_in_text > visible_end - 2.0f )
			{
				state.scroll_offset = cursor_x_in_text - text_area_w + 2.0f;
			}

			state.scroll_offset = std::max( 0.0f, state.scroll_offset );
		}

		active_draw_list( ).push_clip_rect( frame_rect.x + text_padding_x, frame_rect.y, frame_rect.x + text_padding_x + text_area_w, frame_rect.y + frame_rect.h );

		const auto text_x = frame_rect.x + text_padding_x - state.scroll_offset;
		const auto text_y = frame_rect.y + ( frame_rect.h - text_h ) * 0.5f;

		if ( selection_anim > 0.01f )
		{
			const auto sel_min_local = std::min( state.selection_start, state.selection_end );
			const auto sel_max_local = std::max( state.selection_start, state.selection_end );

			const auto [sel_start_w, sel_start_h] = sel_min_local == 0 ? std::pair{ 0.0f, 0.0f } : zdraw::measure_text( std::string_view{ text.data( ), sel_min_local } );
			const auto [sel_end_w, sel_end_h] = zdraw::measure_text( std::string_view{ text.data( ), sel_max_local } );

			const auto sel_x = text_x + sel_start_w;
			const auto sel_w = sel_end_w - sel_start_w;

			const auto ease_t = ease::out_quad( selection_anim );
			const auto base_alpha = 100.0f;
			const auto alpha = static_cast< std::uint8_t >( base_alpha * ease_t );
			const auto scale_y = 0.6f + ease_t * 0.4f;
			const auto scaled_h = ( text_h + 2.0f ) * scale_y;
			const auto offset_y = ( ( text_h + 2.0f ) - scaled_h ) * 0.5f;

			auto sel_col = style.accent;
			sel_col.a = alpha;
			active_draw_list( ).add_rect_filled( sel_x, text_y - 1.0f + offset_y, sel_w, scaled_h, sel_col );
		}

		if ( text.empty( ) && !is_active && !hint.empty( ) )
		{
			auto hint_col = style.text;
			hint_col.a = 100;
			active_draw_list( ).add_text( frame_rect.x + text_padding_x, text_y, hint, zdraw::get_font( ), hint_col );
		}
		else if ( !text.empty( ) )
		{
			active_draw_list( ).add_text( text_x, text_y, text, zdraw::get_font( ), style.text );
		}

		if ( is_active )
		{
			const auto cursor_visible = std::fmod( state.cursor_blink_timer, 1.0f ) < 0.5f;
			const auto [cursor_offset_w, cursor_offset_h] = state.cursor_pos == 0 ? std::pair{ 0.0f, 0.0f } : zdraw::measure_text( std::string_view{ text.data( ), state.cursor_pos } );
			const auto target_cursor_x = text_x + cursor_offset_w;

			if ( !state.cursor_anim_initialized )
			{
				state.cursor_anim_x = target_cursor_x;
				state.cursor_anim_start_x = target_cursor_x;
				state.cursor_anim_target_x = target_cursor_x;
				state.cursor_anim_progress = 1.0f;
				state.cursor_anim_initialized = true;
			}
			else if ( std::abs( target_cursor_x - state.cursor_anim_target_x ) > 0.5f )
			{
				state.cursor_anim_start_x = state.cursor_anim_x;
				state.cursor_anim_target_x = target_cursor_x;
				state.cursor_anim_progress = 0.0f;
			}

			if ( state.cursor_anim_progress < 1.0f )
			{
				state.cursor_anim_progress = std::min( 1.0f, state.cursor_anim_progress + dt * 12.0f );
				const auto eased_progress = zui::ease::out_cubic( state.cursor_anim_progress );
				state.cursor_anim_x = state.cursor_anim_start_x + ( state.cursor_anim_target_x - state.cursor_anim_start_x ) * eased_progress;
			}
			else
			{
				state.cursor_anim_x = state.cursor_anim_target_x;
			}

			const auto cursor_y = text_y;
			const auto cursor_h = text_h;

			const auto blink_alpha = ( std::sin( state.cursor_blink_timer * 6.28318f ) * 0.5f + 0.5f );
			auto cursor_col = style.text;
			cursor_col.a = static_cast< std::uint8_t >( 255.0f * ( 0.4f + 0.6f * blink_alpha ) );

			active_draw_list( ).add_rect_filled( state.cursor_anim_x, cursor_y, 1.0f, cursor_h, cursor_col );
		}
		else
		{
			state.cursor_anim_initialized = false;
		}

		active_draw_list( ).pop_clip_rect( );

		return changed;
	}

	bool begin_popup(std::string_view label, float width)
	{
		const auto win = ctx().current_window();
		if (!win)
			return false;

		const auto id = ctx().generate_id(label);
		const auto& style = ctx().get_style();
		const auto& input = ctx().input().current();
		auto& anims = ctx().anims();

		constexpr auto dot_area_w{ 20.0f };
		const auto dot_area_h = win->last_item.h;

		const auto far_right_x = win->bounds.w - style.window_padding_x - dot_area_w;
		const auto dot_local_y = win->last_item.y;

		const float button_x = std::floorf(win->bounds.x + far_right_x);
		const float button_y = std::floorf(win->bounds.y + dot_local_y);

		const auto is_open = ctx().overlays().is_open(id);
		const auto can_interact = !ctx().overlay_blocking_input() || is_open;
		const auto hovered = can_interact &&
			(button_x <= input.mouse_x && input.mouse_x < button_x + dot_area_w) &&
			(button_y <= input.mouse_y && input.mouse_y < button_y + dot_area_h);

		const auto hover_anim = anims.animate(id + 1, hovered ? 1.0f : 0.0f, 12.0f);
		const auto open_color_anim = anims.animate(id + 2, is_open ? 1.0f : 0.0f, 10.0f);

		auto& dl = active_draw_list();

		// === Draw small box around the dots ===
		const float rounding = 3.0f;

		// Background
		auto box_col = lerp(style.window_bg, style.accent, hover_anim * 0.12f);
		box_col.a = static_cast<std::uint8_t>(box_col.a * (0.65f + hover_anim * 0.35f));

		dl.add_rect_filled(button_x, button_y, dot_area_w, dot_area_h, box_col);  // using your signature

		// Border
		auto border_col = lerp(darken(style.popup_border, 0.15f), style.accent, std::max(hover_anim, open_color_anim * 0.7f));
		border_col.a = static_cast<std::uint8_t>(border_col.a * (0.75f + hover_anim * 0.25f));

		dl.add_rect(button_x - 1.5f, button_y, dot_area_w + 3.f, dot_area_h, border_col, 1.f);

		// === Draw the three dots ===
		auto dot_col = lerp(darken(style.text, 0.6f), style.accent, std::max(hover_anim, open_color_anim));

		constexpr auto dot_r{ 1.5f };
		constexpr auto dot_spacing{ 4.0f };
		const auto total_dots_w = dot_r * 2.0f * 3.0f + dot_spacing * 2.0f;
		const auto start_x = button_x + (dot_area_w - total_dots_w) * 0.5f;
		const auto cy = button_y + dot_area_h * 0.5f;

		for (int i = 0; i < 3; ++i)
		{
			const auto cx = start_x + dot_r + static_cast<float>(i) * (dot_r * 2.0f + dot_spacing);
			dl.add_circle_filled(cx, cy, dot_r, dot_col, 8);
		}

		// === Click handling ===
		if (hovered && input.mouse_clicked)
		{
			if (is_open)
				ctx().overlays().close(id);
			else
				ctx().overlays().add<popup_overlay>(id, rect{ button_x, button_y, dot_area_w, dot_area_h }, width);
		}

		// === Popup logic (unchanged) ===
		auto* ov = static_cast<popup_overlay*>(ctx().overlays().find(id));
		if (ov && !ov->is_closed())
		{
			ov->update_anchor(rect{ button_x, button_y, dot_area_w, dot_area_h });
			ov->tick_animation();

			const auto popup_rect = ov->get_popup_rect();
			const auto ease_t = ease::out_cubic(ov->get_open_anim());
			const auto animated_h = popup_rect.h * ease_t;
			const auto alpha_mult = ease_t;

			auto& top_dl = zdraw::get_draw_list(zdraw::draw_layer::topmost);

			auto bg_col = style.popup_bg;
			bg_col.a = static_cast<std::uint8_t>(bg_col.a * alpha_mult);

			auto border_popup_col = lighten(style.popup_border, 1.1f);
			border_popup_col.a = static_cast<std::uint8_t>(border_popup_col.a * alpha_mult);

			top_dl.add_rect_filled(popup_rect.x, popup_rect.y, popup_rect.w, animated_h, bg_col);
			top_dl.add_rect(popup_rect.x, popup_rect.y, popup_rect.w, animated_h, border_popup_col, 1.0f);

			ctx().m_active_popup_id = id;
			ctx().m_draw_layer_override = zdraw::draw_layer::topmost;
			ctx().m_inside_popup = !ov->is_closing();

			window_state state{};
			state.title = std::string(label);
			state.bounds = popup_rect;
			state.cursor_x = style.window_padding_x;
			state.cursor_y = style.window_padding_y;
			state.line_height = 0.0f;
			state.is_child = true;

			ctx().push_window(std::move(state));
			ctx().push_id(id);

			top_dl.push_clip_rect(popup_rect.x, popup_rect.y, popup_rect.right(), popup_rect.y + animated_h);

			return true;
		}

		return false;
	}

	void end_popup( )
	{
		const auto win = ctx( ).current_window( );
		if ( !win )
		{
			return;
		}

		const auto& style = ctx( ).get_style( );
		const auto content_h = win->content_height + style.window_padding_y;

		auto* ov = static_cast< popup_overlay* >( ctx( ).overlays( ).find( ctx( ).m_active_popup_id ) );
		if ( ov && !ov->is_closing( ) )
		{
			ov->set_content_height( content_h );
		}

		zdraw::get_draw_list( zdraw::draw_layer::topmost ).pop_clip_rect( );

		ctx( ).pop_id( );
		ctx( ).pop_window( );

		ctx( ).m_draw_layer_override.reset( );
		ctx( ).m_active_popup_id = invalid_id;
		ctx( ).m_inside_popup = false;
	}

} // namespace zui
