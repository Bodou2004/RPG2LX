// Replays an external MV project's event/menu route against the real engine.
// No game data is generated, patched or copied by this executable.
#include "rpg2lx/session.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <queue>
#include <stdexcept>
#ifdef RPG2LX_WITH_SDL
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "rpg2lx/map_view.hpp"
#endif
namespace fs=std::filesystem;
using namespace rpg2lx;
namespace {
const JsonValue empty;
const JsonValue& get(const JsonValue& j,const std::string& k){const auto* p=j.find(k);return p?*p:empty;}
void require(bool v,const std::string& message){if(!v)throw std::runtime_error(message);}
void healthy(const GameSession& s){require(s.error().empty(),s.error());}
void tick(GameSession& s,int frames=8){for(int i=0;i<frames;++i){s.update(.05);healthy(s);}}
void dismiss(GameSession& s){int budget=100;while(!s.message().empty()&&!s.choosing()&&budget--){s.confirm();healthy(s);}require(budget>0,"Dialogue did not finish");tick(s);}
void walk(GameSession& s,int tx,int ty){
    require(s.scene()==GameScene::Map&&!s.choosing()&&s.message().empty(),"Walking while a window is open");
    const auto& map=s.map();const int width=map.width,height=map.height,map_id=s.map_id();
    require(tx>=0&&ty>=0&&tx<width&&ty<height,"Walking target outside map");
    std::vector<int> previous(width*height,-1),direction(width*height,0);std::queue<int> pending;
    int start=s.y()*width+s.x(),target=ty*width+tx;previous[start]=start;pending.push(start);
    while(!pending.empty()&&previous[target]<0){
        const int from=pending.front();pending.pop();int x=from%width,y=from/width;
        for(int d:{2,4,6,8}){
            const int xx=x+(d==6)-(d==4),yy=y+(d==2)-(d==8);
            if(xx<0||yy<0||xx>=width||yy>=height||!s.passable(x,y,d))continue;
            const int next=yy*width+xx;if(previous[next]>=0)continue;
            bool touch=false;
            for(const auto& e:map.events)if(e.x==xx&&e.y==yy)if(const auto* p=s.active_page(e))if(p->trigger==1||p->trigger==2)touch=true;
            if(touch&&next!=target)continue;
            previous[next]=from;direction[next]=d;pending.push(next);
        }
    }
    require(previous[target]>=0,"No traversable route to "+std::to_string(tx)+","+std::to_string(ty));
    std::vector<int> route;for(int p=target;p!=start;p=previous[p])route.push_back(direction[p]);
    for(auto it=route.rbegin();it!=route.rend();++it){require(s.move(*it),"Movement blocked on computed route");tick(s);require(s.map_id()==map_id||it+1==route.rend(),"Unexpected transfer along route");}
}
void talk(GameSession& s,int id){
    const auto found=std::find_if(s.map().events.begin(),s.map().events.end(),[&](const MapEvent& e){return e.id==id;});
    require(found!=s.map().events.end(),"Event not found: "+std::to_string(id));
    const auto event=*found;const auto* page=s.active_page(event);require(page!=nullptr,"Event has no active page");
    const int priority=get(page->settings,"priorityType").get_int(1);
    if(priority!=1){walk(s,event.x,event.y);s.confirm();healthy(s);return;}
    const int dirs[]={2,4,6,8};bool reached=false;const int original_map=s.map_id();
    for(int d:dirs){
        const int x=event.x-(d==6)+(d==4),y=event.y-(d==2)+(d==8);
        try{walk(s,x,y);}catch(const std::exception&){healthy(s);require(s.map_id()==original_map,"Unexpected map transfer while approaching event");continue;}
        require(s.map_id()==original_map,"Unexpected map transfer while approaching event");
        s.move(d);require(!s.moving(),"Normal-priority NPC must block interaction step");s.confirm();healthy(s);reached=true;break;
    }
    require(reached,"Could not approach event "+std::to_string(id));
}
void select(GameSession& s,const std::string& symbol,int value=-1){
    for(std::size_t i=0;i<s.scene_commands().size();++i){
        const auto c=s.scene_commands()[s.selection()];
        if(c.symbol==symbol&&(value<0||c.value==value)){require(c.enabled,"Disabled command: "+c.label);s.confirm();healthy(s);return;}
        s.navigate(2);
    }
    throw std::runtime_error("Menu command not found: "+symbol+" value "+std::to_string(value));
}
void expectations(const GameSession& s,const JsonValue& step){
    if(const auto* n=step.find("map"))require(s.map_id()==n->get_int(),"Wrong map");
    if(const auto* n=step.find("x"))require(s.x()==n->get_int(),"Wrong X coordinate");
    if(const auto* n=step.find("y"))require(s.y()==n->get_int(),"Wrong Y coordinate");
    if(const auto* n=step.find("moving"))require(s.moving()==n->get_bool(),"Unexpected movement state");
    if(const auto* n=step.find("gold"))require(s.gold()==n->get_int(),"Wrong gold amount");
    if(const auto* n=step.find("party")){require(s.actors().size()==n->arr_val.size(),"Wrong party size");for(std::size_t i=0;i<n->arr_val.size();++i)require(s.actors()[i].id==n->arr_val[i].get_int(),"Wrong party order");}
    for(const auto& entry:get(step,"items").obj_val)require(s.item_count(std::stoi(entry.first))==entry.second.get_int(),"Wrong item count: "+entry.first);
    for(const auto& entry:get(step,"switches").obj_val)require(s.switch_value(std::stoi(entry.first))==entry.second.get_bool(),"Wrong switch: "+entry.first);
    for(const auto& entry:get(step,"variables").obj_val)require(s.variable_value(std::stoi(entry.first))==entry.second.get_int(),"Wrong variable: "+entry.first);
    for(const auto& actor:get(step,"actors").arr_val){
        const int id=get(actor,"id").get_int();auto found=std::find_if(s.actors().begin(),s.actors().end(),[&](const PartyActor& a){return a.id==id;});
        require(found!=s.actors().end(),"Expected actor missing");
        if(const auto* n=actor.find("hp"))require(found->hp==n->get_int(),"Wrong actor HP");
        if(const auto* n=actor.find("mp"))require(found->mp==n->get_int(),"Wrong actor MP");
        if(const auto* n=actor.find("weapon"))require(!found->equips.empty()&&found->equips[0].second==n->get_int(),"Wrong equipped weapon");
        if(const auto* n=actor.find("attack"))require(found->params.size()>2&&found->params[2]==n->get_int(),"Wrong attack parameter");
        if(const auto* n=actor.find("skill"))require(std::find(found->skills.begin(),found->skills.end(),n->get_int())!=found->skills.end(),"Expected skill missing");
    }
    if(const auto* t=step.find("message"))require(s.message().find(t->get_string())!=std::string::npos,"Expected dialogue missing");
}
}
int main(int argc,char** argv){
    const auto temp=fs::temp_directory_path()/("rpg2lx-route-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup{fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);
#ifdef RPG2LX_WITH_SDL
        SDL_Quit();
#endif
    }}cleanup{temp};
    std::size_t completed=0;
    try{
        require(argc>=3,"Usage: project_scenario <project> <route.json> [capture-directory]");
        std::ifstream in(fs::u8path(argv[2]),std::ios::binary);require(bool(in),"Cannot read route");std::string data((std::istreambuf_iterator<char>(in)),{}),error;
        const auto route=JsonParser::parse(data,error);require(error.empty()&&route.is_array(),"Invalid route JSON: "+error);
        GameSession s;require(s.boot(fs::u8path(argv[1]),error,false),error);s.set_save_directory(temp/"saves");s.show_title();
#ifdef RPG2LX_WITH_SDL
        SDL_SetMainReady();require(SDL_Init(SDL_INIT_VIDEO)==0,SDL_GetError());
        using Window=std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)>;using Renderer=std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)>;
        Window window(SDL_CreateWindow("Route check",0,0,s.screen_width(),s.screen_height(),SDL_WINDOW_HIDDEN),SDL_DestroyWindow);require(bool(window),SDL_GetError());
        Renderer renderer(SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_SOFTWARE),SDL_DestroyRenderer);require(bool(renderer),SDL_GetError());MapView view(renderer.get());
#endif
        for(const auto& step:route.arr_val){
            const auto action=get(step,"do").get_string();
            if(action=="confirm")s.confirm();else if(action=="cancel")s.cancel();else if(action=="title")s.show_title();
            else if(action=="talk")talk(s,get(step,"event").get_int());
            else if(action=="walk")walk(s,get(step,"x").get_int(),get(step,"y").get_int());
            else if(action=="move")require(s.move(get(step,"direction").get_int()),"Movement command blocked");
            else if(action=="dismiss")dismiss(s);
            else if(action=="choose"){require(s.choosing(),"No choices open");const int choice=get(step,"index").get_int();require(choice>=0&&static_cast<std::size_t>(choice)<s.choices().size(),"Invalid choice index");while(s.choice_index()!=choice)s.navigate(2);s.confirm();}
            else if(action=="menu"){if(s.scene()==GameScene::Map)s.cancel();select(s,get(step,"symbol").get_string(),get(step,"value").get_int(-1));}
            else if(action=="select")select(s,get(step,"symbol").get_string(),get(step,"value").get_int(-1));
            else if(action=="save")require(s.save(get(step,"slot").get_int(1),error),error);
            else if(action=="load")require(s.load(get(step,"slot").get_int(1),error),error);
            else if(action=="tick")tick(s,get(step,"frames").get_int(8));
            else if(action!="expect"&&action!="capture")throw std::runtime_error("Unknown route action "+action);
            healthy(s);expectations(s,step);
#ifdef RPG2LX_WITH_SDL
            if(argc>=4){
                SDL_RenderSetLogicalSize(renderer.get(),s.screen_width(),s.screen_height());require(view.render(s,500,error),error);
                if(action=="capture"){
                    fs::path name=fs::u8path(get(step,"name").get_string());require(name==name.filename()&&name.extension()==".bmp","Capture must be a BMP filename");fs::path out=fs::u8path(argv[3]);fs::create_directories(out);
                    auto* pixels=SDL_CreateRGBSurfaceWithFormat(0,s.screen_width(),s.screen_height(),32,SDL_PIXELFORMAT_ARGB8888);require(pixels!=nullptr,SDL_GetError());
                    const int read=SDL_RenderReadPixels(renderer.get(),nullptr,pixels->format->format,pixels->pixels,pixels->pitch);const int saved=read==0?SDL_SaveBMP(pixels,(out/name).u8string().c_str()):-1;SDL_FreeSurface(pixels);require(saved==0,SDL_GetError());
                }
            }
#endif
            ++completed;std::cout<<completed<<" "<<action<<" map="<<s.map_id()<<" position="<<s.x()<<","<<s.y()<<'\n';
        }
        std::cout<<"PASS: "<<completed<<" external-project route steps\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL after step "<<completed<<": "<<e.what()<<'\n';return 1;}
}
