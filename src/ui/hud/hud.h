#pragma once

class Player;
class Inventory;
struct WorldEntity;

// Forward declare the individual HUD element renderers.
// Each element is a free function in its own file for easy extension.
//
// To add a new HUD element:
//   1. Create src/ui/hud/my_element.h/.cpp with a render function
//   2. Call it from renderHUD() in hud.cpp
//   3. Add the .cpp to CMakeLists.txt

// Master HUD render — call after fb.render() each frame.
// screenW/screenH = terminal dimensions.
// gameTime = elapsed game time (for animations).
// nearbyItem = item within pickup range (or nullptr).
void renderHUD(int screenW, int screenH, const Player& player, float gameTime,
               const WorldEntity* nearbyItem = nullptr,
               float health = 100.0f,
               float maxHealth = 100.0f,
               int kills = 0,
               int deaths = 0);
