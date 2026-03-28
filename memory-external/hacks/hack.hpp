#include <thread>
#include <cmath>
#include "reader.hpp"
#include "weapon_icons.hpp"
#include "../classes/render.hpp"
#include "../classes/config.hpp"
#include "../classes/globals.hpp"

namespace hack {

	static const char* boneNames[] = {
		"head", "neck_0", "spine_1", "spine_2", "pelvis",
		"arm_upper_L", "arm_lower_L", "hand_L",
		"arm_upper_R", "arm_lower_R", "hand_R",
		"leg_upper_L", "leg_lower_L", "ankle_L",
		"leg_upper_R", "leg_lower_R", "ankle_R"
	};
	static const int BONE_COUNT = 17;

	enum BoneIndex {
		B_HEAD = 0, B_NECK, B_SPINE1, B_SPINE2, B_PELVIS,
		B_ARM_UL, B_ARM_LL, B_HAND_L,
		B_ARM_UR, B_ARM_LR, B_HAND_R,
		B_LEG_UL, B_LEG_LL, B_ANKLE_L,
		B_LEG_UR, B_LEG_LR, B_ANKLE_R
	};

	static const int boneConnectionsIndexed[][2] = {
		{B_HEAD, B_NECK},
		{B_NECK, B_SPINE1},
		{B_SPINE1, B_SPINE2},
		{B_SPINE2, B_PELVIS},
		{B_SPINE1, B_ARM_UL},
		{B_ARM_UL, B_ARM_LL},
		{B_ARM_LL, B_HAND_L},
		{B_SPINE1, B_ARM_UR},
		{B_ARM_UR, B_ARM_LR},
		{B_ARM_LR, B_HAND_R},
		{B_PELVIS, B_LEG_UL},
		{B_LEG_UL, B_LEG_LL},
		{B_LEG_LL, B_ANKLE_L},
		{B_PELVIS, B_LEG_UR},
		{B_LEG_UR, B_LEG_LR},
		{B_LEG_LR, B_ANKLE_R},
	};
	static const int BONE_CONNECTION_COUNT = 16;

	static const std::map<std::string, std::string> weaponNames = {
		{"glock", "GLOCK-18"}, {"hkp2000", "P2000"}, {"usp_silencer", "USP-S"},
		{"usp_silencer_off", "USP-S"}, {"elite", "DUAL BERETTAS"}, {"p250", "P250"},
		{"tec9", "TEC-9"}, {"fiveseven", "FIVE-SEVEN"}, {"cz75a", "CZ75-AUTO"},
		{"deagle", "DEAGLE"}, {"revolver", "R8 REVOLVER"},
		{"mac10", "MAC-10"}, {"mp9", "MP9"}, {"mp7", "MP7"}, {"mp5sd", "MP5-SD"},
		{"ump45", "UMP-45"}, {"p90", "P90"}, {"bizon", "PP-BIZON"},
		{"galilar", "GALIL AR"}, {"famas", "FAMAS"}, {"ak47", "AK-47"},
		{"m4a1", "M4A4"}, {"m4a1_silencer", "M4A1-S"}, {"m4a1_silencer_off", "M4A1-S"},
		{"sg556", "SG 553"}, {"aug", "AUG"},
		{"ssg08", "SCOUT"}, {"awp", "AWP"}, {"scar20", "SCAR-20"}, {"g3sg1", "G3SG1"},
		{"nova", "NOVA"}, {"xm1014", "XM1014"}, {"sawedoff", "SAWED-OFF"}, {"mag7", "MAG-7"},
		{"m249", "M249"}, {"negev", "NEGEV"},
		{"knife", "KNIFE"}, {"knife_t", "KNIFE"}, {"bayonet", "BAYONET"},
		{"knife_flip", "FLIP KNIFE"}, {"knife_gut", "GUT KNIFE"},
		{"knife_karambit", "KARAMBIT"}, {"knife_m9_bayonet", "M9 BAYONET"},
		{"knife_tactical", "HUNTSMAN"}, {"knife_falchion", "FALCHION"},
		{"knife_survival_bowie", "BOWIE"}, {"knife_butterfly", "BUTTERFLY"},
		{"knife_push", "SHADOW DAGGERS"}, {"knife_ursus", "URSUS"},
		{"knife_gypsy_jackknife", "NAVAJA"}, {"knife_stiletto", "STILETTO"},
		{"knife_widowmaker", "TALON"}, {"knife_css", "CLASSIC"},
		{"knife_cord", "PARACORD"}, {"knife_canis", "SURVIVAL"},
		{"knife_outdoor", "NOMAD"}, {"knife_skeleton", "SKELETON"}, {"knife_kukri", "KUKRI"},
		{"hegrenade", "HE GRENADE"}, {"flashbang", "FLASH"}, {"smokegrenade", "SMOKE"},
		{"molotov", "MOLOTOV"}, {"incgrenade", "INCENDIARY"}, {"decoy", "DECOY"},
		{"taser", "ZEUS x27"}, {"c4", "C4"},
	};

	std::string GetWeaponDisplayName(const std::string& rawName) {
		auto it = weaponNames.find(rawName);
		if (it != weaponNames.end()) return it->second;
		std::string upper = rawName;
		for (auto& c : upper) c = toupper(c);
		for (auto& c : upper) if (c == '_') c = ' ';
		return upper;
	}

	COLORREF GetEspColorEnemy() { return config::esp_box_color_enemy; }
	COLORREF GetEspColorTeam() { return config::esp_box_color_team; }

	void loop() {

		g_game.update_view_matrix();

		std::lock_guard<std::mutex> lock(reader_mutex);

		if (g_game.isC4Planted)
		{
			Vector3 c4Origin = g_game.c4.get_origin();
			const Vector3 c4ScreenPos = g_game.world_to_screen(&c4Origin);

			if (c4ScreenPos.z >= 0.01f) {
				float c4Distance = g_game.localOrigin.calculate_distance(c4Origin);
				float c4RoundedDistance = std::round(c4Distance / 500.f);

				float height = 10 - c4RoundedDistance;
				float width = height * 1.4f;

				render::DrawFilledBox(g::hdcBuffer, c4ScreenPos.x - (width / 2), c4ScreenPos.y - (height / 2), width, height, GetEspColorEnemy());
				render::RenderTextOutlinedCentered(g::hdcBuffer, c4ScreenPos.x, c4ScreenPos.y - 15, "C4", RGB(255, 50, 50), 10);
			}
		}

		char textBuffer[64];

		for (auto player = g_game.players.begin(); player < g_game.players.end(); player++) {

			bool isEnemy = (g_game.localTeam != player->team);
			bool hasBones = !player->bones.bonePositions.empty();
			COLORREF espColor = isEnemy ? GetEspColorEnemy() : GetEspColorTeam();

			Vector3 boneScreenCache[BONE_COUNT];
			bool boneValid[BONE_COUNT];

			float screenTop = 99999.f;
			float screenBottom = -99999.f;
			float screenLeft = 99999.f;
			float screenRight = -99999.f;
			bool validBox = false;

			if (hasBones) {
				for (int i = 0; i < BONE_COUNT; i++) {
					auto it = player->bones.bonePositions.find(boneNames[i]);
					if (it != player->bones.bonePositions.end()) {
						Vector3 bonePos = it->second;
						boneScreenCache[i] = g_game.world_to_screen(&bonePos);
						boneValid[i] = (boneScreenCache[i].z >= 0.01f);
					}
					else {
						boneValid[i] = false;
					}
				}

				Vector3 feetPos = player->origin;
				Vector3 headPos = { player->origin.x, player->origin.y, player->origin.z + 75.f };

				Vector3 screenFeet = g_game.world_to_screen(&feetPos);
				Vector3 screenHead = g_game.world_to_screen(&headPos);

				if (screenFeet.z >= 0.01f && screenHead.z >= 0.01f) {
					screenTop = screenHead.y;
					screenBottom = screenFeet.y;
					float h = screenBottom - screenTop;
					float w = h / 2.4f;
					float cx = (screenHead.x + screenFeet.x) / 2.f;
					screenLeft = cx - w / 2;
					screenRight = cx + w / 2;
					validBox = true;
				}
			}

			if (!validBox) {
				const Vector3 screenPos = g_game.world_to_screen(&player->origin);
				Vector3 headFallback = { player->origin.x, player->origin.y, player->origin.z + 75.f };
				const Vector3 screenHead = g_game.world_to_screen(&headFallback);
				if (screenPos.z < 0.01f) continue;
				if (!utils.is_in_bounds(screenPos, g_game.game_bounds.right, g_game.game_bounds.bottom)) continue;
				screenTop = screenHead.y;
				screenBottom = screenPos.y;
				float h = screenBottom - screenTop;
				float w = h / 2.4f;
				float cx = (screenHead.x + screenPos.x) / 2.f;
				screenLeft = cx - w / 2;
				screenRight = cx + w / 2;
			}

			float padding = (screenBottom - screenTop) * 0.08f;
			screenTop -= padding;

			float height = screenBottom - screenTop;
			float boneWidth = screenRight - screenLeft;
			float width = boneWidth + padding * 2;
			float centerX = (screenLeft + screenRight) / 2.f;

			if (height < 5.f) continue;
			if (!utils.is_in_bounds({ centerX, screenTop, 1.f }, g_game.game_bounds.right, g_game.game_bounds.bottom)) continue;

			if (player->origin.x == g_game.localOrigin.x &&
				player->origin.y == g_game.localOrigin.y &&
				player->origin.z == g_game.localOrigin.z)
				continue;

			float distance = g_game.localOrigin.calculate_distance(player->origin);
			int roundedDistance = std::round(distance / 10.f);

			bool showBoxEsp = isEnemy ? config::show_box_esp : config::show_box_esp_team;
			bool showSkeletonEsp = isEnemy ? config::show_skeleton_esp : config::show_skeleton_esp_team;

			// SKELETON
			if (showSkeletonEsp && hasBones) {
				int boneOutline = (height > 60.f) ? 3 : 1;
				int boneThick = 1;

				for (int i = 0; i < BONE_CONNECTION_COUNT; i++) {
					int from = boneConnectionsIndexed[i][0];
					int to = boneConnectionsIndexed[i][1];

					if (boneValid[from] && boneValid[to]) {
						render::DrawLine(g::hdcBuffer,
							(int)boneScreenCache[from].x, (int)boneScreenCache[from].y,
							(int)boneScreenCache[to].x, (int)boneScreenCache[to].y,
							RGB(0, 0, 0), boneOutline);
						render::DrawLine(g::hdcBuffer,
							(int)boneScreenCache[from].x, (int)boneScreenCache[from].y,
							(int)boneScreenCache[to].x, (int)boneScreenCache[to].y,
							espColor, boneThick);
					}
				}
			}

			// HEAD TRACKER
			if (config::show_head_tracker && hasBones && boneValid[B_HEAD]) {
				render::DrawCircle(g::hdcBuffer, (int)boneScreenCache[B_HEAD].x, (int)boneScreenCache[B_HEAD].y - (int)(width / 12), (int)(width / 5), espColor);
			}

			// BOX ESP
			if (showBoxEsp) {
				int boxThickness = (height > 80.f) ? 2 : 1;
				render::DrawCornerBox(g::hdcBuffer, (int)(centerX - width / 2), (int)screenTop, (int)width, (int)height, espColor, RGB(0, 0, 0), boxThickness);
			}

			// HEALTH BAR
			if (config::show_health_bar) {
				int barWidth = (height > 80.f) ? 3 : 2;
				int barX = (int)(centerX - width / 2 - 6);
				int barY = (int)screenTop;
				int barH = (int)height;
				COLORREF healthColor;
				if (player->health > 70) healthColor = RGB(0, 255, 50);
				else if (player->health > 40) healthColor = RGB(255, 255, 0);
				else healthColor = RGB(255, 30, 30);
				render::DrawBarOutlined(g::hdcBuffer, barX, barY, barWidth, barH, (float)player->health, healthColor, RGB(20, 20, 20), RGB(0, 0, 0));

				snprintf(textBuffer, sizeof(textBuffer), "%d", player->health);
				render::RenderTextOutlined(g::hdcBuffer, barX - 15, barY - 12, textBuffer, healthColor, 9);
			}

			// ARMOR BAR
			if (config::show_armor_bar) {
				int barWidth = (height > 80.f) ? 3 : 2;
				int barX = (int)(centerX - width / 2 - 12);
				int barY = (int)screenTop;
				int barH = (int)height;
				render::DrawBarOutlined(g::hdcBuffer, barX, barY, barWidth, barH, (float)player->armor, RGB(0, 150, 255), RGB(20, 20, 20), RGB(0, 0, 0));

				snprintf(textBuffer, sizeof(textBuffer), "%d", player->armor);
				render::RenderTextOutlined(g::hdcBuffer, barX - 15, barY - 25, textBuffer, RGB(0, 150, 255), 9);
			}

			// FLAGS
			if (config::show_extra_flags) {
				if (roundedDistance > config::flag_render_distance) continue;

				int fontSize = 10;
				int lineHeight = 13;

				if (config::show_flag_name) {
					render::RenderTextOutlinedCentered(g::hdcBuffer, (int)centerX, (int)screenTop - lineHeight - 2, player->name.c_str(), RGB(255, 255, 255), fontSize);
				}

				int bottomY = (int)screenBottom + 3;
				if (config::show_flag_health) {
					snprintf(textBuffer, sizeof(textBuffer), "%d HP", player->health);
					render::RenderTextOutlinedCentered(g::hdcBuffer, (int)centerX, bottomY, textBuffer, RGB(255, 255, 255), fontSize);
					bottomY += lineHeight;
				}
				if (config::show_flag_armor && player->armor > 0) {
					snprintf(textBuffer, sizeof(textBuffer), "%d AP", player->armor);
					render::RenderTextOutlinedCentered(g::hdcBuffer, (int)centerX, bottomY, textBuffer, RGB(255, 255, 255), fontSize);
					bottomY += lineHeight;
				}

				if (config::show_flag_weapon_icon && render::weaponFontLoaded) {
					const char* icon = GetWeaponIcon(player->weapon);
					render::RenderWeaponIconCentered(g::hdcBuffer, (int)centerX, bottomY, icon, RGB(255, 255, 255));
					bottomY += 22;
				}

				int rightX = (int)(centerX + width / 2 + 5);
				int rightY = (int)screenTop;

				if (config::show_flag_weapon && !(config::show_flag_weapon_icon && render::weaponFontLoaded)) {
					std::string displayName = GetWeaponDisplayName(player->weapon);
					render::RenderTextOutlined(g::hdcBuffer, rightX, rightY, displayName.c_str(), RGB(255, 255, 255), fontSize);
					rightY += lineHeight;
				}
				if (config::show_flag_distance) {
					snprintf(textBuffer, sizeof(textBuffer), "%dm", roundedDistance);
					render::RenderTextOutlined(g::hdcBuffer, rightX, rightY, textBuffer, RGB(255, 255, 255), fontSize);
					rightY += lineHeight;
				}
				if (config::show_flag_money) {
					snprintf(textBuffer, sizeof(textBuffer), "$%d", player->money);
					render::RenderTextOutlined(g::hdcBuffer, rightX, rightY, textBuffer, RGB(255, 255, 255), fontSize);
					rightY += lineHeight;
				}
				if (config::show_flag_flashed && player->flashAlpha > 100) {
					render::RenderTextOutlined(g::hdcBuffer, rightX, rightY, "FLASHED", RGB(255, 255, 255), fontSize);
					rightY += lineHeight;
				}
				if (config::show_flag_defusing && player->is_defusing) {
					render::RenderTextOutlined(g::hdcBuffer, rightX, rightY, "DEFUSING", RGB(255, 255, 255), fontSize);
					rightY += lineHeight;
				}
			}
		}
	}
}