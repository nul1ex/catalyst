#include <stdafx.hpp>

namespace features::esp {

    void projectile::on_render( zdraw::draw_list& draw_list )
    {
        const auto& cfg = settings::g_esp.m_projectile;
        if ( !cfg.enabled )
        {
            return;
        }

        const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
        if ( !global_vars )
            return;

        const auto current_time = g::memory.read<float>( global_vars + 0x30 );

        for ( const auto& proj : systems::g_collector.projectiles( ) )
        {
        if ( proj.origin.length_sqr( ) < 0.1f )
        {
            continue;
        }

            if ( proj.detonated )
            {
                continue;
            }

            if ( proj.subtype == systems::collector::projectile_subtype::molotov_fire )
            {
                if ( cfg.show_inferno_bounds && !proj.fire_points.empty( ) )
                {
                    this->draw_inferno_bounds( draw_list, proj, cfg );
                }

                const auto screen = systems::g_view.project( proj.origin );
                if ( !systems::g_view.projection_valid( screen ) )
                {
                    continue;
                }

                auto y_offset{ 0.0f };

                if ( cfg.show_icon )
                {
                    zdraw::push_font( g::render.fonts( ).weapons_40 );

                    const auto icon = std::string( "l" );
                    const auto [tw, th] = zdraw::measure_text( icon );
                    const auto x = std::floorf( screen.x - tw * 0.5f );
                    const auto y = std::floorf( screen.y - th * 0.5f );

                    draw_list.add_text( x, y, icon, zdraw::get_font( ), cfg.color_molotov, zdraw::text_style::outlined );
                    zdraw::pop_font( );

                    y_offset += th - 5.5f;
                }

                if ( cfg.show_name )
                {
                    zdraw::push_font( g::render.fonts( ).pixel7_10 );

                    const auto name = std::string( "fire" );
                    const auto [tw, th] = zdraw::measure_text( name );
                    const auto x = std::floorf( screen.x - tw * 0.5f );
                    const auto y = std::floorf( screen.y + y_offset );

                    draw_list.add_text( x, y, name, zdraw::get_font( ), cfg.color_molotov, zdraw::text_style::outlined );
                    zdraw::pop_font( );

                    y_offset += th - 5.5f;
                }

                if ( proj.expire_time > 0.0f )
                {
                    constexpr auto inferno_duration{ 7.0f };
                    const auto remaining = std::max( 0.0f, proj.expire_time - current_time );
                    const auto frac = std::clamp( remaining / inferno_duration, 0.0f, 1.0f );

                    this->draw_timer( draw_list, screen, y_offset, remaining, frac, cfg );
                }

                continue;
            }

            const auto screen = systems::g_view.project( proj.origin );
            if ( !systems::g_view.projection_valid( screen ) )
            {
                continue;
            }

            const auto color = get_color( proj.subtype, cfg );
            auto y_offset{ 0.0f };

            if ( cfg.show_icon )
            {
                zdraw::push_font( g::render.fonts( ).weapons_40 );

                const auto icon = this->get_icon( proj.subtype );
                const auto [tw, th] = zdraw::measure_text( icon );
                const auto x = std::floorf( screen.x - tw * 0.5f );
                const auto y = std::floorf( screen.y - th * 0.5f );

                draw_list.add_text( x, y, icon, zdraw::get_font( ), color, zdraw::text_style::outlined );
                zdraw::pop_font( );

                y_offset += th - 5.5f;
            }

            if ( cfg.show_name )
            {
                zdraw::push_font( g::render.fonts( ).pixel7_10 );

                const auto name = this->get_name( proj.subtype );
                const auto [tw, th] = zdraw::measure_text( name );
                const auto x = std::floorf( screen.x - tw * 0.5f );
                const auto y = std::floorf( screen.y + y_offset );

                draw_list.add_text( x, y, name, zdraw::get_font( ), color, zdraw::text_style::outlined );
                zdraw::pop_font( );

                y_offset += th - 5.5f;
            }

            if ( proj.subtype == systems::collector::projectile_subtype::smoke_grenade && proj.smoke_active )
            {
                constexpr auto smoke_duration{ 18.0f };
                const auto smoke_start = static_cast< float >( proj.effect_tick_begin ) * ( 1.0f / 64.0f );
                const auto remaining = std::max( 0.0f, smoke_duration - ( current_time - smoke_start ) );
                const auto frac = std::clamp( remaining / smoke_duration, 0.0f, 1.0f );

                this->draw_timer( draw_list, screen, y_offset, remaining, frac, cfg );
            }

            if ( proj.subtype == systems::collector::projectile_subtype::decoy && proj.effect_tick_begin > 0 )
            {
                const auto fuse{ 15.0f };
                const auto throw_time = static_cast< float >( proj.effect_tick_begin ) * ( 1.0f / 64.0f );
                const auto remaining = std::max( 0.0f, fuse - ( current_time - throw_time ) );
                const auto frac = std::clamp( remaining / fuse, 0.0f, 1.0f );

                this->draw_timer( draw_list, screen, y_offset, remaining, frac, cfg );
            }
        }

        this->draw_smoke_voxels( draw_list, cfg );
    }

    void projectile::draw_timer( zdraw::draw_list& draw_list, const math::vector2& screen, float& y_offset, float remaining, float frac, const settings::esp::projectile& cfg ) const
    {
        if ( cfg.show_timer_bar )
        {
            const auto timer_color = this->lerp_color( cfg.timer_low_color, cfg.timer_high_color, frac );
            const auto bar_w{ 30.0f };
            const auto bar_h{ 3.0f };
            const auto bx = std::floorf( screen.x - bar_w * 0.5f );
            const auto by = std::floorf( screen.y + y_offset + 6.0f );

            draw_list.add_rect_filled( bx - 1.0f, by - 1.0f, bar_w + 2.0f, bar_h + 2.0f, cfg.bar_background );
            draw_list.add_rect_filled( bx, by, bar_w * frac, bar_h, timer_color );

            y_offset += bar_h + 2.0f;
        }
    }

    void projectile::draw_inferno_bounds( zdraw::draw_list& draw_list, const systems::collector::projectile& proj, const settings::esp::projectile& cfg ) const
    {
        constexpr auto fire_radius{ 60.0f };
        constexpr auto points_per_fire{ 12 };

        std::vector<math::vector2> points{};
        points.reserve( proj.fire_points.size( ) * points_per_fire );

        for ( const auto& point : proj.fire_points )
        {
            for ( int i = 0; i < points_per_fire; ++i )
            {
                const auto angle = static_cast< float >( i ) / points_per_fire * std::numbers::pi_v<float> *2.0f;
                const auto world = point + math::vector3{ std::cosf( angle ) * fire_radius, std::sinf( angle ) * fire_radius, 0.0f };
                const auto projected = systems::g_view.project( world );

                if ( systems::g_view.projection_valid( projected ) )
                {
                    points.push_back( projected );
                }
            }
        }

        if ( points.size( ) < 3 )
        {
            return;
        }

        std::ranges::sort( points, [ ]( const math::vector2& a, const math::vector2& b ) { return a.x < b.x || ( a.x == b.x && a.y < b.y ); } );

        std::vector<math::vector2> lower{};
        std::vector<math::vector2> upper{};

        for ( const auto& p : points )
        {
            while ( lower.size( ) >= 2 )
            {
                const auto& p1 = lower[ lower.size( ) - 2 ];
                const auto& p2 = lower[ lower.size( ) - 1 ];

                if ( ( p2.x - p1.x ) * ( p.y - p1.y ) - ( p2.y - p1.y ) * ( p.x - p1.x ) > 0.0f )
                {
                    break;
                }

                lower.pop_back( );
            }
            lower.push_back( p );
        }

        for ( auto it = points.rbegin( ); it != points.rend( ); ++it )
        {
            while ( upper.size( ) >= 2 )
            {
                const auto& p1 = upper[ upper.size( ) - 2 ];
                const auto& p2 = upper[ upper.size( ) - 1 ];

                if ( ( p2.x - p1.x ) * ( it->y - p1.y ) - ( p2.y - p1.y ) * ( it->x - p1.x ) > 0.0f )
                {
                    break;
                }

                upper.pop_back( );
            }
            upper.push_back( *it );
        }

        lower.pop_back( );
        upper.pop_back( );
        lower.insert( lower.end( ), upper.begin( ), upper.end( ) );

        if ( lower.size( ) < 3 )
        {
            return;
        }

        const auto hull = std::span<const float>( reinterpret_cast< const float* >( lower.data( ) ), lower.size( ) * 2 );
        const auto fill = zdraw::rgba( cfg.color_molotov.r, cfg.color_molotov.g, cfg.color_molotov.b, 50 );
        const auto outline = zdraw::rgba( cfg.color_molotov.r, cfg.color_molotov.g, cfg.color_molotov.b, 150 );

        draw_list.add_convex_poly_filled( hull, fill );
        draw_list.add_polyline( hull, outline, true, 2.0f );
    }

    void projectile::draw_smoke_voxels( zdraw::draw_list& draw_list, const settings::esp::projectile& cfg ) const
    {
        if ( !cfg.show_smoke_voxels )
        {
            return;
        }

        const auto& voxels = systems::g_voxels.active_voxels( );
        if ( voxels.empty( ) )
        {
            return;
        }

        for ( const auto& v : voxels )
        {
            const auto screen = systems::g_view.project( v.world );
            if ( !systems::g_view.projection_valid( screen ) )
            {
                continue;
            }

            const auto color = zdraw::rgba{ v.r, v.g, v.b, static_cast< std::uint8_t >( v.a * 0.4f ) };
            draw_list.add_rect_filled( screen.x - 2.0f, screen.y - 2.0f, 4.0f, 4.0f, color );
        }
    }

    zdraw::rgba projectile::get_color( systems::collector::projectile_subtype type, const settings::esp::projectile& cfg ) const
    {
        using st = systems::collector::projectile_subtype;

        switch ( type )
        {
        case st::he_grenade:    return cfg.color_he;
        case st::flashbang:     return cfg.color_flash;
        case st::smoke_grenade: return cfg.color_smoke;
        case st::molotov:       return cfg.color_molotov;
        case st::incendiary:    return cfg.color_molotov;
        case st::molotov_fire:  return cfg.color_molotov;
        case st::decoy:         return cfg.color_decoy;
        default:                return cfg.default_color;
        }
    }

    std::string projectile::get_icon( systems::collector::projectile_subtype type ) const
    {
        using st = systems::collector::projectile_subtype;

        switch ( type )
        {
        case st::he_grenade:    return "j";
        case st::flashbang:     return "i";
        case st::smoke_grenade: return "k";
        case st::molotov:       return "l";
        case st::incendiary:    return "n";
        case st::molotov_fire:  return "l";
        case st::decoy:         return "m";
        default:                return "?";
        }
    }

    std::string projectile::get_name( systems::collector::projectile_subtype type ) const
    {
        using st = systems::collector::projectile_subtype;

        switch ( type )
        {
        case st::he_grenade:    return "he";
        case st::flashbang:     return "flash";
        case st::smoke_grenade: return "smoke";
        case st::molotov:       return "molotov";
        case st::incendiary:    return "incendiary";
        case st::molotov_fire:  return "fire";
        case st::decoy:         return "decoy";
        default:                return "grenade";
        }
    }

    zdraw::rgba projectile::lerp_color( const zdraw::rgba& a, const zdraw::rgba& b, float t ) const
    {
        return zdraw::rgba
        (
            static_cast< std::uint8_t >( a.r + ( b.r - a.r ) * t ),
            static_cast< std::uint8_t >( a.g + ( b.g - a.g ) * t ),
            static_cast< std::uint8_t >( a.b + ( b.b - a.b ) * t ),
            static_cast< std::uint8_t >( a.a + ( b.a - a.a ) * t )
        );
    }

} // namespace features::esp