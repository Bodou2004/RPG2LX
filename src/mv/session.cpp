#include "rpg2lx/session.hpp"
#include "rpg2lx/project_source.hpp"
#include "rpg2lx/core.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace rpg2lx {
namespace {
JsonValue read_json(const fs::path& path) {
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec || size > 32 * 1024 * 1024) throw std::runtime_error("Cannot read JSON or file too large: " + path.u8string());
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open: " + path.u8string());
    std::string data;
    char buffer[8192];
    while (input.read(buffer, sizeof buffer) || input.gcount()) {
        if (data.size() + static_cast<std::size_t>(input.gcount()) > 32 * 1024 * 1024)
            throw std::runtime_error("JSON exceeds input budget");
        data.append(buffer, static_cast<std::size_t>(input.gcount()));
    }
    if (!input.eof()) throw std::runtime_error("Read error: " + path.u8string());
    std::string error;
    auto json = JsonParser::parse(data, error);
    if (!error.empty()) throw std::runtime_error(path.u8string() + ": " + error);
    return json;
}
const JsonValue& field(const JsonValue& object, const std::string& key) {
    const auto* value = object.find(key);
    if (!value) throw std::runtime_error("Missing field: " + key);
    return *value;
}
int number(const JsonValue& value, int low, int high, const std::string& label) {
    const int n = value.get_int(low - 1);
    if (n < low || n > high) throw std::runtime_error(label + ": invalid integer");
    return n;
}
int member(const JsonValue& object, const std::string& name, int fallback) {
    const auto* value = object.find(name);
    return value ? value->get_int(fallback) : fallback;
}
bool enabled(const JsonValue& object, const std::string& name) {
    const auto* value = object.find(name); return value && value->get_bool();
}
bool valid_direction(int d) { return d == 2 || d == 4 || d == 6 || d == 8; }
int dx(int d) { return d == 6 ? 1 : d == 4 ? -1 : 0; }
int dy(int d) { return d == 2 ? 1 : d == 8 ? -1 : 0; }
int wrap(int n, int length) { return (n % length + length) % length; }
std::string self_key(int map, int event, const std::string& ch) {
    return std::to_string(map) + ":" + std::to_string(event) + ":" + ch;
}
int parameter(const EventCommand& command, std::size_t i, int low, int high) {
    if (i >= command.parameters.size()) throw std::runtime_error("Missing event parameter");
    return number(command.parameters[i], low, high, "Event parameter");
}
} // namespace

void GameSession::bind_js_hooks() {
    js_runtime_.register_game_state_hooks(
        [this](int id) { return variable_value(id); },
        [this](int id, int val) { variables_[id] = val; autorun_pending_ = true; },
        [this](int id) { return switch_value(id); },
        [this](int id, bool val) { switches_[id] = val; autorun_pending_ = true; },
        [this](const std::string& msg) { warnings_.push_back("[JS] " + msg); }
    );
}

std::uint32_t TilesetData::flag(int tile) const {
    if (tile < 0 || static_cast<std::size_t>(tile) >= flags.size())
        throw std::runtime_error("Tileset has no flags for tile " + std::to_string(tile));
    return flags[static_cast<std::size_t>(tile)];
}

bool GameSession::boot(const fs::path& project, std::string& error, bool run_start) {
    error.clear();
    try {
        GameSession next;
        const auto source=resolve_mv_project(project);
        if(!source)throw std::runtime_error(source.error);
        next.project_ = source.source->root;
        const auto system = read_json(next.project_ / "data/System.json");
        next.system_=system;
        if (!system.is_object()) throw std::runtime_error("System.json must be object");
        if (enabled(system, "hasEncryptedImages")) { next.warnings_.push_back("Proyecto con imagenes cifradas MV: descifrado automatico activo."); }
        const int map = number(field(system, "startMapId"), 1, 999999, "startMapId");
        const int x = number(field(system, "startX"), 0, 10000, "startX");
        const int y = number(field(system, "startY"), 0, 10000, "startY");
        if (const auto* party = system.find("partyMembers")) {
            if (!party->is_array()) throw std::runtime_error("partyMembers must be array");
            for (const auto& id : party->arr_val) next.party_.push_back(number(id, 1, 999999, "party actor"));
        }
        if (!next.party_.empty()) next.actor_database_=read_json(next.project_/"data/Actors.json");
        if(fs::exists(next.project_/"data/Classes.json"))next.classes_=read_json(next.project_/"data/Classes.json");
        if(fs::exists(next.project_/"data/Items.json"))next.items_=read_json(next.project_/"data/Items.json");
        if(fs::exists(next.project_/"data/CommonEvents.json"))next.common_events_=read_json(next.project_/"data/CommonEvents.json");
        if(fs::exists(next.project_/"data/Weapons.json"))next.weapons_=read_json(next.project_/"data/Weapons.json");
        if(fs::exists(next.project_/"data/Armors.json"))next.armors_=read_json(next.project_/"data/Armors.json");
        if(fs::exists(next.project_/"data/Skills.json"))next.skills_=read_json(next.project_/"data/Skills.json");
        next.followers_visible_=enabled(system,"optFollowers");
        next.refresh_party();
        if (!next.load_map(map, x, y, error)) return false;
        next.warnings_.push_back("Modo nativo experimental: plugins JavaScript, batalla y audio pendientes.");
        const auto metadata=ProjectInspector::inspect(next.project_.u8string());
        next.screen_width_=std::clamp(metadata.screen_width,320,1920);next.screen_height_=std::clamp(metadata.screen_height,240,1080);
        for(const auto& plugin:metadata.plugins)if(plugin.status) {
            next.warnings_.push_back("Plugin activo no ejecutado: "+plugin.name);
        }
        std::string js_err;
        if (!next.js_runtime_.initialize(js_err)) {
            next.warnings_.push_back("JS initialization warning: " + js_err);
        }
        *this = std::move(next);
        bind_js_hooks();
        if(run_start)check_autorun();
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}

bool GameSession::load_map(int id, int x, int y, std::string& error) {
    try {
        std::ostringstream filename; filename << "Map" << std::setw(3) << std::setfill('0') << id << ".json";
        auto result = MapLoader::load(project_ / "data" / filename.str());
        if (!result) throw std::runtime_error(result.error);
        auto& map = *result.map;
        if (x < 0 || y < 0 || x >= map.width || y >= map.height) throw std::runtime_error("Player position is outside the map");
        const auto database = read_json(project_ / "data/Tilesets.json");
        if (!database.is_array() || static_cast<std::size_t>(map.tileset_id) >= database.arr_val.size())
            throw std::runtime_error("Tileset not found in Tilesets.json");
        const auto& source = database.arr_val[map.tileset_id];
        TilesetData tileset;
        const auto& images = field(source, "tilesetNames");
        const auto& flags = field(source, "flags");
        if (!images.is_array() || images.arr_val.size() != 9 || !flags.is_array() || flags.arr_val.empty() || flags.arr_val.size() > 8192)
            throw std::runtime_error("Invalid tileset image list or flags");
        for (const auto& image : images.arr_val) {
            if (!image.is_string()) throw std::runtime_error("Tileset image name must be string");
            tileset.images.push_back(image.str_val);
        }
        for (const auto& flag : flags.arr_val) tileset.flags.push_back(static_cast<unsigned>(number(flag, 0, 0x1fffff, "tile flag")));
        for (int layer = 0; layer < 4; ++layer)
            for (int yy = 0; yy < map.height; ++yy)
                for (int xx = 0; xx < map.width; ++xx) (void)tileset.flag(map.cell(xx, yy, layer));
        for (const auto& event : map.events)
            for (const auto& page : event.pages) {
                const auto image = event_image(page);
                if (image.tile_id > 0) (void)tileset.flag(image.tile_id);
            }
        map_ = std::move(map); tileset_ = std::move(tileset); map_id_ = id;
        x_ = x; y_ = y; from_x_ = target_x_ = x; from_y_ = target_y_ = y; progress_ = 1;
        autorun_pending_ = true;
        reset_followers();
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}

bool GameSession::switch_value(int id) const {
    const auto found = switches_.find(id); return found != switches_.end() && found->second;
}

bool GameSession::conditions_met(const JsonValue& c, int event_id) const {
    for (const auto* prefix : {"switch1", "switch2"})
        if (enabled(c, std::string(prefix)+"Valid") && !switch_value(member(c, std::string(prefix)+"Id", 0))) return false;
    if (enabled(c, "variableValid")) {
        const auto found = variables_.find(member(c, "variableId", 0));
        if ((found == variables_.end() ? 0 : found->second) < member(c, "variableValue", 0)) return false;
    }
    if (enabled(c, "selfSwitchValid")) {
        const auto* ch = c.find("selfSwitchCh");
        if (!ch || !ch->is_string() || !self_switches_.count(self_key(map_id_, event_id, ch->str_val))) return false;
    }
    if (enabled(c, "itemValid") && item_count(member(c,"itemId",0))==0) return false;
    if (enabled(c, "actorValid") && std::find(party_.begin(), party_.end(), member(c, "actorId", 0)) == party_.end()) return false;
    return true;
}

const EventPage* GameSession::active_page(const MapEvent& event) const {
    for (auto it = event.pages.rbegin(); it != event.pages.rend(); ++it) {
        const auto* c = it->settings.find("conditions");
        if (!c || conditions_met(*c, event.id)) return &*it;
    }
    return nullptr;
}

CharacterImage GameSession::event_image(const EventPage& page) const {
    CharacterImage image;
    if (const auto* value = page.settings.find("image")) {
        if (const auto* name = value->find("characterName")) image.name = name->get_string();
        image.index = member(*value, "characterIndex", 0);
        image.direction = member(*value, "direction", 2);
        image.pattern = member(*value, "pattern", 1);
        image.tile_id = member(*value, "tileId", 0);
    }
    return image;
}

bool GameSession::tile_passage(int x, int y, unsigned bit) const {
    auto decision = [&](int tile) {
        const auto flags = tileset_.flag(tile);
        return (flags & 0x10) ? -1 : (flags & bit) ? 0 : 1;
    };
    for (const auto& event : map_.events) {
        const auto* page = active_page(event);
        if (!page || event.x != x || event.y != y || enabled(page->settings, "through")) continue;
        const auto image = event_image(*page);
        if (image.tile_id > 0 && member(page->settings, "priorityType", 1) == 0) {
            const auto result = decision(image.tile_id); if (result != -1) return result == 1;
        }
    }
    for (int layer = 3; layer >= 0; --layer) {
        const auto result = decision(map_.cell(x, y, layer)); if (result != -1) return result == 1;
    }
    return false;
}

bool GameSession::passable(int x, int y, int direction) const {
    if (!valid_direction(direction) || map_.width <= 0 || map_.height <= 0 ||
        x < 0 || y < 0 || x >= map_.width || y >= map_.height) return false;
    int xx = x + dx(direction), yy = y + dy(direction);
    if (map_.loops_x()) xx = wrap(xx, map_.width);
    if (map_.loops_y()) yy = wrap(yy, map_.height);
    if (xx < 0 || yy < 0 || xx >= map_.width || yy >= map_.height) return false;
    const unsigned bit = 1u << (direction / 2 - 1);
    const unsigned reverse = 1u << ((10-direction) / 2 - 1);
    if (!tile_passage(x,y,bit) || !tile_passage(xx,yy,reverse)) return false;
    for (const auto& event : map_.events) {
        const auto* page = active_page(event);
        if (page && event.x == xx && event.y == yy && !enabled(page->settings, "through") &&
            member(page->settings, "priorityType", 1) == 1) return false;
    }
    return true;
}

bool GameSession::move(int direction) {
    if (scene_!=GameScene::Map || !valid_direction(direction) || moving() || !message_.empty() || choosing() || !error_.empty() || !commands_.empty()) return false;
    direction_ = direction;
    if (!passable(x_,y_,direction)) {
        trigger_at(x_+dx(direction), y_+dy(direction), 1, true);
        trigger_at(x_+dx(direction), y_+dy(direction), 2, true);
        return false;
    }
    from_x_ = x_; from_y_ = y_; target_x_ = x_ + dx(direction); target_y_ = y_ + dy(direction);
    x_ += dx(direction); y_ += dy(direction);
    if (map_.loops_x()) x_ = wrap(x_, map_.width);
    if (map_.loops_y()) y_ = wrap(y_, map_.height);
    progress_ = 0;
    // Followers chase the previous grid position of the preceding party member.
    // Use a shortest wrapped displacement, preserving interpolation across map seams.
    if(followers_visible_) {
        int tx=static_cast<int>(from_x_),ty=static_cast<int>(from_y_);
        for(auto& f:followers_) {
            const int old_x=f.x,old_y=f.y;
            int xx=tx-f.x,yy=ty-f.y;
            if(map_.loops_x()&&std::abs(xx)>map_.width/2)xx+=xx>0?-map_.width:map_.width;
            if(map_.loops_y()&&std::abs(yy)>map_.height/2)yy+=yy>0?-map_.height:map_.height;
            f.from_x=f.x;f.from_y=f.y;f.to_x=f.x+xx;f.to_y=f.y+yy;
            if(xx)f.direction=xx>0?6:4;else if(yy)f.direction=yy>0?2:8;
            f.x=tx;f.y=ty;tx=old_x;ty=old_y;
        }
    }
    ++steps_;
    return true;
}

void GameSession::trigger_at(int x, int y, int trigger, bool normal) {
    if (!commands_.empty() || !error_.empty()) return;
    if (map_.loops_x()) x = wrap(x, map_.width);
    if (map_.loops_y()) y = wrap(y, map_.height);
    for (const auto& event : map_.events) {
        const auto* page = active_page(event);
        if (page && event.x == x && event.y == y && page->trigger == trigger &&
            (member(page->settings,"priorityType",1) == 1) == normal) {
            start_event(event, *page); return;
        }
    }
}

void GameSession::start_event(const MapEvent& event, const EventPage& page) {
    event_id_ = event.id;event_map_id_=map_id_; commands_ = page.commands; command_index_ = 0; branches_.clear();call_frames_.clear();execute();
}

void GameSession::check_autorun() {
    if (!autorun_pending_ || !commands_.empty() || !message_.empty() || !error_.empty()) return;
    autorun_pending_ = false;
    for (const auto& event : map_.events) {
        const auto* page = active_page(event);
        if (page && page->trigger == 3) { start_event(event, *page); return; }
    }
    for(const auto& event:common_events_.arr_val)if(member(event,"trigger",0)==1 && switch_value(member(event,"switchId",0))) {
        EventCommand call;call.code=117;JsonValue id;id.type=JsonType::Number;id.num_val=member(event,"id",0);call.parameters.push_back(id);
        commands_={call};command_index_=0;event_id_=0;execute();return;
    }
}

void GameSession::confirm() {
    if(scene_!=GameScene::Map){confirm_scene();return;}
    if(choosing()){finish_choice(choice_index_);return;}
    if (!error_.empty() || moving()) return;
    if (!message_.empty()) {
        if (!message_queue_.empty()) { message_ = std::move(message_queue_.front()); message_queue_.pop_front(); }
        else { message_.clear(); execute(); }
        return;
    }
    trigger_at(x_, y_, 0, false);
    if (commands_.empty() && message_.empty()) trigger_at(x_+dx(direction_), y_+dy(direction_), 0, true);
}

void GameSession::skip_branch(int indent) {
    while(command_index_<commands_.size()&&commands_[command_index_].indent>indent)++command_index_;
}
void GameSession::finish_choice(int choice) {
    branches_[choice_indent_]=choice;choices_.clear();message_.clear();execute();
}
bool GameSession::conditional(const EventCommand& c) const {
    switch(parameter(c,0,0,13)) {
        case 0:return switch_value(parameter(c,1,1,999999))==(parameter(c,2,0,1)==0);
        case 1: {
            const int lhs=variable_value(parameter(c,1,1,999999));
            const int rhs=parameter(c,2,0,1)==0?parameter(c,3,-1000000000,1000000000):variable_value(parameter(c,3,1,999999));
            switch(parameter(c,4,0,5)){case 0:return lhs==rhs;case 1:return lhs>=rhs;case 2:return lhs<=rhs;case 3:return lhs>rhs;case 4:return lhs<rhs;default:return lhs!=rhs;}
        }
        case 2: {
            if(c.parameters.size()<3||!c.parameters[1].is_string())throw std::runtime_error("Invalid self-switch condition");
            return (self_switches_.count(self_key(event_map_id_,event_id_,c.parameters[1].str_val))!=0)==(parameter(c,2,0,1)==0);
        }
        case 4: {
            if(parameter(c,2,0,6)!=0)throw std::runtime_error("Unsupported actor condition");
            return std::find(party_.begin(),party_.end(),parameter(c,1,1,999999))!=party_.end();
        }
        case 7: {const int value=parameter(c,1,0,99999999);const int op=parameter(c,2,0,2);return op==0?gold_>=value:op==1?gold_<=value:gold_<value;}
        case 8:return item_count(parameter(c,1,1,999999))>0;
        case 9: { auto it=weapon_inventory_.find(parameter(c,1,1,999999)); return it!=weapon_inventory_.end() && it->second>0; }
        case 10: { auto it=armor_inventory_.find(parameter(c,1,1,999999)); return it!=armor_inventory_.end() && it->second>0; }
        case 12: {
            if (c.parameters.size() >= 2 && c.parameters[1].is_string()) {
                return const_cast<JsRuntime&>(js_runtime_).eval_bool(c.parameters[1].str_val, false);
            }
            return false;
        }
        default: return false;
    }
}

void GameSession::execute() {
    try {
        unsigned budget = 1000;
        while ((command_index_ < commands_.size() || !call_frames_.empty()) && budget--) {
            if(command_index_>=commands_.size()) {
                auto frame=std::move(call_frames_.back());call_frames_.pop_back();commands_=std::move(frame.commands);command_index_=frame.index;branches_=std::move(frame.branches);continue;
            }
            const auto command = commands_[command_index_++];
            switch (command.code) {
                case 0: break;
                case 108: case 408: break; // Comments.
                case 102: {
                    if(command.parameters.empty()||!command.parameters[0].is_array())throw std::runtime_error("Invalid choices");
                    choices_.clear();for(const auto& choice:command.parameters[0].arr_val) {
                        if(!choice.is_string())throw std::runtime_error("Invalid choice label");choices_.push_back(choice.str_val);
                    }
                    if(choices_.empty()||choices_.size()>100)throw std::runtime_error("Invalid choice count");
                    choice_cancel_=parameter(command,1,-2,99);choice_index_=parameter(command,2,-1,99);
                    choice_index_=std::clamp(choice_index_,0,static_cast<int>(choices_.size())-1);choice_indent_=command.indent;return;
                }
                case 402:if(branches_[command.indent]!=parameter(command,0,0,99))skip_branch(command.indent);break;
                case 403:if(branches_[command.indent]!=-2)skip_branch(command.indent);break;
                case 404:branches_.erase(command.indent);break;
                case 111:branches_[command.indent]=conditional(command)?1:0;if(!branches_[command.indent])skip_branch(command.indent);break;
                case 411:if(branches_[command.indent])skip_branch(command.indent);break;
                case 412:branches_.erase(command.indent);break;
                case 112:break;
                case 113: {
                    int depth=0;bool found=false;
                    while(command_index_<commands_.size()) {
                        const auto& next=commands_[command_index_++];
                        if(next.code==112)++depth;
                        if(next.code==413){if(depth==0){found=true;break;}--depth;}
                    }
                    if(!found)throw std::runtime_error("Break loop outside loop");break;
                }
                case 413: {
                    bool found=false;std::size_t i=command_index_-1;
                    while(i>0){--i;if(commands_[i].code==112&&commands_[i].indent==command.indent){command_index_=i+1;found=true;break;}}
                    if(!found)throw std::runtime_error("Loop start missing");break;
                }
                case 115:command_index_=commands_.size();break;
                case 117: {
                    const int id=parameter(command,0,1,999999);
                    if(static_cast<std::size_t>(id)>=common_events_.arr_val.size())throw std::runtime_error("Common event not found");
                    const auto* list=common_events_.arr_val[id].find("list");
                    if(!list||!list->is_array()||call_frames_.size()>=32)throw std::runtime_error("Invalid common event or call depth exceeded");
                    std::vector<EventCommand> code;
                    for(const auto& source:list->arr_val) {
                        EventCommand c;c.code=member(source,"code",0);c.indent=member(source,"indent",0);
                        if(const auto* params=source.find("parameters")) {if(!params->is_array())throw std::runtime_error("Invalid common event parameters");c.parameters=params->arr_val;}
                        code.push_back(std::move(c));
                    }
                    call_frames_.push_back({std::move(commands_),command_index_,std::move(branches_)});commands_=std::move(code);command_index_=0;branches_.clear();break;
                }
                case 118:break;
                case 119: {
                    if(command.parameters.empty()||!command.parameters[0].is_string())throw std::runtime_error("Invalid label");
                    bool found=false;
                    for(std::size_t i=0;i<commands_.size();++i)if(commands_[i].code==118&&!commands_[i].parameters.empty()&&commands_[i].parameters[0].get_string()==command.parameters[0].str_val){command_index_=i+1;found=true;break;}
                    if(!found)throw std::runtime_error("Event label not found");break;
                }
                case 101: {
                    message_face_=command.parameters.size()>0?command.parameters[0].get_string():"";
                    message_face_index_=command.parameters.size()>1?command.parameters[1].get_int():0;
                    message_background_=command.parameters.size()>2?command.parameters[2].get_int():0;
                    message_position_=command.parameters.size()>3?command.parameters[3].get_int(2):2;
                    message_.clear();
                    message_queue_.clear();
                    std::vector<std::string> lines;
                    while (command_index_ < commands_.size() && commands_[command_index_].code == 401) {
                        const auto& line = commands_[command_index_++];
                        if (line.parameters.empty() || !line.parameters[0].is_string()) throw std::runtime_error("Invalid message text");
                        std::string visible;
                        unsigned columns=0;
                        for (const char ch : line.parameters[0].str_val) {
                            const bool lead = (static_cast<unsigned char>(ch)&0xC0) != 0x80;
                            if (ch=='\n' || (lead && columns==36)) {
                                lines.push_back(std::move(visible)); visible.clear(); columns=0;
                            }
                            if (ch=='\n') continue;
                            visible += ch;
                            if (lead) ++columns;
                        }
                        lines.push_back(std::move(visible));
                    }
                    for (std::size_t i=0;i<lines.size();i+=4) {
                        std::string page;
                        for (std::size_t j=i;j<std::min(i+4,lines.size());++j) {
                            if (j!=i) page+='\n';
                            page+=lines[j];
                        }
                        if (page.empty()) page="\n";
                        message_queue_.push_back(std::move(page));
                    }
                    if (!message_queue_.empty()) { message_=std::move(message_queue_.front()); message_queue_.pop_front(); }
                    if (!message_.empty() && !(command_index_<commands_.size()&&commands_[command_index_].code==102)) return;
                    break;
                }
                case 121: {
                    const int first = parameter(command,0,1,999999), last = parameter(command,1,first,999999);
                    if (last-first > 10000) throw std::runtime_error("Switch range exceeds budget");
                    const bool on = parameter(command,2,0,1) == 0;
                    for (int id = first; id <= last; ++id) switches_[id] = on;
                    autorun_pending_ = true; break;
                }
                case 122: {
                    const int first=parameter(command,0,1,999999),last=parameter(command,1,first,999999),op=parameter(command,2,0,5),operand=parameter(command,3,0,4);
                    if(last-first>10000)throw std::runtime_error("Variable range exceeds budget");
                    if(operand>1)throw std::runtime_error("Unsupported variable operand");
                    const long long value=operand==0?parameter(command,4,-1000000000,1000000000):variable_value(parameter(command,4,1,999999));
                    for(int id=first;id<=last;++id) {
                        long long n=variable_value(id);
                        switch(op){case 0:n=value;break;case 1:n+=value;break;case 2:n-=value;break;case 3:n*=value;break;case 4:n=value?n/value:0;break;case 5:n=value?n%value:0;break;}
                        variables_[id]=static_cast<int>(std::clamp(n,-1000000000LL,1000000000LL));
                    }
                    autorun_pending_=true;break;
                }
                case 125: case 126: case 127: case 128: {
                    const int offset=command.code==125?0:1;
                    int amount=parameter(command,offset+1,0,1)==0?parameter(command,offset+2,0,99999999):variable_value(parameter(command,offset+2,1,999999));
                    if(parameter(command,offset,0,1)==1)amount=-amount;
                    if(command.code==125)gold_=std::clamp(gold_+amount,0,99999999);
                    else {
                        int id=parameter(command,0,1,999999);auto& pool=command.code==126?inventory_:command.code==127?weapon_inventory_:armor_inventory_;
                        const int previous=pool[id];pool[id]=std::clamp(previous+amount,0,99);
                        if(command.code!=126 && amount<0 && command.parameters.size()>4 && command.parameters[4].get_bool()) {
                            int remaining=std::max(0,-amount-previous);
                            for(auto& actor:actors_)for(auto& gear:actor.equips)if(remaining>0&&gear.first==(command.code==127?1:2)&&gear.second==id){gear.second=0;--remaining;refresh_stats(actor);}
                        }
                    }
                    autorun_pending_=true;break;
                }
                case 129: {
                    const int id=parameter(command,0,1,999999),op=parameter(command,1,0,1);
                    auto it=std::find(party_.begin(),party_.end(),id);
                    if(op==0&&it==party_.end())party_.push_back(id);else if(op==1&&it!=party_.end())party_.erase(it);
                    refresh_party();autorun_pending_=true;break;
                }
                case 311: case 312: case 314: case 318: {
                    const int id=parameter(command,0,0,1)==0?parameter(command,1,0,999999):variable_value(parameter(command,1,1,999999));
                    for(auto& actor:actors_)if(id==0||actor.id==id) {
                        if(command.code==314){actor.hp=actor.params[0];actor.mp=actor.params[1];continue;}
                        if(command.code==318){
                            const int skill=parameter(command,3,1,999999);auto it=std::find(actor.skills.begin(),actor.skills.end(),skill);
                            if(parameter(command,2,0,1)==0){if(it==actor.skills.end())actor.skills.push_back(skill);}else if(it!=actor.skills.end())actor.skills.erase(it);continue;
                        }
                        int value=parameter(command,3,0,1)==0?parameter(command,4,0,99999999):variable_value(parameter(command,4,1,999999));
                        if(parameter(command,2,0,1))value=-value;
                        if(command.code==311&&actor.hp>0)actor.hp=std::clamp(actor.hp+value,command.parameters.size()>5&&command.parameters[5].get_bool()?0:1,actor.params[0]);
                        else if(command.code==312)actor.mp=std::clamp(actor.mp+value,0,actor.params[1]);
                    }
                    break;
                }
                case 134:save_enabled_=parameter(command,0,0,1)!=0;break;
                case 135:menu_enabled_=parameter(command,0,0,1)!=0;break;
                case 216:followers_visible_=parameter(command,0,0,1)==0;reset_followers();break;
                case 217:reset_followers();break;
                case 351:set_scene(GameScene::Menu);return;
                case 352:set_scene(GameScene::Save);return;
                case 354:show_title();commands_.clear();return;
                case 123: {
                    if (command.parameters.empty() || !command.parameters[0].is_string()) throw std::runtime_error("Invalid self-switch key");
                    const auto ch = command.parameters[0].str_val;
                    if (ch != "A" && ch != "B" && ch != "C" && ch != "D") throw std::runtime_error("Invalid self-switch key");
                    if(event_id_==0)break;
                    const auto key = self_key(event_map_id_,event_id_,ch);
                    if (parameter(command,1,0,1) == 0) self_switches_.insert(key); else self_switches_.erase(key);
                    autorun_pending_ = true; break;
                }
                case 201: {
                    const bool variable=parameter(command,0,0,1)!=0;
                    const int id = variable?variable_value(parameter(command,1,1,999999)):parameter(command,1,1,999999);
                    const int x = variable?variable_value(parameter(command,2,1,999999)):parameter(command,2,0,10000);
                    const int y = variable?variable_value(parameter(command,3,1,999999)):parameter(command,3,0,10000);
                    const int direction = parameter(command,4,0,8);
                    if (direction && !valid_direction(direction)) throw std::runtime_error("Invalid transfer direction");
                    std::string error;
                    if (!load_map(id,x,y,error)) throw std::runtime_error(error);
                    if (direction) direction_ = direction;
                    reset_followers();break;
                }
                case 230: wait_ = parameter(command,0,0,36000) / 60.0; if (wait_ > 0) return; break;
                case 355: {
                    std::string script;
                    if (!command.parameters.empty() && command.parameters[0].is_string()) {
                        script = command.parameters[0].str_val;
                    }
                    while (command_index_ < commands_.size() && commands_[command_index_].code == 655) {
                        const auto& next_cmd = commands_[command_index_++];
                        if (!next_cmd.parameters.empty() && next_cmd.parameters[0].is_string()) {
                            script += "\n" + next_cmd.parameters[0].str_val;
                        }
                    }
                    if (!script.empty()) {
                        auto res = js_runtime_.eval(script);
                        if (!res.success) {
                            warnings_.push_back("JS script error: " + res.error);
                        }
                    }
                    break;
                }
                case 655: break;
                default:
                    warnings_.push_back("Ignorado comando " + std::to_string(command.code) +
                                        " en mapa " + std::to_string(map_id_) + ", evento " + std::to_string(event_id_));
                    break;
            }
        }
        if (command_index_ < commands_.size() || !call_frames_.empty()) throw std::runtime_error("Event instruction budget exceeded");
        commands_.clear(); command_index_ = 0; autorun_pending_ = true;
    } catch (const std::exception& e) { error_ = e.what(); commands_.clear(); message_.clear(); message_queue_.clear(); }
}

void GameSession::update(double seconds) {
    if (!std::isfinite(seconds)) return;
    seconds = std::clamp(seconds, 0.0, 0.1);
    if(scene_!=GameScene::Map)return;
    play_seconds_+=seconds;
    if (moving()) {
        progress_ = std::min(1.0, progress_ + seconds * ((dashing_||always_dash_)?7.5:3.75));
        if (!moving()) {
            from_x_ = target_x_ = x_; from_y_ = target_y_ = y_;
            trigger_at(x_,y_,1,false);
            trigger_at(x_,y_,2,false);
        }
    }
    if (wait_ > 0) { wait_ = std::max(0.0, wait_-seconds); if (wait_ == 0) execute(); }
    else if(!commands_.empty()&&message_.empty()&&!choosing())execute();
    if(menu_requested_&&!moving()) {
        menu_requested_=false;
        if(message_.empty()&&commands_.empty()&&menu_enabled_){set_scene(GameScene::Menu);return;}
    }
    check_autorun();
}
} // namespace rpg2lx
