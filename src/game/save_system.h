#pragma once
#include "core/vec_math.h"
#include <string>
#include <vector>
#include <cstdint>

// Data stored in a save file
struct SaveData {
    std::string name;       // World name (display name)
    uint32_t seed = 0;      // World generation seed
    Vec3 playerPos;         // Player position
    float playerYaw = 0.0f;
    float playerPitch = 0.0f;
    int64_t timestamp = 0;  // Unix timestamp of last save
    
    // Derived/display
    std::string filename;   // Filename on disk (without path)
};

// Manages saving/loading game state to ~/.ascii3d/saves/
class SaveSystem {
public:
    SaveSystem();
    
    // Save current game state. Returns true on success.
    bool save(const SaveData& data);
    
    // Load a specific save file by filename. Returns true on success.
    bool load(const std::string& filename, SaveData& outData);
    
    // Delete a save file by filename
    bool deleteSave(const std::string& filename);
    
    // Scan for all saved worlds, sorted by timestamp (newest first)
    std::vector<SaveData> listSaves();
    
    // Generate a save filename from a world name
    static std::string makeFilename(const std::string& worldName);
    
    // Get the saves directory path
    const std::string& savesDir() const { return m_savesDir; }

private:
    std::string m_savesDir;
    
    // Ensure the saves directory exists
    bool ensureDirectory();
};
