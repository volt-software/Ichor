#include <fmt/format.h>
#include <iostream>
#include <chrono>
#include <regex>
#include <ichor/stl/StringUtils.h>
#include <ichor/services/metrics/MemoryUsageFunctions.h>
#include <ichor/services/network/http/IHttpHostService.h>
#include <ichor/ichor-mimalloc.h>
#include "../../examples/common/lyra.hpp"


#ifdef ICHOR_USE_RE2
#include <re2/re2.h>
#endif

template<size_t N>
struct StringLiteral {
    constexpr StringLiteral(const char (&str)[N]) {
        std::copy_n(str, N, value);
    }

    char value[N];
};

template <StringLiteral REGEX, decltype(std::regex::ECMAScript) flags>
struct StdRegexRouteMatch final : public Ichor::v1::RouteMatcher {
    ~StdRegexRouteMatch() noexcept final = default;

    bool matches(std::string_view route) noexcept final {
        std::match_results<typename decltype(route)::const_iterator> matches;
        auto result = std::regex_match(route.cbegin(), route.cend(), matches, _r);

        if(!result) {
            return false;
        }

        _params.reserve(matches.size() - 1);

        for(decltype(matches.size()) i = 1; i < matches.size(); i++) {
            _params.emplace_back(matches[i].str());
        }

        return true;
    }

    std::vector<std::string> route_params() noexcept final {
        return std::move(_params);
    }

private:
    std::vector<std::string> _params{};
    std::regex _r{REGEX.value, flags};
};

#ifdef ICHOR_USE_RE2
template <StringLiteral REGEX>
struct Re2RegexRouteMatch final : public Ichor::v1::RouteMatcher {
    Re2RegexRouteMatch() : _r(REGEX.value, RE2::CannedOptions::Latin1) {
        if(!_r.ok()) {
            fmt::print("Couldn't compile RE2 {}\n", REGEX.value);
            std::terminate();
        }
    }
    ~Re2RegexRouteMatch() noexcept final = default;

    bool matches(std::string_view route) noexcept final {
        std::size_t args_count = static_cast<size_t>(_r.NumberOfCapturingGroups());
        _params.resize(static_cast<unsigned long>(args_count));
        std::vector<RE2::Arg> args{args_count};
        std::vector<RE2::Arg*> arg_ptrs{args_count};
        for (std::size_t i = 0; i < args_count; ++i) {
            args[i] = &_params[i];
            arg_ptrs[i] = &args[i];
        }

        return RE2::FullMatchN(route, _r, arg_ptrs.data(), static_cast<int>(args_count));
    }

    std::vector<std::string> route_params() noexcept final {
        return std::move(_params);
    }

private:
    std::vector<std::string> _params{};
    RE2 _r;
};
#endif

#define FMT_INLINE_BUFFER_SIZE 1024

#if defined(ICHOR_ENABLE_INTERNAL_DEBUGGING) || (defined(ICHOR_BUILDING_DEBUG) && (defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)))
const uint64_t ITERATION_COUNT = 100;
#elif defined(__SANITIZE_ADDRESS__) || defined(ICHOR_ENABLE_INTERNAL_DEBUGGING)
const uint64_t ITERATION_COUNT = 5'000;
#else
const uint64_t ITERATION_COUNT = 500'000;
#endif

template <StringLiteral REGEX, int EXPECTED_MATCHES>
void run_regex_bench(char *argv) {
    Ichor::v1::RegexRouteMatch<REGEX.value> ctreMatcher{};
    StdRegexRouteMatch<REGEX, std::regex::ECMAScript> stdMatcher{};
#ifdef ICHOR_USE_RE2
    Re2RegexRouteMatch<REGEX> re2Matcher{};
#endif

    if(!ctreMatcher.matches("/some/http/10/11/12/test?one=two&three=four")) {
        fmt::print("ctre matcher error\n");
        std::terminate();
    }
    auto route_params_size = ctreMatcher.route_params().size();
    if(route_params_size != EXPECTED_MATCHES) {
        fmt::print("ctre matcher size error expected {} got {}\n", EXPECTED_MATCHES, route_params_size);
        std::terminate();
    }
    if(!stdMatcher.matches("/some/http/10/11/12/test?one=two&three=four")) {
        fmt::print("std matcher error\n");
        std::terminate();
    }
    route_params_size = stdMatcher.route_params().size();
    if(route_params_size != EXPECTED_MATCHES) {
        fmt::print("std matcher size error expected {} got {}\n", EXPECTED_MATCHES, route_params_size);
        std::terminate();
    }
#ifdef ICHOR_USE_RE2
    if(!re2Matcher.matches("/some/http/10/11/12/test?one=two&three=four")) {
        fmt::print("re matcher error\n");
        std::terminate();
    }
    route_params_size = re2Matcher.route_params().size();
    if(route_params_size != EXPECTED_MATCHES) {
        fmt::print("re matcher size error expected {} got {}\n", EXPECTED_MATCHES, route_params_size);
        std::terminate();
    }
#endif

    auto startCtre = std::chrono::steady_clock::now();
    for(uint64_t j = 0; j < ITERATION_COUNT; j++) {
        static_cast<void>(ctreMatcher.matches("/some/http/10/11/12/test?one=two&three=four"));
        static_cast<void>(ctreMatcher.route_params());
    }
    auto endCtr = std::chrono::steady_clock::now();

    auto startStd = std::chrono::steady_clock::now();
    for(uint64_t j = 0; j < ITERATION_COUNT; j++) {
        stdMatcher.matches("/some/http/10/11/12/test?one=two&three=four");
        stdMatcher.route_params();
    }
    auto endStd = std::chrono::steady_clock::now();

#ifdef ICHOR_USE_RE2
    auto startRe2 = std::chrono::steady_clock::now();
    for(uint64_t j = 0; j < ITERATION_COUNT; j++) {
        re2Matcher.matches("/some/http/10/11/12/test?one=two&three=four");
        re2Matcher.route_params();
    }
    auto endRe2 = std::chrono::steady_clock::now();
#endif

    fmt::print("{} ctreMatcher {} ran for {:L} µs with {:L} peak memory usage {:L} matches/s\n", argv, REGEX.value, std::chrono::duration_cast<std::chrono::microseconds>(endCtr - startCtre).count(), getPeakRSS(),
               std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endCtr - startCtre).count()) * ITERATION_COUNT));
    fmt::print("{} stdMatcher  {} ran for {:L} µs with {:L} peak memory usage {:L} matches/s\n", argv, REGEX.value, std::chrono::duration_cast<std::chrono::microseconds>(endStd - startStd).count(), getPeakRSS(),
               std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endStd - startStd).count()) * ITERATION_COUNT));
#ifdef ICHOR_USE_RE2
    fmt::print("{} re2Matcher  {} ran for {:L} µs with {:L} peak memory usage {:L} matches/s\n", argv, REGEX.value, std::chrono::duration_cast<std::chrono::microseconds>(endRe2 - startRe2).count(), getPeakRSS(),
               std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endRe2 - startRe2).count()) * ITERATION_COUNT));
#endif
}

int main(int argc, char *argv[]) {
#if ICHOR_EXCEPTIONS_ENABLED
    try {
#endif
        std::locale::global(std::locale("en_US.UTF-8"));
#if ICHOR_EXCEPTIONS_ENABLED
    } catch(std::runtime_error const &e) {
        fmt::println("Couldn't set locale to en_US.UTF-8: {}", e.what());
    }
#endif

    bool onlyPrint{};
    bool onlyRegex{};
    bool onlyAtoi{};
    bool showHelp{};

    auto cli = lyra::help(showHelp)
               | lyra::opt(onlyPrint)["-p"]["--print"]("Only run print and other explicitly selected benchmarks")
               | lyra::opt(onlyRegex)["-r"]["--regex"]("Only run regex and other explicitly selected benchmarks")
               | lyra::opt(onlyAtoi)["-a"]["--atoi"]("Only run atoi and other explicitly selected benchmarks");

    auto result = cli.parse( { argc, argv } );
    if (!result) {
        fmt::print("Error in command line: {}\n", result.message());
        return 1;
    }

    if (showHelp) {
        std::cout << cli << "\n";
        return 0;
    }

    if(!onlyPrint && !onlyRegex && !onlyAtoi) {
        onlyPrint = true;
        onlyRegex = true;
        onlyAtoi = true;
    }

    if(onlyPrint) {
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
        fmt::print("\n\n{} fmt::print ran for {:L} µs with {:L} peak memory usage {:L} prints/s\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(endFmt - startFmt).count(), getPeakRSS(),
                                 std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endFmt - startFmt).count()) * ITERATION_COUNT));
        fmt::print("{} std::cout with fmt::format_to ran for {:L} µs with {:L} peak memory usage {:L} prints/s\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(endCout - startCout).count(), getPeakRSS(),
                                 std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endCout - startCout).count()) * ITERATION_COUNT));
    }


    if(onlyRegex) {
        run_regex_bench<R"(\/some\/http\/10\/11\/12\/test\?one=two&three=four)", 0>(argv[0]);
        run_regex_bench<R"(\/some\/http\/(\d{1,2})\/(\d{1,2})\/(\d{1,2})\/test\?*.*)", 3>(argv[0]);
        run_regex_bench<R"(\/some\/http\/(\d{1,2})\/(\d{1,2})\/(\d{1,2})\/test\?one=([a-zA-Z0-9]+)&three=([a-zA-Z0-9]+))", 5>(argv[0]);
        run_regex_bench<R"(\/some\/http\/(\d{1,2})\/(\d{1,2})\/(\d{1,2})\/test\?*(.*))", 4>(argv[0]);
        run_regex_bench<R"(\/some\/http\/(\d{1,2})\/(\d{1,2})\/(\d{1,2})\/test\?*([a-zA-Z0-9]+=[a-zA-Z0-9]+)*&*([a-zA-Z0-9]+=[a-zA-Z0-9]+)*)", 5>(argv[0]);
    }

    if(onlyAtoi) {
        std::string small = "1";
        std::string large = "1234567891011";
        auto startSmallStd = std::chrono::steady_clock::now();
        for(uint64_t j = 0; j < ITERATION_COUNT * 10; j++) {
            if(std::stoll(small) != 1ll) {
                std::terminate();
            }
        }
        auto endSmallStd = std::chrono::steady_clock::now();
        auto startLargeStd = std::chrono::steady_clock::now();
        for(uint64_t j = 0; j < ITERATION_COUNT * 10; j++) {
            if(std::stoll(large) != 1234567891011ll) {
                std::terminate();
            }
        }
        auto endLargeStd = std::chrono::steady_clock::now();
        auto startSmallIchor = std::chrono::steady_clock::now();
        for(uint64_t j = 0; j < ITERATION_COUNT * 10; j++) {
            if(Ichor::v1::FastAtoi(small.c_str()) != 1ll) {
                std::terminate();
            }
        }
        auto endSmallIchor = std::chrono::steady_clock::now();
        auto startLargeIchor = std::chrono::steady_clock::now();
        for(uint64_t j = 0; j < ITERATION_COUNT * 10; j++) {
            if(Ichor::v1::FastAtoi(large.c_str()) != 1234567891011ll) {
                std::terminate();
            }
        }
        auto endLargeIchor = std::chrono::steady_clock::now();
        fmt::print("{} small std   ran for {:L} µs with {:L} peak memory usage {:L} converts/s\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(endSmallStd - startSmallStd).count(), getPeakRSS(),
                   std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endSmallStd - startSmallStd).count()) * ITERATION_COUNT * 10));
        fmt::print("{} large std   ran for {:L} µs with {:L} peak memory usage {:L} converts/s\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(endLargeStd - startLargeStd).count(), getPeakRSS(),
                   std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endLargeStd - startLargeStd).count()) * ITERATION_COUNT * 10));
        fmt::print("{} small ichor ran for {:L} µs with {:L} peak memory usage {:L} converts/s\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(endSmallIchor - startSmallIchor).count(), getPeakRSS(),
                   std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endSmallIchor - startSmallIchor).count()) * ITERATION_COUNT * 10));
        fmt::print("{} large ichor ran for {:L} µs with {:L} peak memory usage {:L} converts/s\n", argv[0], std::chrono::duration_cast<std::chrono::microseconds>(endLargeIchor - startLargeIchor).count(), getPeakRSS(),
                   std::floor(1'000'000. / static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endLargeIchor - startLargeIchor).count()) * ITERATION_COUNT * 10));
    }

}
