#define NOMINMAX
#include <windows.h>

#include <vector>
#include <mutex>
#include <cassert>
#include <psapi.h>
#include <optional>

#include <witch_cult/math.h>
#include "request_api.h"
#include "memory.h"

class WinProcessMemoryReader {
  public:
    explicit WinProcessMemoryReader(DWORD pid) noexcept : pid(pid) {
        handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
        assert(handle);
        assert(handle != INVALID_HANDLE_VALUE);
    }
    // ── 核心读取接口 ─────────────────────────────────────────────────────
    [[nodiscard]] auto read_into(std::size_t virtualAddress, std::span<std::uint8_t> dest) -> std::size_t {
        if (virtualAddress == 0) {
            return 0;
        }
        if (dest.empty()) {
            return 0;
        }

        SIZE_T bytes = 0;
        BOOL ok = ReadProcessMemory(handle, reinterpret_cast<LPCVOID>(virtualAddress), dest.data(), dest.size_bytes(),
                                    &bytes);

        if (!ok) {
            return 0;
        }

        return static_cast<std::size_t>(bytes);
    }

    auto get_module_base(std::string_view moduleName) -> std::size_t {
        DWORD cbNeeded;

        std::vector<HMODULE> modules(1024);

        if (EnumProcessModulesEx(handle, modules.data(), modules.size() * sizeof(HMODULE), &cbNeeded,
                                 LIST_MODULES_ALL)) {
            size_t moduleCount = cbNeeded / sizeof(HMODULE);

            for (size_t i = 0; i < moduleCount; i++) {
                char moduleNameBuffer[MAX_PATH];

                if (GetModuleBaseNameA(handle, modules[i], moduleNameBuffer, sizeof(moduleNameBuffer))) {
                    if (_stricmp(moduleNameBuffer, moduleName.data()) == 0) {
                        return (intptr_t)modules[i];
                    }
                }
            }
        }

        return 0;
    }

    DWORD pid;
    HANDLE handle;
};

// struct CheatTemplate {
//     std::optional<WinProcessMemoryReader> r1;
//     std::optional<RequestContext> r2;
// };

namespace cheat {

    namespace offset {
        constexpr uintptr_t dwLocalPlayerController = 0x22F3178;
        constexpr uintptr_t EntityList = 0x24AF268;
        constexpr uintptr_t ViewMatrix = 0x230FF20;
        constexpr uintptr_t m_vOldOrigin = 0x1588;
        constexpr uintptr_t m_hPlayerPawn = 0x90C;
        constexpr uintptr_t m_vecOrigin = 0x608;
        constexpr uintptr_t m_vecViewOffset = 0xD58;

    }; // namespace offset
    struct PlayerSnapshot {
        witch_cult::math::Vector3 origin{};
        witch_cult::math::Vector3 head{};
        witch_cult::math::Vector3 angEyeAngles;
    };

    struct GameSnapshot {
        std::vector<PlayerSnapshot> players;
        witch_cult::math::Vector3 localOrigin;
        witch_cult::math::Matrix4 viewMatrix;
        witch_cult::math::Vector3 angEyeAngles;
    };

    struct SnapshotManager {
        SnapshotManager() {
            buffers[0].players.reserve(64);
            buffers[1].players.reserve(64);
        }

        auto getWriteBuffer() -> GameSnapshot & {
            int idx = writeIdx.load(std::memory_order_relaxed);
            return buffers[idx];
        }

        auto publish() {
            int old_write = writeIdx.load(std::memory_order_relaxed);
            int new_write = 1 - old_write;

            {
                std::lock_guard<std::mutex> lock(swapMutex);
                writeIdx.store(new_write, std::memory_order_release);
                readIdx.store(old_write, std::memory_order_release);
            }
        }

        auto tryGetLatst() const -> const GameSnapshot * {
            const int idx = readIdx.load(std::memory_order_acquire);
            if (idx < 0)
                return nullptr;
            const auto &snap = buffers[idx];
            return &snap;
        }

        std::mutex swapMutex;
        std::atomic<int> writeIdx{0};
        std::atomic<int> readIdx{-1};
        std::array<GameSnapshot, 2> buffers;
    };
    static uintptr_t clientHandle = 0;
    // 游戏窗口不激活时，不执行
    auto loop(SnapshotManager &snapshots, MemoryReaderAdapter<RequestContext> &reader) {
        auto &snapshot = snapshots.getWriteBuffer();
        snapshot.players.clear();
        // static std::once_flag flag;
        // std::call_once(flag, [&] {
        //     clientHandle = reader.get_module_base(L"client.dll");
        //     spdlog::debug("client.dll at 0x{:x}", clientHandle);
        // });
        if (!clientHandle) {
            spdlog::error("Failed to read module client");
            return;
        }
        // auto LocalController = reader.read_as<uintptr_t>(clientHandle + offset::dwLocalPlayerController);
        // if (!LocalController) {
        //     spdlog::error("Failed to read LocalController");
        //     return;
        // }
        auto EntityList = reader.read_as<uintptr_t>(clientHandle + offset::EntityList);
        if (!EntityList) {
            spdlog::error("Failed to read EntityList");
            return;
        }
        auto viewMatrix = reader.read_as<witch_cult::math::Matrix4>(clientHandle + offset::ViewMatrix);
        if (!viewMatrix) {
            spdlog::error("Failed to read viewMatrix");
            return;
        }
        snapshot.viewMatrix = *viewMatrix;

        std::size_t playerIndex{0};
        while (true) {
            PlayerSnapshot playerSnap{};
            playerIndex++;

            auto list_entry = reader.read_as<uintptr_t>(*EntityList + (8 * (playerIndex & 0x7FFF) >> 9) + 16);
            if (!list_entry) {
                spdlog::error("Failed to read list_entry");
                break;
            }
            if (!*list_entry) {
                spdlog::error("Failed to read list_entry2");
                break;
            }

            auto entity = reader.read_as<uintptr_t>(*list_entry + 112 * (playerIndex & 0x1FF));
            if (!entity) {
                spdlog::error("Failed to read entity");
                continue;
            }
            if (!*entity) {
                spdlog::error("Failed to read entity2");
                continue;
            }

            auto playerPawn = reader.read_as<std::uint32_t>(*entity + offset::m_hPlayerPawn);
            if (!playerPawn) {
                spdlog::error("Failed to read playerPawn");
                continue;
            }

            auto list_entry2 = reader.read_as<uintptr_t>(*EntityList + 0x8 * ((*playerPawn & 0x7FFF) >> 9) + 16);
            if (!list_entry2) {
                spdlog::error("Failed to read list_entry2");
                continue;
            }
            if (!*list_entry2) {
                spdlog::debug("client.dll at 0x{:x}", clientHandle);
                spdlog::error("Failed to read list_entry2 {},EntityList {}", *list_entry2, *EntityList);
                continue;
            }

            auto pCSPlayerPawn = reader.read_as<uintptr_t>(*list_entry2 + 112 * (*playerPawn & 0x1FF));
            if (!pCSPlayerPawn) {
                spdlog::error("Failed to read pCSPlayerPawn");
                continue;
            }
            if (!*pCSPlayerPawn) {
                spdlog::error("Failed to read pCSPlayerPawn");
                continue;
            }

            // playerSnap.angEyeAngles = read<witch_cult::math::Vector3>(player.pCSPlayerPawn + m_angEyeAngles);

            auto origin = reader.read_as<witch_cult::math::Vector3>(*pCSPlayerPawn + offset::m_vOldOrigin);
            if (!origin) {
                spdlog::error("Failed to read origin");
                continue;
            }
            spdlog::info("origin->x {}, origin->y {}, origin->z {}", origin->x, origin->y, origin->z);
            playerSnap.origin = *origin;
            playerSnap.head = {origin->x, origin->y, origin->z + 75.f};
            snapshot.players.push_back(playerSnap);
        }
#if 0
        std::lock_guard<std::mutex> lock(reader_mutex);
        inGame = false;
        isC4Planted = false;
        localPlayer = read<uintptr_t>(clientHandle + dwLocalPlayerController);
        if (!localPlayer)
            return;

        localPlayerPawn = read<std::uint32_t>(localPlayer + m_hPlayerPawn);
        if (!localPlayerPawn)
            return;
        entity_list = read<uintptr_t>(clientHandle + dwEntityList);
        localList_entry2 = read<uintptr_t>(entity_list + 0x8 * ((localPlayerPawn & 0x7FFF) >> 9) + 16);
        localpCSPlayerPawn = read<uintptr_t>(localList_entry2 + 112 * (localPlayerPawn & 0x1FF));
        if (!localpCSPlayerPawn)
            return;

        view_matrix = read<view_matrix_t>(clientHandle + dwViewMatrix);
        snapshot.viewMat = read<witch_cult::math::Matrix4>(clientHandle + dwViewMatrix);

        localTeam = read<int>(localPlayer + m_iTeamNum);
        localOrigin = read<Vector3>(localpCSPlayerPawn + m_vOldOrigin);
        snapshot.localOrigin = read<witch_cult::math::Vector3>(localpCSPlayerPawn + m_vOldOrigin);
        snapshot.angEyeAngles = read<witch_cult::math::Vector3>(localpCSPlayerPawn + m_angEyeAngles);
        isC4Planted = read<bool>(clientHandle + dwPlantedC4 - 0x8);

        inGame = true;
        int playerIndex = 0;
        std::vector<CPlayer> list;
        CPlayer player;
        uintptr_t list_entry, list_entry2, playerPawn, playerMoneyServices, clippingWeapon, weaponData, playerNameData;
        while (true) {
            PlayerSnapshot playerSnap{};
            playerIndex++;
            list_entry = read<uintptr_t>(entity_list + (8 * (playerIndex & 0x7FFF) >> 9) + 16);
            if (!list_entry)
                break;

            player.entity = read<uintptr_t>(list_entry + 112 * (playerIndex & 0x1FF));
            if (!player.entity)
                continue;

            /**
             * Skip rendering your own character and teammates
             *
             * If you really want you can exclude your own character from the check but
             * since you are in the same team as yourself it will be excluded anyway
             **/
            player.team = read<int>(player.entity + m_iTeamNum);
            // if (config::team_esp && (player.team == localTeam))
            //     continue;

            playerPawn = read<std::uint32_t>(player.entity + m_hPlayerPawn);

            list_entry2 = read<uintptr_t>(entity_list + 0x8 * ((playerPawn & 0x7FFF) >> 9) + 16);
            if (!list_entry2)
                continue;

            player.pCSPlayerPawn = read<uintptr_t>(list_entry2 + 112 * (playerPawn & 0x1FF));
            if (!player.pCSPlayerPawn)
                continue;

            player.health = read<int>(player.pCSPlayerPawn + m_iHealth);
            player.armor = read<int>(player.pCSPlayerPawn + m_ArmorValue);
            if (player.health <= 0 || player.health > 100)
                continue;

            if ((player.pCSPlayerPawn == localPlayer))
                continue;

            /*
             * Unused for now, but for a vis check
             *
             * player.spottedState = process->read<uintptr_t>(player.pCSPlayerPawn + 0x1630);
             * player.is_spotted = process->read<DWORD_PTR>(player.spottedState + 0xC); // bSpottedByMask
             * player.is_spotted = process->read<bool>(player.spottedState + 0x8); // bSpotted
             */

            // Read entity controller from the player pawn
            uintptr_t handle = read<std::uintptr_t>(player.pCSPlayerPawn + m_hController);
            int index = handle & 0x7FFF;
            int segment = index >> 9;
            int entry = index & 0x1FF;

            uintptr_t controllerListSegment = read<uintptr_t>(entity_list + 0x8 * segment + 0x10);
            uintptr_t controller = read<uintptr_t>(controllerListSegment + 112 * entry);

            if (!controller)
                continue;

            // Read player name from the controller
            char buffer[256] = {};
            read_raw(controller + m_iszPlayerName, buffer, sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';
            player.name = buffer;

            player.gameSceneNode = read<uintptr_t>(player.pCSPlayerPawn + m_pGameSceneNode);
            player.origin = read<Vector3>(player.pCSPlayerPawn + m_vOldOrigin);
            playerSnap.origin = read<witch_cult::math::Vector3>(player.pCSPlayerPawn + m_vOldOrigin);
            player.head = {player.origin.x, player.origin.y, player.origin.z + 75.f};
            playerSnap.head = {player.origin.x, player.origin.y, player.origin.z + 75.f};

            playerSnap.angEyeAngles = read<witch_cult::math::Vector3>(player.pCSPlayerPawn + m_angEyeAngles);

            if (player.origin.x == localOrigin.x && player.origin.y == localOrigin.y &&
                player.origin.z == localOrigin.z)
                continue;

            // if (config::render_distance != -1 && (localOrigin - player.origin).length2d() > config::render_distance)
            //     continue;
            if (player.origin.x == 0 && player.origin.y == 0)
                continue;

            // Bone array offset updated from 0x1F0 to 0x210 (m_modelState + 128)
            // if (config::show_skeleton_esp) {
            //     player.gameSceneNode = process->read<uintptr_t>(player.pCSPlayerPawn +
            //     updater::offsets::m_pGameSceneNode); player.boneArray = process->read<uintptr_t>(player.gameSceneNode
            //     + 0x210); player.ReadBones();
            // }

            // // Apply the same fix here
            // if (config::show_head_tracker && !config::show_skeleton_esp) {
            //     player.gameSceneNode = process->read<uintptr_t>(player.pCSPlayerPawn +
            //     updater::offsets::m_pGameSceneNode); player.boneArray = process->read<uintptr_t>(player.gameSceneNode
            //     + 0x210); player.ReadHead();
            // }

            // if (config::show_extra_flags) {
            //     /*
            //      * Reading values for extra flags is now separated from the other reads
            //      * This removes unnecessary memory reads, improving performance when not showing extra flags
            //      */
            //     player.is_defusing = process->read<bool>(player.pCSPlayerPawn + updater::offsets::m_bIsDefusing);

            //     playerMoneyServices = process->read<uintptr_t>(player.entity +
            //     updater::offsets::m_pInGameMoneyServices); player.money = process->read<int32_t>(playerMoneyServices
            //     + updater::offsets::m_iAccount);

            //     player.flashAlpha = process->read<float>(player.pCSPlayerPawn +
            //     updater::offsets::m_flFlashOverlayAlpha);

            //     clippingWeapon = process->read<std::uint64_t>(player.pCSPlayerPawn +
            //     updater::offsets::m_pClippingWeapon); std::uint64_t firstLevel =
            //     process->read<std::uint64_t>(clippingWeapon + 0x10); // First offset weaponData =
            //     process->read<std::uint64_t>(firstLevel + 0x20);                   // Final offset
            //     /*weaponData = process->read<std::uint64_t>(clippingWeapon + 0x10);
            //     weaponData = process->read<std::uint64_t>(weaponData + updater::offsets::m_szName);*/
            //     char buffer[MAX_PATH];
            //     process->read_raw(weaponData, buffer, sizeof(buffer));
            //     std::string weaponName = std::string(buffer);
            //     if (weaponName.compare(0, 7, "weapon_") == 0)
            //         player.weapon = weaponName.substr(7, weaponName.length()); // Remove weapon_ prefix
            //     else
            //         player.weapon = "Invalid Weapon Name";
            // }

            list.push_back(player);
            snapshot.players.push_back(playerSnap);
        }
        players.clear();
        players.assign(list.begin(), list.end());
#endif
        snapshots.publish();
    }
    auto readTask(std::stop_token st, SnapshotManager &snapshots, MemoryReaderAdapter<RequestContext> reader) {
        while (clientHandle == 0) {
            clientHandle = reader.get_module_base(L"client.dll");
            spdlog::debug("Failed to read client.dll");
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
        spdlog::debug("client.dll at 0x{:x}", clientHandle);
        while (!st.stop_requested()) {
            loop(snapshots, reader);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        reader.underlying().terminate();
    }

    struct Renderer {
        using Vector2 = witch_cult::math::Vector2;
        auto drawRect(const Vector2 pos, const Vector2 size, float thickness = 1.) {
            ImDrawList *draw = ImGui::GetBackgroundDrawList();
            draw->AddRect(ImVec2(pos.x, pos.y), ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(255, 0, 0, 255), 0.0f,
                          0, thickness);
        }
        auto drawRectFilled(const Vector2 pos, const Vector2 size) {}
        auto drawLine(const Vector2 pos1, const Vector2 pos2, float thickness = 1.) {
            ImDrawList *draw = ImGui::GetBackgroundDrawList();
            draw->AddLine(ImVec2(pos1.x, pos1.y), ImVec2(pos2.x, pos2.y), IM_COL32(255, 0, 0, 255), thickness);
        }

        auto drawCircleFilled(const Vector2 p, const float radius, std::uint32_t color) {
            ImDrawList *draw = ImGui::GetBackgroundDrawList();
            draw->AddCircleFilled(ImVec2(p.x, p.y), radius, color, 64);
        }
        auto drawCircle() {}
        auto drawTriangleFilled(const Vector2 p1, const Vector2 p2, const Vector2 p3, std::uint32_t color) {
            ImDrawList *draw = ImGui::GetBackgroundDrawList();
            draw->AddTriangleFilled(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), ImVec2(p3.x, p3.y), color);
        }
        auto drawTriangle() {}
    };
    Renderer renderer;
    witch_cult::math::Vector3 worldToScreen(const GameSnapshot &snapshot, const witch_cult::math::Vector3 &v) {
        // 1. 坐标变换：World Space -> Clip Space
        auto clipSpace = snapshot.viewMatrix * witch_cult::math::Vector4{v, 1.f};
        // 3. 透视除法：Clip Space -> Normalized Device Coordinates (NDC)
        // NDC 范围通常是 [-1, 1]
        float invW = 1.0f / clipSpace.w;
        witch_cult::math::Vector2 ndc{clipSpace.x * invW, clipSpace.y * invW};

        // 4. 视口变换：NDC -> Screen Space
        // 优化：预先计算一半的宽高
        float halfWidth = GetSystemMetrics(SM_CXSCREEN) * 0.5f;
        float halfHeight = GetSystemMetrics(SM_CYSCREEN) * 0.5f;

        float screenX = halfWidth + (ndc.x * halfWidth);
        float screenY = halfHeight - (ndc.y * halfHeight); // Y轴翻转，屏幕坐标系通常左上角为(0,0)

        return {screenX, screenY, clipSpace.w};
    }
    auto render(const GameSnapshot &snapshot) {
        for (const auto &player : snapshot.players) {
            const auto screenPos = worldToScreen(snapshot, player.origin);
            const auto screenHead = worldToScreen(snapshot, player.head);

            // 2. 裁剪检查,玩家在摄像机后面,过滤掉
            if (screenPos.z < 0.01f) {
                continue;
            }
            auto in_rect = [](const witch_cult::math::Vector3 &p, float left, float top, float right,
                              float bottom) -> bool {
                return p.x >= left && p.x <= right && p.y >= top && p.y <= bottom;
            };
            // 2. 判断是否在屏幕内 + 是否在可视范围内
            if (!in_rect(screenPos, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)))
                continue;
            // 3. 计算框的大小（基于头部和脚部高度）
            const float height = screenPos.y - screenHead.y;
            const float width = height / 2.4f; // 2.4 是经验值（人物身高比例）
            // // 4. 计算距离（用于过滤和显示），本地玩家 到 敌人 的距离
            float distance = snapshot.localOrigin.distance_to(player.origin);
            int roundedDistance = std::round(distance / 10.f);
            // 单位换算比例,游戏里的单位 ≠ 现实世界的“米”,把“游戏单位”转换成“米”
            // int roundedDistance = std::round(distance / 10.f);
            // 框要以头为中心
            const auto rectPos = screenHead.with_x(screenHead.x - width / 2).to_vec2();
            renderer.drawRect(rectPos, {width, height});
        }
    }
} // namespace cheat