#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "kota/zest/zest.h"
#include "kota/codec/json/json.h"

namespace kota::zest {

namespace {

std::string read_file(std::string_view path) {
    std::ifstream file(std::string(path), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), {});
}

TEST_SUITE(snapshot) {

TEST_CASE(basic_named) {
    ASSERT_SNAPSHOT("hello snapshot", "basic_named");
}

TEST_CASE(unnamed) {
    ASSERT_SNAPSHOT("auto-named snapshot content");
}

TEST_CASE(multiple_named) {
    ASSERT_SNAPSHOT("first value", "multi_first");
    ASSERT_SNAPSHOT("second value", "multi_second");
    ASSERT_SNAPSHOT("third value", "multi_third");
}

TEST_CASE(multiline) {
    std::string content = "line one\nline two\nline three";
    ASSERT_SNAPSHOT(content, "multiline");
}

TEST_CASE(empty_string) {
    ASSERT_SNAPSHOT("", "empty_string");
}

TEST_CASE(special_chars) {
    ASSERT_SNAPSHOT("tabs\there\nnewlines\nand \"quotes\"", "special_chars");
}

TEST_CASE(json_vector) {
    auto vec = std::vector<int>{1, 2, 3};
    ASSERT_SNAPSHOT_JSON(vec, "json_vector");
}

TEST_CASE(json_map) {
    auto m = std::map<std::string, int>{{"alpha", 1}, {"beta", 2}};
    ASSERT_SNAPSHOT_JSON(m, "json_map");
}

TEST_CASE(glob_fixtures) {
    ASSERT_SNAPSHOT_GLOB("fixtures/**/*.txt", read_file);
}

};  // TEST_SUITE(snapshot)

}  // namespace

}  // namespace kota::zest
