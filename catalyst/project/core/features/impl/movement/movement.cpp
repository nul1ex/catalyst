#include <stdafx.hpp>

namespace features::movement {

	void movement_features::tick()
	{
		const auto pawn = systems::g_local.pawn();
		if (!pawn)
		{
			return;
		}

		HWND foreground = GetForegroundWindow();
		HWND cs2_window = FindWindowA("SDL_app", "Counter-Strike 2");
		if (cs2_window && foreground != cs2_window) {
			return;
		}

		const auto flags = g::memory.read<std::uint32_t>(pawn + SCHEMA("C_BaseEntity", "m_fFlags"_hash));
		const bool on_ground = (flags & (1 << 0));
		if (settings::g_movement.bhop.enabled)
		{
			const bool is_space_down = (GetAsyncKeyState(VK_SPACE) & 0x8000);
			if (is_space_down)
			{
				if (on_ground) {
					g::input.inject_keyboard(VK_F11, false); 
					g::input.inject_keyboard(VK_F11, true);
					g::input.inject_keyboard(VK_F11, false);
				}
			}
		}

		static bool qs_active = false;
		static std::vector<std::uint16_t> qs_keys{};

		if (settings::g_movement.quickstop.enabled)
		{
			const auto velocity = g::memory.read<math::vector3>(pawn + SCHEMA("C_BaseEntity", "m_vecAbsVelocity"_hash));
			const float speed = velocity.length_2d();

			const bool pressing_w = (GetAsyncKeyState('W') & 0x8000);
			const bool pressing_a = (GetAsyncKeyState('A') & 0x8000);
			const bool pressing_s = (GetAsyncKeyState('S') & 0x8000);
			const bool pressing_d = (GetAsyncKeyState('D') & 0x8000);
			const bool manual_move = pressing_w || pressing_a || pressing_s || pressing_d;

			if (on_ground && speed > (13.0f / settings::g_movement.quickstop.strength) && !manual_move)
			{
				if (!qs_active)
				{
					const auto angles = systems::g_view.angles();
					math::vector3 forward, right;
					angles.to_directions(&forward, &right, nullptr);

					const float fwd_vel = velocity.dot(forward);
					const float side_vel = velocity.dot(right);

					qs_keys.clear();
					if (std::abs(fwd_vel) > 13.0f)
					{
						qs_keys.push_back(fwd_vel > 0.0f ? 'S' : 'W');
					}
					if (std::abs(side_vel) > 13.0f)
					{
						qs_keys.push_back(side_vel > 0.0f ? 'A' : 'D');
					}

					for (auto key : qs_keys)
					{
						g::input.inject_keyboard(key, true);
					}
					qs_active = true;
				}
			}
			else if (qs_active)
			{
				for (auto key : qs_keys)
				{
					g::input.inject_keyboard(key, false);
				}
				qs_keys.clear();
				qs_active = false;
			}
		}
		else if (qs_active)
		{
			for (auto key : qs_keys)
			{
				g::input.inject_keyboard(key, false);
			}
			qs_keys.clear();
			qs_active = false;
		}
	}

}
