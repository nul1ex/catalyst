#include <stdafx.hpp>
#include <iostream>

namespace {
	bool ray_hits_capsule( const math::vector3& ray_origin, const math::vector3& ray_dir, const math::vector3& capsule_start, const math::vector3& capsule_end, float radius )
	{
		const auto capsule_vec = capsule_end - capsule_start;
		const auto capsule_length = capsule_vec.length( );

		if ( capsule_length < 0.001f )
		{
			const auto to_center = capsule_start - ray_origin;
			const auto projection = to_center.dot( ray_dir );

			if ( projection < 0.0f )
			{
				return false;
			}

			const auto closest = ray_origin + ray_dir * projection;
			return ( closest - capsule_start ).length_sqr( ) <= radius * radius;
		}

		const auto capsule_dir = capsule_vec / capsule_length;
		const auto w = ray_origin - capsule_start;

		const auto a = ray_dir.dot( ray_dir );
		const auto b = ray_dir.dot( capsule_dir );
		const auto c = capsule_dir.dot( capsule_dir );
		const auto d = ray_dir.dot( w );
		const auto e = capsule_dir.dot( w );

		const auto denom = a * c - b * b;

		float s, t;

		if ( std::abs( denom ) < 0.0001f )
		{
			s = 0.0f;
			t = ( b > c ? d / b : e / c );
		}
		else
		{
			s = ( b * e - c * d ) / denom;
			t = ( a * e - b * d ) / denom;
		}

		t = std::clamp( t, 0.0f, capsule_length );
		if ( s < 0.0f )
		{
			return false;
		}

		const auto point_on_capsule = capsule_start + capsule_dir * t;
		const auto point_on_ray = ray_origin + ray_dir * s;

		return ( point_on_ray - point_on_capsule ).length_sqr( ) <= radius * radius;
	}
}

namespace features::combat {

	void legit::on_render( zdraw::draw_list& draw_list )
	{
		const auto eye_pos = systems::g_view.origin();
		const auto view_angles = systems::g_view.angles();

		const auto& ctx = g_shared.ctx();
		if (!ctx.valid)
		{
			return;
		}

		const auto valid_weapon = cstypes::is_weapon_valid(ctx.weapon_type);
		const auto& cfg = settings::g_combat.get(ctx.weapon_type);

		this->m_fov_alpha.set_target(valid_weapon && cfg.aimbot.draw_fov && cfg.aimbot.enabled ? 1.0f : 0.0f);
		this->m_fov_alpha.update();

		if (this->m_fov_alpha.value() > 0.01f)
		{
			this->draw_fov(draw_list, eye_pos, view_angles, cfg.aimbot);
		}

		if (cfg.aimbot.autowall_info)
		{
			const auto [w, h] = zdraw::get_display_size();

			struct InfoLine
			{
				std::string text;
				bool enabled = true;
			};

			std::vector<InfoLine> lines;

			if (cfg.triggerbot.autowall)
			{
				lines.emplace_back(std::format("awall - min: {:.1f}", cfg.triggerbot.min_damage));
			}

			lines.emplace_back(std::format("fov: {:.1f} hc: {:.1f}",
				static_cast<float>(cfg.aimbot.fov),
				cfg.triggerbot.hitchance));

			if (lines.empty())
				return;

			zdraw::push_font(g::render.fonts().pretzel_24);

			float max_width = 0.0f;
			float total_height = 0.0f;
			const float line_spacing = 2.0f;

			std::vector<std::pair<float, float>> text_sizes;

			for (const auto& line : lines)
			{
				if (!line.enabled || line.text.empty())
					continue;

				auto [tw, th] = zdraw::measure_text(line.text.c_str());
				text_sizes.emplace_back(tw, th);

				max_width = std::max(max_width, tw);
				total_height += th + line_spacing;
			}

			if (text_sizes.empty())
			{
				zdraw::pop_font();
				return;
			}

			total_height -= line_spacing;

			const float x = w * 0.5f;
			const float y = (h * 0.5f) + 35.0f;

			const float padding_x = 4.0f;
			const float padding_y = 2.0f;

			draw_list.add_rect_filled(
				x - (max_width * 0.5f) - padding_x,
				y - padding_y,
				max_width + padding_x * 2.0f,
				total_height + padding_y * 2.0f,
				zdraw::rgba{ 12, 12, 12, 160 }
			);

			draw_list.add_rect(
				x - (max_width * 0.5f) - padding_x,
				y - padding_y,
				max_width + padding_x * 2.0f,
				total_height + padding_y * 2.0f,
				zdraw::rgba{ 45, 45, 45, 200 }
			);

			float current_y = y;

			for (size_t i = 0; i < lines.size(); ++i)
			{
				if (!lines[i].enabled || lines[i].text.empty())
					continue;

				const auto [tw, th] = text_sizes[i];

				draw_list.add_text(
					x - (tw * 0.5f),
					current_y,
					lines[i].text.c_str(),
					zdraw::get_font(),
					cfg.aimbot.autowall_info_color,
					zdraw::text_style::outlined
				);

				current_y += th + line_spacing;
			}

			zdraw::pop_font();
		}

		if (valid_weapon && cfg.other.penetration_crosshair)
		{
			this->draw_penetration_crosshair(draw_list, systems::g_local.eye_position(), view_angles, cfg);
		}
		
		// holy slop never let me make visuals ever again
		if (cfg.triggerbot.enabled && cfg.triggerbot.predictive && cfg.triggerbot.predictive_visualize)
		{
			const auto players = systems::g_collector.players();
			const auto prediction_time = static_cast<float>(cfg.triggerbot.predictive_ms) * 0.001f;

			for (const auto& player : players)
			{
				if (!systems::g_local.is_enemy(player.team) || !player.alive)
					continue;

				const auto bones = systems::g_bones.get(player.bone_cache);
				if (!bones.is_valid()) 
					continue;

				const auto prediction_offset = (player.velocity * prediction_time) + (player.acceleration * 0.5f * prediction_time * prediction_time);

				auto& history = this->m_prediction_history[player.pawn];
				const auto dt = zdraw::get_delta_time();

				if (history.smoothed_offset.length_sqr() == 0.0f)
				{
					history.smoothed_offset = prediction_offset;
				}
				else
				{
					history.smoothed_offset.x += (prediction_offset.x - history.smoothed_offset.x) * 12.0f * dt;
					history.smoothed_offset.y += (prediction_offset.y - history.smoothed_offset.y) * 12.0f * dt;
					history.smoothed_offset.z += (prediction_offset.z - history.smoothed_offset.z) * 12.0f * dt;
				}

				const auto color = zdraw::rgba{ 50, 255, 50, 150 };

				static constexpr std::pair<int, int> connections[] = {
					{ 6, 5 }, { 5, 4 }, { 4, 3 }, { 3, 2 }, { 2, 8 }, { 8, 9 }, { 9, 10 }, { 10, 11 },
					{ 3, 13 }, { 13, 14 }, { 14, 15 }, { 15, 16 }, { 4, 18 }, { 18, 19 }, { 19, 20 },
					{ 4, 21 }, { 21, 22 }, { 22, 23 }
				};

				for (const auto& [a, b] : connections)
				{
					const auto pos_a = systems::g_view.project(bones.get_position(a) + history.smoothed_offset);
					const auto pos_b = systems::g_view.project(bones.get_position(b) + history.smoothed_offset);

					if (systems::g_view.projection_valid(pos_a) && systems::g_view.projection_valid(pos_b))
					{
						draw_list.add_line(pos_a.x, pos_a.y, pos_b.x, pos_b.y, color, 1.0f);
					}
				}
			}
		}

		if (cfg.triggerbot.enabled && cfg.triggerbot.show_spread)
		{
			const auto pawn = systems::g_local.pawn();
			if (pawn && ctx.weapon && ctx.weapon_vdata)
			{
				const auto [display_w, display_h] = zdraw::get_display_size();
				const auto center_x = static_cast<float>(display_w) * 0.5f;
				const auto center_y = static_cast<float>(display_h) * 0.5f;

				const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
				const auto render_tick = g::memory.read<std::uint32_t>(systems::g_local.controller() + SCHEMA("CBasePlayerController", "m_nTickBase"_hash));
				
				const auto fov_rad = systems::g_view.fov() * ( std::numbers::pi_v<float> / 180.0f );
				const auto pixels_per_rad = static_cast<float>(display_w) / ( 2.0f * std::tanf( fov_rad * 0.5f ) );
				
				const auto inac_radius_px = ctx.inaccuracy * pixels_per_rad;
				if ( inac_radius_px > 1.0f && inac_radius_px < center_x )
				{
					draw_list.add_circle( center_x, center_y, inac_radius_px, zdraw::rgba{ 200, 200, 220, 60 }, 64, 1.0f );
				}

				const auto spread_radius_px = ctx.spread * pixels_per_rad;
				if ( spread_radius_px > 1.0f && spread_radius_px < center_x )
				{
					draw_list.add_circle( center_x, center_y, spread_radius_px, zdraw::rgba{ 180, 180, 200, 45 }, 64, 1.0f );
				}

				const auto aim_punch = g::memory.read<math::vector3>( pawn + SCHEMA("C_CSPlayerPawn", "m_aimPunchAngle"_hash) );
				const auto view = view_angles + aim_punch;

				constexpr auto deg2rad = std::numbers::pi_v<float> / 180.0f;
				const auto sp = std::sinf( view.x * deg2rad );
				const auto cp = std::cosf( view.x * deg2rad );
				const auto sy = std::sinf( view.y * deg2rad );
				const auto cy = std::cosf( view.y * deg2rad );

				const math::vector3 forward{ cp * cy, cp * sy, -sp };
				const math::vector3 right{ -sy, cy, 0.0f };
				const math::vector3 up{ sp * cy, sp * sy, cp };

				struct capsule_t
				{
					math::vector3 start;
					math::vector3 end;
					float radius;
				};

				std::vector<capsule_t> all_capsules;
				const auto players = systems::g_collector.players();

				for ( const auto& player : players )
				{
					if ( !systems::g_local.is_enemy(player.team) || player.health <= 0 || player.invulnerable || player.hitboxes.count <= 0 )
						continue;

					const auto bone_data = systems::g_bones.get( player.bone_cache );
					if ( !bone_data.is_valid( ) )
						continue;

					for ( const auto& hb : player.hitboxes )
					{
						if ( hb.index < 0 || hb.bone < 0 || hb.radius <= 0.0f )
							continue;

						const auto& bone = bone_data.bones[ hb.bone ];
						const auto center_local = ( hb.mins + hb.maxs ) * 0.5f;
						const auto center_world = bone.position + math::helpers::rotate_by_quat( bone.rotation, center_local );

						const auto half_extent = ( hb.maxs - hb.mins ) * 0.5f;
						const auto longest = std::max( { std::abs( half_extent.x ), std::abs( half_extent.y ), std::abs( half_extent.z ) } );

						math::vector3 axis_local{};
						if ( std::abs( half_extent.x ) >= std::abs( half_extent.y ) && std::abs( half_extent.x ) >= std::abs( half_extent.z ) )
							axis_local = { longest, 0.0f, 0.0f };
						else if ( std::abs( half_extent.y ) >= std::abs( half_extent.z ) )
							axis_local = { 0.0f, longest, 0.0f };
						else
							axis_local = { 0.0f, 0.0f, longest };

						const auto axis_world = math::helpers::rotate_by_quat( bone.rotation, axis_local );
						const auto capsule_start = center_world - axis_world;
						const auto capsule_end = center_world + axis_world;

						all_capsules.push_back( { capsule_start, capsule_end, hb.radius * 0.9f } );
					}
				}

				const auto eye_origin = eye_pos;

				for ( int i = 0; i < 64; ++i )
				{
					const auto tick = render_tick - 1 + i;
					const auto seed = g_shared.get_spread_seed( view_angles, tick );
					const auto sv = g_shared.calculate_spread( seed + 1, ctx.inaccuracy, ctx.spread, ctx.recoil_index, ctx.item_def_idx, 0 );

					const auto dot_x = center_x + ( sv.x * pixels_per_rad );
					const auto dot_y = center_y + ( -sv.y * pixels_per_rad );

					const auto dir = ( forward + right * -sv.x + up * sv.y ).normalized( );

					auto would_hit{ false };

					for ( const auto& cap : all_capsules )
					{
						if ( ray_hits_capsule( eye_origin, dir, cap.start, cap.end, cap.radius ) )
						{
							would_hit = true;
							break;
						}
					}

					const auto is_fire_tick = ( i < 2 );
					const auto alpha_scale = is_fire_tick ? 1.0f : ( 1.0f - static_cast< float >( i ) / 64.0f );

					if ( is_fire_tick )
					{
						const auto color = would_hit ? zdraw::rgba{ 180, 240, 180, 255 } : zdraw::rgba{ 240, 160, 170, 240 };
						draw_list.add_circle_filled( dot_x, dot_y, 5, color, 8 );
					}
					else
					{
						const auto color = would_hit ? zdraw::rgba{ 160, 220, 190, static_cast< std::uint8_t >( 200.0f * alpha_scale ) } : zdraw::rgba{ 220, 160, 170, static_cast< std::uint8_t >( 140.0f * alpha_scale ) };
						draw_list.add_circle_filled( dot_x, dot_y, 3, color, 6 );
					}
				}
			}
		}
	}

	void legit::tick()
	{
		if (!this->m_rng_seeded)
		{
			this->m_rng.seed(static_cast<int>(std::chrono::steady_clock::now().time_since_epoch().count() & 0x7fffffff));
			this->m_rng_seeded = true;
		}

		if (this->m_trigger_held)
		{
			const auto& ctx = g_shared.ctx();
			if (!ctx.valid || ctx.current_time >= this->m_trigger_release_time)
			{
				g::input.inject_mouse(0, 0, input::left_up);
				this->m_trigger_held = false;
			}
		}

		const auto& ctx = g_shared.ctx();
		if (!ctx.valid)
		{
			return;
		}

		const auto valid_weapon = cstypes::is_weapon_valid(ctx.weapon_type);
		const auto& cfg = settings::g_combat.get(ctx.weapon_type);

		if (!valid_weapon)
		{
			return;
		}

		const auto eye_pos = systems::g_local.eye_position();
		const auto view_angles = g::memory.read<math::vector3>(systems::g_local.pawn() + SCHEMA("C_BasePlayerPawn", "v_angle"_hash));
		const auto camera_angles = systems::g_view.angles();

		const auto players = systems::g_collector.players();

		if (!ctx.is_reloading && ctx.weapon_ready)
		{
			bool aimbot_active = false;
			const auto pawn = systems::g_local.pawn();
			const auto shots = pawn ? g::memory.read<int>(pawn + SCHEMA("C_CSPlayerPawn", "m_iShotsFired"_hash)) : 0;
			
			if (cfg.aimbot.enabled)
			{
				const auto target = this->select_target(eye_pos, view_angles, players, cfg);
				if (target.player)
				{
					if (GetAsyncKeyState(cfg.aimbot.key) & 0x8000)
					{
						this->aimbot(eye_pos, camera_angles, target, cfg.aimbot);
						aimbot_active = true;
					}
				}
			}

			if (cfg.triggerbot.enabled)
			{
				this->triggerbot(eye_pos, view_angles, camera_angles, players, cfg.triggerbot);
			}
		}
		else
		{
			this->m_last_punch = {};
			this->release_autostop(true);
			this->s_magnet_pawn = 0;
		}
	}

	legit::target legit::select_target(const math::vector3& eye_pos, const math::vector3& view_angles, const std::vector<systems::collector::player>& players, const settings::combat::group_config& cfg) const
	{
		target best{};
		best.fov = static_cast<float>(cfg.aimbot.fov);

		for (const auto& player : players)
		{
			if (!systems::g_local.is_enemy(player.team) || !player.alive)
			{
				continue;
			}

			if (player.invulnerable || player.hitboxes.count <= 0)
			{
				continue;
			}

			const auto bones = systems::g_bones.get(player.bone_cache);
			if (!bones.is_valid())
			{
				continue;
			}

			auto damage{ 0.0f };
			auto bone{ -1 };
			auto hitgroup{ -1 };
			math::vector3 offset{};
			auto penetrated{ false };

			const auto aim_point = this->get_aim_point(eye_pos, player, bones, cfg, damage, bone, offset, hitgroup, penetrated);
			if (bone < 0)
			{
				continue;
			}

			const auto fov = this->get_fov(view_angles, eye_pos, aim_point);
			if (fov > best.fov)
			{
				continue;
			}

			best.player = &player;
			best.bones = bones;
			best.aim_point = aim_point;
			best.bone = bone;
			best.offset = offset;
			best.hitgroup = hitgroup;
			best.damage = damage;
			best.fov = fov;
			best.penetrated = penetrated;
		}

		return best;
	}

	math::vector3 legit::get_aim_point(const math::vector3& eye_pos, const systems::collector::player& player, const systems::bones::data& bones, const settings::combat::group_config& cfg, float& out_damage, int& out_bone, math::vector3& out_offset, int& out_hitgroup, bool& out_penetrated) const
	{
		out_bone = -1;

		auto is_hg_enabled = [&](int hg) {
			switch (hg) {
			case 1: return cfg.aimbot.hitgroups.head;
			case 2: return cfg.aimbot.hitgroups.chest;
			case 3: return cfg.aimbot.hitgroups.stomach;
			case 6: case 7: return cfg.aimbot.hitgroups.arms;
			case 4: case 5: return cfg.aimbot.hitgroups.legs;
			default: return false;
			}
		};

		const auto view_angles = systems::g_view.angles();
		float best_fov = static_cast<float>(cfg.aimbot.fov);
		math::vector3 best_point{};

		for (const auto& hb : player.hitboxes)
		{
			if (hb.index < 0 || hb.bone < 0)
			{
				continue;
			}

			const auto hitgroup = systems::g_hitboxes.hitgroup_from_hitbox(hb.index);
			if (!is_hg_enabled(hitgroup))
			{
				continue;
			}

			const auto bone_pos = bones.get_position(hb.bone);
			const auto bone_rot = bones.get_rotation(hb.bone);
			
			// Use the center of the hitbox segment (mins/maxs are usually local to the bone)
			const auto local_center = (hb.mins + hb.maxs) * 0.5f;
			const auto center = bone_pos + bone_rot.rotate_vector(local_center);

			std::vector<math::vector3> points{};
			points.push_back(center);

			if (cfg.aimbot.multipoint)
			{
				const auto scale = cfg.aimbot.multipoint_scale;
				const auto radius = hb.radius * scale;

				if (radius > 0.05f)
				{
					const auto right = bone_rot.rotate_vector({ 0.0f, 1.0f, 0.0f });
					const auto up = bone_rot.rotate_vector({ 0.0f, 0.0f, 1.0f });

					if (hitgroup == 1) // head
					{
						points.push_back(center + up * radius);
						points.push_back(center - up * radius);
						points.push_back(center + right * radius);
						points.push_back(center - right * radius);
						
						const auto diagonal = (right + up).normalized();
						points.push_back(center + diagonal * radius);
						points.push_back(center - diagonal * radius);
					}
					else
					{
						points.push_back(center + right * radius);
						points.push_back(center - right * radius);
						points.push_back(center + up * radius);
						points.push_back(center - up * radius);
					}
				}
			}

			for (const auto& pos : points)
			{
				if (cfg.aimbot.smoke_check && systems::g_voxels.line_goes_through_smoke(eye_pos, pos))
				{
					continue;
				}

				float current_damage = 0.0f;
				bool current_penetrated = false;
				bool valid = false;

				if (!cfg.aimbot.visible_only)
				{
					current_damage = combat::g_shared.pen().get_max_damage(hitgroup, player.armor, player.has_helmet, player.team);
					current_penetrated = false;
					valid = true;
				}
				else
				{
					const auto trace = systems::g_bvh.trace_ray(eye_pos, pos);
					auto visible = !trace.hit || trace.fraction > 0.97f;

					if (visible)
					{
						current_damage = combat::g_shared.pen().get_max_damage(hitgroup, player.armor, player.has_helmet, player.team);
						current_penetrated = false;
						valid = true;
					}
					else if (cfg.aimbot.autowall)
					{
						shared::penetration::result pen_result{};
						if (combat::g_shared.pen().run(eye_pos, pos, player, bones, pen_result))
						{
							if (pen_result.damage >= cfg.aimbot.min_damage)
							{
								current_damage = pen_result.damage;
								current_penetrated = pen_result.penetrated;
								valid = true;
							}
						}
					}
				}

				if (valid)
				{
					const auto fov = this->get_fov(view_angles, eye_pos, pos);
					if (fov < best_fov)
					{
						best_fov = fov;
						best_point = pos;
						out_damage = current_damage;
						out_bone = hb.bone;
						out_offset = pos - bone_pos; // Store offset relative to the base bone position
						out_hitgroup = hitgroup;
						out_penetrated = current_penetrated;
					}
				}
			}
		}

		return best_point;
	}

	float legit::get_fov(const math::vector3& view_angles, const math::vector3& eye_pos, const math::vector3& target_pos) const
	{
		return math::helpers::calculate_fov(view_angles, eye_pos, target_pos);
	}

	float legit::get_fov_radius(const math::vector3& eye_pos, const math::vector3& view_angles, float fov_degrees) const
	{
		if (fov_degrees <= 0.0f)
		{
			return 0.0f;
		}

		math::vector3 forward{};
		view_angles.to_directions(&forward, nullptr, nullptr);

		auto offset_angles = view_angles;
		offset_angles.x -= fov_degrees;

		math::vector3 offset_forward{};
		offset_angles.to_directions(&offset_forward, nullptr, nullptr);

		const auto center = systems::g_view.project(eye_pos + forward * 1000.0f);
		const auto edge = systems::g_view.project(eye_pos + offset_forward * 1000.0f);

		if (!systems::g_view.projection_valid(center) || !systems::g_view.projection_valid(edge))
		{
			return 0.0f;
		}

		const auto dx = edge.x - center.x;
		const auto dy = edge.y - center.y;

		return std::sqrtf(dx * dx + dy * dy);
	}

	void legit::draw_penetration_crosshair(zdraw::draw_list& draw_list, const math::vector3& eye_pos, const math::vector3& view_angles, const settings::combat::group_config& cfg)
	{
		math::vector3 forward{};
		view_angles.to_directions(&forward, nullptr, nullptr);

		const auto first_hit = systems::g_bvh.trace_ray(eye_pos, eye_pos + forward * g_shared.pen().get_weapon_data().range);
		if (!first_hit.hit)
		{
			return;
		}

		auto pen_damage{ 0.0f };
		const auto can_pen = g_shared.pen().can(eye_pos, forward, pen_damage);

		const auto& n = first_hit.normal;
		const auto ref = (std::abs(n.z) < 0.9f) ? math::vector3{ 0.0f, 0.0f, 1.0f } : math::vector3{ 1.0f, 0.0f, 0.0f };

		const auto d = ref.dot(n);
		const auto tangent = (ref - n * d).normalized();
		const auto bitangent = n.cross(tangent);

		const auto center = first_hit.end_pos + n * 0.05f;
		constexpr auto half_size{ 3.5f };

		const math::vector3 corners[4]
		{
			center - tangent * half_size - bitangent * half_size,
			center + tangent * half_size - bitangent * half_size,
			center + tangent * half_size + bitangent * half_size,
			center - tangent * half_size + bitangent * half_size,
		};

		float sx[5]{}, sy[5]{};

		for (int i = 0; i < 4; ++i)
		{
			const auto proj = systems::g_view.project(corners[i]);
			if (!systems::g_view.projection_valid(proj))
			{
				return;
			}

			sx[i] = proj.x;
			sy[i] = proj.y;
		}

		const auto center_proj = systems::g_view.project(center);
		if (!systems::g_view.projection_valid(center_proj))
		{
			return;
		}

		sx[4] = center_proj.x;
		sy[4] = center_proj.y;

		const auto& color = can_pen ? cfg.other.penetration_color_yes : cfg.other.penetration_color_no;
		const auto edge = zdraw::rgba{ color.r, color.g, color.b, static_cast<std::uint8_t>(color.a / 4) };

		for (int i = 0; i < 4; ++i)
		{
			const auto j = (i + 1) % 4;
			draw_list.add_triangle_filled_multi_color(sx[4], sy[4], sx[i], sy[i], sx[j], sy[j], color, edge, edge);
		}

		float screen[8]{ sx[0], sy[0], sx[1], sy[1], sx[2], sy[2], sx[3], sy[3] };
		draw_list.add_polyline({ screen, 8 }, { color.r, color.g, color.b, 255 }, true, 1.0f);

		if (cfg.other.penetration_damage && can_pen)
		{
			const auto text = std::format("{:.0f}", pen_damage);
			zdraw::push_font(g::render.fonts().pretzel_12);
			const auto [tw, th] = zdraw::measure_text(text.c_str());
			
			draw_list.add_text(
				sx[4] - (tw * 0.5f),
				sy[4] + 8.0f,
				text.c_str(),
				zdraw::get_font(),
				{ 255, 255, 255, 220 },
				zdraw::text_style::outlined
			);
			
			zdraw::pop_font();
		}
	}

	void legit::draw_fov( zdraw::draw_list& draw_list, const math::vector3& eye_pos, const math::vector3& view_angles, const settings::combat::aimbot_settings& cfg )
	{
		const auto target_radius = this->get_fov_radius(eye_pos, view_angles, static_cast<float>(cfg.fov));
		const auto alpha = this->m_fov_alpha.value();
		const auto radius = target_radius * alpha;

		if (radius <= 0.5f)
		{
			return;
		}

		const auto [w, h] = zdraw::get_display_size();
		const auto color = zdraw::rgba{ cfg.fov_color.r, cfg.fov_color.g, cfg.fov_color.b, static_cast<std::uint8_t>(alpha * 125.0f) };

		draw_list.add_circle(w * 0.5f, h * 0.5f, radius, color, 16);
	}

	void legit::aimbot(const math::vector3& eye_pos, const math::vector3& view_angles, const target& tgt, const settings::combat::aimbot_settings& cfg)
	{
		HWND foreground = GetForegroundWindow();
		HWND cs2_window = FindWindowA("SDL_app", "Counter-Strike 2");
		if (cs2_window && foreground != cs2_window) {
			this->m_aim_error = {};
			return;
		}

		if (!(GetAsyncKeyState(cfg.key) & 0x8000)) {
			this->m_aim_error = {};
			return;
		}

		constexpr auto m_yaw = 0.022f;
		const auto sensitivity = systems::g_convars.get<float>(CONVAR("sensitivity"_hash));
		const auto fov_adjust = g::memory.read<float>(systems::g_local.pawn() + SCHEMA("C_BasePlayerPawn", "m_flFOVSensitivityAdjust"_hash));
		const auto deg_per_pixel = sensitivity * m_yaw * fov_adjust;

		if (deg_per_pixel <= 0.0f) return;

		const auto freshest = systems::g_bones.get(tgt.player->bone_cache);
		if (!freshest.is_valid()) return;

		auto aim_point = freshest.get_position(tgt.bone) + tgt.offset;

		if (cfg.predictive) {
			const auto time = g_shared.get_prediction_time( );
			aim_point += ( tgt.player->velocity * time ) + ( tgt.player->acceleration * 0.5f * time * time );
		}

		auto desired = math::helpers::calculate_angle(eye_pos, aim_point);

		math::helpers::normalize_angles(desired);

		if (cfg.silent) {
			for (int i = 0; i < 200; i++)
				g::memory.write(systems::g_local.pawn() + SCHEMA("C_BasePlayerPawn", "v_angle"_hash), desired);
			return;
		}

		if (cfg.rcs && cfg.rcs_factor > 0.0f) {
			const auto aim_punch = g::memory.read<math::vector3>(
				systems::g_local.pawn() + SCHEMA("C_CSPlayerPawn", "m_aimPunchAngle"_hash)
			);
			const auto shots_fired = g::memory.read<int>(
				systems::g_local.pawn() + SCHEMA("C_CSPlayerPawn", "m_iShotsFired"_hash)
			);

			if (cfg.rcs && shots_fired > 1) {
				desired.x -= aim_punch.x * cfg.rcs_factor * 2.0f;
				desired.y -= aim_punch.y * cfg.rcs_factor * 1.5f;
			}
		}

		auto delta_x = desired.x - view_angles.x;
		auto delta_y = math::helpers::normalize_yaw(desired.y - view_angles.y);

		if (cfg.smoothing > 1)
		{
			const auto factor = static_cast<float>(cfg.smoothing);
			delta_x /= factor;
			delta_y /= factor;
		}

		const auto move_x = -delta_y / deg_per_pixel; 
		const auto move_y = delta_x / deg_per_pixel;  

		this->m_aim_error.x += move_x;
		this->m_aim_error.y += move_y;

		auto dx = static_cast<int>(this->m_aim_error.x);
		auto dy = static_cast<int>(this->m_aim_error.y);

		this->m_aim_error.x -= static_cast<float>(dx);
		this->m_aim_error.y -= static_cast<float>(dy);

		if (dx != 0 || dy != 0) {
			if (cfg.smoothing <= 1.0f) {
				constexpr auto max_move = 120;
				dx = std::clamp(dx, -max_move, max_move);
				dy = std::clamp(dy, -max_move, max_move);
			}
			
			g::input.inject_mouse(dx, dy, input::move);
		}
	}

	legit::trigger_result legit::trace_crosshair(const math::vector3& eye_pos, const math::vector3& view_angles, const std::vector<systems::collector::player>& players, const settings::combat::triggerbot_settings& cfg) const
	{
		trigger_result result{};

		math::vector3 forward{};
		view_angles.to_directions(&forward, nullptr, nullptr);

		constexpr auto max_range{ 8192.0f };
		const auto end_pos = eye_pos + forward * max_range;

		const auto world_trace = systems::g_bvh.trace_ray(eye_pos, end_pos);
		auto best_dist_sq = max_range * max_range;

		auto is_hg_enabled = [&](int hg) {
			switch (hg) {
			case 1: return cfg.hitgroups.head;
			case 2: return cfg.hitgroups.chest;
			case 3: return cfg.hitgroups.stomach;
			case 6: case 7: return cfg.hitgroups.arms;
			case 4: case 5: return cfg.hitgroups.legs;
			default: return false;
			}
		};

		for (const auto& player : players)
		{
			if (!systems::g_local.is_enemy(player.team) || !player.alive)
			{
				continue;
			}

			if (player.invulnerable || player.hitboxes.count <= 0)
			{
				continue;
			}

			const auto bones = systems::g_bones.get(player.bone_cache);
			if (!bones.is_valid())
			{
				continue;
			}

			auto& history = this->m_prediction_history[ player.pawn ];

			if ( cfg.predictive )
			{
				const auto prediction_time = static_cast< float >( cfg.predictive_ms ) * 0.001f;
				const auto prediction_offset = ( player.velocity * prediction_time ) + ( player.acceleration * 0.5f * prediction_time * prediction_time );

				if ( history.smoothed_offset.length_sqr( ) == 0.0f )
				{
					history.smoothed_offset = prediction_offset;
				}
				else
				{
					constexpr float tick_dt = 1.0f / 64.0f;
					history.smoothed_offset.x += ( prediction_offset.x - history.smoothed_offset.x ) * 25.0f * tick_dt;
					history.smoothed_offset.y += ( prediction_offset.y - history.smoothed_offset.y ) * 25.0f * tick_dt;
					history.smoothed_offset.z += ( prediction_offset.z - history.smoothed_offset.z ) * 25.0f * tick_dt;
				}
			}
			else
			{
				history.smoothed_offset = {};
			}

			for ( const auto& hb : player.hitboxes )
			{
				if ( hb.index < 0 || hb.bone < 0 )
				{
					continue;
				}

				const auto hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( hb.index );
				if ( !is_hg_enabled( hitgroup ) )
				{
					continue;
				}

				const auto& bone = bones.bones[ hb.bone ];
				const auto center_local = ( hb.mins + hb.maxs ) * 0.5f;
				const auto center_world = bone.position + math::helpers::rotate_by_quat( bone.rotation, center_local ) + history.smoothed_offset;

				const auto half_extent = (hb.maxs - hb.mins) * 0.5f;
				const auto longest = std::max({ std::abs(half_extent.x), std::abs(half_extent.y), std::abs(half_extent.z) });

				math::vector3 axis_local{};
				if (std::abs(half_extent.x) >= std::abs(half_extent.y) && std::abs(half_extent.x) >= std::abs(half_extent.z))
					axis_local = { longest, 0.0f, 0.0f };
				else if (std::abs(half_extent.y) >= std::abs(half_extent.z))
					axis_local = { 0.0f, longest, 0.0f };
				else
					axis_local = { 0.0f, 0.0f, longest };

				const auto axis_world = math::helpers::rotate_by_quat(bone.rotation, axis_local);
				const auto capsule_start = center_world - axis_world;
				const auto capsule_end = center_world + axis_world;

				if (!ray_hits_capsule(eye_pos, forward, capsule_start, capsule_end, hb.radius))
				{
					continue;
				}

				const auto dist_sq = (center_world - eye_pos).length_sqr();

				if (dist_sq >= best_dist_sq)
				{
					continue;
				}

				if (systems::g_voxels.line_goes_through_smoke(eye_pos, center_world))
				{
					continue;
				}

				const auto vis_trace = systems::g_bvh.trace_ray(eye_pos, center_world);
				auto visible = !vis_trace.hit || vis_trace.fraction > 0.97f;

				if (visible)
				{
					const auto damage = combat::g_shared.pen().get_max_damage(hitgroup, player.armor, player.has_helmet, player.team);

					best_dist_sq = dist_sq;
					result.player = &player;
					result.bones = bones;
					result.hitbox = hb.index;
					result.hitgroup = hitgroup;
					result.damage = damage;
					result.penetrated = false;
					result.sim_time = 0.0f;
					result.smoothed_offset = history.smoothed_offset;
				}
				else if (cfg.autowall)
				{
					shared::penetration::result pen_result{};
					if (combat::g_shared.pen().run(eye_pos, center_world, player, bones, pen_result))
					{
						if (pen_result.damage >= cfg.min_damage)
						{
							best_dist_sq = dist_sq;
							result.player = &player;
							result.bones = bones;
							result.hitbox = pen_result.hitbox;
							result.hitgroup = systems::g_hitboxes.hitgroup_from_hitbox(pen_result.hitbox);
							result.damage = pen_result.damage;
							result.penetrated = pen_result.penetrated;
							result.sim_time = 0.0f;
							result.smoothed_offset = history.smoothed_offset;
						}
					}
				}
			}

			if (result.player)
			{
				break;
			}
		}

		return result;
	}

	void legit::triggerbot(const math::vector3& eye_pos, const math::vector3& view_angles, const math::vector3& camera_angles, const std::vector<systems::collector::player>& players, const settings::combat::triggerbot_settings& cfg)
	{		
		HWND foreground = GetForegroundWindow();
		HWND cs2_window = FindWindowA("SDL_app", "Counter-Strike 2");
		if (cs2_window && foreground != cs2_window) {
			this->release_autostop(true);
			s_magnet_pawn = 0;
			this->m_trigger_waiting = false;
			return;
		}

		if (!(GetAsyncKeyState(cfg.key) & 0x8000))
		{
			this->release_autostop(true);
			s_magnet_pawn = 0;
			this->m_trigger_waiting = false;
			return;
		}

		this->release_autostop();

		const auto pawn = systems::g_local.pawn();
		const auto velocity = pawn ? g::memory.read<math::vector3>(pawn + SCHEMA("C_BaseEntity", "m_vecAbsVelocity"_hash)) : math::vector3{};
		const auto speed = velocity.length_2d();
		const auto flags = pawn ? g::memory.read<std::uint32_t>(pawn + SCHEMA("C_BaseEntity", "m_fFlags"_hash)) : 0u;
		const auto on_ground = (flags & 1) != 0;
		const auto is_moving = speed > 5.0f;

		auto trace_pos = eye_pos;
		auto found_at_extrapolated{ false };

		if (cfg.autostop && cfg.early_autostop && on_ground && is_moving)
		{
			constexpr auto lookahead_ticks{ 4 };
			constexpr auto tick_interval{ 0.015625f };
			const auto lookahead_pos = eye_pos + math::vector3{ velocity.x * tick_interval * lookahead_ticks, velocity.y * tick_interval * lookahead_ticks, 0.0f };

			trace_pos = g_shared.extrapolate_stop(lookahead_pos);

			const auto extrap_result = this->trace_crosshair(trace_pos, camera_angles, players, cfg);
			if (extrap_result.player)
			{
				found_at_extrapolated = true;
			}
		}

		const auto result = found_at_extrapolated ? this->trace_crosshair(trace_pos, camera_angles, players, cfg) : this->trace_crosshair(eye_pos, camera_angles, players, cfg);
		
		this->m_last_trigger_damage = result.damage;
		this->m_min_trigger_damage = cfg.min_damage;

		bool has_target = result.player != nullptr;
		if (has_target && result.penetrated && result.damage < cfg.min_damage)
		{
			has_target = false;
		}

		if (has_target && cfg.magnet)
		{
			s_magnet_pawn = result.player->pawn;
		}

		float dist_to_center = 0.0f;

		if (cfg.magnet && s_magnet_pawn != 0)
		{
			const systems::collector::player* target_ptr = nullptr;
			systems::bones::data target_bones{};

			for (const auto& p : players) {
				if (p.pawn == s_magnet_pawn) {
					target_ptr = &p;
					target_bones = systems::g_bones.get(p.bone_cache);
					if (!target_ptr->alive) {
						s_magnet_pawn = 0;
					}
					break;
				}
			}

			if (target_ptr && target_ptr->alive && target_ptr->health > 0 && !target_ptr->invulnerable && target_bones.is_valid())
			{
				math::vector3 aim_point{};
				bool found_head = false;

				for (const auto& hb : target_ptr->hitboxes)
				{
					if (hb.index < 0 || hb.bone < 0) continue;
					if (systems::g_hitboxes.hitgroup_from_hitbox(hb.index) == 1) // head
					{
						const auto bone_pos = target_bones.get_position(hb.bone);
						const auto bone_rot = target_bones.get_rotation(hb.bone);
						const auto local_center = (hb.mins + hb.maxs) * 0.5f;
						aim_point = bone_pos + bone_rot.rotate_vector(local_center);
						found_head = true;
						break;
					}
				}

				if (found_head)
				{
					const auto smoked = systems::g_voxels.line_goes_through_smoke(eye_pos, aim_point);

					if (smoked)
					{
						s_magnet_pawn = 0;
					}
					else
					{
						const auto trace = systems::g_bvh.trace_ray(eye_pos, aim_point);
						const auto visible = !trace.hit || trace.fraction > 0.97f;

						if (!visible && !cfg.autowall)
						{
							s_magnet_pawn = 0;
						}
						else
						{
							if (cfg.predictive) {
								const auto prediction_time = static_cast<float>(cfg.predictive_ms) * 0.001f;
								const auto prediction_offset = (target_ptr->velocity * prediction_time) + (target_ptr->acceleration * 0.5f * prediction_time * prediction_time);

								auto& history = this->m_prediction_history[target_ptr->pawn];
								if (history.smoothed_offset.length_sqr() == 0.0f)
								{
									history.smoothed_offset = prediction_offset;
								}
								else
								{
									constexpr float tick_dt = 1.0f / 64.0f;
									history.smoothed_offset.x += (prediction_offset.x - history.smoothed_offset.x) * 25.0f * tick_dt;
									history.smoothed_offset.y += (prediction_offset.y - history.smoothed_offset.y) * 25.0f * tick_dt;
									history.smoothed_offset.z += (prediction_offset.z - history.smoothed_offset.z) * 25.0f * tick_dt;
								}

								aim_point += history.smoothed_offset;
							}

							if (this->get_fov(camera_angles, eye_pos, aim_point) > 35.0f)
							{
								s_magnet_pawn = 0;
							}
							else
							{
								auto desired = math::helpers::calculate_angle(eye_pos, aim_point);

								const auto pawn_ptr = systems::g_local.pawn();
								const auto aim_punch = pawn_ptr ? g::memory.read<math::vector3>(pawn_ptr + SCHEMA("C_CSPlayerPawn", "m_aimPunchAngle"_hash)) : math::vector3{};
								const auto shots_fired = pawn_ptr ? g::memory.read<int>(pawn_ptr + SCHEMA("C_CSPlayerPawn", "m_iShotsFired"_hash)) : 0;

								if (shots_fired > 0) {
									desired.x -= aim_punch.x * 2.0f;
									desired.y -= aim_punch.y * 2.0f;
								}

								math::helpers::normalize_angles(desired);

								auto delta_x = desired.x - camera_angles.x;
								auto delta_y = math::helpers::normalize_yaw(desired.y - camera_angles.y);

								dist_to_center = std::sqrtf(delta_x * delta_x + delta_y * delta_y);

								if (cfg.magnet_smoothing > 1)
								{
									const auto factor = static_cast<float>(cfg.magnet_smoothing);
									delta_x /= factor;
									delta_y /= factor;
								}

								const auto sensitivity = systems::g_convars.get<float>(CONVAR("sensitivity"_hash));
								const auto fov_adjust = pawn_ptr ? g::memory.read<float>(pawn_ptr + SCHEMA("C_BasePlayerPawn", "m_flFOVSensitivityAdjust"_hash)) : 1.0f;
								const auto deg_per_pixel = sensitivity * 0.022f * fov_adjust;

								if (deg_per_pixel > 0.0f) {
									const auto move_x = -delta_y / deg_per_pixel;
									const auto move_y = delta_x / deg_per_pixel;

									this->m_aim_error.x += move_x;
									this->m_aim_error.y += move_y;

									auto dx = static_cast<int>(this->m_aim_error.x);
									auto dy = static_cast<int>(this->m_aim_error.y);

									this->m_aim_error.x -= static_cast<float>(dx);
									this->m_aim_error.y -= static_cast<float>(dy);

									if (dx != 0 || dy != 0) {
										if (cfg.magnet_smoothing <= 1) {
											dx = std::clamp(dx, -240, 240);
											dy = std::clamp(dy, -240, 240);
										}
										g::input.inject_mouse(dx, dy, input::move);
									}
								}
							}
						}
					}
				}
				else
				{
					s_magnet_pawn = 0;
				}
			}
		}

		if (!has_target && !cfg.seed_triggerbot)
		{
			this->m_trigger_waiting = false;
			return;
		}

		if (cfg.magnet && (!has_target || result.hitgroup != 1 || dist_to_center > 0.75f))
		{
			this->m_trigger_waiting = false;
			return;
		}

		if (this->m_trigger_held)
		{
			return;
		}

		if (cfg.autostop && on_ground && is_moving && has_target)
		{
			this->apply_autostop();

			if (speed > 30.0f)
			{
				return;
			}
		}

		const auto& ctx = g_shared.ctx();
		if (!ctx.weapon_ready)
		{
			this->m_trigger_waiting = false;
			return;
		}

		const bool is_autowall_hit = has_target && result.penetrated;

		bool seed_hit = false;
		if (cfg.seed_triggerbot)
		{
			constexpr auto required_ticks{ 1 };
			auto all_ticks_hit{ true };

			const auto aim_punch = g::memory.read<math::vector3>(pawn + SCHEMA("C_CSPlayerPawn", "m_aimPunchAngle"_hash));
			const auto view = view_angles + aim_punch;

			constexpr auto deg2rad = std::numbers::pi_v<float> / 180.f;
			const auto sp = std::sinf(view.x * deg2rad);
			const auto cp = std::cosf(view.x * deg2rad);
			const auto sy = std::sinf(view.y * deg2rad);
			const auto cy = std::cosf(view.y * deg2rad);

			const math::vector3 fwd{ cp * cy, cp * sy, -sp };
			const math::vector3 right{ -sy, cy, 0.f };
			const math::vector3 up{ sp * cy, sp * sy, cp };

			auto is_hg_enabled_seed = [&](int hg) {
				switch (hg) {
				case 1: return cfg.hitgroups.head;
				case 2: return cfg.hitgroups.chest;
				case 3: return cfg.hitgroups.stomach;
				case 6: case 7: return cfg.hitgroups.arms;
				case 4: case 5: return cfg.hitgroups.legs;
				default: return false;
				}
			};


			const auto render_tick = g::memory.read<std::uint32_t>(systems::g_local.controller() + SCHEMA("CBasePlayerController", "m_nTickBase"_hash));
			static auto prev_render_tick{ 0 };

			prev_render_tick = render_tick;

			for (int tick_offset = 0; tick_offset < required_ticks; ++tick_offset)
			{
				const auto seed = g_shared.get_spread_seed(view_angles, render_tick - 1 + tick_offset);
				const auto sv = g_shared.calculate_spread(seed + 1, ctx.inaccuracy, ctx.spread, ctx.recoil_index, ctx.item_def_idx, 0);
				const auto dir = (fwd + right * -sv.x + up * sv.y).normalized();

				auto hit_any{ false };

				for (const auto& target : players)
				{
					if (!systems::g_local.is_enemy(target.team) || target.health <= 0 || target.invulnerable || target.hitboxes.count <= 0)
						continue;

					const auto bone_data = systems::g_bones.get(target.bone_cache);
					if (!bone_data.is_valid())
						continue;

					for (const auto& hb : target.hitboxes)
					{
						if (hb.index < 0 || hb.bone < 0 || hb.radius <= 0.0f)
							continue;

						if (!is_hg_enabled_seed(systems::g_hitboxes.hitgroup_from_hitbox(hb.index)))
							continue;

						const auto& bone = bone_data.bones[hb.bone];
						const auto center_local = (hb.mins + hb.maxs) * 0.5f;
						const auto center_world = bone.position + math::helpers::rotate_by_quat(bone.rotation, center_local);

						const auto half_extent = (hb.maxs - hb.mins) * 0.5f;
						const auto longest = std::max({ std::abs(half_extent.x), std::abs(half_extent.y), std::abs(half_extent.z) });

						math::vector3 axis_local{};
						if (std::abs(half_extent.x) >= std::abs(half_extent.y) && std::abs(half_extent.x) >= std::abs(half_extent.z))
							axis_local = { longest, 0.0f, 0.0f };
						else if (std::abs(half_extent.y) >= std::abs(half_extent.z))
							axis_local = { 0.0f, longest, 0.0f };
						else
							axis_local = { 0.0f, 0.0f, longest };

						const auto axis_world = math::helpers::rotate_by_quat(bone.rotation, axis_local);
						const auto capsule_start = center_world - axis_world;
						const auto capsule_end = center_world + axis_world;

						if (ray_hits_capsule(eye_pos, dir, capsule_start, capsule_end, hb.radius))
						{

							shared::penetration::result pen_result{};
							if (g_shared.pen().run(eye_pos, eye_pos + dir * 8192.0f, target, bone_data, pen_result))
							{
								const auto required_damage = is_autowall_hit ? 1.0f : cfg.min_damage;
								if (pen_result.damage >= required_damage)
								{
									hit_any = true;
									this->m_last_trigger_damage = pen_result.damage;
									break;
								}
							}
						}
					}

					if (hit_any)
						break;
				}

				if (!hit_any)
				{
					all_ticks_hit = false;
					break;
				}
			}

			if (all_ticks_hit)
			{
				seed_hit = true;
			}
		}

		if (cfg.seed_triggerbot && !seed_hit)
		{
			if (has_target && cfg.hitchance > 0.0f)
			{
				const auto required = cfg.hitchance / 100.0f;
				const auto hc = g_shared.calculate_hitchance(eye_pos, camera_angles, *result.player, result.bones, result.smoothed_offset);

				if (hc < required)
				{
					this->m_trigger_waiting = false;
					return;
				}
			}
			else
			{
				this->m_trigger_waiting = false;
				return;
			}
		}
		else if (!cfg.seed_triggerbot && cfg.hitchance > 0.0f)
		{
			const auto required = cfg.hitchance / 100.0f;
			const auto hc = g_shared.calculate_hitchance(eye_pos, camera_angles, *result.player, result.bones, result.smoothed_offset);

			if (hc < required)
			{
				this->m_trigger_waiting = false;
				return;
			}
		}

		const auto now = ctx.current_time;

		if (!this->m_trigger_waiting)
		{
			this->m_trigger_waiting = true;
			this->m_trigger_delay_end = now + static_cast<float>(cfg.delay) * 0.001f;
			return;
		}

		if (now < this->m_trigger_delay_end)
		{
			return;
		}

		this->m_trigger_waiting = false;

		const auto hold_ms = this->m_rng.random_float(50.0f, 120.0f);

		g::input.inject_mouse(0, 0, input::left_down);
		this->m_trigger_held = true;
		this->m_trigger_release_time = now + hold_ms * 0.001f;
	}

	void legit::apply_autostop()
	{
		const auto pawn = systems::g_local.pawn();
		if (!pawn)
		{
			return;
		}

		const auto flags = g::memory.read<std::uint32_t>(pawn + SCHEMA("C_BaseEntity", "m_fFlags"_hash));
		if (!(flags & (1 << 0)))
		{
			return;
		}

		const auto velocity = g::memory.read<math::vector3>(pawn + SCHEMA("C_BaseEntity", "m_vecAbsVelocity"_hash));
		const float speed = velocity.length_2d();
		if (speed <= 13.0f)
		{
			return;
		}

		if (this->m_autostop_active)
		{
			return;
		}

		const auto angles = systems::g_view.angles();
		math::vector3 forward, right;
		angles.to_directions(&forward, &right, nullptr);

		// Get current velocity vectors relative to view
		const float fwd_vel = velocity.dot(forward);
		const float side_vel = velocity.dot(right);

		this->m_autostop_keys.clear();
		this->m_autostop_inhibited_keys.clear();

		auto inhibit_key = [&](std::uint16_t key) {
			if (GetAsyncKeyState(key) & 0x8000) {
				g::input.inject_keyboard(key, false);
				this->m_autostop_inhibited_keys.push_back(key);
			}
		};

		// Determine counter-strafe keys
		if (fwd_vel > 13.0f) {
			this->m_autostop_keys.push_back('S');
			inhibit_key('W');
		}
		else if (fwd_vel < -13.0f) {
			this->m_autostop_keys.push_back('W');
			inhibit_key('S');
		}

		if (side_vel > 13.0f) {
			this->m_autostop_keys.push_back('A');
			inhibit_key('D');
		}
		else if (side_vel < -13.0f) {
			this->m_autostop_keys.push_back('D');
			inhibit_key('A');
		}

		if (this->m_autostop_keys.empty())
		{
			return;
		}

		for (const auto key : this->m_autostop_keys)
		{
			g::input.inject_keyboard(key, true);
		}

		this->m_autostop_active = true;
		this->m_autostop_start = std::chrono::steady_clock::now();
	}

	void legit::release_autostop(bool force)
	{
		if (!this->m_autostop_active)
		{
			return;
		}

		const auto pawn = systems::g_local.pawn();
		if (!pawn && !force)
		{
			return;
		}

		const auto velocity = pawn ? g::memory.read<math::vector3>(pawn + SCHEMA("C_BaseEntity", "m_vecAbsVelocity"_hash)) : math::vector3{};
		const float speed = velocity.length_2d();

		const auto angles = systems::g_view.angles();
		math::vector3 forward, right;
		angles.to_directions(&forward, &right, nullptr);

		const float fwd_vel = velocity.dot(forward);
		const float side_vel = velocity.dot(right);

		bool should_release = force || speed < 13.0f;

		if (!should_release)
		{
			for (const auto key : this->m_autostop_keys)
			{
				if (key == 'S' && fwd_vel <= 0.0f) should_release = true;
				else if (key == 'W' && fwd_vel >= 0.0f) should_release = true;
				else if (key == 'A' && side_vel <= 0.0f) should_release = true;
				else if (key == 'D' && side_vel >= 0.0f) should_release = true;

				if (should_release)
					break;
			}
		}

		if (!should_release)
		{
			return;
		}

		for (const auto key : this->m_autostop_keys)
		{
			g::input.inject_keyboard(key, false);
		}

		for (const auto key : this->m_autostop_inhibited_keys)
		{
			if (GetAsyncKeyState(key) & 0x8000) {
				g::input.inject_keyboard(key, true);
			}
		}

		this->m_autostop_keys.clear();
		this->m_autostop_inhibited_keys.clear();
		this->m_autostop_active = false;
	}

} // namespace features::combat