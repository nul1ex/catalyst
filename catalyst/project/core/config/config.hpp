#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <string_view>

class config_system {
public:
	void save( std::string_view name );
	void load( std::string_view name );
	void remove( std::string_view name );

	void refresh_list( );

	[[nodiscard]] const std::vector<std::string>& get_configs( ) const { return this->m_configs; }
	[[nodiscard]] const std::filesystem::path& get_path( ) const { return this->m_path; }

private:
	void load_combat( const std::string& key, const std::string& val );
	void load_esp_player( const std::string& key, const std::string& val );
	void load_esp_item( const std::string& key, const std::string& val );
	void load_esp_projectile( const std::string& key, const std::string& val );
	void load_esp_radar(const std::string& key, const std::string& val);
	void load_misc( const std::string& key, const std::string& val );
	void load_movement( const std::string& key, const std::string& val );
	void load_skinchanger( const std::string& key, const std::string& val );

private:
	std::vector<std::string> m_configs{};
	std::filesystem::path m_path{};

public:
	bool initialized{ false };
	void init( );
};

namespace g {
	inline config_system config{};
}
