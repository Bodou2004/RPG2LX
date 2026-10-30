#include "rpg2lx/project_source.hpp"
#include <fstream>
#include <regex>

namespace fs = std::filesystem;
namespace rpg2lx {
namespace {
bool file(const fs::path& path) { std::error_code ec; return fs::is_regular_file(path,ec); }
}
ProjectSourceResult resolve_mv_project(const fs::path& input) {
    try {
        std::error_code ec;
        auto root=fs::canonical(input,ec);
        if(ec)return {std::nullopt,"Cannot access project: "+input.u8string()+": "+ec.message()};
        if(file(root)) {
            const auto ext=root.extension().u8string();
            if(ext!=".html" && ext!=".exe" && ext!=".rpgproject")
                return {std::nullopt,"Select the MV project folder, index.html, Game.exe or .rpgproject file"};
            root=root.parent_path();
        }
        if(!fs::is_directory(root,ec))return {std::nullopt,"Project path must be a directory"};
        if(!file(root/"data/System.json") && file(root/"www/data/System.json"))root/="www";
        if(!file(root/"data/System.json"))
            return {std::nullopt,"No data/System.json found in the selected folder or its www/ directory"};
        const auto core=root/"js/rpg_core.js";
        if(file(root/"js/rmmz_core.js") && !file(core))
            return {std::nullopt,"RPG Maker MZ export detected; this runtime targets RPG Maker MV"};
        ProjectSource source{root,{},file(core)};
        if(source.mv_script_found) {
            std::ifstream stream(core,std::ios::binary);
            if(!stream)return {std::nullopt,"Cannot read js/rpg_core.js"};
            // Engine identity lives near the beginning of the generated core.
            std::string prefix(128*1024,'\0');stream.read(prefix.data(),static_cast<std::streamsize>(prefix.size()));
            prefix.resize(static_cast<std::size_t>(stream.gcount()));
            if(stream.bad())return {std::nullopt,"Read error in js/rpg_core.js"};
            std::smatch match;
            if(std::regex_search(prefix,match,std::regex(R"(Utils\.RPGMAKER_NAME\s*=\s*['"]([^'"]+)['"])")) && match[1]!="MV")
                return {std::nullopt,"Core identifies a different RPG Maker engine: "+match[1].str()};
            if(std::regex_search(prefix,match,std::regex(R"(Utils\.RPGMAKER_VERSION\s*=\s*['"]([^'"]+)['"])")))
                source.engine_version=match[1].str();
        }
        return {std::move(source),{}};
    }catch(const std::exception& e){return {std::nullopt,e.what()};}
}
} // namespace rpg2lx
