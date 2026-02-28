#include "../../src/async/awaiter.h"
#include "eventide/zest/zest.h"

namespace eventide {

TEST_SUITE(async_internal) {

TEST_CASE(latest_error) {
    uv::latest_value_delivery<int> delivery;

    delivery.deliver(error::io_error);

    EXPECT_TRUE(delivery.has_pending());

    auto result = delivery.take_pending();
    EXPECT_FALSE(result.has_value());
    if(!result.has_value()) {
        EXPECT_EQ(result.error().value(), error::io_error.value());
    }
}

};  // TEST_SUITE(async_internal)

}  // namespace eventide
