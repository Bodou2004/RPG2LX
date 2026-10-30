#include "rpg2lx/json/json_parser.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        for (const std::string bad : {"", "hello", "{]", "[1,]", "{\"x\":1,}",
             "01", "1.", "1e", "1e+", "-", "1e999", "true false",
             "\"\\q\"", "\"\\uGGGG\"", "\"\\uD800\"", "\"\\uDC00\"",
             "\"a\nb\"", "{\"x\":0,\"x\":1}"}) {
            std::string error;
            rpg2lx::JsonParser::parse(bad, error);
            if (error.empty()) throw std::runtime_error("Invalid JSON accepted: " + bad);
        }
        std::string error = "stale error";
        auto value = rpg2lx::JsonParser::parse(R"({"text":"\u00f1\uD83D\uDE00","number":-1.25e2,"values":[true,false,null]})", error);
        if (!error.empty() || value.find("text")->str_val != "\xC3\xB1\xF0\x9F\x98\x80" ||
            value.find("number")->num_val != -125)
            throw std::runtime_error("Valid JSON / Unicode / number decoding failed: " + error);
        rpg2lx::JsonParser::parse(std::string(129, '[') + "0" + std::string(129, ']'), error);
        if (error.empty()) throw std::runtime_error("Depth budget not enforced");
        rpg2lx::JsonParser::parse(std::string("\"") + char(0xC0) + char(0xAF) + "\"", error);
        if (error.empty()) throw std::runtime_error("Overlong UTF-8 accepted");
        rpg2lx::JsonParser::parse("\xEF\xBB\xBF{}", error);
        if (!error.empty()) throw std::runtime_error("UTF-8 BOM rejected");
        std::cout << "JSON tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
