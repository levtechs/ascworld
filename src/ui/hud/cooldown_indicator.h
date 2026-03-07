#pragma once

class Inventory;

// Render a cooldown bar below the hotbar for the active weapon.
void renderCooldownIndicator(int screenW, int screenH, const Inventory& inv);
