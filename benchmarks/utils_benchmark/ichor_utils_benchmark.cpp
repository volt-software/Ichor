#include <fmt/format.h>
#include <iostream>
#include <chrono>
#include <ichor/services/metrics/MemoryUsageFunctions.h>

#define FMT_INLINE_BUFFER_SIZE 1024
const uint64_t ITERATION_COUNT = 1'000'000;

int main(int argc, char *argv[]) {
    std::locale::global(std::locale("en_US.UTF-8"));
    int i = 1'234'567;
    double d = 1.234567;
    std::string s{"some arbitrary string"};

    auto startFmt = std::chrono::steady_clock::now();
    for(uint64_t j = 0; j < ITERATION_COUNT; j++) {
        fmt::print("This is a test! {} {} {}", i, d, s);
    }
    auto endFmt = std::chrono::steady_clock::now();
    auto startCout = std::chrono::steady_clock::now();
    for(uint64_t j = 0; j < ITERATION_COUNT; j++) {
        fmt::basic_memory_buffer<char, FMT_INLINE_BUFFER_SIZE> buf{};
        fmt::format_to(std::back_inserter(buf), "This is a test! {} {} {}", i, d, s);
        std::cout.write(buf.data(), static_cast<int64_t>(buf.size()));
    }
    auto endCout = std::chrono::steady_clock::now();
    fmt::print("\n\n{} fmt::print ran for {:L} µs with {:L} peak memory usage {:L} prints/s\n", argv[0],  std::chrono::duration_cast<std::chrono::microseconds>(endFmt - startFmt).count(), getPeakRSS(),
                             std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endFmt - startFmt).count()) * ITERATION_COUNT));
    fmt::print("{} std::cout with fmt::format_to ran for {:L} µs with {:L} peak memory usage {:L} prints/s\n", argv[0],  std::chrono::duration_cast<std::chrono::microseconds>(endCout - startCout).count(), getPeakRSS(),
                             std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endCout - startCout).count()) * ITERATION_COUNT));
}
