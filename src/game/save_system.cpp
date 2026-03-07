#include "game/save_system.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>
#include <dirent.h>

SaveSystem::SaveSystem() {
    // Determine saves directory: ~/.ascii3d/saves/
    const char* home = std::getenv("HOME");
    if (home) {
        m_savesDir = std::string(home) + "/.ascii3d/saves";
    } else {
        m_savesDir = ".ascii3d/saves";
    }
}

bool SaveSystem::ensureDirectory() {
    // Create ~/.ascii3d/ then ~/.ascii3d/saves/
    std::string base = m_savesDir.substr(0, m_savesDir.rfind('/'));
    mkdir(base.c_str(), 0755);
    mkdir(m_savesDir.c_str(), 0755);
    
    // Verify it exists
    struct stat st;
    return stat(m_savesDir.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool SaveSystem::save(const SaveData& data) {
    if (!ensureDirectory()) return false;
    
    std::string filename = data.filename.empty() ? makeFilename(data.name) : data.filename;
    std::string path = m_savesDir + "/" + filename;
    
    std::ofstream out(path);
    if (!out.is_open()) return false;
    
    // Simple text format, one key=value per line
    out << "name=" << data.name << "\n";
    out << "seed=" << data.seed << "\n";
    out << "px=" << data.playerPos.x << "\n";
    out << "py=" << data.playerPos.y << "\n";
    out << "pz=" << data.playerPos.z << "\n";
    out << "yaw=" << data.playerYaw << "\n";
    out << "pitch=" << data.playerPitch << "\n";
    
    int64_t ts = data.timestamp;
    if (ts == 0) ts = static_cast<int64_t>(std::time(nullptr));
    out << "timestamp=" << ts << "\n";
    
    out.close();
    return out.good() || !out.fail();
}

bool SaveSystem::load(const std::string& filename, SaveData& outData) {
    std::string path = m_savesDir + "/" + filename;
    std::ifstream in(path);
    if (!in.is_open()) return false;
    
    outData = SaveData{};
    outData.filename = filename;
    
    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        
        if (key == "name") outData.name = val;
        else if (key == "seed") outData.seed = static_cast<uint32_t>(std::stoul(val));
        else if (key == "px") outData.playerPos.x = std::stof(val);
        else if (key == "py") outData.playerPos.y = std::stof(val);
        else if (key == "pz") outData.playerPos.z = std::stof(val);
        else if (key == "yaw") outData.playerYaw = std::stof(val);
        else if (key == "pitch") outData.playerPitch = std::stof(val);
        else if (key == "timestamp") outData.timestamp = std::stoll(val);
    }
    
    return !outData.name.empty();
}

bool SaveSystem::deleteSave(const std::string& filename) {
    std::string path = m_savesDir + "/" + filename;
    return std::remove(path.c_str()) == 0;
}

std::vector<SaveData> SaveSystem::listSaves() {
    std::vector<SaveData> saves;
    
    DIR* dir = opendir(m_savesDir.c_str());
    if (!dir) return saves;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        // Only load .sav files
        if (name.size() > 4 && name.substr(name.size() - 4) == ".sav") {
            SaveData data;
            if (load(name, data)) {
                saves.push_back(data);
            }
        }
    }
    closedir(dir);
    
    // Sort by timestamp, newest first
    std::sort(saves.begin(), saves.end(), [](const SaveData& a, const SaveData& b) {
        return a.timestamp > b.timestamp;
    });
    
    return saves;
}

std::string SaveSystem::makeFilename(const std::string& worldName) {
    std::string filename;
    for (char c : worldName) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
            (c >= '0' && c <= '9') || c == '_' || c == '-') {
            filename += c;
        } else if (c == ' ') {
            filename += '_';
        }
    }
    if (filename.empty()) filename = "world";
    filename += ".sav";
    return filename;
}
