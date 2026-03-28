#include "reader.hpp"
#include <thread>
#include <map>
#include <cmath>
#include <iostream>
#include "../classes/auto_updater.hpp"
#include "../classes/config.hpp"

// ✅ STATIC — não recria todo frame
static const std::map<std::string, int> boneMap = {
	{"head", 6},
	{"neck_0", 5},
	{"spine_1", 4},
	{"spine_2", 2},
	{"pelvis", 0},
	{"arm_upper_L", 8},
	{"arm_lower_L", 9},
	{"hand_L", 10},
	{"arm_upper_R", 13},
	{"arm_lower_R", 14},
	{"hand_R", 15},
	{"leg_upper_L", 22},
	{"leg_lower_L", 23},
	{"ankle_L", 24},
	{"leg_upper_R", 25},
	{"leg_lower_R", 26},
	{"ankle_R", 27}
};

// ✅ ARRAYS PARA LEITURA BATCH — índice do bone na memória
static const char* boneNamesList[] = {
	"head", "neck_0", "spine_1", "spine_2", "pelvis",
	"arm_upper_L", "arm_lower_L", "hand_L",
	"arm_upper_R", "arm_lower_R", "hand_R",
	"leg_upper_L", "leg_lower_L", "ankle_L",
	"leg_upper_R", "leg_lower_R", "ankle_R"
};
static const int boneIndices[] = {
	6, 5, 4, 2, 0,
	8, 9, 10,
	13, 14, 15,
	22, 23, 24,
	25, 26, 27
};
static const int TOTAL_BONES = 17;

// ✅ Maior bone index é 27, então precisamos ler 28 bones × 0x20 bytes
static const int MAX_BONE_INDEX = 28;
static const int BONE_STRIDE = 0x20;
static const uintptr_t BONE_OFFSET = 0x1E0;

// CC4
uintptr_t CC4::get_planted() {
	return g_game.process->read<uintptr_t>(g_game.process->read<uintptr_t>(g_game.base_client.base + updater::offsets::dwPlantedC4));
}

uintptr_t CC4::get_node() {
	return g_game.process->read<uintptr_t>(get_planted() + updater::offsets::m_pGameSceneNode);
}

Vector3 CC4::get_origin() {
	return g_game.process->read<Vector3>(get_node() + updater::offsets::m_vecAbsOrigin);
}

void CGame::init() {
	std::cout << "[cs2] Waiting for cs2.exe..." << std::endl;

	process = std::make_shared<pProcess>();
	while (!process->AttachProcess("cs2.exe"))
		std::this_thread::sleep_for(std::chrono::seconds(1));

	std::cout << "[cs2] Attached to cs2.exe\n" << std::endl;

	do {
		base_client = process->GetModule("client.dll");
		base_engine = process->GetModule("engine2.dll");
		if (base_client.base == 0 || base_engine.base == 0) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			std::cout << "[cs2] Failed to find module client.dll/engine2.dll, waiting for the game to load it..." << std::endl;
		}
	} while (base_client.base == 0 || base_engine.base == 0);

	GetClientRect(process->hwnd_, &game_bounds);

	buildNumber = process->read<uintptr_t>(base_engine.base + updater::offsets::dwBuildNumber);
}

void CGame::close() {
	std::cout << "[cs2] Deatachig from process" << std::endl;
	process->Close();
}

void CGame::update_view_matrix() {
	view_matrix = process->read<view_matrix_t>(base_client.base + updater::offsets::dwViewMatrix);
}

// ✅ STRUCT PRA LER PLAYER DATA EM BATCH (1 read em vez de 4)
struct PlayerBatchData {
	int health;       // m_iHealth
	int armor;        // m_ArmorValue — PRECISA ESTAR NO OFFSET CERTO
};

void CGame::loop() {
	std::lock_guard<std::mutex> lock(reader_mutex);

	if (updater::offsets::dwLocalPlayerController == 0x0)
		throw std::runtime_error("Offsets have not been correctly set, cannot proceed.");

	inGame = false;
	isC4Planted = false;

	localPlayer = process->read<uintptr_t>(base_client.base + updater::offsets::dwLocalPlayerController);
	if (!localPlayer) return;

	localPlayerPawn = process->read<std::uint32_t>(localPlayer + updater::offsets::m_hPlayerPawn);
	if (!localPlayerPawn) return;

	entity_list = process->read<uintptr_t>(base_client.base + updater::offsets::dwEntityList);

	localList_entry2 = process->read<uintptr_t>(entity_list + 0x8 * ((localPlayerPawn & 0x7FFF) >> 9) + 16);
	localpCSPlayerPawn = process->read<uintptr_t>(localList_entry2 + 112 * (localPlayerPawn & 0x1FF));
	if (!localpCSPlayerPawn) return;

	localTeam = process->read<int>(localPlayer + updater::offsets::m_iTeamNum);

	uintptr_t localGameSceneNode = process->read<uintptr_t>(localpCSPlayerPawn + updater::offsets::m_pGameSceneNode);
	if (localGameSceneNode)
		localOrigin = process->read<Vector3>(localGameSceneNode + updater::offsets::m_vecAbsOrigin);
	else
		localOrigin = process->read<Vector3>(localpCSPlayerPawn + updater::offsets::m_vOldOrigin);

	isC4Planted = process->read<bool>(base_client.base + updater::offsets::dwPlantedC4 - 0x8);

	inGame = true;
	int playerIndex = 0;
	std::vector<CPlayer> list;
	CPlayer player;
	uintptr_t list_entry, list_entry2, playerPawn, playerMoneyServices, clippingWeapon, weaponData;

	while (true) {
		playerIndex++;
		list_entry = process->read<uintptr_t>(entity_list + (8 * (playerIndex & 0x7FFF) >> 9) + 16);
		if (!list_entry) break;

		player.entity = process->read<uintptr_t>(list_entry + 112 * (playerIndex & 0x1FF));
		if (!player.entity) continue;

		player.team = process->read<int>(player.entity + updater::offsets::m_iTeamNum);
		//if (config::team_esp && (player.team == localTeam)) continue;

		playerPawn = process->read<std::uint32_t>(player.entity + updater::offsets::m_hPlayerPawn);

		list_entry2 = process->read<uintptr_t>(entity_list + 0x8 * ((playerPawn & 0x7FFF) >> 9) + 16);
		if (!list_entry2) continue;

		player.pCSPlayerPawn = process->read<uintptr_t>(list_entry2 + 112 * (playerPawn & 0x1FF));
		if (!player.pCSPlayerPawn) continue;

		player.health = process->read<int>(player.pCSPlayerPawn + updater::offsets::m_iHealth);
		player.armor = process->read<int>(player.pCSPlayerPawn + updater::offsets::m_ArmorValue);
		if (player.health <= 0 || player.health > 100) continue;

		//if (config::team_esp && (player.pCSPlayerPawn == localPlayer)) continue;

		uintptr_t handle = process->read<std::uintptr_t>(player.pCSPlayerPawn + updater::offsets::m_hController);
		int index = handle & 0x7FFF;
		int segment = index >> 9;
		int entry = index & 0x1FF;

		uintptr_t controllerListSegment = process->read<uintptr_t>(entity_list + 0x8 * segment + 0x10);
		uintptr_t controller = process->read<uintptr_t>(controllerListSegment + 112 * entry);

		if (!controller) continue;

		char nameBuffer[256] = {};
		process->read_raw(controller + updater::offsets::m_iszPlayerName, nameBuffer, sizeof(nameBuffer) - 1);
		nameBuffer[sizeof(nameBuffer) - 1] = '\0';
		player.name = nameBuffer;

		player.gameSceneNode = process->read<uintptr_t>(player.pCSPlayerPawn + updater::offsets::m_pGameSceneNode);

		if (player.gameSceneNode)
			player.origin = process->read<Vector3>(player.gameSceneNode + updater::offsets::m_vecAbsOrigin);
		else
			player.origin = process->read<Vector3>(player.pCSPlayerPawn + updater::offsets::m_vOldOrigin);

		if (player.origin.x == localOrigin.x && player.origin.y == localOrigin.y && player.origin.z == localOrigin.z)
			continue;

		if (config::render_distance != -1 && (localOrigin - player.origin).length2d() > config::render_distance) continue;
		if (player.origin.x == 0 && player.origin.y == 0) continue;

		// ✅ LÊ BONES EM BATCH — 1 READ em vez de 17
		if (config::show_skeleton_esp || config::show_head_tracker || config::show_box_esp) {
			if (player.gameSceneNode != 0) {
				player.boneArray = process->read<uintptr_t>(player.gameSceneNode + BONE_OFFSET);

				if (player.boneArray != 0) {
					player.ReadBonesBatch();  // ✅ USA BATCH
				}
			}
		}

		auto headBone = player.bones.bonePositions.find("head");
		if (headBone != player.bones.bonePositions.end()) {
			player.head = { headBone->second.x, headBone->second.y, headBone->second.z + 8.f };
		}
		else {
			player.head = { player.origin.x, player.origin.y, player.origin.z + 75.f };
		}

		if (config::show_extra_flags) {
			player.is_defusing = process->read<bool>(player.pCSPlayerPawn + updater::offsets::m_bIsDefusing);

			playerMoneyServices = process->read<uintptr_t>(player.entity + updater::offsets::m_pInGameMoneyServices);
			player.money = process->read<int32_t>(playerMoneyServices + updater::offsets::m_iAccount);

			player.flashAlpha = process->read<float>(player.pCSPlayerPawn + updater::offsets::m_flFlashOverlayAlpha);

			clippingWeapon = process->read<std::uint64_t>(player.pCSPlayerPawn + updater::offsets::m_pClippingWeapon);
			std::uint64_t firstLevel = process->read<std::uint64_t>(clippingWeapon + 0x10);
			weaponData = process->read<std::uint64_t>(firstLevel + 0x20);

			char weaponBuffer[MAX_PATH];
			process->read_raw(weaponData, weaponBuffer, sizeof(weaponBuffer));
			std::string weaponName = std::string(weaponBuffer);
			if (weaponName.compare(0, 7, "weapon_") == 0)
				player.weapon = weaponName.substr(7, weaponName.length());
			else
				player.weapon = "Invalid Weapon Name";
		}

		list.push_back(player);
	}

	players.clear();
	players.assign(list.begin(), list.end());
}

// ✅ LEITURA BATCH DE BONES — 1 READ GRANDE em vez de 17 reads pequenos
void CPlayer::ReadBonesBatch() {
	if (boneArray == 0) return;

	bones.bonePositions.clear();

	// Lê TODOS os bones de uma vez (28 bones × 0x20 = 896 bytes)
	// Antes: 17 chamadas NtReadVirtualMemory
	// Agora: 1 chamada NtReadVirtualMemory
	char boneBuffer[MAX_BONE_INDEX * BONE_STRIDE];
	g_game.process->read_raw(boneArray, boneBuffer, sizeof(boneBuffer));

	for (int i = 0; i < TOTAL_BONES; i++) {
		int boneIdx = boneIndices[i];
		Vector3* pos = (Vector3*)(boneBuffer + boneIdx * BONE_STRIDE);
		bones.bonePositions[boneNamesList[i]] = *pos;
	}
}

void CPlayer::ReadHead() {
	if (boneArray == 0) return;

	uintptr_t boneAddress = boneArray + 6 * BONE_STRIDE;
	Vector3 bonePosition = g_game.process->read<Vector3>(boneAddress);
	bones.bonePositions["head"] = bonePosition;
}

// ✅ MANTÉM O ANTIGO PRA COMPATIBILIDADE (mas não é mais usado)
void CPlayer::ReadBones() {
	if (boneArray == 0) return;

	bones.bonePositions.clear();

	for (const auto& entry : boneMap) {
		const std::string& boneName = entry.first;
		int boneIndex = entry.second;
		uintptr_t boneAddress = boneArray + boneIndex * BONE_STRIDE;
		Vector3 bonePosition = g_game.process->read<Vector3>(boneAddress);
		bones.bonePositions[boneName] = bonePosition;
	}
}

Vector3 CGame::world_to_screen(Vector3* v) {
	float _x = view_matrix[0][0] * v->x + view_matrix[0][1] * v->y + view_matrix[0][2] * v->z + view_matrix[0][3];
	float _y = view_matrix[1][0] * v->x + view_matrix[1][1] * v->y + view_matrix[1][2] * v->z + view_matrix[1][3];

	float w = view_matrix[3][0] * v->x + view_matrix[3][1] * v->y + view_matrix[3][2] * v->z + view_matrix[3][3];

	float inv_w = 1.f / w;
	_x *= inv_w;
	_y *= inv_w;

	float x = game_bounds.right * .5f;
	float y = game_bounds.bottom * .5f;

	x += 0.5f * _x * game_bounds.right + 0.5f;
	y -= 0.5f * _y * game_bounds.bottom + 0.5f;

	return { x, y, w };
}