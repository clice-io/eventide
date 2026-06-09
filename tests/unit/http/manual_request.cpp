#include "kota/http/http.h"
#include "kota/zest/macro.h"
#include "kota/async/io/loop.h"

TEST_SUITE(http_manual_request) {

TEST_SUITE_ATTRS(skip = true);

TEST_CASE(get_request) {
    using namespace kota;
    event_loop loop;
    http::client client;
    auto request = client.on(loop).get("https://github.com").send();
    loop.schedule(request);
    loop.run();
    auto result = request.result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, 200);
};

};  // TEST_SUITE(http_manual_request)
