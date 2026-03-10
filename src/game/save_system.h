#pragma once
#include "game/root_state.h"
#include <string>
#include <vector>
#include <cstdint>

// Summary of a save file for the menu
struct SaveSummary {
    std::string name;
    uint32_t seed;
    int64_t timestamp;
    std::string filename;
};

// Manages saving/loading game state to ~/.ascii3d/saves/
class SaveSystem {
public:
    SaveSystem();
    
    // Save current game state. Returns true on success.
    bool save(const RootState& state);
    
    // Load a specific save file by filename. Returns true on success.
    bool load(const std::string& filename, RootState& outState);
    
    // Delete a save file by filename
    bool deleteSave(const std::string& filename);
    
    // Scan for all saved worlds, sorted by timestamp (newest first)
    std::vector<SaveSummary> listSaves();
    
    // Generate a save filename from a world name
    static std::string makeFilename(const std::string& worldName);
    
    // Get the saves directory path
    const std::string& savesDir() const { return m_savesDir; }

private:
    std::string m_savesDir;
    
    // Ensure the saves directory exists
    bool ensureDirectory();
};
