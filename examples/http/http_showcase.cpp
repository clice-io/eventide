#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "kota/http/http.h"
#include "kota/async/async.h"

using namespace std::chrono_literals;
using namespace kota;

namespace {

void print_error(std::string_view label, const http::error& err) {
    std::cout << label << ": " << http::message(err) << "\n";
}

http::client build_demo_client() {
    auto built = http::client::builder()
                     .default_header("accept", "application/json")
                     .default_header("x-demo-client", "eventide")
                     .default_cookie("demo=1; theme=light")
                     .user_agent("eventide-http-showcase/1.0")
                     .timeout(10s)
                     .cookie_store(true)
                     .redirect(http::redirect_policy::limited(5))
                     .referer(true)
                     .https_only(false)
                     .danger_accept_invalid_certs(false)
                     .danger_accept_invalid_hostnames(false)
                     .min_tls_version(http::tls_version::tls1_2)
                     .max_tls_version(http::tls_version::tls1_3)
                     .ca_file("/etc/ssl/certs/ca-certificates.crt")
                     .build();

    if(!built) {
        std::cerr << "failed to build client: " << http::message(built.error()) << "\n";
        std::exit(1);
    }

    return std::move(*built);
}

void showcase_request_builders(http::client& client) {
    auto get_req = client.get("https://example.com")
                       .query("lang", "en")
                       .header("accept-language", "en-US")
                       .cookie("session=manual; preview=true")
                       .timeout(2s)
                       .build();

    auto post_json = client.post("https://api.example.com/items")
                         .bearer_auth("demo-token")
                         .json_text(R"({"name":"eventide","kind":"demo"})")
                         .build();

    auto post_form = client.post("https://api.example.com/form")
                         .basic_auth("demo", "secret")
                         .form({
                             {"name", "eventide"},
                             {"kind", "form"    }
    })
                         .build();

    auto put_body = client.put("https://api.example.com/blob")
                        .header("content-type", "text/plain")
                        .body("plain request body")
                        .build();

    auto patch_req = client.patch("https://api.example.com/items/42")
                         .header("content-type", "application/merge-patch+json")
                         .body(R"({"enabled":true})")
                         .build();

    auto delete_req =
        client.del("https://api.example.com/items/42").no_proxy().no_cookies().build();

    auto head_req = client.head("https://example.com").build();
}

task<> run_showcase(event_loop& loop) {
    auto client = build_demo_client();
    showcase_request_builders(client);
    auto api = client.on(loop);

    // Cookie store management goes straight to libcurl's cookie engine.
    client.store_cookie("https://example.com",
                        "Set-Cookie: stored_demo=1; Domain=example.com; Path=/; Secure");

    auto cookies = client.cookie_list();
    std::cout << "cookie store entries: " << cookies.size() << "\n";
    for(const auto& line: cookies) {
        std::cout << "  " << line << "\n";
    }

    auto home = co_await api.get("https://example.com")
                    .header("accept", "text/html")
                    .query("from", "showcase")
                    .send()
                    .catch_cancel();

    if(home.is_cancelled()) {
        std::cout << "home request cancelled\n";
        co_return;
    }
    if(home.has_error()) {
        print_error("home request failed", home.error());
        co_return;
    }

    std::cout << "GET " << home->url << " -> " << home->status << "\n";
    std::cout << "body bytes: " << home->bytes().size() << "\n";

    auto built_head = client.head("https://example.com").header("accept", "*/*").build();

    auto head = co_await api.execute(std::move(built_head)).catch_cancel();
    if(head.is_cancelled()) {
        std::cout << "head request cancelled\n";
        co_return;
    }
    if(head.has_error()) {
        print_error("head request failed", head.error());
        co_return;
    }

    std::cout << "HEAD " << head->url << " -> " << head->status << "\n";

    client.clear_cookies();
    std::cout << "cookie store entries after clear: " << client.cookie_list().size() << "\n";
}

}  // namespace

int main() {
    event_loop loop;

    auto root = run_showcase(loop);
    loop.schedule(root);
    loop.run();

    http::manager::unregister_loop(loop);
    return 0;
}
