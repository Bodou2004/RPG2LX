#include "rpg2lx/core.hpp"
#include <iostream>
#include <string>
#include <cstdlib>

static int g_failed_tests = 0;

#define TEST_EXPECT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] (" << __FILE__ << ":" << __LINE__ << "): " << msg << std::endl; \
            g_failed_tests++; \
        } \
    } while(0)

void test_valid_project(const std::string& fixtures_dir) {
    std::cout << "[TEST] Running test_valid_project on: " << fixtures_dir << "/valid_project ...\n";
    auto meta = rpg2lx::ProjectInspector::inspect(fixtures_dir + "/valid_project");
    TEST_EXPECT(meta.is_valid, "Project should be valid");
    TEST_EXPECT(meta.game_title == "Test Adventure", "Game title mismatch");
    TEST_EXPECT(meta.start_map_id == "1", "Start map ID mismatch");
    TEST_EXPECT(meta.screen_width == 816, "Screen width mismatch");
    TEST_EXPECT(meta.screen_height == 624, "Screen height mismatch");
    TEST_EXPECT(meta.plugins.size() == 2, "Plugin count mismatch");
    if (meta.plugins.size() >= 2) {
        TEST_EXPECT(meta.plugins[0].name == "Community_Basic", "First plugin name mismatch");
        TEST_EXPECT(meta.plugins[0].status == true, "First plugin status mismatch");
        TEST_EXPECT(meta.plugins[0].parameters.find("cacheLimit") != meta.plugins[0].parameters.end(), "cacheLimit param missing");
        TEST_EXPECT(meta.plugins[1].name == "MadeWithMv", "Second plugin name mismatch");
    }
    TEST_EXPECT(meta.actor_count == 1, "Actor count should be 1");
    TEST_EXPECT(meta.map_count == 1, "Map count should be 1");
    TEST_EXPECT(meta.common_event_count == 1, "Common event count should be 1");
    TEST_EXPECT(meta.missing_files.empty(), "Should have no missing files");
    TEST_EXPECT(meta.warnings.empty(), "Should have no warnings");
}

void test_incomplete_project(const std::string& fixtures_dir) {
    std::cout << "[TEST] Running test_incomplete_project on: " << fixtures_dir << "/incomplete_project ...\n";
    auto meta = rpg2lx::ProjectInspector::inspect(fixtures_dir + "/incomplete_project");
    TEST_EXPECT(!meta.is_valid, "Project should be invalid");
    TEST_EXPECT(!meta.missing_files.empty(), "Should report missing files");
}

void test_invalid_json_project(const std::string& fixtures_dir) {
    std::cout << "[TEST] Running test_invalid_json_project on: " << fixtures_dir << "/invalid_json_project ...\n";
    auto meta = rpg2lx::ProjectInspector::inspect(fixtures_dir + "/invalid_json_project");
    TEST_EXPECT(!meta.is_valid, "Project should be invalid");
    TEST_EXPECT(!meta.warnings.empty(), "Should have syntax error warnings");
}

void test_invalid_json_balanced(const std::string& fixtures_dir) {
    std::cout << "[TEST] Running test_invalid_json_balanced on: " << fixtures_dir << "/invalid_json_balanced ...\n";
    auto meta = rpg2lx::ProjectInspector::inspect(fixtures_dir + "/invalid_json_balanced");
    TEST_EXPECT(!meta.is_valid, "Project with balanced invalid JSON should be invalid");
    TEST_EXPECT(!meta.warnings.empty(), "Balanced invalid JSON must trigger syntax error warnings");
}

void test_type_mismatch_project(const std::string& fixtures_dir) {
    std::cout << "[TEST] Running test_type_mismatch_project on: " << fixtures_dir << "/type_mismatch_project ...\n";
    auto meta = rpg2lx::ProjectInspector::inspect(fixtures_dir + "/type_mismatch_project");
    TEST_EXPECT(!meta.is_valid, "Project with type mismatches should be invalid");
    TEST_EXPECT(!meta.warnings.empty(), "Type mismatches must produce warnings");
}

void test_path_with_spaces(const std::string& fixtures_dir) {
    std::cout << "[TEST] Running test_path_with_spaces on: " << fixtures_dir << "/path with spaces ...\n";
    auto meta = rpg2lx::ProjectInspector::inspect(fixtures_dir + "/path with spaces");
    TEST_EXPECT(meta.is_valid, "Project inside path with spaces should be valid");
    TEST_EXPECT(meta.game_title == "Test Adventure", "Game title mismatch on path with spaces");
}

int main(int argc, char* argv[]) {
    std::cout << "Starting RPG2LX Core C++ Unit Tests...\n";

    std::string fixtures_dir = "./fixtures";
    if (argc > 1) {
        fixtures_dir = argv[1];
    }

    test_valid_project(fixtures_dir);
    test_incomplete_project(fixtures_dir);
    test_invalid_json_project(fixtures_dir);
    test_invalid_json_balanced(fixtures_dir);
    test_type_mismatch_project(fixtures_dir);
    test_path_with_spaces(fixtures_dir);

    if (g_failed_tests > 0) {
        std::cerr << "[ERROR] " << g_failed_tests << " test(s) failed!\n";
        return 1;
    }

    std::cout << "All RPG2LX Core C++ Unit Tests PASSED successfully.\n";
    return 0;
}
