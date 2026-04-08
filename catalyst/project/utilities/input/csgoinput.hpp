#pragma once
#include <stdafx.hpp>
class csgoinput
{
public:
    void set_silent_view_angles( const math::vector3& angles ) const;
    [[nodiscard]] int get_frame_count( ) const;
    [[nodiscard]] std::uintptr_t get_frame_history( ) const;
    [[nodiscard]] std::uintptr_t get_last_frame( ) const;
    [[nodiscard]] std::uintptr_t get_frame( int index ) const;
};
