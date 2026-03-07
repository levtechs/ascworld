#pragma once

struct WorldEntity;

// Render a "[G] Pick up <ItemName>" prompt when near a dropped item.
// Pass nullptr if no item is nearby.
void renderPickupPrompt(int screenW, int screenH, const WorldEntity* nearbyItem);
