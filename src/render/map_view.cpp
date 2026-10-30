#include "rpg2lx/map_view.hpp"
#include "rpg2lx/tile_layout.hpp"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <stdexcept>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 8192
#include "stb_image.h"
#include "font8x8_basic.h"
#include "font8x8_ext_latin.h"

namespace fs = std::filesystem;
namespace rpg2lx {
struct MapView::Impl {
    SDL_Renderer* renderer;
    using Texture = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;
    struct Image { Texture texture; int w, h; };
    std::map<fs::path, Image> images;
    std::size_t bytes = 0;
    fs::path current_project;
    int current_map = 0;
    explicit Impl(SDL_Renderer* r) : renderer(r) {}

    void fill(SDL_Rect area,Uint8 r,Uint8 g,Uint8 b,Uint8 a=255) {
        SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(renderer,r,g,b,a);SDL_RenderFillRect(renderer,&area);
    }
    void text(const std::string& value,int x,int y,int scale=3,bool enabled=true,int max_chars=60) {
        int count=0,start=x;
        for(std::size_t i=0;i<value.size() && count<max_chars;) {
            unsigned cp=static_cast<unsigned char>(value[i++]),remaining=0;
            if(cp>=0xf0){cp&=7;remaining=3;}else if(cp>=0xe0){cp&=15;remaining=2;}else if(cp>=0xc0){cp&=31;remaining=1;}
            while(remaining--&&i<value.size())cp=(cp<<6)|(static_cast<unsigned char>(value[i++])&63);
            if(cp=='\n'){x=start;y+=36;count=0;continue;}
            const auto* glyph=cp<128?font8x8_basic[cp]:cp>=160&&cp<=255?font8x8_ext_latin[cp-160]:font8x8_basic['?'];
            for(int row=0;row<8;++row)for(int col=0;col<8;++col)if(static_cast<unsigned char>(glyph[row])&(1u<<col)) {
                fill({x+col*scale+1,y+row*scale+2,scale,scale},0,0,0,enabled?200:100);
                fill({x+col*scale,y+row*scale,scale,scale},255,255,255,enabled?255:115);
            }
            x+=8*scale;++count;
        }
    }
    void window(const GameSession& session,SDL_Rect area) {
        const auto path=session.project()/"img/system/Window.png";
        if(!fs::exists(path)){fill(area,15,25,60,235);return;}
        const auto& skin=image(session.project(),"system","Window");
        if(skin.w<192||skin.h<192)throw std::runtime_error("Invalid MV Window.png dimensions");
        SDL_SetTextureAlphaMod(skin.texture.get(),192);
        blit(skin,{0,0,96,96},{area.x+4,area.y+4,area.w-8,area.h-8});
        for(int y=4;y<area.h-4;y+=96)for(int x=4;x<area.w-4;x+=96) {
            const int w=std::min(96,area.w-4-x),h=std::min(96,area.h-4-y);
            blit(skin,{0,96,w,h},{area.x+x,area.y+y,w,h});
        }
        SDL_SetTextureAlphaMod(skin.texture.get(),255);
        const int xs[]={0,24,area.w-24},ys[]={0,24,area.h-24};
        const int ws[]={24,area.w-48,24},hs[]={24,area.h-48,24};
        for(int y=0;y<3;++y)for(int x=0;x<3;++x)if(x!=1||y!=1)
            blit(skin,{96+x*24+(x==2?24:0),y*24+(y==2?24:0),x==1?48:24,y==1?48:24},{area.x+xs[x],area.y+ys[y],ws[x],hs[y]});
    }
    void face(const GameSession& s,const std::string& name,int index,int x,int y,int size=96) {
        if(name.empty())return;
        const auto& f=image(s.project(),"faces",name);
        if(index<0||index>7)throw std::runtime_error("Invalid face index");
        blit(f,{(index%4)*144,(index/4)*144,144,144},{x,y,size,size});
    }
    void command_window(const GameSession& s,SDL_Rect box,int rows=0) {
        window(s,box);const auto& commands=s.scene_commands();
        const int capacity=rows?rows:std::max(1,(box.h-36)/36);
        const auto first=(s.selection()/capacity)*capacity;
        for(std::size_t i=first;i<std::min(commands.size(),first+capacity);++i) {
            const int y=box.y+18+static_cast<int>(i-first)*36;
            if(i==s.selection())fill({box.x+14,y-3,box.w-28,32},110,150,230,100);
            text(commands[i].label,box.x+24,y,2,commands[i].enabled,(box.w-48)/16);
        }
    }
    void actor(const GameSession& s,const PartyActor& a,int x,int y,int w,int face_size=96) {
        face(s,a.face_name,a.face_index,x,y,face_size);
        const int tx=x+(a.face_name.empty()?0:face_size+18);
        text(a.name,tx,y,3,true,std::max(1,(w-(tx-x))/24));
        text(a.class_name,tx,y+30,2,true,20);
        text(s.term("basic",1,"Lv")+" "+std::to_string(a.level),tx,y+54,2);
        const int bar=std::max(20,w-(tx-x)-12);
        fill({tx,y+89,bar,6},25,25,30);fill({tx,y+89,bar*a.hp/std::max(1,a.params[0]),6},99,210,92);
        text(s.term("basic",2,"HP")+" "+std::to_string(a.hp)+"/"+std::to_string(a.params[0]),tx,y+76,2);
        text(s.term("basic",4,"MP")+" "+std::to_string(a.mp)+"/"+std::to_string(a.params[1]),tx,y+101,2);
    }
    void overlay(const GameSession& s) {
        const int w=s.screen_width(),h=s.screen_height();
        const auto scene=s.scene();
        if(scene==GameScene::Title) {
            fill({0,0,w,h},0,0,0);
            for(int layer=1;layer<=2;++layer)if(!s.title_image(layer).empty()) {
                const auto& picture=image(s.project(),layer==1?"titles1":"titles2",s.title_image(layer));
                blit(picture,{0,0,picture.w,picture.h},{0,0,w,h});
            }
            if(s.draw_title())text(s.game_title(),std::max(20,(w-static_cast<int>(s.game_title().size())*32)/2),h/4,4,true,25);
            command_window(s,{(w-280)/2,h-230,280,144},3);
        } else if(scene!=GameScene::Map) {
            fill({0,0,w,h},0,0,0,140);
            if(scene==GameScene::Menu) {
                command_window(s,{0,0,240,static_cast<int>(s.scene_commands().size())*36+36});
                window(s,{0,h-72,240,72});
                const auto* currency=s.system().find("currencyUnit");
                text(std::to_string(s.gold())+(currency?currency->get_string():" G"),24,h-48,2,true,12);
                window(s,{240,0,w-240,h});
                for(std::size_t i=0;i<std::min<std::size_t>(4,s.actors().size());++i)actor(s,s.actors()[i],260,18+static_cast<int>(i)*144,w-280);
            } else if(scene==GameScene::Status) {
                window(s,{0,0,w,h});
                if(!s.actors().empty()) {
                    const auto& a=s.actors()[std::min(s.selection(),s.actors().size()-1)];actor(s,a,24,24,w-48,144);
                    text(a.nickname,24,200,3,true,28);
                    for(std::size_t i=2;i<a.params.size();++i)text(s.term("params",i,"Param")+"   "+std::to_string(a.params[i]),48,250+static_cast<int>(i-2)*42,3,true,28);
                }
            } else if(scene==GameScene::Formation || scene==GameScene::ItemTarget || scene==GameScene::SkillTarget || scene==GameScene::SkillActors || scene==GameScene::EquipActors) {
                window(s,{0,0,w,h});
                for(std::size_t i=0;i<std::min<std::size_t>(4,s.actors().size());++i) {
                    const int y=18+static_cast<int>(i)*144;
                    if(i==s.selection())fill({14,y-4,w-28,140},110,150,230,90);
                    actor(s,s.actors()[i],24,y,w-48);
                }
            } else if(scene==GameScene::GameEnd)command_window(s,{(w-320)/2,(h-144)/2,320,144},3);
            else {
                command_window(s,{0,72,w,h-72});
                window(s,{0,0,w,72});
                std::string label;
                if(scene==GameScene::Save||scene==GameScene::Load) {
                    const auto* terms=s.system().find("terms");const auto* messages=terms?terms->find("messages"):nullptr;
                    const auto* value=messages?messages->find(scene==GameScene::Save?"saveMessage":"loadMessage"):nullptr;
                    label=value?value->get_string():"File";
                }else label=scene==GameScene::Options?s.term("commands",11,"Options"):scene==GameScene::Skills?s.term("commands",5,"Skill"):(scene==GameScene::EquipItems||scene==GameScene::EquipSlots)?s.term("commands",6,"Equip"):s.term("commands",4,"Item");
                text(label,24,24,2,true,(w-48)/16);
            }
            if(!s.notice().empty()){window(s,{12,h-120,w-24,108});text(s.notice(),32,h-96,2,true,(w-64)/16);}
        } else {
            if(!s.message().empty()) {
                const int mh=180,my=s.message_position()==0?0:s.message_position()==1?(h-mh)/2:h-mh;
                if(s.message_background()==0)window(s,{0,my,w,mh});else if(s.message_background()==1)fill({0,my,w,mh},0,0,0,180);
                face(s,s.message_face(),s.message_face_index(),20,my+18,144);
                text(s.message(),s.message_face().empty()?24:180,my+24,2,true,1000);
            }
            if(s.choosing()) {
                const int cw=360,ch=36+static_cast<int>(s.choices().size())*36;
                window(s,{w-cw,std::max(0,h-180-ch),cw,ch});
                for(std::size_t i=0;i<s.choices().size();++i) {
                    const int y=std::max(0,h-180-ch)+18+static_cast<int>(i)*36;
                    if(static_cast<int>(i)==s.choice_index())fill({w-cw+12,y-3,cw-24,32},110,150,230,100);
                    text(s.choices()[i],w-cw+24,y,2,true,19);
                }
            }
        }
    }

    const Image& image(const fs::path& project, const std::string& category, const std::string& name) {
        const auto file = fs::u8path(name);
        if (name.empty() || file != file.filename() || name == "." || name == "..")
            throw std::runtime_error("Invalid image name: " + name);
        const auto path = project / "img" / category / fs::u8path(file.u8string()+".png");
        const auto found = images.find(path);
        if (found != images.end()) return found->second;
        std::error_code ec;
        const auto size = fs::file_size(path,ec);
        if (ec || size > 32*1024*1024) throw std::runtime_error("Missing or oversized PNG: " + path.u8string());
        std::ifstream input(path,std::ios::binary);
        std::vector<unsigned char> encoded(static_cast<std::size_t>(size));
        if (!input.read(reinterpret_cast<char*>(encoded.data()), static_cast<std::streamsize>(size)))
            throw std::runtime_error("Cannot read PNG: " + path.u8string());
        int w=0,h=0,channels=0;
        if (!stbi_info_from_memory(encoded.data(),static_cast<int>(encoded.size()),&w,&h,&channels) ||
            w <= 0 || h <= 0 || static_cast<std::size_t>(w)*h*4 > 64*1024*1024)
            throw std::runtime_error("Invalid PNG or decoded image exceeds 64 MiB");
        const auto decoded_size = static_cast<std::size_t>(w)*h*4;
        if (bytes + decoded_size > 96*1024*1024) throw std::runtime_error("Texture cache exceeds 96 MiB budget");
        auto* pixels = stbi_load_from_memory(encoded.data(),static_cast<int>(encoded.size()),&w,&h,&channels,4);
        if (!pixels) throw std::runtime_error("PNG decode failed: " + path.u8string());
        std::unique_ptr<unsigned char,decltype(&stbi_image_free)> data(pixels,stbi_image_free);
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels,w,h,32,w*4,SDL_PIXELFORMAT_RGBA32);
        if (!surface) throw std::runtime_error(SDL_GetError());
        Texture texture(SDL_CreateTextureFromSurface(renderer,surface),SDL_DestroyTexture);
        SDL_FreeSurface(surface);
        if (!texture) throw std::runtime_error(SDL_GetError());
        SDL_SetTextureBlendMode(texture.get(),SDL_BLENDMODE_BLEND);
        bytes += decoded_size;
        return images.emplace(path,Image{std::move(texture),w,h}).first->second;
    }

    void blit(const Image& image, SDL_Rect source, SDL_Rect target) {
        if (source.x < 0 || source.y < 0 || source.x+source.w > image.w || source.y+source.h > image.h)
            throw std::runtime_error("Sprite or tile source exceeds PNG dimensions");
        if (SDL_RenderCopy(renderer,image.texture.get(),&source,&target) != 0) throw std::runtime_error(SDL_GetError());
    }

    void tile(const GameSession& session,int id,int x,int y,unsigned frame) {
        for (const auto& patch : tile_patches(id,frame,(session.tileset().flag(id)&0x80) != 0)) {
            const auto& name = session.tileset().images.at(static_cast<std::size_t>(patch.image));
            if (name.empty()) throw std::runtime_error("Missing tileset image for tile " + std::to_string(id));
            blit(image(session.project(),"tilesets",name), {patch.sx,patch.sy,patch.w,patch.h},
                 {x+patch.dx,y+patch.dy,patch.w,patch.h});
        }
    }

    void character(const GameSession& session,const CharacterImage& sprite,double x,double y,
                   double camera_x,double camera_y,unsigned frame,bool walking,int direction) {
        const int screen_x = static_cast<int>(std::round(x*48-camera_x));
        const int screen_y = static_cast<int>(std::round(y*48-camera_y));
        if (sprite.tile_id > 0) { tile(session,sprite.tile_id,screen_x,screen_y,frame); return; }
        if (sprite.name.empty()) return;
        const auto& sheet = image(session.project(),"characters",sprite.name);
        const auto prefix = sprite.name.substr(0,sprite.name.find_first_not_of("!$"));
        const bool single = prefix.find('$') != std::string::npos;
        const int cols = single ? 3 : 12, rows = single ? 4 : 8;
        if (sheet.w%cols || sheet.h%rows) throw std::runtime_error("Invalid MV character sheet dimensions");
        const int cw = sheet.w/cols, ch = sheet.h/rows;
        const int index = single ? 0 : sprite.index;
        if (index < 0 || index > 7) throw std::runtime_error("Invalid character index");
        const int patterns[] = {0,1,2,1};
        const int pattern = walking ? patterns[(frame/8)%4] : std::clamp(sprite.pattern,0,2);
        const int facing = direction == 2 ? 0 : direction == 4 ? 1 : direction == 6 ? 2 : 3;
        const int shift = prefix.find('!') == std::string::npos ? 6 : 0;
        blit(sheet,{(index%4*3+pattern)*cw,(index/4*4+facing)*ch,cw,ch},
             {screen_x+24-cw/2,screen_y+48-ch-shift,cw,ch});
    }
};

MapView::MapView(SDL_Renderer* renderer) : impl_(std::make_unique<Impl>(renderer)) {}
MapView::~MapView() = default;

bool MapView::render(const GameSession& session,unsigned milliseconds,std::string& error) {
    error.clear();
    try {
        auto& impl = *impl_;
        if (impl.current_project != session.project() || impl.current_map != session.map_id()) {
            impl.images.clear(); impl.bytes = 0; impl.current_project = session.project(); impl.current_map = session.map_id();
        }
        if(session.scene()==GameScene::Title){impl.overlay(session);return true;}
        SDL_SetRenderDrawColor(impl.renderer,10,14,20,255); SDL_RenderClear(impl.renderer);
        const auto& map = session.map();
        auto camera = [](double real,int length,int viewport,bool loop) {
            const double wanted = (real+0.5)*48-viewport/2.0;
            if (loop) return wanted;
            if (length*48 < viewport) return (length*48-viewport)/2.0;
            return std::clamp(wanted,0.0,static_cast<double>(length*48-viewport));
        };
        const int vw=session.screen_width(),vh=session.screen_height();
        const double cx = camera(session.real_x(),map.width,vw,map.loops_x());
        const double cy = camera(session.real_y(),map.height,vh,map.loops_y());
        const int first_x = static_cast<int>(std::floor(cx/48)), first_y = static_cast<int>(std::floor(cy/48));
        const unsigned animation = milliseconds/500;
        auto draw_tiles = [&](bool above) {
            for (int y = first_y; y <= first_y+vh/48+1; ++y)
                for (int x = first_x; x <= first_x+vw/48+1; ++x) {
                    if ((!map.loops_x() && (x<0 || x>=map.width)) || (!map.loops_y() && (y<0 || y>=map.height))) continue;
                    const int px = static_cast<int>(std::round(x*48-cx)), py = static_cast<int>(std::round(y*48-cy));
                    for (int z = 0; z < 4; ++z) {
                        const int id = map.cell(x,y,z);
                        if (((session.tileset().flag(id)&0x10) != 0) == above) impl.tile(session,id,px,py,animation);
                    }
                    if (!above) {
                        const auto shadow = map.cell(x,y,4);
                        SDL_SetRenderDrawBlendMode(impl.renderer,SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(impl.renderer,0,0,0,128);
                        for (int i = 0; i < 4; ++i) if (shadow & (1<<i)) {
                            SDL_Rect area{px+(i%2)*24,py+(i/2)*24,24,24}; SDL_RenderFillRect(impl.renderer,&area);
                        }
                        SDL_SetRenderDrawBlendMode(impl.renderer,SDL_BLENDMODE_NONE);
                    }
                }
        };
        draw_tiles(false);
        struct Sprite { CharacterImage image; double x,y; bool moving; int direction,priority; };
        std::vector<Sprite> sprites;
        for (const auto& event : map.events) if (const auto* page = session.active_page(event)) {
            auto image = session.event_image(*page);
            const auto* priority = page->settings.find("priorityType");
            sprites.push_back({image,static_cast<double>(event.x),static_cast<double>(event.y),false,image.direction,
                               priority ? priority->get_int(1) : 1});
        }
        if(session.followers_visible())for(auto it=session.followers().rbegin();it!=session.followers().rend();++it) {
            const auto& f=*it;const auto p=session.follower_progress();
            sprites.push_back({f.image,f.from_x+(f.to_x-f.from_x)*p,f.from_y+(f.to_y-f.from_y)*p,session.moving()&&(f.from_x!=f.to_x||f.from_y!=f.to_y),f.direction,1});
        }
        sprites.push_back({session.player_image(),session.real_x(),session.real_y(),session.moving(),session.direction(),1});
        std::stable_sort(sprites.begin(),sprites.end(),[](const Sprite& a,const Sprite& b) {
            return a.priority == b.priority ? a.y < b.y : a.priority < b.priority;
        });
        auto draw_sprites = [&](bool above) {
            for (const auto& sprite : sprites) {
                if ((sprite.priority == 2) != above) continue;
                // Repeat only on wrapped map axes; normalize around the camera.
                double x=sprite.x,y=sprite.y;
                if (map.loops_x()) x += std::round((cx/48 + 6.0-x)/map.width)*map.width;
                if (map.loops_y()) y += std::round((cy/48 + 4.5-y)/map.height)*map.height;
                const int rx = map.loops_x() ? 1+vw/(map.width*48) : 0;
                const int ry = map.loops_y() ? 1+vh/(map.height*48) : 0;
                for (int yy=-ry; yy<=ry; ++yy) for (int xx=-rx; xx<=rx; ++xx)
                    impl.character(session,sprite.image,x+xx*map.width,y+yy*map.height,cx,cy,milliseconds/16,
                                   sprite.moving,sprite.direction);
            }
        };
        draw_sprites(false); draw_tiles(true); draw_sprites(true);
        impl.overlay(session);
        return true;
    } catch (const std::exception& e) { error=e.what(); return false; }
}
} // namespace rpg2lx
