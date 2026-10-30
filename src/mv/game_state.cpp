#include "rpg2lx/session.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs=std::filesystem;
namespace rpg2lx {
namespace {
const JsonValue empty;
const JsonValue& get(const JsonValue& j,const std::string& name) { const auto* p=j.find(name); return p?*p:empty; }
const JsonValue& at(const JsonValue& j,int i) { return j.is_array() && i>=0 && static_cast<std::size_t>(i)<j.arr_val.size()?j.arr_val[i]:empty; }
JsonValue read(const fs::path& p) {
    std::error_code ec; const auto size=fs::file_size(p,ec);
    if(ec || size>8*1024*1024)throw std::runtime_error("Cannot read state data: "+p.u8string());
    std::ifstream in(p,std::ios::binary); std::string s(static_cast<std::size_t>(size),'\0');
    if(!in.read(s.data(),static_cast<std::streamsize>(s.size())))throw std::runtime_error("Cannot read: "+p.u8string());
    std::string error;auto j=JsonParser::parse(s,error);if(!error.empty())throw std::runtime_error(error);return j;
}
std::string quote(const std::string& s) {
    std::ostringstream out;out<<'"'; const char* hex="0123456789abcdef";
    for(unsigned char c:s) {if(c=='"'||c=='\\')out<<'\\'<<c;else if(c<32)out<<"\\u00"<<hex[c>>4]<<hex[c&15];else out<<c;}out<<'"';return out.str();
}
int integer(const JsonValue& j,int low,int high) {
    const int n=j.get_int(low-1);if(n<low||n>high)throw std::runtime_error("Invalid save value");return n;
}
}

std::string GameSession::term(const std::string& group,int index,const std::string& fallback) const {
    return at(get(get(system_,"terms"),group),index).get_string(fallback);
}
std::string GameSession::title_image(int layer) const {return get(system_,layer==1?"title1Name":"title2Name").get_string();}
std::string GameSession::game_title() const {return get(system_,"gameTitle").get_string();}
bool GameSession::draw_title() const {return get(system_,"optDrawTitle").get_bool(true);}
int GameSession::variable_value(int id) const {auto it=variables_.find(id);return it==variables_.end()?0:it->second;}
int GameSession::item_count(int id) const {auto it=inventory_.find(id);return it==inventory_.end()?0:it->second;}

void GameSession::refresh_party() {
    auto old=std::move(actors_);actors_.clear();
    for(int id:party_) {
        auto existing=std::find_if(old.begin(),old.end(),[&](const PartyActor& a){return a.id==id;});
        if(existing!=old.end()){actors_.push_back(*existing);continue;}
        const auto& data=at(actor_database_,id);
        if(!data.is_object())throw std::runtime_error("Party actor not found: "+std::to_string(id));
        PartyActor a;a.id=id;a.level=get(data,"initialLevel").get_int(1);a.name=get(data,"name").get_string();a.nickname=get(data,"nickname").get_string();
        a.image.name=get(data,"characterName").get_string();a.image.index=get(data,"characterIndex").get_int();
        a.face_name=get(data,"faceName").get_string();a.face_index=get(data,"faceIndex").get_int();
        a.class_id=get(data,"classId").get_int();const auto& klass=at(classes_,a.class_id);a.class_name=get(klass,"name").get_string();
        const auto& equips=get(data,"equips");
        for(std::size_t slot=0;slot<equips.arr_val.size();++slot)a.equips.push_back({slot==0?1:2,equips.arr_val[slot].get_int()});
        for(const auto& learning:get(klass,"learnings").arr_val)if(get(learning,"level").get_int()<=a.level)a.skills.push_back(get(learning,"skillId").get_int());
        refresh_stats(a);
        a.hp=a.params[0];a.mp=a.params[1];actors_.push_back(std::move(a));
    }
    player_image_=actors_.empty()?CharacterImage{}:actors_.front().image;
    reset_followers();
}
void GameSession::refresh_stats(PartyActor& a) {
    const auto& klass=at(classes_,a.class_id);a.params.clear();
    for(int p=0;p<8;++p) {
        int base=at(at(get(klass,"params"),p),a.level).get_int(p==0?100:p==1?30:10);double rate=1;
        auto traits=[&](const JsonValue& data){for(const auto& t:get(data,"traits").arr_val)if(get(t,"code").get_int()==21&&get(t,"dataId").get_int()==p)rate*=get(t,"value").get_double(1);};
        traits(at(actor_database_,a.id));traits(klass);
        for(const auto& gear:a.equips)if(gear.second>0) {
            const auto& data=at(gear.first==1?weapons_:armors_,gear.second);base+=at(get(data,"params"),p).get_int();traits(data);
        }
        a.params.push_back(static_cast<int>(std::clamp(std::floor(base*rate),p==0?1.0:0.0,p==0?999999.0:9999.0)));
    }
    a.hp=std::clamp(a.hp,0,a.params[0]);a.mp=std::clamp(a.mp,0,a.params[1]);
}
bool GameSession::equip_allowed(const PartyActor& a,int kind,int id,int etype) const {
    const auto& data=at(kind==1?weapons_:armors_,id);
    if(id&&(!data.is_object()||get(data,"etypeId").get_int()!=etype))return false;
    const int type=get(data,kind==1?"wtypeId":"atypeId").get_int();bool allowed=id==0,sealed=false;
    auto traits=[&](const JsonValue& source){for(const auto& t:get(source,"traits").arr_val) {
        const int code=get(t,"code").get_int(),value=get(t,"dataId").get_int();
        if(code==(kind==1?51:52)&&value==type)allowed=true;
        if((code==53||code==54)&&value==etype)sealed=true;
    }};
    traits(at(actor_database_,a.id));traits(at(classes_,a.class_id));
    for(const auto& gear:a.equips)if(gear.second)traits(at(gear.first==1?weapons_:armors_,gear.second));
    return allowed&&!sealed;
}
void GameSession::reset_followers() {
    followers_.clear();
    for(std::size_t i=1;i<std::min<std::size_t>(4,actors_.size());++i)
        followers_.push_back({actors_[i].image,x_,y_,direction_,static_cast<double>(x_),static_cast<double>(y_),static_cast<double>(x_),static_cast<double>(y_)});
}

void GameSession::set_scene(GameScene next) {
    scene_=next;selection_=0;scene_commands_.clear();notice_.clear();
    auto add=[&](std::string label,std::string symbol,bool enabled=true,int value=0){scene_commands_.push_back({std::move(label),std::move(symbol),enabled,value});};
    if(next==GameScene::Title) {
        add(term("commands",18,"New Game"),"new");
        bool any=false;for(int i=1;i<=20;++i)any=any||has_save(i);
        add(term("commands",19,"Continue"),"load",any);add(term("commands",11,"Options"),"options");
    } else if(next==GameScene::Menu) {
        const auto& flags=get(system_,"menuCommands");
        if(at(flags,0).get_bool(true))add(term("commands",4,"Item"),"items");
        if(at(flags,1).get_bool(true))add(term("commands",5,"Skill"),"skills",!actors_.empty());
        if(at(flags,2).get_bool(true))add(term("commands",6,"Equip"),"equip",!actors_.empty());
        if(at(flags,3).get_bool(true))add(term("commands",7,"Status"),"status",!actors_.empty());
        if(at(flags,4).get_bool(true))add(term("commands",8,"Formation"),"formation",actors_.size()>1);
        add(term("commands",11,"Options"),"options");
        if(at(flags,5).get_bool(true))add(term("commands",9,"Save"),"save",save_enabled_&&!save_directory_.empty());
        add(term("commands",10,"Game End"),"end");
    } else if(next==GameScene::Save || next==GameScene::Load) {
        const auto label=get(get(get(system_,"terms"),"messages"),"file").get_string("File");
        for(int i=1;i<=20;++i)add(label+" "+std::to_string(i),next==GameScene::Save?"save_slot":"load_slot",next==GameScene::Save||has_save(i),i);
    } else if(next==GameScene::Status || next==GameScene::Formation || next==GameScene::ItemTarget || next==GameScene::SkillActors || next==GameScene::EquipActors || next==GameScene::SkillTarget) {
        for(const auto& a:actors_)add(a.name,"actor",true,a.id);
    } else if(next==GameScene::Skills && actor_selection_<actors_.size()) {
        for(int id:actors_[actor_selection_].skills) {
            const auto& skill=at(skills_,id);const int occasion=get(skill,"occasion").get_int();
            add(get(skill,"name").get_string()+"  "+std::to_string(get(skill,"mpCost").get_int()),"skill",occasion==0||occasion==2,id);
        }
    } else if(next==GameScene::EquipSlots && actor_selection_<actors_.size()) {
        const auto& a=actors_[actor_selection_];
        for(std::size_t i=0;i<a.equips.size();++i) {
            const auto& gear=a.equips[i];
            add(at(get(system_,"equipTypes"),static_cast<int>(i)+1).get_string("Equip")+"  "+get(at(gear.first==1?weapons_:armors_,gear.second),"name").get_string(),"slot",equip_allowed(a,gear.first,0,static_cast<int>(i)+1),static_cast<int>(i));
        }
    } else if(next==GameScene::EquipItems && actor_selection_<actors_.size()) {
        add("-","gear",true,0);const int kind=equip_slot_==0?1:2;
        for(const auto& gear:kind==1?weapon_inventory_:armor_inventory_)if(gear.second>0&&equip_allowed(actors_[actor_selection_],kind,gear.first,equip_slot_+1))
            add(get(at(kind==1?weapons_:armors_,gear.first),"name").get_string(),"gear",true,gear.first);
    } else if(next==GameScene::Items) {
        for(const auto& entry:inventory_)if(entry.second>0) {
            const auto& item=at(items_,entry.first);
            add(get(item,"name").get_string()+"  x"+std::to_string(entry.second),"item",true,entry.first);
        }
    } else if(next==GameScene::Options) {
        add(get(get(get(system_,"terms"),"messages"),"alwaysDash").get_string("Always Dash")+": "+(always_dash_?"ON":"OFF"),"dash");
    } else if(next==GameScene::GameEnd) {
        add(term("commands",21,"To Title"),"title");add("Biblioteca RPG2LX","library");add(term("commands",22,"Cancel"),"cancel");
        selection_=2; // Opening this screen never selects an exit by default.
    }
}
void GameSession::show_title() {set_scene(GameScene::Title);}
void GameSession::navigate(int d) {
    if(choosing()){choice_index_=(choice_index_+(d==8?-1:1)+static_cast<int>(choices_.size()))%choices_.size();return;}
    if(scene_==GameScene::Map||scene_commands_.empty())return;
    selection_=(selection_+(d==8?scene_commands_.size()-1:1))%scene_commands_.size();notice_.clear();
}
void GameSession::cancel() {
    if(choosing()){if(choice_cancel_!=-1)finish_choice(choice_cancel_);return;}
    if(scene_==GameScene::Map) {
        if(message_.empty()&&commands_.empty()&&menu_enabled_) {
            if(moving())menu_requested_=true;else set_scene(GameScene::Menu);
        }
    } else if(scene_==GameScene::Title) { /* MV title cancel does not end the game. */ }
    else if(scene_==GameScene::Menu)set_scene(GameScene::Map);
    else if(scene_==GameScene::Load)set_scene(GameScene::Title);
    else if(scene_==GameScene::ItemTarget)set_scene(GameScene::Items);
    else if(scene_==GameScene::SkillTarget)set_scene(GameScene::Skills);
    else if(scene_==GameScene::Skills)set_scene(GameScene::SkillActors);
    else if(scene_==GameScene::EquipItems)set_scene(GameScene::EquipSlots);
    else if(scene_==GameScene::EquipSlots)set_scene(GameScene::EquipActors);
    else if(scene_==GameScene::Options && options_from_title_)set_scene(GameScene::Title);
    else set_scene(GameScene::Menu);
}
void GameSession::confirm_scene() {
    if(scene_commands_.empty()||selection_>=scene_commands_.size())return;
    const auto c=scene_commands_[selection_];if(!c.enabled)return;
    if(c.symbol=="new") {
        const auto project=project_,saves=save_directory_;std::string error;
        GameSession next;if(next.boot(project,error)){next.save_directory_=saves;next.always_dash_=always_dash_;*this=std::move(next);}else notice_=error;
    } else if(c.symbol=="load")set_scene(GameScene::Load);
    else if(c.symbol=="items")set_scene(GameScene::Items);
    else if(c.symbol=="skills")set_scene(GameScene::SkillActors);
    else if(c.symbol=="equip")set_scene(GameScene::EquipActors);
    else if(c.symbol=="status")set_scene(GameScene::Status);
    else if(c.symbol=="formation")set_scene(GameScene::Formation);
    else if(c.symbol=="save")set_scene(GameScene::Save);
    else if(c.symbol=="end")set_scene(GameScene::GameEnd);
    else if(c.symbol=="title")show_title();
    else if(c.symbol=="library")library_requested_=true;
    else if(c.symbol=="cancel")cancel();
    else if(c.symbol=="options"){options_from_title_=scene_==GameScene::Title;set_scene(GameScene::Options);}
    else if(c.symbol=="dash"){always_dash_=!always_dash_;set_scene(GameScene::Options);}
    else if(c.symbol=="save_slot") {std::string error;if(save(c.value,error))set_scene(GameScene::Map);else notice_=error;}
    else if(c.symbol=="load_slot") {std::string error;if(!load(c.value,error))notice_=error;}
    else if(c.symbol=="actor" && scene_==GameScene::Formation && selection_>0) {
        std::swap(party_[0],party_[selection_]);refresh_party();set_scene(GameScene::Formation);
    } else if(c.symbol=="item") {
        const auto& item=at(items_,c.value);const int occasion=get(item,"occasion").get_int();
        if(occasion==1||occasion==3||get(item,"itypeId").get_int()==2)return;
        pending_item_=c.value;const int scope=get(item,"scope").get_int();
        if(scope>=7&&scope<=11)set_scene(GameScene::ItemTarget);
        else notice_=get(item,"description").get_string();
    } else if(c.symbol=="actor" && (scene_==GameScene::SkillActors || scene_==GameScene::EquipActors)) {
        actor_selection_=selection_;set_scene(scene_==GameScene::SkillActors?GameScene::Skills:GameScene::EquipSlots);
    } else if(c.symbol=="slot") {equip_slot_=c.value;set_scene(GameScene::EquipItems);}
    else if(c.symbol=="gear" && actor_selection_<actors_.size()) {
        auto& a=actors_[actor_selection_];if(equip_slot_<0||static_cast<std::size_t>(equip_slot_)>=a.equips.size())return;
        auto& old=a.equips[equip_slot_];const int kind=equip_slot_==0?1:2;auto& pool=kind==1?weapon_inventory_:armor_inventory_;
        if(c.value && (pool[c.value]<=0||!equip_allowed(a,kind,c.value,equip_slot_+1)))return;
        if(old.second>0)pool[old.second]=std::min(99,pool[old.second]+1);if(c.value)--pool[c.value];old={kind,c.value};refresh_stats(a);set_scene(GameScene::EquipSlots);
    } else if(c.symbol=="skill") {pending_item_=c.value;set_scene(GameScene::SkillTarget);}
    else if(c.symbol=="actor" && scene_==GameScene::SkillTarget && actor_selection_<actors_.size()) {
        const auto& skill=at(skills_,pending_item_);auto& user=actors_[actor_selection_];const int cost=get(skill,"mpCost").get_int();
        if(user.mp>=cost && get(skill,"tpCost").get_int()==0 && apply_effects(skill,selection_)){user.mp-=cost;set_scene(GameScene::Skills);}
    } else if(c.symbol=="actor" && scene_==GameScene::ItemTarget) {
        if(use_item(pending_item_,selection_))set_scene(GameScene::Items);
    }
}

bool GameSession::use_item(int id,std::size_t target) {
    if(item_count(id)<=0||target>=actors_.size())return false;
    const auto& item=at(items_,id);const bool applied=apply_effects(item,target);
    if(applied&&get(item,"consumable").get_bool())--inventory_[id];return applied;
}
bool GameSession::apply_effects(const JsonValue& item,std::size_t target) {
    const int scope=get(item,"scope").get_int();
    // Damage formulas are JavaScript; reject them rather than applying a guessed effect.
    if(get(get(item,"damage"),"type").get_int()!=0)return false;
    const auto& effects=get(item,"effects");
    for(const auto& e:effects.arr_val) {
        const int code=get(e,"code").get_int();
        if(code!=11&&code!=12&&!(code==22&&get(e,"dataId").get_int()==1))return false;
    }
    bool applied=false;
    for(std::size_t i=0;i<actors_.size();++i) {
        if(scope!=8&&scope!=10&&i!=target)continue;
        auto& a=actors_[i];const bool dead=a.hp==0;
        if(dead!=(scope==9||scope==10))continue;
        for(const auto& e:effects.arr_val) {
            const int code=get(e,"code").get_int();
            if(code==22){if(a.hp==0){a.hp=1;applied=true;}continue;}
            const int max=code==11?a.params[0]:a.params[1];int& stat=code==11?a.hp:a.mp;
            const int amount=static_cast<int>(std::floor(max*get(e,"value1").get_double()+get(e,"value2").get_double()));
            const int value=std::clamp(stat+amount,0,max);applied=applied||value!=stat;stat=value;
        }
    }
    return applied;
}

bool GameSession::has_save(int slot) const {
    if(slot<1||slot>20||save_directory_.empty())return false;
    std::error_code ec;return fs::is_regular_file(save_directory_/("file"+std::to_string(slot)+".json"),ec);
}
bool GameSession::save(int slot,std::string& error) const {
    error.clear();
    try {
        if(slot<1||slot>20||save_directory_.empty()||moving()||!commands_.empty()||!message_.empty())throw std::runtime_error("Cannot save at this time");
        fs::create_directories(save_directory_);
        const auto path=save_directory_/("file"+std::to_string(slot)+".json");
        auto tmp=path,backup=path;tmp+=".tmp";backup+=".bak";
        std::ofstream out(tmp,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("Cannot create save");
        out<<"{\"format\":1,\"title\":"<<quote(game_title())<<",\"map\":"<<map_id_<<",\"x\":"<<x_<<",\"y\":"<<y_<<",\"direction\":"<<direction_
           <<",\"gold\":"<<gold_<<",\"steps\":"<<steps_<<",\"seconds\":"<<static_cast<long long>(play_seconds_)<<",\"followers\":"<<(followers_visible_?"true":"false")
           <<",\"menuEnabled\":"<<(menu_enabled_?"true":"false")<<",\"saveEnabled\":"<<(save_enabled_?"true":"false")<<",\"party\":[";
        for(std::size_t i=0;i<party_.size();++i){if(i)out<<',';out<<party_[i];}out<<"],\"actors\":[";
        for(std::size_t i=0;i<actors_.size();++i){
            if(i)out<<',';const auto& a=actors_[i];out<<'['<<a.id<<','<<a.hp<<','<<a.mp<<",[";
            for(std::size_t e=0;e<a.equips.size();++e){if(e)out<<',';out<<'['<<a.equips[e].first<<','<<a.equips[e].second<<']';}out<<"],[";
            for(std::size_t k=0;k<a.skills.size();++k){if(k)out<<',';out<<a.skills[k];}out<<"]]";
        }out<<"],\"weapons\":[";
        bool comma=false;for(const auto& v:weapon_inventory_){if(comma)out<<',';out<<'['<<v.first<<','<<v.second<<']';comma=true;}out<<"],\"armors\":[";
        comma=false;for(const auto& v:armor_inventory_){if(comma)out<<',';out<<'['<<v.first<<','<<v.second<<']';comma=true;}out<<"],\"switches\":[";
        comma=false;for(const auto& s:switches_)if(s.second){if(comma)out<<',';out<<s.first;comma=true;}out<<"],\"variables\":[";
        comma=false;for(const auto& v:variables_){if(comma)out<<',';out<<'['<<v.first<<','<<v.second<<']';comma=true;}out<<"],\"items\":[";
        comma=false;for(const auto& v:inventory_){if(comma)out<<',';out<<'['<<v.first<<','<<v.second<<']';comma=true;}out<<"],\"selfSwitches\":[";
        comma=false;for(const auto& key:self_switches_){if(comma)out<<',';out<<quote(key);comma=true;}out<<"]}\n";
        out.flush();if(!out)throw std::runtime_error("Cannot write save");out.close();
        std::error_code ec;
        if(fs::exists(path)) {fs::remove(backup,ec);ec.clear();fs::rename(path,backup,ec);if(ec)throw std::runtime_error("Cannot back up previous save");}
        fs::rename(tmp,path,ec);
        if(ec){std::error_code ignored;if(fs::exists(backup))fs::rename(backup,path,ignored);throw std::runtime_error("Cannot commit save");}
        return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool GameSession::load(int slot,std::string& error) {
    error.clear();try {
        if(!has_save(slot))throw std::runtime_error("Save does not exist");
        const auto j=read(save_directory_/("file"+std::to_string(slot)+".json"));
        if(get(j,"format").get_int()!=1||get(j,"title").get_string()!=game_title())throw std::runtime_error("Incompatible save");
        GameSession next;
        // Copy database and start settings without executing starting autoruns.
        next.project_=project_;next.system_=system_;next.actor_database_=actor_database_;next.classes_=classes_;next.items_=items_;next.common_events_=common_events_;next.warnings_=warnings_;next.save_directory_=save_directory_;
        next.weapons_=weapons_;next.armors_=armors_;next.skills_=skills_;
        next.screen_width_=screen_width_;next.screen_height_=screen_height_;
        const auto& party=get(j,"party");if(!party.is_array()||party.arr_val.size()>100)throw std::runtime_error("Invalid saved party");
        for(const auto& v:party.arr_val){int id=integer(v,1,999999);if(std::find(next.party_.begin(),next.party_.end(),id)!=next.party_.end())throw std::runtime_error("Duplicate saved actor");next.party_.push_back(id);}
        next.refresh_party();
        for(const auto& a:get(j,"actors").arr_val) {
            int id=integer(at(a,0),1,999999);auto it=std::find_if(next.actors_.begin(),next.actors_.end(),[&](const PartyActor& actor){return actor.id==id;});
            if(it==next.actors_.end())throw std::runtime_error("Saved actor missing");
            if(at(a,3).is_array()) {
                if(at(a,3).arr_val.size()!=it->equips.size())throw std::runtime_error("Invalid saved equipment slots");
                for(std::size_t slot=0;slot<it->equips.size();++slot) {
                    const auto& gear=at(a,3).arr_val[slot];const int kind=integer(at(gear,0),1,2),id=integer(at(gear,1),0,999999);
                    if(id&&!at(kind==1?weapons_:armors_,id).is_object())throw std::runtime_error("Saved equipment missing");
                    it->equips[slot]={kind,id};
                }
                next.refresh_stats(*it);
            }
            if(at(a,4).is_array()){it->skills.clear();for(const auto& skill:at(a,4).arr_val)it->skills.push_back(integer(skill,1,999999));}
            it->hp=integer(at(a,1),0,it->params[0]);it->mp=integer(at(a,2),0,it->params[1]);
        }
        for(const auto& s:get(j,"switches").arr_val)next.switches_[integer(s,1,999999)]=true;
        for(const auto& v:get(j,"variables").arr_val)next.variables_[integer(at(v,0),1,999999)]=integer(at(v,1),-1000000000,1000000000);
        for(const auto& v:get(j,"items").arr_val)next.inventory_[integer(at(v,0),1,999999)]=integer(at(v,1),0,99);
        for(const auto& v:get(j,"weapons").arr_val)next.weapon_inventory_[integer(at(v,0),1,999999)]=integer(at(v,1),0,99);
        for(const auto& v:get(j,"armors").arr_val)next.armor_inventory_[integer(at(v,0),1,999999)]=integer(at(v,1),0,99);
        for(const auto& s:get(j,"selfSwitches").arr_val){if(!s.is_string()||s.str_val.size()>64)throw std::runtime_error("Invalid self switch");next.self_switches_.insert(s.str_val);}
        next.gold_=integer(get(j,"gold"),0,99999999);next.steps_=integer(get(j,"steps"),0,1000000000);next.play_seconds_=integer(get(j,"seconds"),0,1000000000);
        next.followers_visible_=get(j,"followers").get_bool();next.always_dash_=always_dash_;
        next.menu_enabled_=get(j,"menuEnabled").get_bool(true);next.save_enabled_=get(j,"saveEnabled").get_bool(true);
        if(!next.load_map(integer(get(j,"map"),1,999999),integer(get(j,"x"),0,10000),integer(get(j,"y"),0,10000),error))return false;
        next.direction_=integer(get(j,"direction"),2,8);if(next.direction_%2)throw std::runtime_error("Invalid saved direction");next.reset_followers();
        *this=std::move(next);return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
} // namespace rpg2lx
