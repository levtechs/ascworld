#pragma once
#include "game/root_state.h"
#include "game/player.h"
#include "game/character_appearance.h"
#include "game/weapon_meshes.h"
#include "game/camera.h"
#include "rendering/mesh.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

// Responsible for synchronizing local Player data into the shared RootState
// and producing SceneObjects for remote players. Keeps sync logic out of main.cpp.
class StateSync {
public:
    // Sync the local player's full state into activeState.players[clientUUID].
    // Call every frame before networking sends deltas.
    // combatItemType/combatProgress carry the current attack animation state.
    static void syncLocalPlayer(RootState& state,
                                const std::string& clientUUID,
                                const Player& player,
                                const std::string& username,
                                const CharacterAppearance& appearance,
                                ItemType combatItemType = ItemType::None,
                                float combatProgress = -1.0f);

    // Gather SceneObjects representing all active remote players (everyone except localUUID
    // whose UUID is in activeUUIDs). Dead players are rendered as horizontal bodies.
    // Includes body model + third-person held weapon + attack animation.
    // Appends to `out`.
    static void gatherRemotePlayerObjects(std::vector<SceneObject>& out,
                                          const std::unordered_map<std::string, PlayerState>& players,
                                          const std::string& localUUID,
                                          const std::unordered_set<std::string>& activeUUIDs,
                                          const WeaponMeshes& weaponMeshes,
                                          float gameTime);

    // Render usernames above active remote players by projecting world positions to screen coords.
    // Call AFTER fb.render() so text overlays the 3D scene.
    static void renderRemoteNameplates(const std::unordered_map<std::string, PlayerState>& players,
                                       const std::string& localUUID,
                                       const std::unordered_set<std::string>& activeUUIDs,
                                       const Camera& camera,
                                       int screenW, int screenH);
};
