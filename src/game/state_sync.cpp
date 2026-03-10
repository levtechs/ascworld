#include "game/state_sync.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <unordered_set>

void StateSync::syncLocalPlayer(RootState& state,
                                const std::string& clientUUID,
                                const Player& player,
                                const std::string& username,
                                const CharacterAppearance& appearance,
                                ItemType combatItemType,
                                float combatProgress) {
    auto& ps = state.players[clientUUID];
    ps.uuid = clientUUID;
    ps.name = username;
    ps.appearance = appearance;
    ps.position = player.position();
    ps.yaw = player.yaw();
    ps.pitch = player.pitch();
    // Inventory is host-authoritative (modified via pickup/drop/death through HostAuthority).
    // Don't overwrite it from the Player object - sync flows PlayerState -> Player, not reverse.
    ps.selectedSlot = player.inventory().selectedSlot();
    ps.gameTime = state.world.gameTime;
    // Combat animation state for remote rendering
    ps.activeItemType = combatItemType;
    ps.attackProgress = combatProgress;
}

void StateSync::gatherRemotePlayerObjects(std::vector<SceneObject>& out,
                                           const std::unordered_map<std::string, PlayerState>& players,
                                           const std::string& localUUID,
                                           const std::unordered_set<std::string>& activeUUIDs,
                                           const WeaponMeshes& weaponMeshes,
                                           float gameTime) {
    // Player model meshes:
    // createCylinder is BOTTOM-ALIGNED (Y=0 is bottom, Y=height is top).
    // Player position is at their feet (Y=0 of the player).
    // Player height = 1.8, so body/legs/head must stack up from feet.
    //
    // Layout (from bottom):
    //   Legs:  Y=0.0  to Y=0.7  (two thin cylinders, bottom-aligned at feet)
    //   Body:  Y=0.7  to Y=1.45 (torso cylinder, bottom-aligned at hip)
    //   Head:  centered at Y=1.6 (sphere, radius 0.2)
    static Mesh headMesh = createSphere(6, 8, 0.2f);
    static Mesh bodyMesh = createCylinder(0.18f, 0.75f, 8);
    static Mesh legMesh  = createCylinder(0.1f, 0.7f, 6);
    static bool init = false;
    if (!init) {
        headMesh.computeBoundingRadius();
        bodyMesh.computeBoundingRadius();
        legMesh.computeBoundingRadius();
        init = true;
    }

    for (const auto& [uuid, ps] : players) {
        if (uuid == localUUID) continue;
        // Only render players who are currently active (connected)
        if (activeUUIDs.find(uuid) == activeUUIDs.end()) continue;
        if (ps.position.x == 0.0f && ps.position.y == 0.0f && ps.position.z == 0.0f) continue;

        Color3 col = ps.appearance.color();
        Material mat(col, 16.0f, 0.3f);
        Material legMat(col * 0.7f, 8.0f, 0.2f);

        Vec3 feet = ps.position; // feet position

        if (ps.isDead) {
            // Death fall-over animation: tilt from upright to horizontal over 0.6s
            constexpr float FALL_DURATION = 0.6f;
            constexpr float HALF_PI = 1.5708f;
            float timeSinceDeath = gameTime - ps.deathTime;
            float fallProgress = std::min(timeSinceDeath / FALL_DURATION, 1.0f);
            // Ease-in (accelerating fall, like gravity)
            float easedProgress = fallProgress * fallProgress;
            float tiltAngle = -HALF_PI * easedProgress;

            // Darken more as they fall
            float darkFactor = 0.4f + 0.6f * (1.0f - easedProgress);
            Material deadMat(col * darkFactor, 4.0f + 12.0f * (1.0f - easedProgress), 0.1f + 0.2f * (1.0f - easedProgress));
            Material deadLegMat(col * (darkFactor * 0.75f), 4.0f, 0.1f + 0.1f * (1.0f - easedProgress));

            // Pivot point is at the feet. Body tilts forward (around X axis).
            Mat4 base = Mat4::translate(feet) * Mat4::rotateY(-ps.yaw);

            SceneObject body;
            body.mesh = &bodyMesh;
            body.transform = base * Mat4::rotateX(tiltAngle) * Mat4::translate(Vec3(0.0f, 0.7f, 0.0f));
            body.material = deadMat;
            out.push_back(body);

            SceneObject head;
            head.mesh = &headMesh;
            head.transform = base * Mat4::rotateX(tiltAngle) * Mat4::translate(Vec3(0.0f, 1.6f, 0.0f));
            head.material = deadMat;
            out.push_back(head);

            SceneObject lleg;
            lleg.mesh = &legMesh;
            lleg.transform = base * Mat4::rotateX(tiltAngle) * Mat4::translate(Vec3(-0.1f, 0.0f, 0.0f));
            lleg.material = deadLegMat;
            out.push_back(lleg);

            SceneObject rleg;
            rleg.mesh = &legMesh;
            rleg.transform = base * Mat4::rotateX(tiltAngle) * Mat4::translate(Vec3(0.1f, 0.0f, 0.0f));
            rleg.material = deadLegMat;
            out.push_back(rleg);
            continue;
        }

        // All body parts rotate around feet with player yaw
        // (negate yaw: player yaw is camera convention, opposite of world rotation)
        Mat4 yawRot = Mat4::translate(feet) * Mat4::rotateY(-ps.yaw);

        // Left leg: bottom-aligned at feet, offset left
        SceneObject lleg;
        lleg.mesh = &legMesh;
        lleg.transform = yawRot * Mat4::translate(Vec3(-0.1f, 0.0f, 0.0f));
        lleg.material = legMat;
        out.push_back(lleg);

        // Right leg: bottom-aligned at feet, offset right
        SceneObject rleg;
        rleg.mesh = &legMesh;
        rleg.transform = yawRot * Mat4::translate(Vec3(0.1f, 0.0f, 0.0f));
        rleg.material = legMat;
        out.push_back(rleg);

        // Body: bottom-aligned at hip height (Y=0.7)
        SceneObject body;
        body.mesh = &bodyMesh;
        body.transform = yawRot * Mat4::translate(Vec3(0.0f, 0.7f, 0.0f));
        body.material = mat;
        out.push_back(body);

        // Head: sphere centered at Y=1.6
        SceneObject head;
        head.mesh = &headMesh;
        head.transform = yawRot * Mat4::translate(Vec3(0.0f, 1.6f, 0.0f));
        head.material = mat;
        out.push_back(head);

        // Third-person weapon (if holding something)
        if (ps.activeItemType != ItemType::None) {
            auto weaponObjs = weaponMeshes.getThirdPersonObjects(
                ps.activeItemType, feet, ps.yaw, ps.pitch, ps.attackProgress);
            for (auto& wo : weaponObjs) out.push_back(wo);
        }
    }
}

void StateSync::renderRemoteNameplates(const std::unordered_map<std::string, PlayerState>& players,
                                        const std::string& localUUID,
                                        const std::unordered_set<std::string>& activeUUIDs,
                                        const Camera& camera,
                                        int screenW, int screenH) {
    Mat4 view = camera.viewMatrix();
    Mat4 proj = camera.projectionMatrix(screenW, screenH);

    for (const auto& [uuid, ps] : players) {
        if (uuid == localUUID) continue;
        if (activeUUIDs.find(uuid) == activeUUIDs.end()) continue;
        if (ps.isDead) continue;
        if (ps.name.empty()) continue;
        if (ps.position.x == 0.0f && ps.position.y == 0.0f && ps.position.z == 0.0f) continue;

        // Nameplate position: above head
        Vec3 worldPos = ps.position + Vec3(0.0f, 2.0f, 0.0f);

        // Distance check - only show within 30 units
        Vec3 diff = worldPos - camera.position();
        float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
        if (dist > 30.0f || dist < 0.5f) continue;

        // Project to clip space: MVP * pos
        // view * worldPos
        float vx = view.m[0][0]*worldPos.x + view.m[0][1]*worldPos.y + view.m[0][2]*worldPos.z + view.m[0][3];
        float vy = view.m[1][0]*worldPos.x + view.m[1][1]*worldPos.y + view.m[1][2]*worldPos.z + view.m[1][3];
        float vz = view.m[2][0]*worldPos.x + view.m[2][1]*worldPos.y + view.m[2][2]*worldPos.z + view.m[2][3];
        float vw = view.m[3][0]*worldPos.x + view.m[3][1]*worldPos.y + view.m[3][2]*worldPos.z + view.m[3][3];

        // proj * viewPos
        float cx = proj.m[0][0]*vx + proj.m[0][1]*vy + proj.m[0][2]*vz + proj.m[0][3]*vw;
        float cy = proj.m[1][0]*vx + proj.m[1][1]*vy + proj.m[1][2]*vz + proj.m[1][3]*vw;
        float cw = proj.m[3][0]*vx + proj.m[3][1]*vy + proj.m[3][2]*vz + proj.m[3][3]*vw;

        // Behind camera
        if (cw <= 0.0f) continue;

        // NDC
        float ndcX = cx / cw;
        float ndcY = cy / cw;

        // Out of screen
        if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f) continue;

        // Screen coordinates (terminal: row 1..screenH, col 1..screenW)
        int sx = (int)((ndcX * 0.5f + 0.5f) * screenW);
        int sy = (int)((1.0f - (ndcY * 0.5f + 0.5f)) * screenH);

        // Build display string: "Name HP"
        char label[128];
        snprintf(label, sizeof(label), "%s %dHP", ps.name.c_str(), (int)ps.health);
        int labelLen = (int)std::strlen(label);

        // Center the label
        sx -= labelLen / 2;
        if (sx < 1) sx = 1;
        if (sx + labelLen > screenW) sx = screenW - labelLen;
        if (sy < 1) sy = 1;
        if (sy > screenH) sy = screenH;

        // Fade based on distance
        int brightness = (int)(255.0f * (1.0f - dist / 30.0f));
        if (brightness < 80) brightness = 80;

        // Health color: green > yellow > red
        int hr, hg, hb;
        float hpFrac = ps.health / 100.0f;
        if (hpFrac > 0.5f) { hr = (int)(255*(1.0f - hpFrac)*2); hg = 255; hb = 50; }
        else { hr = 255; hg = (int)(255*hpFrac*2); hb = 50; }

        // Print name in white, then HP in health color
        int nameLen = (int)ps.name.size();
        printf("\033[%d;%dH\033[38;2;%d;%d;%dm%s \033[38;2;%d;%d;%dm%dHP\033[0m",
               sy, sx, brightness, brightness, brightness, ps.name.c_str(),
               hr, hg, hb, (int)ps.health);
    }
}
