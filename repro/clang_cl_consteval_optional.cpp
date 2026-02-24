#include <cstddef>
#include <optional>
#include <string>

template <typename T>
struct DecoOption {
    std::optional<T> value = std::nullopt;
};

struct Url {
    std::string url;
};

struct Request {
    DecoOption<Url> url;
};

struct WebCliOpt {
    Request request;
};

struct BuildStats {
    std::size_t optCount;
    std::size_t strPoolBytes;
};

template <typename RootTy, bool counting, std::size_t OptCount = 0, std::size_t StrPoolBytes = 0>
struct OptBuilder {
    consteval void build() {
        // This construction/destruction is the key trigger in constant evaluation.
        auto object = RootTy{};
        (void)object;
    }

    consteval BuildStats finish() const {
        return BuildStats{1, 1};
    }
};

template <typename OptDeco>
consteval BuildStats build_stats() {
    OptBuilder<OptDeco, true> counter;
    counter.build();
    return counter.finish();
}

template <typename OptDeco>
struct BuildStorage {
    inline static constexpr BuildStats stats = build_stats<OptDeco>();
    using builder_t = OptBuilder<OptDeco, false, stats.optCount, stats.strPoolBytes>;
    inline static constexpr builder_t value{};
};

template <typename OptDeco>
constexpr const auto& build_storage() {
    return BuildStorage<OptDeco>::value;
}

constexpr const auto& g_storage = build_storage<WebCliOpt>();

int main() {
    (void)g_storage;
    return 0;
}
