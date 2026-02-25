#include "eventide/zest/zest.h"
#include "eventide/language/uri.h"

namespace eventide::language {

TEST_SUITE(language_uri) {

TEST_CASE(parse_full_uri) {
    auto uri = URI::parse("https://example.com/a/b?x=1#frag");
    ASSERT_TRUE(uri.has_value());

    EXPECT_EQ(uri->scheme(), "https");
    EXPECT_TRUE(uri->has_authority());
    EXPECT_EQ(uri->authority(), "example.com");
    EXPECT_EQ(uri->path(), "/a/b");
    EXPECT_TRUE(uri->has_query());
    EXPECT_EQ(uri->query(), "x=1");
    EXPECT_TRUE(uri->has_fragment());
    EXPECT_EQ(uri->fragment(), "frag");
    EXPECT_EQ(uri->str(), "https://example.com/a/b?x=1#frag");
}

TEST_CASE(parse_uri_without_authority) {
    auto uri = URI::parse("mailto:user@example.com");
    ASSERT_TRUE(uri.has_value());

    EXPECT_EQ(uri->scheme(), "mailto");
    EXPECT_FALSE(uri->has_authority());
    EXPECT_EQ(uri->path(), "user@example.com");
    EXPECT_FALSE(uri->has_query());
    EXPECT_FALSE(uri->has_fragment());
}

TEST_CASE(parse_invalid_uri_rejected) {
    EXPECT_FALSE(URI::parse("noscheme").has_value());
    EXPECT_FALSE(URI::parse("1abc://example.com").has_value());
    EXPECT_FALSE(URI::parse("://example.com").has_value());
}

TEST_CASE(percent_encode_decode_roundtrip) {
    std::string_view raw = "a b/c?d";
    auto encoded = URI::percent_encode(raw, false);
    EXPECT_EQ(encoded, "a%20b/c%3Fd");

    auto decoded = URI::percent_decode(encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, raw);
}

TEST_CASE(percent_decode_invalid_input) {
    EXPECT_FALSE(URI::percent_decode("%").has_value());
    EXPECT_FALSE(URI::percent_decode("%1").has_value());
    EXPECT_FALSE(URI::percent_decode("%GG").has_value());
}

TEST_CASE(file_uri_from_path_and_back) {
    auto uri = URI::from_file_path("/tmp/a b.txt");
    ASSERT_TRUE(uri.has_value());

    EXPECT_TRUE(uri->is_file());
    EXPECT_EQ(uri->str(), "file:///tmp/a%20b.txt");

    auto path = uri->file_path();
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(*path, "/tmp/a b.txt");
}

TEST_CASE(file_uri_from_windows_drive_path_and_back) {
    auto uri = URI::from_file_path("C:\\work\\a b.txt");
    ASSERT_TRUE(uri.has_value());

    EXPECT_EQ(uri->str(), "file:///C:/work/a%20b.txt");

    auto path = uri->file_path();
    ASSERT_TRUE(path.has_value());

#if defined(_WIN32)
    EXPECT_EQ(*path, "C:/work/a b.txt");
#else
    EXPECT_EQ(*path, "/C:/work/a b.txt");
#endif
}

TEST_CASE(file_uri_from_unc_path_and_back) {
    auto uri = URI::from_file_path("\\\\server\\share\\a b.txt");
    ASSERT_TRUE(uri.has_value());

    EXPECT_EQ(uri->str(), "file://server/share/a%20b.txt");

    auto path = uri->file_path();
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(*path, "//server/share/a b.txt");
}

TEST_CASE(file_uri_rejects_relative_path) {
    EXPECT_FALSE(URI::from_file_path("relative/file.txt").has_value());
    EXPECT_FALSE(URI::from_file_path("C:relative.txt").has_value());
}

TEST_CASE(file_uri_authority_handling) {
    auto local = URI::parse("file://localhost/tmp/a.txt");
    ASSERT_TRUE(local.has_value());
    auto local_path = local->file_path();
    ASSERT_TRUE(local_path.has_value());
    EXPECT_EQ(*local_path, "/tmp/a.txt");

    auto remote = URI::parse("file://server/share/a.txt");
    ASSERT_TRUE(remote.has_value());
    auto remote_path = remote->file_path();
    ASSERT_TRUE(remote_path.has_value());
    EXPECT_EQ(*remote_path, "//server/share/a.txt");
}

TEST_CASE(file_path_on_non_file_uri_fails) {
    auto uri = URI::parse("https://example.com/a.txt");
    ASSERT_TRUE(uri.has_value());
    EXPECT_FALSE(uri->file_path().has_value());
}

};  // TEST_SUITE(language_uri)

}  // namespace eventide::language
