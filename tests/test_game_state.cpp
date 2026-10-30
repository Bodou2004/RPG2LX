#include "rpg2lx/session.hpp"
#include "rpg2lx/launcher.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs=std::filesystem;
using namespace rpg2lx;
static void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
static void write(const fs::path& path,const std::string& s){std::ofstream out(path,std::ios::binary);out<<s;}
static void finish(GameSession& s){for(int i=0;i<6;++i)s.update(.05);}
int main(int argc,char** argv) {
    const auto temp=fs::temp_directory_path()/("rpg2lx-scenes-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup{fs::path p;~Cleanup(){std::error_code ec;fs::remove_all(p,ec);}}cleanup{temp};
    try {
        require(argc==2,"Pass fixture root");const auto game=temp/"games/Party";fs::create_directories(game.parent_path());fs::copy(argv[1],game,fs::copy_options::recursive);
        write(game/"data/System.json",R"({"gameTitle":"Party test","startMapId":1,"startX":4,"startY":7,"partyMembers":[1,2,3,4],"optFollowers":true,"terms":{"commands":["Fight","Escape","Attack","Guard","Items","Skills","Equip","Status","Formation","Save","End","Options",null,null,null,null,null,null,"New","Continue"]}})");
        write(game/"data/Actors.json",R"([null,{"id":1,"name":"One","characterName":"$Hero","characterIndex":0},{"id":2,"name":"Two","characterName":"$Hero","characterIndex":0},{"id":3,"name":"Three","characterName":"$Hero","characterIndex":0},{"id":4,"name":"Four","characterName":"$Hero","characterIndex":0}])");
        GameSession s;std::string error;require(s.boot(game,error),error.c_str());s.set_save_directory(temp/"saves/Party");
        require(s.actors().size()==4 && s.followers().size()==3 && s.followers_visible(),"Full initial party / followers");
        s.show_title();s.cancel();require(s.scene()==GameScene::Title,"Title cancel must not return to launcher");
        require(!s.move(6),"Title blocks walking");s.confirm();require(s.scene()==GameScene::Map,"New game enters map");
        require(s.move(6),"First step");s.update(.1);require(s.followers()[0].to_x==4,"Followers begin stacked, not ahead");
        s.cancel();require(s.scene()==GameScene::Map,"Menu waits until the current step finishes");finish(s);
        require(s.scene()==GameScene::Menu&&s.x()==5,"Menu requested mid-step opens without losing movement");s.cancel();
        require(s.move(6),"Second step");s.update(.1);require(s.followers()[0].x==5&&s.followers()[1].x==4,"Followers chase in order");finish(s);
        s.cancel();require(s.scene()==GameScene::Menu,"Cancel opens MV menu");const auto px=s.real_x();s.update(.1);require(!s.move(6)&&s.real_x()==px,"Menu pauses exploration");
        s.cancel();require(s.scene()==GameScene::Map&&s.x()==6,"Cancel preserves live position");
        // Existing guide event changes a switch after dialogue; persist that state.
        s.move(8);s.confirm();s.confirm();require(s.switch_value(1),"Dialogue then switch");
        require(s.save(1,error),error.c_str());require(s.has_save(1),"Save file exists");
        require(s.move(4),"Move after save");finish(s);require(s.x()==5,"Changed position");
        require(s.load(1,error),error.c_str());require(s.x()==6&&s.switch_value(1)&&s.actors().size()==4,"Save restores map, switches and party");
        write(temp/"saves/Party/file2.json",R"({"format":1,"title":"Party test","map":999,"x":0,"y":0,"direction":2,"gold":0,"steps":0,"seconds":0,"party":[1]})");
        require(!s.load(2,error)&&!error.empty()&&s.x()==6&&s.actors().size()==4,"Failed load must preserve live state atomically");
        require(s.save(1,error),error.c_str());require(fs::is_regular_file(temp/"saves/Party/file1.json.bak"),"Replacing slot preserves previous save backup");
        require(!fs::exists(game/"saves"),"Saves remain outside game assets");
        // Choice -> selected branch -> common event -> caller continuation.
        write(game/"data/CommonEvents.json",R"([null,{"id":1,"name":"Award","trigger":0,"switchId":1,"list":[{"code":125,"indent":0,"parameters":[0,0,25]},{"code":0,"indent":0,"parameters":[]}]}])");
        const std::string data=R"({"width":2,"height":2,"tilesetId":1,"scrollType":0,"data":[1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],"events":[null,{"id":1,"name":"Choice","x":0,"y":0,"pages":[{"trigger":3,"conditions":{"selfSwitchValid":true,"selfSwitchCh":"A"},"list":[{"code":0,"indent":0,"parameters":[]}]},{"trigger":3,"conditions":{"switch1Valid":true,"switch1Id":99},"list":[]},{"trigger":3,"conditions":{},"list":[{"code":102,"indent":0,"parameters":[["First","Second"],-1,0,2,0]},{"code":402,"indent":0,"parameters":[0,"First"]},{"code":121,"indent":1,"parameters":[5,5,0]},{"code":402,"indent":0,"parameters":[1,"Second"]},{"code":122,"indent":1,"parameters":[1,1,0,0,7]},{"code":111,"indent":1,"parameters":[1,1,0,7,0]},{"code":117,"indent":2,"parameters":[1]},{"code":411,"indent":1,"parameters":[]},{"code":125,"indent":2,"parameters":[0,0,999]},{"code":412,"indent":1,"parameters":[]},{"code":404,"indent":0,"parameters":[]},{"code":121,"indent":0,"parameters":[6,6,0]},{"code":230,"indent":0,"parameters":[600]},{"code":0,"indent":0,"parameters":[]}]}]}]})";
        write(game/"data/Map003.json",data);
        write(game/"data/System.json",R"({"gameTitle":"Choice test","startMapId":3,"startX":1,"startY":1,"partyMembers":[1]})");
        require(s.boot(game,error),error.c_str());require(s.choosing(),"Autorun presents choices");s.cancel();require(s.choosing(),"Disallowed choice cancellation respected");
        s.navigate(2);s.confirm();require(s.error().empty(),s.error().c_str());
        require(!s.switch_value(5)&&s.switch_value(6)&&s.variable_value(1)==7&&s.gold()==25,"Correct choice branch, variable condition, common event return");
        // Real database-style equipment changes affect stats and survive save/load.
        write(game/"data/Classes.json",R"([null,{"id":1,"name":"Hero","traits":[{"code":51,"dataId":1,"value":1}],"learnings":[{"level":1,"skillId":1}]}])");
        write(game/"data/Weapons.json",R"([null,{"id":1,"name":"Sword","etypeId":1,"wtypeId":1,"params":[0,0,5,0,0,0,0,0],"traits":[]}])");
        write(game/"data/Armors.json","[null]");
        write(game/"data/Items.json",R"([null,{"id":1,"name":"Potion","occasion":0,"scope":7,"itypeId":1,"consumable":true,"damage":{"type":0},"effects":[{"code":11,"dataId":0,"value1":0,"value2":40}]}])");
        write(game/"data/Skills.json",R"([null,{"id":1,"name":"Heal","occasion":0,"scope":7,"mpCost":5,"tpCost":0,"damage":{"type":0},"effects":[{"code":11,"value1":0,"value2":20}]}])");
        write(game/"data/Actors.json",R"([null,{"id":1,"name":"One","classId":1,"equips":[0,0,0,0,0],"characterName":"$Hero","characterIndex":0}])");
        const auto inventory_event=R"({"width":2,"height":2,"tilesetId":1,"scrollType":0,"data":[1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],"events":[null,{"id":1,"name":"Inventory","x":0,"y":0,"pages":[{"trigger":3,"conditions":{},"list":[{"code":127,"indent":0,"parameters":[1,0,0,1]},{"code":126,"indent":0,"parameters":[1,0,0,2]},{"code":311,"indent":0,"parameters":[0,1,1,0,80,false]},{"code":121,"indent":0,"parameters":[10,10,0]},{"code":0,"indent":0,"parameters":[]}]},{"trigger":0,"conditions":{"switch1Valid":true,"switch1Id":10},"list":[{"code":0,"indent":0,"parameters":[]}]}]}]})";
        write(game/"data/Map003.json",inventory_event);require(s.boot(game,error),error.c_str());s.set_save_directory(temp/"saves/Inventory");
        require(s.actors()[0].hp==20&&s.item_count(1)==2,"HP and inventory event commands");s.cancel();
        auto select=[&](const std::string& symbol){for(std::size_t i=0;i<s.scene_commands().size();++i){if(s.scene_commands()[s.selection()].symbol==symbol){s.confirm();return;}s.navigate(2);}throw std::runtime_error("Missing menu command "+symbol);};
        select("equip");s.confirm();s.confirm();s.navigate(2);s.confirm();require(s.actors()[0].params[2]==15,"Equipping inventory weapon changes attack");
        s.cancel();s.cancel();select("items");s.confirm();s.confirm();require(s.actors()[0].hp==60&&s.item_count(1)==1,"Potion affects selected actor and is consumed");
        s.cancel();select("skills");s.confirm();s.confirm();s.confirm();require(s.actors()[0].hp==80&&s.actors()[0].mp==25,"Skill effects and MP consumption");
        s.cancel();s.cancel();s.cancel();require(s.scene()==GameScene::Map,"Cancel returns through skill scenes");
        require(s.save(1,error),error.c_str());require(s.load(1,error),error.c_str());
        require(s.actors()[0].hp==80&&s.actors()[0].params[2]==15&&s.item_count(1)==1&&s.actors()[0].skills.size()==1,"Equipment, resources, learned skills and inventory persist");
        // Launcher B must never destroy a playing session.
        GameLibrary lib(temp);LauncherModel model(lib);require(model.open_project(game,false),"Open real session through launcher");
        auto* live=model.session();model.act(MenuAction::Back);require(model.session()==live&&!model.quit(),"Launcher cancel retains playing session even during events");
        std::cout<<"Game scenes, followers, saves, choices and common events passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
