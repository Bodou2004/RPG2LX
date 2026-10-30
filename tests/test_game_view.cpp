// Hidden SDL rendering checks, including the actual MV title/window/face assets.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "rpg2lx/map_view.hpp"
#include "rpg2lx/library_view.hpp"
#include "rpg2lx/launcher.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace rpg2lx;
namespace fs=std::filesystem;
static void require(bool v,const std::string& m){if(!v)throw std::runtime_error(m);}
int main(int argc,char** argv) {
    const auto temp=fs::temp_directory_path()/("rpg2lx-view-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup{fs::path p;~Cleanup(){std::error_code ec;fs::remove_all(p,ec);SDL_Quit();}}cleanup{temp};
    try {
        require(argc>=2,"Pass project directory");SDL_SetMainReady();require(SDL_Init(SDL_INIT_VIDEO)==0,SDL_GetError());
        GameSession s;std::string error;require(s.boot(fs::u8path(argv[1]),error,false),error);s.set_save_directory(temp/"saves");s.show_title();
        using Window=std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)>;
        using Renderer=std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)>;
        Window w(SDL_CreateWindow("test",0,0,s.screen_width(),s.screen_height(),SDL_WINDOW_HIDDEN),SDL_DestroyWindow);require(bool(w),SDL_GetError());
        Renderer r(SDL_CreateRenderer(w.get(),-1,SDL_RENDERER_SOFTWARE),SDL_DestroyRenderer);require(bool(r),SDL_GetError());
        MapView view(r.get());
        const fs::path out=argc>=3?fs::u8path(argv[2]):temp/"captures";fs::create_directories(out);
        auto capture=[&](const char* name) {
            SDL_RenderSetLogicalSize(r.get(),s.screen_width(),s.screen_height());require(view.render(s,500,error),error);
            auto* image=SDL_CreateRGBSurfaceWithFormat(0,s.screen_width(),s.screen_height(),32,SDL_PIXELFORMAT_ARGB8888);require(image!=nullptr,SDL_GetError());
            const int read=SDL_RenderReadPixels(r.get(),nullptr,image->format->format,image->pixels,image->pitch);
            const int saved=read==0?SDL_SaveBMP(image,(out/name).u8string().c_str()):-1;SDL_FreeSurface(image);require(saved==0,SDL_GetError());
        };
        capture("title.bmp");s.cancel();require(s.scene()==GameScene::Title,"Title B must keep title");s.confirm();capture("map.bmp");
        for(int d:{6,2,6,2}){s.move(d);for(int i=0;i<6;++i)s.update(.05);}
        capture("followers.bmp");const int x=s.x(),y=s.y();s.cancel();require(s.scene()==GameScene::Menu,"B opens menu");capture("menu.bmp");
        auto select=[&](const std::string& symbol) {
            for(std::size_t i=0;i<s.scene_commands().size();++i){if(s.scene_commands()[s.selection()].symbol==symbol){s.confirm();return;}s.navigate(2);}
            throw std::runtime_error("Missing menu command: "+symbol);
        };
        select("equip");s.confirm();capture("equipment.bmp");s.cancel();s.cancel();
        select("save");capture("save.bmp");s.confirm();require(s.scene()==GameScene::Map&&s.has_save(1),"Save scene writes slot");
        s.show_title();s.navigate(2);s.confirm();require(s.scene()==GameScene::Load,"Continue opens files");s.confirm();
        require(s.scene()==GameScene::Map&&s.x()==x&&s.y()==y,"Continue restores saved position");
        GameLibrary lib(temp/"library");LauncherModel model(lib);LibraryView library(r.get());SDL_RenderSetLogicalSize(r.get(),640,480);library.draw(model);
        std::cout<<"Title, original assets, menu, equipment and save/load rendering passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
