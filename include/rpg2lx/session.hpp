#pragma once
#include "rpg2lx/map.hpp"
#include "rpg2lx/js/js_runtime.hpp"
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

enum class GameScene { Map, Title, Menu, Status, Formation, Items, ItemTarget, SkillActors, Skills, SkillTarget, EquipActors, EquipSlots, EquipItems, Save, Load, GameEnd, Options };
struct PartyActor {
    int id=0, level=1, hp=1, mp=0, class_id=0;
    std::string name, nickname, class_name, face_name;
    int face_index=0;
    CharacterImage image;
    std::vector<int> params;
    std::vector<std::pair<int,int>> equips; // MV database kind (1 weapon, 2 armor), ID.
    std::vector<int> skills;
};
struct Follower {
    CharacterImage image;
    int x=0,y=0,direction=2;
    double from_x=0,from_y=0,to_x=0,to_y=0;
};
struct GameCommand { std::string label, symbol; bool enabled=true; int value=0; };

// Native MV state and scenes. JavaScript/plugins are not evaluated here.
class GameSession {
public:
    bool boot(const std::filesystem::path& project, std::string& error, bool run_start=true);
    bool move(int direction);
    void update(double seconds);
    void confirm();
    void cancel();
    void navigate(int direction);
    void show_title();
    void set_save_directory(const std::filesystem::path& directory) { save_directory_=directory; }
    bool save(int slot, std::string& error) const;
    bool load(int slot, std::string& error);
    bool has_save(int slot) const;
    GameScene scene() const { return scene_; }
    const std::vector<GameCommand>& scene_commands() const { return scene_commands_; }
    std::size_t selection() const { return selection_; }
    const std::string& notice() const { return notice_; }
    bool wants_library() const { return library_requested_; }
    const JsonValue& system() const { return system_; }
    std::string encryption_key() const { const auto* k = system_.find("encryptionKey"); return (k && k->is_string()) ? k->str_val : ""; }
    std::string term(const std::string& group, int index, const std::string& fallback) const;
    std::string title_image(int layer) const;
    std::string game_title() const;
    bool draw_title() const;
    int screen_width() const { return screen_width_; }
    int screen_height() const { return screen_height_; }
    int message_position() const { return message_position_; }
    int message_background() const { return message_background_; }
    const std::string& message_face() const { return message_face_; }
    int message_face_index() const { return message_face_index_; }
    const std::vector<PartyActor>& actors() const { return actors_; }
    const std::vector<Follower>& followers() const { return followers_; }
    double follower_progress() const { return progress_; }
    bool followers_visible() const { return followers_visible_; }
    int gold() const { return gold_; }
    int variable_value(int id) const;
    int item_count(int id) const;
    const std::vector<std::string>& choices() const { return choices_; }
    bool choosing() const { return !choices_.empty(); }
    int choice_index() const { return choice_index_; }
    void set_dash(bool value) { dashing_=value; }
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
    JsRuntime& js_runtime() { return js_runtime_; }
    const JsRuntime& js_runtime() const { return js_runtime_; }
    JsRuntime::BattlerContext actor_context(const PartyActor& actor) const;
private:
    bool load_map(int map, int x, int y, std::string& error);
    bool conditions_met(const JsonValue& conditions, int event_id) const;
    void trigger_at(int x, int y, int trigger, bool normal_priority);
    void start_event(const MapEvent& event, const EventPage& page);
    void execute();
    void check_autorun();
    void set_scene(GameScene scene);
    void confirm_scene();
    bool use_item(int id, std::size_t target);
    bool apply_effects(const JsonValue& action, std::size_t target);
    void refresh_stats(PartyActor& actor);
    bool equip_allowed(const PartyActor& actor,int kind,int id,int etype) const;
    void refresh_party();
    void reset_followers();
    void finish_choice(int choice);
    void skip_branch(int indent);
    bool conditional(const EventCommand& command) const;
    bool tile_passage(int x, int y, unsigned bit) const;
    void bind_js_hooks();
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
    JsonValue system_, actor_database_, classes_, items_, common_events_, weapons_, armors_, skills_;
    std::vector<PartyActor> actors_;
    std::vector<Follower> followers_;
    bool followers_visible_=false, dashing_=false, always_dash_=false, options_from_title_=false;
    bool menu_enabled_=true, save_enabled_=true, library_requested_=false, menu_requested_=false;
    int gold_=0, steps_=0, pending_item_=0;
    double play_seconds_=0;
    std::map<int,int> inventory_;
    std::map<int,int> weapon_inventory_,armor_inventory_;
    std::size_t actor_selection_=0;
    int equip_slot_=0;
    std::filesystem::path save_directory_;
    GameScene scene_=GameScene::Map;
    std::vector<GameCommand> scene_commands_;
    std::size_t selection_=0;
    std::string notice_;
    int screen_width_=816,screen_height_=624,message_position_=2,message_background_=0,message_face_index_=0;
    std::string message_face_;
    std::vector<std::string> choices_;
    int choice_index_=0, choice_cancel_=-1, choice_indent_=0;
    std::map<int,int> branches_;
    struct InterpreterFrame { std::vector<EventCommand> commands; std::size_t index; std::map<int,int> branches; };
    std::vector<InterpreterFrame> call_frames_;
    std::vector<EventCommand> commands_;
    std::size_t command_index_ = 0;
    int event_id_ = 0, event_map_id_=0;
    double wait_ = 0;
    std::string message_, error_;
    std::deque<std::string> message_queue_;
    std::vector<std::string> warnings_;
    bool autorun_pending_ = true;
    JsRuntime js_runtime_;
};
} // namespace rpg2lx
