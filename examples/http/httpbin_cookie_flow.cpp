#include <iostream>
#include <string>

#include "kota/async/async.h"
#include "kota/http/http.h"

using namespace kota;

namespace {

void print_error(std::string_view label, const http::error& err) {
    std::cout << label << ": " << http::message(err) << "\n";
}

void print_cookie_jar(const http::client& client) {
    auto cookies = client.cookie_list();
    std::cout << "cookie jar entries: " << cookies.size() << "\n";
    for(const auto& line: cookies) {
        std::cout << "  " << line << "\n";
    }
}

task<> run_demo(event_loop& loop) {
    http::client client(loop);

    std::cout << "1) start with an empty jar\n";
    print_cookie_jar(client);

    std::cout << "\n2) let httpbin set two cookies into the jar\n";
    auto seeded = co_await client.get("https://httpbin.io/cookies/set?session=jar-demo&theme=light")
                          .send()
                          .catch_cancel();
    if(seeded.is_cancelled()) {
        std::cout << "seed request cancelled\n";
        co_return;
    }
    if(seeded.has_error()) {
        print_error("seed request failed", seeded.error());
        co_return;
    }

    std::cout << "seed response body: " << seeded->text() << "\n";
    print_cookie_jar(client);

    std::cout << "\n3) inject one more cookie locally with store_cookie(...)\n";
    client.store_cookie("https://httpbin.io/cookies",
                        "Set-Cookie: local_pref=from-store-cookie; Domain=httpbin.io; Path=/; Secure");
    print_cookie_jar(client);

    std::cout << "\n4) ask httpbin which cookies it sees from the jar\n";
    auto jar_echo = co_await client.get("https://httpbin.io/cookies").send().catch_cancel();
    if(jar_echo.is_cancelled()) {
        std::cout << "jar echo request cancelled\n";
        co_return;
    }
    if(jar_echo.has_error()) {
        print_error("jar echo request failed", jar_echo.error());
        co_return;
    }

    std::cout << "jar-backed /cookies response: " << jar_echo->text() << "\n";

    std::cout << "\n5) override cookies for a single request and bypass the jar\n";
    auto manual = co_await client.get("https://httpbin.io/cookies")
                          .cookie("session=manual-demo; theme=manual-only")
                          .no_cookies()
                          .send()
                          .catch_cancel();
    if(manual.is_cancelled()) {
        std::cout << "manual request cancelled\n";
        co_return;
    }
    if(manual.has_error()) {
        print_error("manual request failed", manual.error());
        co_return;
    }

    std::cout << "manual-only /cookies response: " << manual->text() << "\n";

    std::cout << "\n6) clear the jar and verify the remote side sees nothing\n";
    client.clear_cookies();
    print_cookie_jar(client);

    auto cleared = co_await client.get("https://httpbin.io/cookies").send().catch_cancel();
    if(cleared.is_cancelled()) {
        std::cout << "clear verification request cancelled\n";
        co_return;
    }
    if(cleared.has_error()) {
        print_error("clear verification request failed", cleared.error());
        co_return;
    }

    std::cout << "after clear /cookies response: " << cleared->text() << "\n";
}

}  // namespace

int main() {
    event_loop loop;
    auto root = run_demo(loop);
    loop.schedule(root);
    loop.run();
    http::manager::unregister_loop(loop);
    return 0;
}
