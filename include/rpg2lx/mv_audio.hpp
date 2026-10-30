#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace rpg2lx {

class MvAudioEngine {
public:
    MvAudioEngine();
    ~MvAudioEngine();

    MvAudioEngine(const MvAudioEngine&) = delete;
    MvAudioEngine& operator=(const MvAudioEngine&) = delete;

    bool init(const std::filesystem::path& project_root, const std::string& encryption_key = "");
    void shutdown();

    void set_encryption_key(const std::string& encryption_key);
    void set_master_volume(float volume);
    float master_volume() const;

    // Returns handle > 0 on valid load, 0 on error
    int create_player(const std::string& relative_url);
    void destroy_player(int player_id);

    bool is_ready(int player_id) const;
    bool is_error(int player_id) const;
    bool is_playing(int player_id) const;

    void play(int player_id, bool loop, double offset_sec = 0.0);
    void stop(int player_id);
    void stop_all();

    void set_volume(int player_id, float volume);
    float volume(int player_id) const;

    void set_pitch(int player_id, float pitch);
    float pitch(int player_id) const;

    void set_pan(int player_id, float pan);
    float pan(int player_id) const;

    void fade_in(int player_id, double duration_sec);
    void fade_out(int player_id, double duration_sec);

    double seek_position(int player_id) const;
    double total_duration(int player_id) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rpg2lx
