#pragma once

class Inventory;

// Render the hotbar at the bottom center of the screen.
// Shows item slots with the selected slot highlighted.
void renderHotbar(int screenW, int screenH, const Inventory& inv);
