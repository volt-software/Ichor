// clang < 17 cannot parse libstdc++ 13+ in C++23 mode: <ranges> uses requires-clauses/syntax clang 16 rejects
// and <utility>/<tuple> add the pair-like std::pair constructor. Merely using these headers is enough to trigger it.
#include <map>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

int main() {
    std::map<int, std::string> m{{1, "a"}};
    std::vector<std::pair<int, int>> v{{1, 2}};
    auto keys = v | std::views::keys;
    auto values = m | std::views::filter([](auto const &kv) { return kv.first > 0; }) | std::views::values;
    return static_cast<int>(std::ranges::distance(keys) + std::ranges::distance(values)) - 2;
}
