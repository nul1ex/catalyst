#pragma once

namespace cstypes {

	enum bone_ids : std::uint32_t
	{
		head = 6,
		neck = 5,
		spine_0 = 1,
		spine_1 = 2,
		spine_2 = 3,
		spine_3 = 4,
		pelvis = 0,

		left_shoulder = 8,
		left_upper_arm = 9,
		left_hand = 10,

		right_shoulder = 13,
		right_upper_arm = 14,
		right_hand = 15,

		left_hip = 22,
		left_knee = 23,
		left_foot = 24,

		right_hip = 25,
		right_knee = 26,
		right_foot = 27,
	};

	enum weapon_type : std::uint32_t
	{
		knife,
		pistol,
		smg,
		rifle,
		shotgun,
		sniper,
		lmg,
		c4,
		taser,
		grenade,
		equipment,
		healthshot
	};

	constexpr inline bool is_weapon_valid( std::uint32_t weapon_type ) { return weapon_type >= 1 && weapon_type <= 6; }

	enum command_buttons : std::uintptr_t
	{
		in_attack = 1 << 0,
		in_jump = 1 << 1,
		in_duck = 1 << 2,
		in_forward = 1 << 3,
		in_back = 1 << 4,
		in_use = 1 << 5,
		in_left = 1 << 7,
		in_right = 1 << 8,
		in_moveleft = 1 << 9,
		in_moveright = 1 << 10,
		in_second_attack = 1 << 11,
		in_reload = 1 << 13,
		in_sprint = 1 << 16,
		in_joyautosprint = 1 << 17,
		in_showscores = 1ULL << 33,
		in_zoom = 1ULL << 34,
		in_lookatweapon = 1ULL << 35
	};

} // namespace cstypes