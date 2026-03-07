#pragma once
#include <string>
#include <functional>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class FirebaseClient {
public:
    FirebaseClient();
    ~FirebaseClient();
    
    // Initialize with the Firebase RTDB base URL
    // e.g., "https://ascworld-c6a94-default-rtdb.firebaseio.com"
    void init(const std::string& baseUrl);
    
    // REST operations (synchronous, blocking)
    // All paths are relative to the base URL, e.g., "rooms/abc123"
    
    // GET: returns the JSON data at the path (null json if not found/error)
    json get(const std::string& path);
    
    // PUT: set data at the path (overwrites). Returns true on success.
    bool put(const std::string& path, const json& data);
    
    // PATCH: update fields at the path (merge). Returns true on success.
    bool patch(const std::string& path, const json& data);
    
    // DELETE: remove data at the path. Returns true on success.
    bool del(const std::string& path);
    
    // POST: push new data under the path (generates unique key). Returns the key.
    std::string push(const std::string& path, const json& data);
    
    // SSE listener for real-time updates
    // Callback receives: event type ("put", "patch"), path, data
    using SSECallback = std::function<void(const std::string& event, 
                                           const std::string& path, 
                                           const json& data)>;
    
    // Start listening to a path via Server-Sent Events
    // The callback is called on a background thread
    void startListening(const std::string& path, SSECallback callback);
    
    // Stop all SSE listeners
    void stopListening();
    
    // Check if connected/initialized
    bool isInitialized() const { return m_initialized; }
    
private:
    std::string m_baseUrl;
    bool m_initialized = false;
    
    // SSE listener thread
    std::thread m_sseThread;
    std::atomic<bool> m_sseRunning{false};
    std::string m_ssePath;
    SSECallback m_sseCallback;
    
    // Build full URL from path
    std::string buildUrl(const std::string& path) const;
    
    // Perform an HTTP request using CURL
    // method: "GET", "PUT", "PATCH", "DELETE", "POST"
    // Returns the response body
    std::string httpRequest(const std::string& method, const std::string& url,
                           const std::string& body = "");
    
    // SSE thread function
    void sseThreadFunc(const std::string& url, SSECallback callback);
};
