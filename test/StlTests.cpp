#include "Common.h"
#include <ichor/stl/RealtimeMutex.h>
#include <ichor/stl/RealtimeReadWriteMutex.h>
#include <ichor/stl/NeverAlwaysNull.h>
#include <ichor/stl/ReferenceCountedPointer.h>
#include <ichor/stl/StringUtils.h>
#include <ichor/stl/StaticVector.h>
#include <memory>
#include <string_view>
#include "TestServices/UselessService.h"

using namespace Ichor;
using namespace std::literals;

struct nonmoveable final {
    nonmoveable() = default;
    nonmoveable(const nonmoveable&) = default;
    nonmoveable(nonmoveable&&) = delete;
    nonmoveable& operator=(const nonmoveable&) = default;
    nonmoveable& operator=(nonmoveable&&) = delete;
};

struct nonmoveable_sbo_buster final {
    nonmoveable_sbo_buster() = default;
    nonmoveable_sbo_buster(const nonmoveable_sbo_buster&) = default;
    nonmoveable_sbo_buster(nonmoveable_sbo_buster&&) = delete;
    nonmoveable_sbo_buster& operator=(const nonmoveable_sbo_buster&) = default;
    nonmoveable_sbo_buster& operator=(nonmoveable_sbo_buster&&) = delete;

    std::array<std::byte, 32> buf;
};

struct noncopyable final {
    noncopyable() = default;
    noncopyable(const noncopyable&) = delete;
    noncopyable(noncopyable&&) = default;
    noncopyable& operator=(const noncopyable&) = delete;
    noncopyable& operator=(noncopyable&&) = default;
};

struct sufficiently_non_trival {
    sufficiently_non_trival() = default;
    sufficiently_non_trival(int _i) : i(_i) {}
    ~sufficiently_non_trival() { destructed = true; }
    sufficiently_non_trival(const sufficiently_non_trival&) = default;
    sufficiently_non_trival(sufficiently_non_trival&&) = default;
    sufficiently_non_trival& operator=(const sufficiently_non_trival&) = default;
    sufficiently_non_trival& operator=(sufficiently_non_trival&&) = default;

    [[nodiscard]] constexpr bool operator==(int _i) const noexcept { return _i == i; }

    int i{};
    bool destructed{};
};

template <>
struct fmt::formatter<nonmoveable> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const nonmoveable& change, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "nonmoveable");
    }
};

struct parent_test_class {

};

struct rc_test_class final : parent_test_class {
    rc_test_class(int _i, float _f, noncopyable) : i(_i), f(_f) {}
    rc_test_class(const rc_test_class&) = default;
    rc_test_class(rc_test_class&&) = delete;
    rc_test_class& operator=(const rc_test_class&) = default;
    rc_test_class& operator=(rc_test_class&&) = delete;

    int i;
    float f;
};

TEST_CASE("STL Tests") {

    SECTION("Any basics") {
        auto someInt = make_any<uint64_t>(5u);
        REQUIRE_NOTHROW(any_cast<uint64_t>(someInt));
        REQUIRE(any_cast<uint64_t>(someInt) == 5u);
        REQUIRE(someInt.to_string() == "5");
        REQUIRE(someInt.get_size() == sizeof(uint64_t));
        REQUIRE(someInt.type_hash() == typeNameHash<uint64_t>());
        REQUIRE(someInt.type_name() == typeName<uint64_t>());
        REQUIRE_THROWS_MATCHES(any_cast<float>(someInt), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<int64_t>(someInt), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<int32_t>(someInt), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<uint32_t>(someInt), bad_any_cast, ExceptionMatcher());

        any_cast<uint64_t&>(someInt) = 10;
        REQUIRE(any_cast<uint64_t>(someInt) == 10u);

        auto someString = make_any<std::string>("test");
        REQUIRE_NOTHROW(any_cast<std::string>(someString));
        REQUIRE(any_cast<std::string>(someString) == "test");
        REQUIRE(someString.to_string() == "test");
        REQUIRE(someString.get_size() == sizeof(std::string));
        REQUIRE(someString.type_hash() == typeNameHash<std::string>());
        REQUIRE(someString.type_name() == typeName<std::string>());
        REQUIRE_THROWS_MATCHES(any_cast<float>(someString), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<int64_t>(someString), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<int32_t>(someString), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<uint32_t>(someString), bad_any_cast, ExceptionMatcher());

        someString = make_any<float>(4.5f);
        REQUIRE_NOTHROW(any_cast<float>(someString));
        REQUIRE(any_cast<float>(someString) == 4.5f);
        REQUIRE_THROWS_MATCHES(any_cast<std::string>(someString), bad_any_cast, ExceptionMatcher());

        auto someMovedFloat{std::move(someString)};
        REQUIRE_NOTHROW(any_cast<float>(someMovedFloat));
        REQUIRE(any_cast<float>(someMovedFloat) == 4.5f);
        REQUIRE_THROWS_MATCHES(any_cast<float>(someString), bad_any_cast, ExceptionMatcher());
        REQUIRE(someMovedFloat.to_string() == "4.5");
        REQUIRE(someString.to_string() == "Unprintable value");

        auto someCopiedInt = someInt;
        REQUIRE_NOTHROW(any_cast<uint64_t>(someCopiedInt));
        REQUIRE(any_cast<uint64_t>(someCopiedInt) == 10u);
        REQUIRE(any_cast<uint64_t>(someInt) == 10u);
        REQUIRE(someInt.to_string() == "10");
        REQUIRE(someCopiedInt.to_string() == "10");

        auto someMovedInt = std::move(someInt);
        REQUIRE_NOTHROW(any_cast<uint64_t>(someMovedInt));
        REQUIRE(any_cast<uint64_t>(someMovedInt) == 10u);
        REQUIRE(someMovedInt.to_string() == "10");
        REQUIRE(someInt.to_string() == "Unprintable value");
        REQUIRE_THROWS_MATCHES(any_cast<uint64_t>(someInt), bad_any_cast, ExceptionMatcher());

        const auto someConstInt = make_any<int>(12);
        REQUIRE_NOTHROW(any_cast<int>(someConstInt));
        REQUIRE(any_cast<int>(someConstInt) == 12);

        auto someCopiedFromConstInt{someConstInt};
        REQUIRE_NOTHROW(any_cast<int>(someConstInt));
        REQUIRE(any_cast<int>(someConstInt) == 12);
        REQUIRE_NOTHROW(any_cast<int>(someCopiedFromConstInt));
        REQUIRE(any_cast<int>(someCopiedFromConstInt) == 12);

        const auto someNonmoveable = make_any<nonmoveable>();
        REQUIRE_NOTHROW(any_cast<nonmoveable>(someNonmoveable));
        REQUIRE(someNonmoveable.to_string() == "nonmoveable");
        auto someMovedNonMoveable = std::move(someNonmoveable);
        REQUIRE(someMovedNonMoveable.to_string() == "nonmoveable");

        any noneAny;
        REQUIRE_THROWS_MATCHES(any_cast<float>(noneAny), bad_any_cast, ExceptionMatcher());
        REQUIRE(noneAny.to_string() == "Unprintable value");

        auto movedNoneAny = std::move(noneAny);
        REQUIRE_THROWS_MATCHES(any_cast<float>(noneAny), bad_any_cast, ExceptionMatcher());
        REQUIRE_THROWS_MATCHES(any_cast<float>(movedNoneAny), bad_any_cast, ExceptionMatcher());
        REQUIRE(movedNoneAny.to_string() == "Unprintable value");
        REQUIRE(noneAny.to_string() == "Unprintable value");


        const auto someNonmoveableSboBuster = make_unformattable_any<nonmoveable_sbo_buster>();
        REQUIRE_NOTHROW(any_cast<nonmoveable_sbo_buster>(someNonmoveableSboBuster));
        auto someMovedNonmoveableSboBuster = std::move(someNonmoveableSboBuster);
        auto someMoveConstructedNonmoveableSboBuster = Ichor::any(std::move(someNonmoveableSboBuster));

        auto emptyAny = Ichor::any();
        auto copyAssignedEmptyAny = emptyAny;
        auto copyConstructedEmptyAny = Ichor::any(emptyAny);
        auto moveAssignedEmptyAny = std::move(emptyAny);
        auto moveConstructedEmptyAny = Ichor::any(std::move(moveAssignedEmptyAny));

        someCopiedFromConstInt = std::move(someMovedNonMoveable);
    }

    SECTION("RealTimeMutex basics") {
        RealtimeMutex m;
        m.lock();

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
        // critical section in windows is recursive
        REQUIRE(m.try_lock() == true);
#else
        REQUIRE(m.try_lock() == false);
#endif

        m.unlock();

        REQUIRE(m.try_lock() == true);

        m.unlock();
    }

    SECTION("RealtimeReadWriteMutex basics") {
        RealtimeReadWriteMutex m;
        m.lock();

        REQUIRE(m.try_lock() == false);
        REQUIRE(m.try_lock_shared() == false);

        m.unlock();

        REQUIRE(m.try_lock() == true);
        REQUIRE(m.try_lock_shared() == false);

        m.unlock();

        REQUIRE(m.try_lock_shared() == true);
        REQUIRE(m.try_lock_shared() == true);
        REQUIRE(m.try_lock() == false);

        m.unlock_shared();
        m.unlock_shared();
    }

    SECTION("typeName tests") {
        REQUIRE(typeName<UselessService>() == typeName<Ichor::UselessService>());
        REQUIRE(typeNameHash<UselessService>() == typeNameHash<Ichor::UselessService>());
    }

    SECTION("NeverNull tests") {
        NeverNull<int*> p = new int(120);
        auto f = [](NeverNull<int*> p2) { *p2 += 1; };
        REQUIRE(*p == 120);
        f(p);
        REQUIRE(*p == 121);
        delete p;
    }

    SECTION("ReferenceCountedPointer tests") {
        {
            ReferenceCountedPointer<int> i;
            REQUIRE(i.use_count() == 0);
            REQUIRE(!i.has_value());
            i = new int(5);
            REQUIRE(i.use_count() == 1);
            REQUIRE(*i == 5);
            REQUIRE(*i.get() == 5);
            REQUIRE(i.has_value());
        }
        {
            ReferenceCountedPointer<int> i{new int(6)};
            REQUIRE(i.use_count() == 1);
            REQUIRE(i.has_value());
            i = new int(5);
            REQUIRE(i.use_count() == 1);
            REQUIRE(i.has_value());
            REQUIRE(*i == 5);
            REQUIRE(*i.get() == 5);
        }
        bool deleted{};
        {
            auto deleter = [&deleted](int *ptr){ deleted = true; delete ptr; };
            std::unique_ptr<int, decltype(deleter)> unique(new int(5), deleter);
            ReferenceCountedPointer<int> tc{std::move(unique)};
            REQUIRE(tc.use_count() == 1);
            REQUIRE(*tc == 5);
            REQUIRE(!unique);
        }
        REQUIRE(deleted);
        deleted = false;
        {
            auto deleter = [&deleted](int *ptr){ deleted = true; delete ptr; };
            std::unique_ptr<int, decltype(deleter)> unique(new int(5), deleter);
            ReferenceCountedPointer<int> tc;
            tc = std::move(unique);
            REQUIRE(tc.use_count() == 1);
            REQUIRE(*tc == 5);
            REQUIRE(!unique);
        }
        REQUIRE(deleted);
        {
            ReferenceCountedPointer<int> tc = std::make_unique<int>(5);
            REQUIRE(tc.use_count() == 1);
            REQUIRE(*tc == 5);
        }
        {
            ReferenceCountedPointer<int> tc = Ichor::make_reference_counted<int>(5);
            REQUIRE(tc.use_count() == 1);
            REQUIRE(*tc == 5);
            tc = nullptr;
            REQUIRE(tc.use_count() == 0);
            REQUIRE(!tc.has_value());
        }
        {
            ReferenceCountedPointer<int> tc;
            REQUIRE(tc.use_count() == 0);
            tc = std::make_unique<int>(5);
            REQUIRE(tc.use_count() == 1);
            REQUIRE(*tc == 5);
        }
        {
            ReferenceCountedPointer<rc_test_class> tc = Ichor::make_reference_counted<rc_test_class>(5, 5.0f, noncopyable{});
            REQUIRE(tc.use_count() == 1);
            REQUIRE(tc->i == 5);
            REQUIRE(tc->f == 5.0f);
            ReferenceCountedPointer<parent_test_class> tc2{tc};
            REQUIRE(tc.use_count() == 2);
            REQUIRE(tc2.use_count() == 2);
            ReferenceCountedPointer<parent_test_class> tc3;
            REQUIRE(tc3.use_count() == 0);
            tc3 = tc;
            REQUIRE(tc.use_count() == 3);
            REQUIRE(tc2.use_count() == 3);
            REQUIRE(tc3.use_count() == 3);
        }
        {
            ReferenceCountedPointer<parent_test_class> tc{new rc_test_class(5, 5.0f, noncopyable{})};
            REQUIRE(tc.use_count() == 1);
            tc = Ichor::make_reference_counted<rc_test_class>(5, 5.0f, noncopyable{});
            REQUIRE(tc.use_count() == 1);
        }
        {
            ReferenceCountedPointer<parent_test_class> tc = Ichor::make_reference_counted<rc_test_class>(5, 5.0f, noncopyable{});
            REQUIRE(tc.use_count() == 1);
        }
        {
            ReferenceCountedPointer<int> i{new int(7)};
            ReferenceCountedPointer<int> i2{i};
            REQUIRE(i.use_count() == 2);
            REQUIRE(i2.use_count() == 2);
            REQUIRE(*i == 7);
            REQUIRE(*i2 == 7);
            i = new int(5);
            REQUIRE(i.use_count() == 1);
            REQUIRE(i2.use_count() == 1);
            REQUIRE(*i == 5);
            REQUIRE(*i2 == 7);
        }
        {
            ReferenceCountedPointer<int> i{new int(7)};
            ReferenceCountedPointer<int> i2;
            REQUIRE(i2.use_count() == 0);
            i2 = i;
            REQUIRE(i.use_count() == 2);
            REQUIRE(i2.use_count() == 2);
            REQUIRE(*i == 7);
            REQUIRE(*i2 == 7);
            i = new int(5);
            REQUIRE(i.use_count() == 1);
            REQUIRE(i2.use_count() == 1);
            REQUIRE(*i == 5);
            REQUIRE(*i2 == 7);
        }
        {
            ReferenceCountedPointer<int> i{new int(7)};
            ReferenceCountedPointer<int> i2{std::move(i)};
            REQUIRE(!i.has_value());
            REQUIRE(i2.has_value());
            REQUIRE(i.use_count() == 0);
            REQUIRE(i2.use_count() == 1);
            REQUIRE(*i2 == 7);
            i = new int(5);
            REQUIRE(i.has_value());
            REQUIRE(i2.has_value());
            REQUIRE(i.use_count() == 1);
            REQUIRE(i2.use_count() == 1);
            REQUIRE(*i == 5);
            REQUIRE(*i2 == 7);
        }
        {
            ReferenceCountedPointer<int> i{new int(7)};
            ReferenceCountedPointer<int> i2{};
            REQUIRE(i2.use_count() == 0);
            i2 = std::move(i);
            REQUIRE(!i.has_value());
            REQUIRE(i2.has_value());
            REQUIRE(i.use_count() == 0);
            REQUIRE(i2.use_count() == 1);
            REQUIRE(*i2 == 7);
            i = new int(5);
            REQUIRE(i.has_value());
            REQUIRE(i2.has_value());
            REQUIRE(i.use_count() == 1);
            REQUIRE(i2.use_count() == 1);
            REQUIRE(*i == 5);
            REQUIRE(*i2 == 7);
        }
    }

    SECTION("FastAtoi(u) tests") {
        REQUIRE(Ichor::FastAtoiu("10") == 10);
        REQUIRE(Ichor::FastAtoiu("0") == 0);
        REQUIRE(Ichor::FastAtoiu("u10") == 0);
        REQUIRE(Ichor::FastAtoiu("10u") == 10);
        REQUIRE(Ichor::FastAtoiu(std::to_string(std::numeric_limits<uint64_t>::max()).c_str()) == std::numeric_limits<uint64_t>::max());
        REQUIRE(Ichor::FastAtoi("10") == 10);
        REQUIRE(Ichor::FastAtoi("0") == 0);
        REQUIRE(Ichor::FastAtoi("u10") == 0);
        REQUIRE(Ichor::FastAtoi("10u") == 10);
        REQUIRE(Ichor::FastAtoi("-10") == -10);
        REQUIRE(Ichor::FastAtoi(std::to_string(std::numeric_limits<int64_t>::max()).c_str()) == std::numeric_limits<int64_t>::max());
        REQUIRE(Ichor::FastAtoi(std::to_string(std::numeric_limits<int64_t>::min()).c_str()) == std::numeric_limits<int64_t>::min());
    }

    SECTION("string_view split tests") {
        std::string_view s{"this\nis\na\nstring"};
        auto ret = split(s, "\n");
        REQUIRE(ret.size() == 4);
        ret = split(s, " ");
        REQUIRE(ret.size() == 1);
    }

    SECTION("Version parse tests") {
        REQUIRE(!parseStringAsVersion("").has_value());
        REQUIRE(!parseStringAsVersion("0").has_value());
        REQUIRE(!parseStringAsVersion("0.0").has_value());
        REQUIRE(!parseStringAsVersion("-1.2.3").has_value());
        REQUIRE(!parseStringAsVersion("a.b.c").has_value());
        REQUIRE(!parseStringAsVersion("1.2.3.4").has_value());
        REQUIRE(!parseStringAsVersion("1.2.3.").has_value());

        auto v = parseStringAsVersion("0.0.0");
        REQUIRE(v.has_value());
        REQUIRE((*v).major == 0);
        REQUIRE((*v).minor == 0);
        REQUIRE((*v).patch == 0);

        v = parseStringAsVersion("27398.128937618973.123123");
        REQUIRE(v.has_value());
        REQUIRE((*v).major == 27398);
        REQUIRE((*v).minor == 128937618973);
        REQUIRE((*v).patch == 123123);
    }

    SECTION("Version tests") {
        Version v1{1, 2, 3};
        Version emptyV{};
        REQUIRE(Version{1, 2, 4} > v1);
        REQUIRE(Version{1, 3, 3} > v1);
        REQUIRE(Version{2, 2, 3} > v1);
        REQUIRE(Version{2, 0, 0} > v1);
        REQUIRE(Version{1, 3, 0} > v1);
        REQUIRE(!(Version{1, 2, 3} > v1));
        REQUIRE(Version{1, 2, 3} >= v1);
        REQUIRE(!(Version{1, 2, 2} >= v1));
        REQUIRE(v1 > Version{1, 2, 2});
        REQUIRE(!(v1 > Version{1, 2, 3}));
        REQUIRE(v1 >= Version{1, 2, 3});
        REQUIRE(!(v1 >= Version{1, 2, 4}));
        REQUIRE(v1 < Version{1, 2, 4});
        REQUIRE(!(v1 < Version{1, 2, 3}));
        REQUIRE(v1 <= Version{1, 2, 3});
        REQUIRE(!(v1 <= Version{1, 2, 2}));
        REQUIRE(v1 == Version{1, 2, 3});
        REQUIRE(v1 != Version{2, 2, 3});
        REQUIRE(v1 != Version{1, 3, 3});
        REQUIRE(v1 != Version{1, 2, 4});
        REQUIRE(emptyV != Version{1, 2, 4});
        REQUIRE(emptyV < Version{1, 2, 4});
        REQUIRE(emptyV <= Version{1, 2, 4});
        REQUIRE(!(emptyV > Version{1, 2, 4}));
        REQUIRE(!(emptyV >= Version{1, 2, 4}));

        REQUIRE(v1.toString() == "1.2.3");
    }

    SECTION("Basename tests") {
        REQUIRE(Ichor::basename("") == ""sv);
        REQUIRE(Ichor::basename("file.cpp") == "file.cpp"sv);
#ifdef _WIN32
        REQUIRE(Ichor::basename("path\\file.cpp") == "file.cpp"sv);
#else
        REQUIRE(Ichor::basename("path/file.cpp") == "file.cpp"sv);
        REQUIRE(Ichor::basename("more/path/file.cpp") == "file.cpp"sv);
#endif
    }

    SECTION("StaticVector tests") {
        auto const check_iteration_count = []<typename T, size_t N>(StaticVector<T, N> &_sv, int expected) {
            int iteratedCount{};
            for(auto i : _sv) {
                ++iteratedCount;
            }
            REQUIRE(iteratedCount == expected);
            iteratedCount = 0;
            for(auto &i : _sv) {
                ++iteratedCount;
            }
            REQUIRE(iteratedCount == expected);
        };
        auto const const_check_iteration_count = []<typename T, size_t N>(StaticVector<T, N> const &_sv, int expected) {
            int iteratedCount{};
            for(auto const i : _sv) {
                ++iteratedCount;
            }
            REQUIRE(iteratedCount == expected);
            iteratedCount = 0;
            for(auto const &i : _sv) {
                ++iteratedCount;
            }
            REQUIRE(iteratedCount == expected);
        };

        StaticVector<int, 10> sv;
        REQUIRE(sv.empty());
        REQUIRE(sv.size() == 0);
        REQUIRE(sv.max_size() == 10);
        REQUIRE(sv.capacity() == 10);
        REQUIRE(sv.begin() == sv.end());
        sv.clear();
        REQUIRE(sv.size() == 0);
        check_iteration_count(sv, 0);
        const_check_iteration_count(sv, 0);

        sv.push_back(123);
        REQUIRE(!sv.empty());
        REQUIRE(sv.size() == 1);
        REQUIRE(sv.front() == 123);
        REQUIRE(sv.back() == 123);
        REQUIRE(*sv.begin() == 123);
        check_iteration_count(sv, 1);
        const_check_iteration_count(sv, 1);

        sv.push_back(234);
        REQUIRE(!sv.empty());
        REQUIRE(sv.size() == 2);
        REQUIRE(sv.front() == 123);
        REQUIRE(sv.back() == 234);
        REQUIRE(*sv.begin() == 123);
        check_iteration_count(sv, 2);
        const_check_iteration_count(sv, 2);

        sv.push_back(345);
        sv.push_back(456);
        sv.push_back(567);
        sv.push_back(678);
        sv.push_back(789);
        sv.push_back(891);
        sv.push_back(912);
        sv.push_back(1234);
        REQUIRE(!sv.empty());
        REQUIRE(sv.size() == 10);
        REQUIRE(sv.front() == 123);
        REQUIRE(sv.back() == 1234);
        REQUIRE(*sv.begin() == 123);
        check_iteration_count(sv, 10);
        const_check_iteration_count(sv, 10);

        REQUIRE(sv[0] == 123);
        REQUIRE(sv[1] == 234);
        REQUIRE(sv[2] == 345);
        REQUIRE(sv[3] == 456);
        REQUIRE(sv[4] == 567);
        REQUIRE(sv[5] == 678);
        REQUIRE(sv[6] == 789);
        REQUIRE(sv[7] == 891);
        REQUIRE(sv[8] == 912);
        REQUIRE(sv[9] == 1234);

        StaticVector<int, 10> copySv{sv};
        REQUIRE(!copySv.empty());
        REQUIRE(copySv.size() == 10);
        REQUIRE(copySv.front() == 123);
        REQUIRE(copySv.back() == 1234);
        REQUIRE(*copySv.begin() == 123);
        check_iteration_count(sv, 10);
        check_iteration_count(copySv, 10);
        const_check_iteration_count(sv, 10);
        const_check_iteration_count(copySv, 10);

        for(std::size_t i = 0; i < sv.size(); ++i) {
            REQUIRE(sv[i] == copySv[i]);
        }

        copySv = sv;
        REQUIRE(!copySv.empty());
        REQUIRE(copySv.size() == 10);
        REQUIRE(copySv.front() == 123);
        REQUIRE(copySv.back() == 1234);
        REQUIRE(*copySv.begin() == 123);
        check_iteration_count(sv, 10);
        check_iteration_count(copySv, 10);
        const_check_iteration_count(sv, 10);
        const_check_iteration_count(copySv, 10);

        for(std::size_t i = 0; i < sv.size(); ++i) {
            REQUIRE(sv[i] == copySv[i]);
        }

        [](StaticVector<int, 10> &_sv) {
            auto begin = _sv.begin();
            REQUIRE(*begin == 123); begin++;
            REQUIRE(*begin == 234); begin++;
            REQUIRE(*begin == 345); begin++;
            REQUIRE(*begin == 456); begin++;
            REQUIRE(*begin == 567); begin++;
            REQUIRE(*begin == 678); begin++;
            REQUIRE(*begin == 789); begin++;
            REQUIRE(*begin == 891); begin++;
            REQUIRE(*begin == 912); begin++;
            REQUIRE(*begin == 1234); begin++;
            REQUIRE(begin == _sv.end());
        }(sv);
        [](StaticVector<int, 10> const &_sv) {
            auto begin = _sv.begin();
            REQUIRE(*begin == 123); begin++;
            REQUIRE(*begin == 234); begin++;
            REQUIRE(*begin == 345); begin++;
            REQUIRE(*begin == 456); begin++;
            REQUIRE(*begin == 567); begin++;
            REQUIRE(*begin == 678); begin++;
            REQUIRE(*begin == 789); begin++;
            REQUIRE(*begin == 891); begin++;
            REQUIRE(*begin == 912); begin++;
            REQUIRE(*begin == 1234); begin++;
            REQUIRE(begin == _sv.end());
        }(sv);
        [](StaticVector<int, 10> &_sv) {
            auto begin = _sv.rbegin();
            REQUIRE(*begin == 1234); begin++;
            REQUIRE(*begin == 912); begin++;
            REQUIRE(*begin == 891); begin++;
            REQUIRE(*begin == 789); begin++;
            REQUIRE(*begin == 678); begin++;
            REQUIRE(*begin == 567); begin++;
            REQUIRE(*begin == 456); begin++;
            REQUIRE(*begin == 345); begin++;
            REQUIRE(*begin == 234); begin++;
            REQUIRE(*begin == 123); begin++;
            REQUIRE(begin == _sv.rend());
        }(sv);
        [](StaticVector<int, 10> const &_sv) {
            auto begin = _sv.rbegin();
            REQUIRE(*begin == 1234); begin++;
            REQUIRE(*begin == 912); begin++;
            REQUIRE(*begin == 891); begin++;
            REQUIRE(*begin == 789); begin++;
            REQUIRE(*begin == 678); begin++;
            REQUIRE(*begin == 567); begin++;
            REQUIRE(*begin == 456); begin++;
            REQUIRE(*begin == 345); begin++;
            REQUIRE(*begin == 234); begin++;
            REQUIRE(*begin == 123); begin++;
            REQUIRE(begin == _sv.rend());
        }(sv);
        [](StaticVector<int, 10> &_sv) {
            auto begin = _sv.cbegin();
            REQUIRE(*begin == 123); begin++;
            REQUIRE(*begin == 234); begin++;
            REQUIRE(*begin == 345); begin++;
            REQUIRE(*begin == 456); begin++;
            REQUIRE(*begin == 567); begin++;
            REQUIRE(*begin == 678); begin++;
            REQUIRE(*begin == 789); begin++;
            REQUIRE(*begin == 891); begin++;
            REQUIRE(*begin == 912); begin++;
            REQUIRE(*begin == 1234); begin++;
            REQUIRE(begin == _sv.cend());
        }(sv);
        [](StaticVector<int, 10> const &_sv) {
            auto begin = _sv.crbegin();
            REQUIRE(*begin == 1234); begin++;
            REQUIRE(*begin == 912); begin++;
            REQUIRE(*begin == 891); begin++;
            REQUIRE(*begin == 789); begin++;
            REQUIRE(*begin == 678); begin++;
            REQUIRE(*begin == 567); begin++;
            REQUIRE(*begin == 456); begin++;
            REQUIRE(*begin == 345); begin++;
            REQUIRE(*begin == 234); begin++;
            REQUIRE(*begin == 123); begin++;
            REQUIRE(begin == _sv.crend());
        }(sv);

        sv.resize(5);
        REQUIRE(!sv.empty());
        REQUIRE(sv.size() == 5);
        REQUIRE(sv.max_size() == 10);
        REQUIRE(sv.capacity() == 10);
        check_iteration_count(sv, 5);
        const_check_iteration_count(sv, 5);

        sv.clear();
        REQUIRE(sv.empty());
        REQUIRE(sv.size() == 0);
        REQUIRE(sv.max_size() == 10);
        REQUIRE(sv.capacity() == 10);
        REQUIRE(sv.begin() == sv.end());
        REQUIRE(sv.cbegin() == sv.cend());
        check_iteration_count(sv, 0);
        const_check_iteration_count(sv, 0);

        sv.resize(5);
        REQUIRE(!sv.empty());
        REQUIRE(sv.size() == 5);
        REQUIRE(sv.max_size() == 10);
        REQUIRE(sv.capacity() == 10);
        check_iteration_count(sv, 5);
        const_check_iteration_count(sv, 5);

        for(int const &i : sv) {
            REQUIRE(i == 0);
        }

        sv.clear();
        sv.resize(5, 5);
        REQUIRE(!sv.empty());
        REQUIRE(sv.size() == 5);
        REQUIRE(sv.max_size() == 10);
        REQUIRE(sv.capacity() == 10);
        check_iteration_count(sv, 5);
        const_check_iteration_count(sv, 5);

        for(int const &i : sv) {
            REQUIRE(i == 5);
        }

        StaticVector<int, 5> defaultInitSv{5};

        StaticVector<int, 10> moveSv{std::move(copySv)};
        REQUIRE(copySv.empty());
        REQUIRE(!moveSv.empty());
        REQUIRE(moveSv.size() == 10);
        REQUIRE(moveSv.front() == 123);
        REQUIRE(moveSv.back() == 1234);
        REQUIRE(*moveSv.begin() == 123);
        check_iteration_count(copySv, 0);
        check_iteration_count(moveSv, 10);
        const_check_iteration_count(copySv, 0);
        const_check_iteration_count(moveSv, 10);

        REQUIRE(moveSv[0] == 123);
        REQUIRE(moveSv[1] == 234);
        REQUIRE(moveSv[2] == 345);
        REQUIRE(moveSv[3] == 456);
        REQUIRE(moveSv[4] == 567);
        REQUIRE(moveSv[5] == 678);
        REQUIRE(moveSv[6] == 789);
        REQUIRE(moveSv[7] == 891);
        REQUIRE(moveSv[8] == 912);
        REQUIRE(moveSv[9] == 1234);


        StaticVector<int, 10> tempCopySv{moveSv};
        moveSv = std::move(copySv);
        REQUIRE(copySv.empty());
        REQUIRE(moveSv.empty());
        REQUIRE(moveSv.size() == 0);
        check_iteration_count(copySv, 0);
        check_iteration_count(moveSv, 0);
        const_check_iteration_count(copySv, 0);
        const_check_iteration_count(moveSv, 0);

        moveSv = std::move(tempCopySv);
        REQUIRE(copySv.empty());
        REQUIRE(!moveSv.empty());
        REQUIRE(moveSv.size() == 10);
        REQUIRE(moveSv.front() == 123);
        REQUIRE(moveSv.back() == 1234);
        REQUIRE(*moveSv.begin() == 123);
        check_iteration_count(copySv, 0);
        check_iteration_count(moveSv, 10);
        const_check_iteration_count(copySv, 0);
        const_check_iteration_count(moveSv, 10);

        REQUIRE(moveSv[0] == 123);
        REQUIRE(moveSv[1] == 234);
        REQUIRE(moveSv[2] == 345);
        REQUIRE(moveSv[3] == 456);
        REQUIRE(moveSv[4] == 567);
        REQUIRE(moveSv[5] == 678);
        REQUIRE(moveSv[6] == 789);
        REQUIRE(moveSv[7] == 891);
        REQUIRE(moveSv[8] == 912);
        REQUIRE(moveSv[9] == 1234);

        static_assert(sizeof(typename Detail::SizeType<255>::type) == 1, "SizeType wrong");
        static_assert(sizeof(typename Detail::SizeType<256>::type) == 2, "SizeType wrong");
        static_assert(sizeof(typename Detail::SizeType<65535>::type) == 2, "SizeType wrong");
        static_assert(sizeof(typename Detail::SizeType<65536>::type) == 4, "SizeType wrong");
        static_assert(sizeof(typename Detail::SizeType<4294967295>::type) == 4, "SizeType wrong");
        static_assert(sizeof(typename Detail::SizeType<4294967296>::type) == 8, "SizeType wrong");
        static_assert(sizeof(Detail::UninitializedArray<int, 0>) == 1, "SizeType wrong");
        static_assert(sizeof(Detail::UninitializedArray<int, 1>) == 4, "SizeType wrong");
        static_assert(sizeof(Detail::AlignedByteArray<int, 0>) == 1, "SizeType wrong");
        static_assert(sizeof(Detail::AlignedByteArray<int, 1>) == 4, "SizeType wrong");
        static_assert(sizeof(StaticVector<int, 0>) == 16, "sizeof static vector wrong");
        static_assert(sizeof(StaticVector<int, 1>) == 16, "sizeof static vector wrong");
        static_assert(sizeof(StaticVector<int, 2>) == 24, "sizeof static vector wrong");
        static_assert(Detail::is_sufficiently_trivial<int>, "type not sufficiently trivial");
        static_assert(Detail::is_sufficiently_trivial<noncopyable>, "type not sufficiently trivial");
        static_assert(!Detail::is_sufficiently_trivial<sufficiently_non_trival>, "type not sufficiently non-trivial");
        static_assert(std::contiguous_iterator<SVIterator<int, false>>, "iterator not contiguous");
        static_assert(std::contiguous_iterator<SVIterator<int, true>>, "iterator not contiguous");
        static_assert(std::contiguous_iterator<SVIterator<sufficiently_non_trival, false>>, "iterator not contiguous");
        static_assert(std::contiguous_iterator<SVIterator<sufficiently_non_trival, true>>, "iterator not contiguous");
        static_assert(std::is_trivially_destructible_v<StaticVector<int, 1>>, "No trivial destructor for trivial type");
        static_assert(!std::is_trivially_destructible_v<StaticVector<sufficiently_non_trival, 1>>, "trivial destructor for non-trivial type");

        auto const check_nontrivial_iteration_count = []<typename T, size_t N>(StaticVector<T, N> &_sv, int expected) {
            int iteratedCount{};
            iteratedCount = 0;
            for(auto &i : _sv) {
                ++iteratedCount;
            }
            REQUIRE(iteratedCount == expected);
        };

        StaticVector<sufficiently_non_trival, 10> nontrivial_sv;
        REQUIRE(nontrivial_sv.empty());
        REQUIRE(nontrivial_sv.size() == 0);
        REQUIRE(nontrivial_sv.max_size() == 10);
        REQUIRE(nontrivial_sv.capacity() == 10);
        REQUIRE(nontrivial_sv.begin() == nontrivial_sv.end());
        nontrivial_sv.clear();
        REQUIRE(nontrivial_sv.size() == 0);
        check_nontrivial_iteration_count(nontrivial_sv, 0);

        nontrivial_sv.emplace_back(123);
        REQUIRE(!nontrivial_sv.empty());
        REQUIRE(nontrivial_sv.size() == 1);
        REQUIRE(nontrivial_sv.front() == 123);
        REQUIRE(nontrivial_sv.back() == 123);
        REQUIRE(*nontrivial_sv.begin() == 123);
        check_nontrivial_iteration_count(nontrivial_sv, 1);

        nontrivial_sv.emplace_back(234);
        REQUIRE(!nontrivial_sv.empty());
        REQUIRE(nontrivial_sv.size() == 2);
        REQUIRE(nontrivial_sv.front() == 123);
        REQUIRE(nontrivial_sv.back() == 234);
        REQUIRE(*nontrivial_sv.begin() == 123);
        check_nontrivial_iteration_count(nontrivial_sv, 2);

        nontrivial_sv.clear();
        REQUIRE(nontrivial_sv.empty());
        REQUIRE(nontrivial_sv.size() == 0);
        REQUIRE(nontrivial_sv.max_size() == 10);
        REQUIRE(nontrivial_sv.capacity() == 10);
        REQUIRE(nontrivial_sv.begin() == nontrivial_sv.end());
        check_nontrivial_iteration_count(nontrivial_sv, 0);

        nontrivial_sv.resize(5);
        REQUIRE(!nontrivial_sv.empty());
        REQUIRE(nontrivial_sv.size() == 5);
        REQUIRE(nontrivial_sv.max_size() == 10);
        REQUIRE(nontrivial_sv.capacity() == 10);
        check_nontrivial_iteration_count(nontrivial_sv, 5);

        for(auto const &i : nontrivial_sv) {
            REQUIRE(i == 0);
        }

        nontrivial_sv.clear();
        nontrivial_sv.resize(5, sufficiently_non_trival{234});
        REQUIRE(!nontrivial_sv.empty());
        REQUIRE(nontrivial_sv.size() == 5);
        REQUIRE(nontrivial_sv.max_size() == 10);
        REQUIRE(nontrivial_sv.capacity() == 10);
        check_nontrivial_iteration_count(nontrivial_sv, 5);

        for(auto const &i : nontrivial_sv) {
            REQUIRE(i == 234);
        }

        {
            StaticVector<int, 4> eraseSv{1, 2, 3, 4};
            REQUIRE(!eraseSv.empty());
            REQUIRE(eraseSv.size() == 4);
            REQUIRE(eraseSv[0] == 1);
            REQUIRE(eraseSv[1] == 2);
            REQUIRE(eraseSv[2] == 3);
            REQUIRE(eraseSv[3] == 4);
            REQUIRE(eraseSv.size() == 4);

            auto erasePos = eraseSv.erase(eraseSv.cbegin() + 1, eraseSv.cbegin() + 3);
            REQUIRE(eraseSv.size() == 2);
            REQUIRE(eraseSv[0] == 1);
            REQUIRE(eraseSv[1] == 4);
            REQUIRE(erasePos == eraseSv.begin() + 1);

            erasePos = eraseSv.erase(eraseSv.cbegin());
            REQUIRE(eraseSv.size() == 1);
            REQUIRE(eraseSv[0] == 4);
            REQUIRE(erasePos == eraseSv.begin());

            eraseSv.pop_back();
            REQUIRE(eraseSv.empty());

            eraseSv.assign((std::size_t)3, 3);
            REQUIRE(eraseSv.size() == 3);
            REQUIRE(eraseSv[0] == 3);
            REQUIRE(eraseSv[1] == 3);
            REQUIRE(eraseSv[2] == 3);
        }

        {
            StaticVector<sufficiently_non_trival, 4> eraseNTSv{1, 2, 3, 4};
            REQUIRE(!eraseNTSv.empty());
            REQUIRE(eraseNTSv.size() == 4);
            REQUIRE(eraseNTSv[0] == 1);
            REQUIRE(eraseNTSv[1] == 2);
            REQUIRE(eraseNTSv[2] == 3);
            REQUIRE(eraseNTSv[3] == 4);
            REQUIRE(eraseNTSv.size() == 4);

            auto erasePos = eraseNTSv.erase(eraseNTSv.cbegin() + 1, eraseNTSv.cbegin() + 3);
            REQUIRE(eraseNTSv.size() == 2);
            REQUIRE(eraseNTSv[0] == 1);
            REQUIRE(eraseNTSv[1] == 4);
            REQUIRE(erasePos == eraseNTSv.begin() + 1);

            erasePos = eraseNTSv.erase(eraseNTSv.cbegin());
            REQUIRE(eraseNTSv.size() == 1);
            REQUIRE(eraseNTSv[0] == 4);
            REQUIRE(erasePos == eraseNTSv.begin());

            sufficiently_non_trival *ptr = eraseNTSv.data();
            REQUIRE(!ptr++->destructed);
            REQUIRE(ptr++->destructed);
            REQUIRE(ptr++->destructed);
            REQUIRE(ptr->destructed);

            eraseNTSv.pop_back();
            REQUIRE(eraseNTSv.empty());

            eraseNTSv.assign((std::size_t)3, 3);
            REQUIRE(eraseNTSv.size() == 3);
            REQUIRE(eraseNTSv[0] == 3);
            REQUIRE(eraseNTSv[1] == 3);
            REQUIRE(eraseNTSv[2] == 3);
        }

        {
            StaticVector<int, 6> sv_from{1, 2, 3, 4};
            StaticVector<int, 8> sv_to{};

            sv_to.assign(sv_from.begin(), sv_from.end());
            REQUIRE(sv_to.size() == 4);
            REQUIRE(sv_to[0] == 1);
            REQUIRE(sv_to[1] == 2);
            REQUIRE(sv_to[2] == 3);
            REQUIRE(sv_to[3] == 4);
        }
    }
}
