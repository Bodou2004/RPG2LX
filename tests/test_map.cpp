#include "rpg2lx/map.hpp"
#include <iostream>
#include <string>
#include <stdexcept>

static void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

static std::string sample(int scroll = 0) {
    return "{\"width\":2,\"height\":1,\"tilesetId\":1,\"scrollType\":" +
        std::to_string(scroll) + R"(,"displayName":"Test","data":[1,2,3,4,5,6,7,8,1,15,42,255],
        "events":[null,{"id":1,"name":"Door","x":1,"y":0,"pages":[
        {"trigger":0,"conditions":{},"list":[{"code":101,"indent":0,"parameters":["Hello",0]}]}]}]})";
}

int main(int argc, char** argv) {
    try {
        auto result = rpg2lx::MapLoader::parse(sample());
        require(bool(result), result.error);
        const auto& map = *result.map;
        require(map.cell(0,0,0) == 1 && map.cell(1,0,3) == 8, "Layer indexing");
        require(map.cell(1,0,4) == 15 && map.region(0,0) == 42, "Shadows and regions");
        require(map.cell(-1,0,0) == 0 && map.cell(2,0,0) == 0 && map.cell(0,0,6) == 0,
                "Out of bounds must return zero");
        require(map.events.size() == 1 && map.events[0].pages[0].commands[0].code == 101,
                "Event command loading");
        require(map.events[0].pages[0].commands[0].parameters[0].str_val == "Hello",
                "Command parameters preserved");
        require(map.events[0].pages[0].settings.find("conditions") != nullptr,
                "Page settings preserved");
        auto loop = rpg2lx::MapLoader::parse(sample(3));
        require(bool(loop), loop.error);
        require(loop.map->cell(-1,-4,0) == 2 && loop.map->cell(2,5,0) == 1, "Loop wrapping");
        auto vertical = rpg2lx::MapLoader::parse(sample(1));
        require(vertical.map->cell(-1,0,0) == 0 && vertical.map->cell(1,9,0) == 2,
                "Vertical-only loop");

        for (const auto& change : {
            std::pair<std::string,std::string>{"\"width\":2", "\"width\":0"},
            {"\"width\":2", "\"width\":2.5"},
            {"\"height\":1", "\"height\":10000"},
            {"42,255", "42"}, {"42,255", "42,256"},
            {"1,15,42", "1,16,42"}, {"\"id\":1", "\"id\":2"},
            {"\"x\":1", "\"x\":2"}, {"\"trigger\":0", "\"trigger\":5"},
            {"\"parameters\":[\"Hello\",0]", "\"parameters\":false"}}) {
            auto input = sample();
            const auto pos = input.find(change.first);
            require(pos != std::string::npos, "Bad test mutation");
            input.replace(pos, change.first.size(), change.second);
            auto bad = rpg2lx::MapLoader::parse(input);
            require(!bad && !bad.error.empty(), "Invalid map accepted: " + change.first);
        }
        require(!rpg2lx::MapLoader::load("does-not-exist-rpg2lx.json"), "Missing file accepted");
        if (argc > 1) {
            auto fixture = rpg2lx::MapLoader::load(argv[1]);
            require(bool(fixture), fixture.error);
        }
        std::cout << "Map tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
