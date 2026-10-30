#include "rpg2lx/session.hpp"
#include "rpg2lx/tile_layout.hpp"
#include "rpg2lx/launcher.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs=std::filesystem;
using namespace rpg2lx;
static void require(bool value,const std::string& message) { if(!value)throw std::runtime_error(message); }
static void finish_move(GameSession& session) { for(int i=0;i<6;++i)session.update(0.05); }
static void walk(GameSession& session,int direction,int steps) {
    for(int i=0;i<steps;++i){require(session.move(direction),"Movement unexpectedly blocked");finish_move(session);}
}

int main(int argc,char** argv) {
    const auto temp=fs::temp_directory_path()/("rpg2lx-session-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directory(temp);
    struct Cleanup{fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}} cleanup{temp};
    try {
        require(argc==2,"Pass playable fixture path");
        fs::copy(fs::path(argv[1]),temp/"game",fs::copy_options::recursive);
        GameSession session;std::string error;
        require(session.boot(temp/"game",error),error);
        require(session.map_id()==1 && session.x()==4 && session.y()==7,"System starting position");
        require(session.player_image().name=="$Hero","Starting actor character");
        require(!session.passable(1,1,4),"Border must block movement");
        require(!session.passable(2,5,8),"Water must block movement");
        require(!session.passable(5,6,6),"NPC blocks destination");
        require(!session.move(7),"Invalid direction accepted");
        require(session.move(6),"First movement");
        require(session.x()==5 && session.real_x()==4,"Grid and interpolated coordinates are separate");
        require(!session.move(6),"Cannot stack movement while walking");
        finish_move(session);require(session.real_x()==5,"Interpolation completes");
        walk(session,6,1); // 6,7 faces up toward guide at 6,6.
        require(!session.move(8),"Guide should stop upward movement");
        session.confirm();
        require(session.message().find("Bienvenido")!=std::string::npos,"Action-button dialogue");
        require(!session.switch_value(1),"Switch must wait until dialogue is dismissed");
        require(!session.move(6),"Dialogue blocks movement");
        session.confirm();require(session.switch_value(1),"Control switches command");
        walk(session,6,6); // 12,7
        walk(session,8,2); // Steps onto doorway 12,5; direct transfer to map 2.
        require(session.map_id()==2 && session.x()==4 && session.y()==7,"Touch event transfer");
        walk(session,2,2);
        require(session.map_id()==1 && session.x()==12 && session.y()==6,"Return transfer");

        const auto full=tile_patches(1);require(full.size()==1 && full[0].image==5 && full[0].sx==48,"B tile geometry");
        const auto a5=tile_patches(1536);require(a5[0].image==4 && a5[0].sx==0,"A5 tile geometry");
        const auto grass=tile_patches(2816);require(grass.size()==4 && grass[0].image==1 && grass[0].sx==48 && grass[0].sy==96,"A2 quarter geometry");
        require(tile_patches(2048,0)[0].sx!=tile_patches(2048,1)[0].sx,"A1 water animation");
        bool rejected=false;try{tile_patches(4352+47);}catch(...){rejected=true;}
        require(rejected,"Invalid wall autotile shape must be rejected");
        require(tile_patches(2816,0,true).size()>=4,"Table tile pieces");

        // Source passability is insufficient: destination reverse direction matters.
        const auto tiles_path=temp/"game/data/Tilesets.json";
        std::ifstream flags_in(tiles_path);std::string original_flags((std::istreambuf_iterator<char>(flags_in)),{});flags_in.close();
        auto modified_flags=original_flags;
        auto flag_pos=modified_flags.find("\"flags\":[16,0,15");
        require(flag_pos!=std::string::npos,"Flag fixture prefix");
        modified_flags.replace(flag_pos,16,"\"flags\":[16,2,15");
        std::ofstream(tiles_path)<<modified_flags;
        GameSession directional;
        require(directional.boot(temp/"game",error),error);
        require(!directional.passable(4,7,6),"Destination entry direction must be checked");
        std::ofstream(tiles_path)<<original_flags;

        // Missing tilesets must fail atomically and preserve the previous session.
        fs::rename(temp/"game/data/Tilesets.json",temp/"game/data/Tilesets.saved");
        require(!session.boot(temp/"game",error) && !error.empty(),"Missing tilesets accepted");
        require(session.map_id()==1 && session.x()==12,"Failed boot changed a live session");
        fs::rename(temp/"game/data/Tilesets.saved",temp/"game/data/Tilesets.json");
        require(session.boot(temp/"game",error),error);
        // Action event unknown command must produce a diagnostic, not silently continue.
        auto map_path=temp/"game/data/Map001.json";
        std::ifstream in(map_path);std::string json((std::istreambuf_iterator<char>(in)),{});in.close();
        const auto original_map=json;
        const std::string greeting="¡Bienvenido a RPG2LX!";
        const auto greeting_pos=json.find(greeting);require(greeting_pos!=std::string::npos,"Greeting fixture");
        json.replace(greeting_pos,greeting.size(),std::string(180,'L'));
        std::ofstream(map_path)<<json;
        require(session.boot(temp/"game",error),error);
        walk(session,6,2);session.move(8);session.confirm();
        require(!session.message().empty(),"First text page");
        session.confirm();
        require(!session.message().empty() && !session.switch_value(1),"Long text must paginate before later commands");
        session.confirm();require(session.message().empty() && session.switch_value(1),"Last text page releases interpreter");
        json=original_map;
        const auto token=json.find("\"code\":101");require(token!=std::string::npos,"Fixture speech not found");
        json.replace(token,10,"\"code\":355");
        std::ofstream(map_path)<<json;
        require(session.boot(temp/"game",error),error);
        // First message in this fixture is the guide's action event.
        walk(session,6,2);session.move(8);session.confirm();
        require(session.error().find("355")!=std::string::npos,"Unsupported script command must be explicit");
        require(!session.move(6),"Cannot continue after unsupported command");
        std::cout<<"Native session and tile-layout tests passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
