#pragma once
#include "rpg2lx/map.hpp"
#include <map>
#include <set>
#include <deque>

namespace rpg2lx {
struct TilesetData {
    std::vector<std::string> images;
    std::vector<std::uint32_t> flags;
    std::uint32_t flag(int tile) const;
};

struct CharacterImage {
    std::string name;
    int index = 0;
    int direction = 2;
    int pattern = 1;
    int tile_id = 0;
};

// Initial native MV exploration session. No JS or plugins are evaluated here.
class GameSession {
public:
    bool boot(const std::filesystem::path& project, std::string& error);
    bool move(int direction);
    void update(double seconds);
    void confirm();
    const MapData& map() const { return map_; }
    const TilesetData& tileset() const { return tileset_; }
    const std::filesystem::path& project() const { return project_; }
    const CharacterImage& player_image() const { return player_image_; }
    int map_id() const { return map_id_; }
    int x() const { return x_; }
    int y() const { return y_; }
    int direction() const { return direction_; }
    bool moving() const { return progress_ < 1; }
    double real_x() const { return from_x_ + (target_x_ - from_x_) * progress_; }
    double real_y() const { return from_y_ + (target_y_ - from_y_) * progress_; }
    bool passable(int x, int y, int direction) const;
    const EventPage* active_page(const MapEvent& event) const;
    CharacterImage event_image(const EventPage& page) const;
    const std::string& message() const { return message_; }
    const std::string& error() const { return error_; }
    const std::vector<std::string>& warnings() const { return warnings_; }
    bool switch_value(int id) const;
private:
    bool load_map(int map, int x, int y, std::string& error);
    bool conditions_met(const JsonValue& conditions, int event_id) const;
    void trigger_at(int x, int y, int trigger, bool normal_priority);
    void start_event(const MapEvent& event, const EventPage& page);
    void execute();
    void check_autorun();
    bool tile_passage(int x, int y, unsigned bit) const;
    std::filesystem::path project_;
    MapData map_;
    TilesetData tileset_;
    CharacterImage player_image_;
    int map_id_ = 0, x_ = 0, y_ = 0, direction_ = 2;
    double from_x_ = 0, from_y_ = 0, target_x_ = 0, target_y_ = 0, progress_ = 1;
    std::map<int, bool> switches_;
    std::map<int, int> variables_;
    std::set<std::string> self_switches_;
    std::vector<int> party_;
    std::vector<EventCommand> commands_;
    std::size_t command_index_ = 0;
    int event_id_ = 0;
    double wait_ = 0;
    std::string message_, error_;
    std::deque<std::string> message_queue_;
    std::vector<std::string> warnings_;
    bool autorun_pending_ = true;
};
} // namespace rpg2lx
