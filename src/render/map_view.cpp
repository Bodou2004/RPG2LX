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

    const Image& image(const fs::path& project, const std::string& category, const std::string& name) {
        const auto file = fs::u8path(name);
        if (name.empty() || file != file.filename() || name == "." || name == "..")
            throw std::runtime_error("Invalid image name: " + name);
        const auto path = project / "img" / category / (file.u8string()+".png");
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
        SDL_SetRenderDrawColor(impl.renderer,10,14,20,255); SDL_RenderClear(impl.renderer);
        const auto& map = session.map();
        auto camera = [](double real,int length,int viewport,bool loop) {
            const double wanted = (real+0.5)*48-viewport/2.0;
            if (loop) return wanted;
            if (length*48 < viewport) return (length*48-viewport)/2.0;
            return std::clamp(wanted,0.0,static_cast<double>(length*48-viewport));
        };
        const double cx = camera(session.real_x(),map.width,640,map.loops_x());
        const double cy = camera(session.real_y(),map.height,480,map.loops_y());
        const int first_x = static_cast<int>(std::floor(cx/48)), first_y = static_cast<int>(std::floor(cy/48));
        const unsigned animation = milliseconds/500;
        auto draw_tiles = [&](bool above) {
            for (int y = first_y; y <= first_y+11; ++y)
                for (int x = first_x; x <= first_x+14; ++x) {
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
                const int rx = map.loops_x() ? 1+640/(map.width*48) : 0;
                const int ry = map.loops_y() ? 1+480/(map.height*48) : 0;
                for (int yy=-ry; yy<=ry; ++yy) for (int xx=-rx; xx<=rx; ++xx)
                    impl.character(session,sprite.image,x+xx*map.width,y+yy*map.height,cx,cy,milliseconds/16,
                                   sprite.moving,sprite.direction);
            }
        };
        draw_sprites(false); draw_tiles(true); draw_sprites(true);
        return true;
    } catch (const std::exception& e) { error=e.what(); return false; }
}
} // namespace rpg2lx
