#pragma once

class menu
{
public:
	void draw( );
	void draw_sidebar(float width, float height);
	[[nodiscard]] bool is_open( ) const noexcept { return this->m_open; }

private:
	enum class tab : int { combat = 0, esp, movement, skinchanger, misc, nades, configs, count };

	void draw_content( float width, float height );
	void draw_accent_lines( const zui::rect& bounds, float fade_ratio = 0.15f );

	void draw_combat( );
	void draw_esp( );
	void draw_movement( );
	void draw_skinchanger( );
	void draw_misc( );
	void draw_nades( );
	void draw_configs( );

	tab m_tab{ tab::combat };
	bool m_open{ false };

	float m_x{ 200.0f };
	float m_y{ 150.0f };
	float m_w{ 580.0f };
	float m_h{ 440.0f };

	static constexpr const char* k_weapon_groups[ ]{ "pistol", "smg", "rifle", "shotgun", "sniper", "lmg" };
	int m_weapon_group{ 0 };
};