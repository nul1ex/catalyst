#include <stdafx.hpp>

namespace features::esp {

	void item::on_render( zdraw::draw_list& draw_list )
	{
		const auto& cfg = settings::g_esp.m_item;
		if ( !cfg.enabled )
		{
			return;
		}

		const auto view_origin = systems::g_view.origin( );

		for ( const auto& item : systems::g_collector.items( ) )
		{
			const auto distance = view_origin.distance( item.origin ) * 0.01905f;
			if ( distance > cfg.max_distance )
			{
				continue;
			}

			if ( !this->passes_filter( item.subtype, cfg.m_filters ) )
			{
				continue;
			}

			const auto screen = systems::g_view.project( item.origin );
			if ( !systems::g_view.projection_valid( screen ) )
			{
				continue;
			}

			auto y_offset{ 0.0f };

			if ( cfg.m_icon.enabled )
			{
				this->add_icon( draw_list, screen, item, cfg.m_icon, y_offset );
			}

			if ( cfg.m_name.enabled )
			{
				this->add_name( draw_list, screen, item, cfg.m_name, y_offset );
			}

			if ( cfg.m_ammo.enabled && item.max_ammo > 0 )
			{
				this->add_ammo( draw_list, screen, item, cfg.m_ammo, y_offset );
			}
		}
	}

	void item::add_icon( zdraw::draw_list& draw_list, const math::vector2& screen, const systems::collector::item& item, const settings::esp::item::icon& cfg, float& y_offset )
	{
		zdraw::push_font( g::render.fonts( ).weapons_15 );

		const auto icon = this->get_icon( item.subtype );
		const auto [text_w, text_h] = zdraw::measure_text( icon );
		const auto x = std::floorf( screen.x - text_w * 0.5f );
		const auto y = std::floorf( screen.y - text_h * 0.5f + y_offset );

		draw_list.add_text( x, y, icon, zdraw::get_font( ), cfg.color, zdraw::text_style::outlined );

		zdraw::pop_font( );

		y_offset += text_h - 5.5f;
	}

	void item::add_name( zdraw::draw_list& draw_list, const math::vector2& screen, const systems::collector::item& item, const settings::esp::item::name& cfg, float& y_offset )
	{
		zdraw::push_font( g::render.fonts( ).pixel7_10 );

		const auto name = this->get_display_name( item.subtype );
		const auto [text_w, text_h] = zdraw::measure_text( name );
		const auto x = std::floorf( screen.x - text_w * 0.5f );
		const auto y = std::floorf( screen.y + y_offset );

		draw_list.add_text( x, y, name, zdraw::get_font( ), cfg.color, zdraw::text_style::outlined );

		zdraw::pop_font( );

		y_offset += text_h - 5.5f;
	}

	void item::add_ammo( zdraw::draw_list& draw_list, const math::vector2& screen, const systems::collector::item& item, const settings::esp::item::ammo& cfg, float& y_offset )
	{
		zdraw::push_font( g::render.fonts( ).pixel7_10 );

		const auto text = std::format( "{}/{}", std::clamp( item.ammo, 0, item.max_ammo ), item.max_ammo );
		const auto [text_w, text_h] = zdraw::measure_text( text );
		const auto x = std::floorf( screen.x - text_w * 0.5f );
		const auto y = std::floorf( screen.y + y_offset );

		const auto fraction = static_cast< float >( std::clamp( item.ammo, 0, item.max_ammo ) ) / item.max_ammo;
		const auto color = fraction > 0.0f ? cfg.color : cfg.empty_color;

		draw_list.add_text( x, y, text, zdraw::get_font( ), color, zdraw::text_style::outlined );

		zdraw::pop_font( );

		y_offset += text_h;
	}

	bool item::passes_filter( systems::collector::item_subtype subtype, const settings::esp::item::filters& filters ) const
	{
		const auto cat = this->get_category( subtype );
		switch ( cat )
		{
		case category::rifle:   return filters.rifles;
		case category::smg:     return filters.smgs;
		case category::shotgun: return filters.shotguns;
		case category::sniper:  return filters.snipers;
		case category::pistol:  return filters.pistols;
		case category::heavy:   return filters.heavy;
		case category::grenade: return filters.grenades;
		case category::utility: return filters.utility;
		}
		return false;
	}

	item::category item::get_category( systems::collector::item_subtype subtype ) const
	{
		using st = systems::collector::item_subtype;

		switch ( subtype )
		{
		case st::ak47:
		case st::m4a4:
		case st::m4a1s:
		case st::aug:
		case st::famas:
		case st::galil_ar:
		case st::sg553:
			return category::rifle;

		case st::mac10:
		case st::mp5sd:
		case st::mp7:
		case st::mp9:
		case st::pp_bizon:
		case st::p90:
		case st::ump45:
			return category::smg;

		case st::nova:
		case st::sawed_off:
		case st::xm1014:
		case st::mag7:
			return category::shotgun;

		case st::awp:
		case st::ssg08:
		case st::g3sg1:
		case st::scar20:
			return category::sniper;

		case st::deagle:
		case st::dual_berettas:
		case st::five_seven:
		case st::glock:
		case st::p2000:
		case st::usps:
		case st::p250:
		case st::cz75:
		case st::tec9:
		case st::r8_revolver:
			return category::pistol;

		case st::m249:
		case st::negev:
			return category::heavy;

		case st::he_grenade:
		case st::flashbang:
		case st::smoke_grenade:
		case st::molotov:
		case st::incendiary:
		case st::decoy:
			return category::grenade;

		case st::c4:
		case st::taser:
		case st::healthshot:
		case st::knife:
			return category::utility;

		default:
			return category::utility;
		}
	}

	std::string item::get_icon( systems::collector::item_subtype subtype ) const
	{
		using st = systems::collector::item_subtype;

		static const std::unordered_map<st, std::string> icons
		{
			{ st::ak47,           "W" }, { st::m4a4,           "S" },
			{ st::m4a1s,          "T" }, { st::aug,            "U" },
			{ st::famas,          "R" }, { st::galil_ar,       "Q" },
			{ st::sg553,          "V" }, { st::awp,            "Z" },
			{ st::ssg08,          "a" }, { st::g3sg1,          "X" },
			{ st::scar20,         "Y" }, { st::mac10,          "K" },
			{ st::mp5sd,          "N" }, { st::mp7,            "N" },
			{ st::mp9,            "R" }, { st::pp_bizon,       "M" },
			{ st::p90,            "O" }, { st::ump45,          "L" },
			{ st::nova,           "e" }, { st::sawed_off,      "c" },
			{ st::xm1014,         "b" }, { st::mag7,           "d" },
			{ st::m249,           "g" }, { st::negev,          "f" },
			{ st::deagle,         "A" }, { st::dual_berettas,  "B" },
			{ st::five_seven,     "C" }, { st::glock,          "D" },
			{ st::p2000,          "E" }, { st::usps,           "G" },
			{ st::p250,           "F" }, { st::cz75,           "I" },
			{ st::tec9,           "H" }, { st::r8_revolver,    "J" },
			{ st::taser,          "h" }, { st::c4,             "o" },
			{ st::he_grenade,     "j" }, { st::flashbang,      "i" },
			{ st::smoke_grenade,  "k" }, { st::molotov,        "l" },
			{ st::incendiary,     "n" }, { st::decoy,          "m" },
			{ st::knife,          "]" }, { st::healthshot,     "?" },
		};

		const auto it = icons.find( subtype );
		return it != icons.end( ) ? it->second : "?";
	}

	std::string item::get_display_name( systems::collector::item_subtype subtype ) const
	{
		using st = systems::collector::item_subtype;

		static const std::unordered_map<st, std::string> names
		{
			{ st::ak47,           "ak47" },            { st::m4a4,           "m4a4" },
			{ st::m4a1s,          "m4a1-s" },          { st::aug,            "aug" },
			{ st::famas,          "famas" },           { st::galil_ar,       "galil" },
			{ st::sg553,          "sg553" },           { st::awp,            "awp" },
			{ st::ssg08,          "scout" },           { st::g3sg1,          "g3sg1" },
			{ st::scar20,         "scar" },            { st::mac10,          "mac10" },
			{ st::mp5sd,          "mp5" },             { st::mp7,            "mp7" },
			{ st::mp9,            "mp9" },             { st::pp_bizon,       "bizon" },
			{ st::p90,            "p90" },             { st::ump45,          "ump" },
			{ st::nova,           "nova" },            { st::sawed_off,      "sawed" },
			{ st::xm1014,         "xm1014" },          { st::mag7,           "mag7" },
			{ st::m249,           "m249" },            { st::negev,          "negev" },
			{ st::deagle,         "deagle" },          { st::dual_berettas,  "dualies" },
			{ st::five_seven,     "five7" },           { st::glock,          "glock" },
			{ st::p2000,          "p2000" },           { st::usps,           "usp-s" },
			{ st::p250,           "p250" },            { st::cz75,           "cz75" },
			{ st::tec9,           "tec9" },            { st::r8_revolver,    "r8" },
			{ st::taser,          "taser" },           { st::c4,             "c4" },
			{ st::he_grenade,     "he" },              { st::flashbang,      "flash" },
			{ st::smoke_grenade,  "smoke" },           { st::molotov,        "molotov" },
			{ st::incendiary,     "incendiary" },      { st::decoy,          "decoy" },
			{ st::knife,          "knife" },           { st::healthshot,     "medkit" },
		};

		const auto it = names.find( subtype );
		return it != names.end( ) ? it->second : "unknown";
	}

} // namespace features::esp