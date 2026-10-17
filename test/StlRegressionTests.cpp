// Regression tests for bugs found while reviewing the Ichor STL implementations.
// System headers that define macros clashing with names in Ichor headers come first, as they do through e.g. Boost.Asio.
#if __has_include(<netdb.h>)
#include <netdb.h> // defines TRY_AGAIN, HOST_NOT_FOUND, NO_DATA, ...
#endif
#include "Common.h"
#include <ichor/stl/Any.h>
#include <ichor/stl/AsyncSingleThreadedMutex.h>
#include <ichor/stl/CompilerSpecific.h>
#include <ichor/stl/ConditionVariable.h>
#include <ichor/stl/ConditionVariableAny.h>
#include <ichor/stl/ErrnoUtils.h>
#include <ichor/stl/EventStackUniquePtr.h>
#include <ichor/stl/NeverAlwaysNull.h>
#include <ichor/stl/RealtimeMutex.h>
#include <ichor/stl/RealtimeReadWriteMutex.h>
#include <ichor/stl/ReferenceCountedPointer.h>
#include <ichor/stl/SectionalPriorityQueue.h>
#include <ichor/stl/Spans.h>
#include <ichor/stl/StaticVector.h>
#include <ichor/stl/StringUtils.h>
#include <fmt/format.h>
#include <atomic>
#include <iterator>
#include <chrono>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std::literals;

namespace {
    // Keeps track of which addresses contain a live object, so that assigning to/destroying raw storage is detected without sanitizers.
    std::set<void const *> liveTracked;

    struct Tracked {
        Tracked() : Tracked(0) {}
        Tracked(int _i) : i(_i) { REQUIRE(liveTracked.insert(this).second); }
        Tracked(Tracked const &o) : i(o.i), s(o.s) { REQUIRE(isLive(&o)); REQUIRE(liveTracked.insert(this).second); }
        Tracked(Tracked &&o) noexcept : i(o.i), s(std::move(o.s)) { REQUIRE(isLive(&o)); REQUIRE(liveTracked.insert(this).second); o.i = -1; }
        Tracked& operator=(Tracked const &o) { REQUIRE(isLive(this)); REQUIRE(isLive(&o)); i = o.i; s = o.s; return *this; }
        Tracked& operator=(Tracked &&o) noexcept { REQUIRE(isLive(this)); REQUIRE(isLive(&o)); i = o.i; s = std::move(o.s); o.i = -1; return *this; }
        ~Tracked() { REQUIRE(liveTracked.erase(this) == 1); }

        static bool isLive(void const *p) { return liveTracked.contains(p); }
        friend bool operator==(Tracked const &a, Tracked const &b) noexcept { return a.i == b.i; }
        friend bool operator<(Tracked const &a, Tracked const &b) noexcept { return a.i < b.i; }

        int i;
        // a heap allocated string, so that leaks/double frees show up in sanitizer builds as well
        std::string s{"a string that is long enough to not fit in the small string buffer"};
    };

    struct CopyOnly {
        CopyOnly(int _i) : i(_i) {}
        CopyOnly(CopyOnly const &) = default;
        CopyOnly(CopyOnly &&) = delete;
        CopyOnly& operator=(CopyOnly const &) = default;
        CopyOnly& operator=(CopyOnly &&) = delete;
        friend bool operator==(CopyOnly const &a, CopyOnly const &b) noexcept { return a.i == b.i; }
        friend void swap(CopyOnly &a, CopyOnly &b) noexcept { std::swap(a.i, b.i); }
        int i;
    };

    template <typename SV>
    std::vector<int> toInts(SV const &sv) {
        std::vector<int> ret;
        for(auto const &t : sv) {
            ret.push_back(t.i);
        }
        return ret;
    }

    struct BaseA { int a{1}; virtual ~BaseA() = default; };
    struct BaseB { int b{2}; virtual ~BaseB() = default; };
    struct Multi final : BaseA, BaseB { };

    int nodesDestroyed{};
    struct Node {
        ~Node() { ++nodesDestroyed; }
        ReferenceCountedPointer<Node> next;
    };

    uint64_t smallDestroyed{};
    struct SmallNonTrivial {
        SmallNonTrivial() = default;
        SmallNonTrivial(SmallNonTrivial const &) = default;
        SmallNonTrivial(SmallNonTrivial &&) noexcept = default;
        SmallNonTrivial& operator=(SmallNonTrivial const &) = default;
        SmallNonTrivial& operator=(SmallNonTrivial &&) noexcept = default;
        ~SmallNonTrivial() { ++smallDestroyed; }
        int i{};
    };

    struct StringEvent final : public Event {
        StringEvent(uint64_t _id, std::string _str) : Event(_id, ServiceIdType{0}, INTERNAL_EVENT_PRIORITY), str(std::move(_str)) {}
        StringEvent(StringEvent &&) noexcept = default;
        ~StringEvent() final { ++destroyed; }

        [[nodiscard]] std::string_view get_name() const noexcept final { return NAME; }
        [[nodiscard]] NameHashType get_type() const noexcept final { return TYPE; }

        std::string str;
        static inline uint64_t destroyed{};
        static constexpr NameHashType TYPE = typeNameHash<StringEvent>();
        static constexpr std::string_view NAME = typeName<StringEvent>();
    };

    constexpr int assumeCompilesInUnbracedIfElse(int x) {
        // ICHOR_ASSUME used to expand to a statement plus an extra ';', which broke if/else without braces.
        if(x > 0)
            ICHOR_ASSUME(x > 0);
        else
            x = 1;
        return x;
    }
}

// ---- compile-time checks ----

// FastAtoi on INT64_MIN used to negate a signed overflowed value, which is UB and thus rejected during constant evaluation.
static_assert(FastAtoi("-9223372036854775808") == std::numeric_limits<int64_t>::min());
static_assert(FastAtoi("9223372036854775807") == std::numeric_limits<int64_t>::max());
static_assert(FastAtoi("-1") == -1);
static_assert(FastAtoi("0") == 0);
static_assert(FastAtoi("-0") == 0);
static_assert(Ichor::v1::basename("a/b/c.cpp")[0] == 'c');
static_assert(assumeCompilesInUnbracedIfElse(5) == 5);

// Any::emplace returned std::decay<T>& (the trait struct) instead of std::decay_t<T>&
static_assert(std::is_same_v<decltype(std::declval<any&>().emplace<true, int>(5)), int&>);

// ReferenceCountedPointer used to convert between types where only the *values* were convertible, reinterpreting the storage
static_assert(!std::is_constructible_v<ReferenceCountedPointer<int>, ReferenceCountedPointer<double> const &>);
static_assert(!std::is_constructible_v<ReferenceCountedPointer<int>, ReferenceCountedPointer<double> &&>);
static_assert(!std::is_constructible_v<ReferenceCountedPointer<int>, double*>);
static_assert(!std::is_assignable_v<ReferenceCountedPointer<int>&, ReferenceCountedPointer<double> const &>);
static_assert(std::is_constructible_v<ReferenceCountedPointer<BaseB>, ReferenceCountedPointer<Multi> const &>);

// Native mutexes and condition variables must not be copied
static_assert(!std::is_copy_constructible_v<RealtimeMutex> && !std::is_move_constructible_v<RealtimeMutex>);
static_assert(!std::is_copy_assignable_v<RealtimeMutex> && !std::is_move_assignable_v<RealtimeMutex>);
static_assert(!std::is_copy_constructible_v<RealtimeReadWriteMutex> && !std::is_move_constructible_v<RealtimeReadWriteMutex>);
static_assert(!std::is_copy_assignable_v<RealtimeReadWriteMutex> && !std::is_move_assignable_v<RealtimeReadWriteMutex>);
static_assert(!std::is_copy_constructible_v<ConditionVariable> && !std::is_copy_assignable_v<ConditionVariable>);
static_assert(!std::is_copy_constructible_v<ConditionVariableAny<RealtimeReadWriteMutex>> && !std::is_copy_assignable_v<ConditionVariableAny<RealtimeReadWriteMutex>>);

// StaticVector iterators convert to const_iterators and capacity queries work on const vectors
static_assert(std::is_convertible_v<StaticVector<int, 4>::iterator, StaticVector<int, 4>::const_iterator>);
static_assert(!std::is_convertible_v<StaticVector<int, 4>::const_iterator, StaticVector<int, 4>::iterator>);
static_assert(StaticVector<int, 4>{}.capacity() == 4);
static_assert(std::contiguous_iterator<StaticVector<int, 4>::iterator>);

// VectorView over a temporary vector would dangle
static_assert(!std::is_constructible_v<VectorView<BaseA>, std::vector<Multi>&&>);
static_assert(std::is_constructible_v<VectorView<BaseA>, std::vector<Multi> const &>);

TEST_CASE("STL regression tests: StaticVector postfix increment/decrement return the old position") {
    {
        StaticVector<int, 4> v{1, 2, 3};
        auto it = v.begin();
        auto old = it++;
        REQUIRE(*old == 1);
        REQUIRE(*it == 2);
        old = it--;
        REQUIRE(*old == 2);
        REQUIRE(*it == 1);

        auto cit = v.cbegin();
        REQUIRE(*(cit++) == 1);
        REQUIRE(*cit == 2);
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: StaticVector ordering is lexicographical") {
    {
        StaticVector<int, 4> a{1, 2};
        StaticVector<int, 4> b{2};
        REQUIRE(a < b);
        REQUIRE(a <= b);
        REQUIRE(!(a > b));
        REQUIRE(!(a >= b));
        REQUIRE(b > a);
        REQUIRE(a != b);

        StaticVector<int, 4> c{1, 2};
        REQUIRE(a <= c);
        REQUIRE(a >= c);
        REQUIRE(!(a < c));

        // used to recurse through the operators that were added to namespace std
        std::vector<StaticVector<int, 4>> x{a, b};
        std::vector<StaticVector<int, 4>> y{a, b};
        REQUIRE(x == y);
        REQUIRE(std::equal(x.begin(), x.end(), y.begin()));
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: StaticVector self assignment keeps the contents") {
    {
        StaticVector<Tracked, 4> v{1, 2, 3};
        auto &ref = v;
        v = ref;
        REQUIRE(toInts(v) == std::vector<int>{1, 2, 3});
        v = std::move(ref);
        REQUIRE(toInts(v) == std::vector<int>{1, 2, 3});
        v.swap(ref);
        REQUIRE(toInts(v) == std::vector<int>{1, 2, 3});
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: StaticVector assign replaces the contents") {
    {
        StaticVector<int, 4> v{1, 2, 3};
        v.assign(1, 9); // int arguments used to pick the iterator overload
        REQUIRE(v.size() == 1);
        REQUIRE(v[0] == 9);
        v.assign(std::size_t{4}, 7);
        REQUIRE(v.size() == 4);
        REQUIRE(std::all_of(v.begin(), v.end(), [](int i) { return i == 7; }));

        StaticVector<Tracked, 6> t{1, 2, 3};
        std::vector<Tracked> src{Tracked{4}, Tracked{5}, Tracked{6}, Tracked{7}, Tracked{8}};
        t.assign(src.begin(), src.end()); // grows into raw storage
        REQUIRE(toInts(t) == std::vector<int>{4, 5, 6, 7, 8});
        t.assign(src.begin(), src.begin() + 2); // shrinks
        REQUIRE(toInts(t) == std::vector<int>{4, 5});
        t.assign(std::size_t{3}, Tracked{9});
        REQUIRE(toInts(t) == std::vector<int>{9, 9, 9});
        t.assign(std::size_t{1}, t[2]); // value aliases an element that gets destroyed
        REQUIRE(toInts(t) == std::vector<int>{9});
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: StaticVector constructors with integral arguments") {
    {
        StaticVector<int, 10> v(3, 5);
        REQUIRE(v.size() == 3);
        REQUIRE(std::all_of(v.begin(), v.end(), [](int i) { return i == 5; }));

        // the count used to be narrowed to the storage size type (unsigned char for small N)
        StaticVector<unsigned char, 300> big(std::size_t{260});
        REQUIRE(big.size() == 260);
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: StaticVector insert/emplace construct into raw storage and assign into live objects") {
    {
        StaticVector<Tracked, 10> v{1, 2, 3};
        Tracked const five{5};
        v.insert(v.cbegin() + 1, five);
        REQUIRE(toInts(v) == std::vector<int>{1, 5, 2, 3});
        v.insert(v.cbegin(), Tracked{0});
        REQUIRE(toInts(v) == std::vector<int>{0, 1, 5, 2, 3});
        v.insert(v.cend(), Tracked{9});
        REQUIRE(toInts(v) == std::vector<int>{0, 1, 5, 2, 3, 9});
        v.insert(v.cbegin() + 1, 2, five);
        REQUIRE(toInts(v) == std::vector<int>{0, 5, 5, 1, 5, 2, 3, 9});
        v.emplace(v.cbegin() + 2, 7);
        REQUIRE(toInts(v) == std::vector<int>{0, 5, 7, 5, 1, 5, 2, 3, 9});
        auto it = v.insert(v.cbegin() + 8, Tracked{8});
        REQUIRE(it == v.begin() + 8);
        REQUIRE(toInts(v) == std::vector<int>{0, 5, 7, 5, 1, 5, 2, 3, 8, 9});
        for(auto const &t : v) {
            REQUIRE(t.s.size() > 20); // the strings survived all the shifting
        }

        // inserting more elements than there are after the position
        StaticVector<Tracked, 10> w{1, 2};
        std::vector<Tracked> src{Tracked{3}, Tracked{4}, Tracked{5}};
        w.insert(w.cbegin() + 1, src.begin(), src.end());
        REQUIRE(toInts(w) == std::vector<int>{1, 3, 4, 5, 2});
        w.insert(w.cbegin() + 4, 4, Tracked{6});
        REQUIRE(toInts(w) == std::vector<int>{1, 3, 4, 5, 6, 6, 6, 6, 2});
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: StaticVector insert/emplace with values aliasing elements") {
    {
        StaticVector<Tracked, 8> v{1, 2, 3};
        v.insert(v.cbegin(), v[2]);
        REQUIRE(toInts(v) == std::vector<int>{3, 1, 2, 3});
        v.insert(v.cbegin(), 2, v.back());
        REQUIRE(toInts(v) == std::vector<int>{3, 3, 3, 1, 2, 3});
        v.insert(v.cbegin(), std::move(v[3]));
        REQUIRE(toInts(v) == std::vector<int>{1, 3, 3, 3, -1, 2, 3});
        v.emplace(v.cbegin(), v[5]);
        REQUIRE(toInts(v) == std::vector<int>{2, 1, 3, 3, 3, -1, 2, 3});
        v.resize(4, v[0]);
        REQUIRE(toInts(v) == std::vector<int>{2, 1, 3, 3});
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: StaticVector insert from single pass iterators") {
    {
        StaticVector<int, 8> v{1, 2};
        std::istringstream in("7 8 9");
        v.insert(v.cbegin() + 1, std::istream_iterator<int>{in}, std::istream_iterator<int>{});
        REQUIRE(v == StaticVector<int, 8>{1, 7, 8, 9, 2});
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: StaticVector with copy-only types") {
    {
        StaticVector<CopyOnly, 6> v{1, 2, 3};
        CopyOnly const four{4};
        v.insert(v.cbegin() + 1, four);
        v.emplace(v.cbegin(), 0);
        v.erase(v.begin() + 2);
        REQUIRE(v == StaticVector<CopyOnly, 6>{0, 1, 2, 3});
        StaticVector<CopyOnly, 6> w{9};
        v.swap(w);
        REQUIRE(v == StaticVector<CopyOnly, 6>{9});
        REQUIRE(w == StaticVector<CopyOnly, 6>{0, 1, 2, 3});
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: StaticVector swap between different sizes") {
    {
        StaticVector<Tracked, 8> a{1, 2, 3, 4, 5};
        StaticVector<Tracked, 8> b{6};
        a.swap(b);
        REQUIRE(toInts(a) == std::vector<int>{6});
        REQUIRE(toInts(b) == std::vector<int>{1, 2, 3, 4, 5});
        swap(a, b); // found through ADL
        REQUIRE(toInts(a) == std::vector<int>{1, 2, 3, 4, 5});
        REQUIRE(toInts(b) == std::vector<int>{6});
        StaticVector<Tracked, 8> empty;
        a.swap(empty);
        REQUIRE(a.empty());
        REQUIRE(toInts(empty) == std::vector<int>{1, 2, 3, 4, 5});
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: StaticVector erase/resize/const access") {
    {
        StaticVector<Tracked, 8> v{1, 2, 3, 4};
        v.erase(v.begin()); // iterator converts to const_iterator
        REQUIRE(toInts(v) == std::vector<int>{2, 3, 4});
        v.erase(v.begin(), v.begin() + 2);
        REQUIRE(toInts(v) == std::vector<int>{4});
        v.resize(3);
        REQUIRE(toInts(v) == std::vector<int>{4, 0, 0});
        v.resize(1);
        REQUIRE(toInts(v) == std::vector<int>{4});

        StaticVector<Tracked, 8> const &cv = v;
        REQUIRE(cv.capacity() == 8);
        REQUIRE(cv.max_size() == 8);
        IStaticVector<Tracked> const &iv = v;
        REQUIRE(iv.capacity() == 8);

        StaticVector<int, 0> zero;
        REQUIRE(zero.data() == nullptr);
        REQUIRE(zero.empty());
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: All StaticVector elements are destroyed") {
    {
        {
            StaticVector<Tracked, 8> v{1, 2, 3};
            v.insert(v.cbegin() + 1, Tracked{4});
            StaticVector<Tracked, 8> copy{v};
            StaticVector<Tracked, 8> moved{std::move(copy)};
            copy = moved;
            moved.clear();
        }
        REQUIRE(liveTracked.empty());
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: ReferenceCountedPointer copies of empty pointers") {
    {
        ReferenceCountedPointer<int> empty;
        ReferenceCountedPointer<int> copy{empty}; // used to dereference nullptr
        REQUIRE(!copy.has_value());
        REQUIRE(copy.use_count() == 0);
        ReferenceCountedPointer<int> other = make_reference_counted<int>(5);
        other = empty;
        REQUIRE(!other.has_value());
        ReferenceCountedPointer<BaseA> base{ReferenceCountedPointer<Multi>{}};
        REQUIRE(!base.has_value());
        ReferenceCountedPointer<Multi> emptyMulti;
        base = emptyMulti;
        REQUIRE(!base.has_value());
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: ReferenceCountedPointer adjusts pointers for multiple inheritance") {
    {
        ReferenceCountedPointer<BaseB> b{new Multi()};
        REQUIRE(b->b == 2);
        auto multi = make_reference_counted<Multi>();
        ReferenceCountedPointer<BaseB> b2{multi};
        REQUIRE(b2.get() == static_cast<BaseB*>(multi.get()));
        REQUIRE(b2->b == 2);
        REQUIRE(multi.use_count() == 2);
        ReferenceCountedPointer<BaseA> a2{multi};
        REQUIRE(a2->a == 1);
        REQUIRE(b2 == a2); // same object
        b2 = std::make_unique<Multi>();
        REQUIRE(b2->b == 2);
        REQUIRE(b2.use_count() == 1);
        REQUIRE(multi.use_count() == 2);
        b2 = new Multi();
        REQUIRE(b2->b == 2);
        ReferenceCountedPointer<BaseB> moved{std::move(multi)};
        REQUIRE(!multi.has_value());
        REQUIRE(moved->b == 2);
        REQUIRE(moved.use_count() == 2);
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: make_reference_counted without arguments constructs a value") {
    {
        auto i = make_reference_counted<int>(); // used to select the default constructor, returning an empty pointer
        REQUIRE(i.has_value());
        REQUIRE(*i == 0);
        REQUIRE(i.use_count() == 1);
        ReferenceCountedPointer<std::string> s{std::in_place};
        REQUIRE(s.has_value());
        REQUIRE(s->empty());
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: ReferenceCountedPointer from null pointers is empty") {
    {
        ReferenceCountedPointer<int> raw{static_cast<int*>(nullptr)};
        REQUIRE(!raw.has_value());
        REQUIRE(raw == nullptr);
        ReferenceCountedPointer<int> unique{std::unique_ptr<int>{}};
        REQUIRE(!unique.has_value());
        ReferenceCountedPointer<int> assigned = make_reference_counted<int>(1);
        assigned = static_cast<int*>(nullptr);
        REQUIRE(!assigned.has_value());
        assigned = std::unique_ptr<int>{};
        REQUIRE(!assigned.has_value());
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: ReferenceCountedPointer assignment from an object owned by the target") {
    {
        nodesDestroyed = 0;
        {
            auto head = make_reference_counted<Node>();
            head->next = make_reference_counted<Node>();
            head->next->next = make_reference_counted<Node>();
            // the source is owned by the node being released, it must stay alive until the assignment is done
            head = head->next;
            REQUIRE(nodesDestroyed == 1);
            REQUIRE(head.use_count() == 1);
            head = std::move(head->next);
            REQUIRE(nodesDestroyed == 2);
            REQUIRE(head.has_value());
            REQUIRE(!head->next.has_value());
        }
        REQUIRE(nodesDestroyed == 3);
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: ReferenceCountedPointer swap and bool") {
    {
        auto a = make_reference_counted<int>(1);
        auto b = make_reference_counted<int>(2);
        swap(a, b);
        REQUIRE(*a == 2);
        REQUIRE(*b == 1);
        std::swap(a, b);
        REQUIRE(*a == 1);
        REQUIRE(*b == 2);

        ReferenceCountedPointer<bool> t{true};
        ReferenceCountedPointer<bool> copy{t}; // non-const lvalue used to construct a new bool from the pointer
        REQUIRE(copy.use_count() == 2);
        REQUIRE(*copy);
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: AsyncSingleThreadedMutex lock guard move assignment releases the held lock") {
    {
        AsyncSingleThreadedMutex m1;
        AsyncSingleThreadedMutex m2;
        auto g1 = m1.non_blocking_lock();
        auto g2 = m2.non_blocking_lock();
        REQUIRE(g1);
        REQUIRE(g2);
        *g1 = std::move(*g2);
        REQUIRE(m1.non_blocking_lock()); // m1 is free again
        REQUIRE(!m2.non_blocking_lock()); // g1 now holds m2
        g1->unlock();
        REQUIRE(m2.non_blocking_lock());
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: Any moves destroy the moved-from inline object") {
    {
        {
            auto a = make_unformattable_any<SmallNonTrivial>();
            auto c = make_unformattable_any<SmallNonTrivial>();
            smallDestroyed = 0; // returning from make_any may or may not have moved
            auto b{std::move(a)};
            REQUIRE(smallDestroyed == 1); // a's moved-from object
            REQUIRE(!a.has_value());
            REQUIRE(a.type_hash() == 0);
            c = std::move(b);
            REQUIRE(smallDestroyed == 3); // c's previous value and b's moved-from object
            REQUIRE(!b.has_value());
            REQUIRE(b.type_hash() == 0);
            REQUIRE(b.type_name().empty());
        }
        REQUIRE(smallDestroyed == 4);

        // heap allocated values move by pointer
        auto big = make_any<std::string>(std::string(100, 'x'));
        auto movedBig{std::move(big)};
        REQUIRE(any_cast<std::string&>(movedBig).size() == 100);
        big = std::move(movedBig);
        REQUIRE(any_cast<std::string&>(big).size() == 100);
        REQUIRE(!movedBig.has_value());
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: Any emplace returns a reference to the stored value and pointer casts are like std::any_cast") {
    {
        any a;
        auto &i = a.emplace<true, int>(5);
        i = 7;
        REQUIRE(any_cast<int>(a) == 7);

        REQUIRE(any_cast<int*>(&a) != nullptr);
        *any_cast<int*>(&a) = 8;
        any const &ca = a;
        REQUIRE(*any_cast<int const*>(&ca) == 8);
        REQUIRE(any_cast<float const*>(&ca) == nullptr);
        REQUIRE(any_cast<float*>(&a) == nullptr);
        REQUIRE(any_cast<int const*>(static_cast<any const*>(nullptr)) == nullptr);
        any empty;
        REQUIRE(any_cast<int*>(&empty) == nullptr);

        // identity comparison
        auto copy = a;
        REQUIRE(a == a);
        REQUIRE(!(a == copy));
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: EventStackUniquePtr moves events properly") {
    {
        {
            EventStackUniquePtr empty;
            EventStackUniquePtr fromEmpty{std::move(empty)};
            REQUIRE(fromEmpty.empty()); // used to become "non-empty" and destroy garbage

            auto shortStr = EventStackUniquePtr::create<StringEvent>(1, "short");
            auto longStr = EventStackUniquePtr::create<StringEvent>(2, std::string(100, 'l'));
            REQUIRE(shortStr.getType() == StringEvent::TYPE);
            StringEvent::destroyed = 0; // returning from create may or may not have moved

            // libstdc++'s std::string points into itself for short strings, so a byte copy would leave a dangling pointer
            EventStackUniquePtr moved{std::move(shortStr)};
            REQUIRE(shortStr.empty());
            REQUIRE(moved.getT<StringEvent>()->str == "short");
            REQUIRE(moved.get()->id == 1);

            REQUIRE(StringEvent::destroyed == 1); // the moved-from source
            moved = std::move(longStr);
            REQUIRE(StringEvent::destroyed == 3); // the overwritten short one + the moved-from long one
            REQUIRE(moved.getT<StringEvent>()->str == std::string(100, 'l'));
            auto &self = moved;
            moved = std::move(self);
            REQUIRE(moved.getT<StringEvent>()->str == std::string(100, 'l'));
            moved = std::move(empty);
            REQUIRE(moved.empty());
            REQUIRE(StringEvent::destroyed == 4);
        }
        REQUIRE(StringEvent::destroyed == 4);
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: NeverNull") {
    {
        int i{};
        NeverNull<int*> nn{&i};
        REQUIRE(nn.get() == &i);
        REQUIRE(std::hash<NeverNull<int*>>{}(nn) == std::hash<int*>{}(&i));
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: String utilities") {
    {
        REQUIRE(FastAtoi(std::to_string(std::numeric_limits<int64_t>::min()).c_str()) == std::numeric_limits<int64_t>::min());
        REQUIRE(FastAtoi("-1234567891011") == -1234567891011);
        REQUIRE(FastAtoi("-") == 0);
        REQUIRE(FastAtoi("") == 0);

        // empty delimiter used to loop forever
        REQUIRE(split("abc", "", false) == std::vector<std::string_view>{"abc"});
        REQUIRE(split("", "", false).empty());
        int calls{};
        split("abc", "", true, [&](std::string_view s) { REQUIRE(s == "abc"); ++calls; });
        REQUIRE(calls == 1);

        // the components used to be parsed until a null terminator, reading past the end of the view
        std::string_view const versionWithTrailer = "1.2.34";
        auto v = parseStringAsVersion(versionWithTrailer.substr(0, 5));
        REQUIRE(v);
        REQUIRE(*v == Version{1, 2, 3});
        REQUIRE(!parseStringAsVersion("1..23"));
        REQUIRE(!parseStringAsVersion(".1.23"));
        REQUIRE(!parseStringAsVersion("99999999999999999999.1.1")); // overflow
        REQUIRE(parseStringAsVersion("18446744073709551615.1.1") == Version{std::numeric_limits<uint64_t>::max(), 1, 1});

        REQUIRE(!SafeHexToUint(""));
        REQUIRE(SafeHexToUint("0") == 0u);
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: errno mapping") {
    {
        REQUIRE(mapErrnoToError(EFAULT) == IOError::BAD_ADDRESS);
        REQUIRE(mapErrnoToError(EACCES) == IOError::NO_PERMISSION);
        REQUIRE(mapErrnoToError(EINVAL) == IOError::INVALID_ARGUMENT);
        REQUIRE(mapErrnoToError(EAGAIN) == IOError::WOULD_BLOCK);
        REQUIRE(mapErrnoToError(EWOULDBLOCK) == IOError::WOULD_BLOCK);
        REQUIRE(mapErrnoToError(EEXIST) == IOError::ALREADY_EXISTS);
        REQUIRE(mapErrnoToError(EFBIG) == IOError::FILE_SIZE_TOO_BIG);
        REQUIRE(mapErrnoToError(ENOSYS) == IOError::KERNEL_TOO_OLD);
        REQUIRE(mapErrnoToError(EPIPE) == IOError::BROKEN_PIPE);
        REQUIRE(mapErrnoToError(ECONNREFUSED) == IOError::CONNECTION_REFUSED);
        REQUIRE(mapErrnoToError(ETIMEDOUT) == IOError::TIMED_OUT);
        REQUIRE(mapErrnoToError(ENOENT) == IOError::FILE_DOES_NOT_EXIST);
        REQUIRE(mapErrnoToError(0) == IOError::FAILED);
        REQUIRE(fmt::format("{}", IOError::TIMED_OUT) == "TIMED_OUT");
        REQUIRE(fmt::format("{}", IOError::BAD_ADDRESS) == "BAD_ADDRESS");
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: ConditionVariable wait_for doesn't extend its timeout on every notify") {
    {
        RealtimeMutex m;
        ConditionVariable cv;
        std::atomic<bool> stop{};
        // keeps notifying, without ever making the predicate true
        std::thread notifier([&]() {
            // stops by itself as well, so that a broken wait_for fails the test instead of hanging it
            auto const notifyUntil = std::chrono::steady_clock::now() + 1s;
            while(!stop.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < notifyUntil) {
                cv.notify_all();
                std::this_thread::sleep_for(5ms);
            }
        });

        auto const start = std::chrono::steady_clock::now();
        {
            std::unique_lock lock(m);
            REQUIRE(!cv.wait_for(lock, 100ms, []() { return false; }));
        }
        auto const elapsed = std::chrono::steady_clock::now() - start;
        stop.store(true, std::memory_order_release);
        notifier.join();
        REQUIRE(elapsed >= 100ms);
        REQUIRE(elapsed < 900ms); // used to wait until the notifications stopped
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: ConditionVariableAny wait_for doesn't extend its timeout on every notify") {
    {
        RealtimeReadWriteMutex m;
        ConditionVariableAny<RealtimeReadWriteMutex> cv;
        std::atomic<bool> stop{};
        std::thread notifier([&]() {
            // stops by itself as well, so that a broken wait_for fails the test instead of hanging it
            auto const notifyUntil = std::chrono::steady_clock::now() + 1s;
            while(!stop.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < notifyUntil) {
                cv.notify_all();
                std::this_thread::sleep_for(5ms);
            }
        });

        auto const start = std::chrono::steady_clock::now();
        {
            std::unique_lock lock(m);
            REQUIRE(!cv.wait_for(lock, 100ms, []() { return false; }));
        }
        auto const elapsed = std::chrono::steady_clock::now() - start;
        stop.store(true, std::memory_order_release);
        notifier.join();
        REQUIRE(elapsed >= 100ms);
        REQUIRE(elapsed < 900ms);

        cv_status status = cv_status::timeout; // available on all platforms
        REQUIRE(status == cv_status::timeout);
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: RealtimeReadWriteMutex shared locking") {
    {
        RealtimeReadWriteMutex m;
        m.lock_shared();
        REQUIRE(!m.try_lock());
        m.unlock_shared();
        REQUIRE(m.try_lock());
        REQUIRE(!m.try_lock_shared());
        m.unlock();
        REQUIRE(m.try_lock_shared());
        m.unlock_shared();
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: SectionalPriorityQueue pop returns the top element") {
    {
        SectionalPriorityQueue<std::unique_ptr<int>, decltype([](auto const &a, auto const &b) { return *a < *b; })> q;
        q.push(std::make_unique<int>(2));
        q.push(std::make_unique<int>(5));
        q.emplace(std::make_unique<int>(3));
        REQUIRE(*q.pop() == 5);
        REQUIRE(*q.pop() == 3);
        REQUIRE(*q.pop() == 2);
        REQUIRE(q.empty());
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: VectorView iterators") {
    {
        std::vector<Multi> vec(3);
        VectorView<BaseB> view{vec};
        REQUIRE(view.size() == 3);
        REQUIRE(view[1]->b == 2);
        REQUIRE(std::distance(view.begin(), view.end()) == 3);
        auto it = view.begin();
        REQUIRE(it + 3 == view.end());
        REQUIRE(it < view.end());
        REQUIRE((*it).get() == static_cast<BaseB const*>(&vec[0]));
    }

    REQUIRE(liveTracked.empty());
}

TEST_CASE("STL regression tests: StaticVector inserting nothing keeps the elements") {
    // long enough to not fit in the small string buffer, a self-move-assignment would empty them
    std::string const longStr(64, 'a');
    StaticVector<std::string, 8> v{longStr, longStr + "b", longStr + "c"};
    std::vector<std::string> const empty;
    v.insert(v.begin(), 0, longStr);
    v.insert(v.begin() + 1, empty.begin(), empty.end());
    REQUIRE(v.size() == 3);
    REQUIRE(v[0] == longStr);
    REQUIRE(v[1] == longStr + "b");
    REQUIRE(v[2] == longStr + "c");
}

TEST_CASE("STL regression tests: VectorView over a vector of unique_ptrs") {
    std::vector<std::unique_ptr<Multi>> vec;
    vec.emplace_back(std::make_unique<Multi>());
    vec.emplace_back(std::make_unique<Multi>());
    VectorView<BaseB> view{vec};
    REQUIRE(view.size() == 2);
    REQUIRE(view[0].get() == static_cast<BaseB const*>(vec[0].get()));
    REQUIRE((*(view.begin() + 1)).get() == static_cast<BaseB const*>(vec[1].get()));
}
