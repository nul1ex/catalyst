#include <stdafx.hpp>

namespace systems {

	void entities::refresh( )
	{
		const auto entity_list = this->get_entity_list( );
		if ( !entity_list )
		{
			std::unique_lock lock( this->m_mutex );
			this->m_entities.clear( );
			return;
		}

		std::vector<cached> fresh{};
		fresh.reserve( 128 );

		for ( std::int32_t i = 0; i < 2048; ++i )
		{
			const auto entity = this->get_by_index( entity_list, i );
			if ( !entity )
			{
				continue;
			}

			const auto schema_hash = this->get_schema_hash( entity );
			if ( !schema_hash )
			{
				continue;
			}

			const auto entity_type = this->classify( schema_hash );
			if ( entity_type == type::unknown )
			{
				continue;
			}

			fresh.push_back( { .ptr = entity, .schema_hash = schema_hash, .index = static_cast< std::int16_t >( i ), .type = entity_type } );
		}

		std::unique_lock lock( this->m_mutex );
		this->m_entities = std::move( fresh );
	}

	std::uintptr_t entities::lookup( std::uint32_t handle ) const
	{
		if ( !handle || handle == 0xffffffff )
		{
			return 0;
		}

		const auto entity_list = this->get_entity_list( );
		if ( !entity_list )
		{
			return 0;
		}

		const auto list_entry = g::memory.read<std::uintptr_t>( entity_list + ( static_cast< std::uintptr_t >( ( handle & 0x7fff ) >> 9 ) * 8 ) + 0x10 );
		if ( !list_entry )
		{
			return 0;
		}

		const auto entity = g::memory.read<std::uintptr_t>( list_entry + ( static_cast< std::uintptr_t >( handle & 0x1ff ) * 112 ) );
		if ( !entity || entity < 0x10000 )
		{
			return 0;
		}

		return entity;
	}

	std::vector<entities::cached> entities::by_type( type filter ) const
	{
		std::shared_lock lock( this->m_mutex );

		std::vector<cached> result{};
		result.reserve( this->m_entities.size( ) );

		for ( const auto& entry : this->m_entities )
		{
			if ( entry.type == filter )
			{
				result.push_back( entry );
			}
		}

		return result;
	}

	std::vector<entities::cached> entities::all( ) const
	{
		std::shared_lock lock( this->m_mutex );
		return this->m_entities;
	}

	std::uintptr_t entities::get_entity_list( ) const
	{
		return g::memory.read<std::uintptr_t>( g::offsets.entity_list );
	}

	std::uintptr_t entities::get_by_index( std::uintptr_t entity_list, std::int32_t index ) const
	{
		const auto chunk_index = index >> 9;
		const auto list_entry = g::memory.read<std::uintptr_t>( entity_list + ( static_cast< std::uintptr_t >( chunk_index ) * 8 ) + 0x10 );

		if ( !list_entry )
		{
			return 0;
		}

		return g::memory.read<std::uintptr_t>( list_entry + ( static_cast< std::uintptr_t >( index & 0x1ff ) * 112 ) );
	}

	std::uint32_t entities::get_schema_hash( std::uintptr_t entity ) const
	{
		const auto entity_identity = g::memory.read<std::uintptr_t>( entity + 0x10 );
		if ( !entity_identity )
		{
			return 0;
		}

		const auto entity_class_info = g::memory.read<std::uintptr_t>( entity_identity + 0x8 );
		if ( !entity_class_info )
		{
			return 0;
		}

		const auto schema_name_ptr = g::memory.read<std::uintptr_t>( entity_class_info + 0x8 );
		if ( !schema_name_ptr )
		{
			return 0;
		}

		const auto schema_name = g::memory.read<std::uintptr_t>( schema_name_ptr );
		if ( !schema_name )
		{
			return 0;
		}

		char class_name[ 64 ]{};
		g::memory.read( schema_name, class_name, sizeof( class_name ) );

		if ( !class_name[ 0 ] )
		{
			return 0;
		}

		return fnv1a::runtime_hash( class_name );
	}

	entities::type entities::classify( std::uint32_t schema_hash ) const
	{
		switch ( schema_hash )
		{
		case "CCSPlayerController"_hash:
			return type::player;

		case "C_AK47"_hash:
		case "C_WeaponM4A1"_hash:
		case "C_WeaponM4A1Silencer"_hash:
		case "C_WeaponAWP"_hash:
		case "C_WeaponAug"_hash:
		case "C_WeaponFamas"_hash:
		case "C_WeaponGalilAR"_hash:
		case "C_WeaponSG556"_hash:
		case "C_WeaponG3SG1"_hash:
		case "C_WeaponSCAR20"_hash:
		case "C_WeaponSSG08"_hash:
		case "C_WeaponMAC10"_hash:
		case "C_WeaponMP5SD"_hash:
		case "C_WeaponMP7"_hash:
		case "C_WeaponMP9"_hash:
		case "C_WeaponBizon"_hash:
		case "C_WeaponP90"_hash:
		case "C_WeaponUMP45"_hash:
		case "C_WeaponNOVA"_hash:
		case "C_WeaponSawedoff"_hash:
		case "C_WeaponXM1014"_hash:
		case "C_WeaponMag7"_hash:
		case "C_WeaponM249"_hash:
		case "C_WeaponNegev"_hash:
		case "C_DEagle"_hash:
		case "C_WeaponElite"_hash:
		case "C_WeaponFiveSeven"_hash:
		case "C_WeaponGlock"_hash:
		case "C_WeaponHKP2000"_hash:
		case "C_WeaponUSPSilencer"_hash:
		case "C_WeaponP250"_hash:
		case "C_WeaponCZ75a"_hash:
		case "C_WeaponTec9"_hash:
		case "C_WeaponRevolver"_hash:
		case "C_WeaponTaser"_hash:
		case "C_Knife"_hash:
		case "C_Item_Healthshot"_hash:
		case "C_HEGrenade"_hash:
		case "C_Flashbang"_hash:
		case "C_SmokeGrenade"_hash:
		case "C_MolotovGrenade"_hash:
		case "C_IncendiaryGrenade"_hash:
		case "C_DecoyGrenade"_hash:
			return type::item;

		case "C_HEGrenadeProjectile"_hash:
		case "C_FlashbangProjectile"_hash:
		case "C_SmokeGrenadeProjectile"_hash:
		case "C_MolotovProjectile"_hash:
		case "C_IncendiaryGrenadeProjectile"_hash:
		case "CIncendiaryGrenadeProjectile"_hash:
		case "C_Inferno"_hash:
		case "C_DecoyProjectile"_hash:
			return type::projectile;

		case "C_C4"_hash:
		case "C_PlantedC4"_hash:
			return type::bomb;

		default:
			return type::unknown;
		}
	}

} // namespace systems