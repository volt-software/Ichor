#include <memory>
#include <type_traits>

consteval bool constexpr_unique_ptr_works() {
    auto p = std::make_unique<int>(42);
    if (*p != 42) return false;

    auto q = std::move(p);
    if (!q || p) return false;

    // destruction happens at end of constant evaluation
    return true;
}

static_assert(constexpr_unique_ptr_works(), "unique_ptr/make_unique not usable in constant evaluation with this STL/compiler combo");

int main() {
    return 0;
}