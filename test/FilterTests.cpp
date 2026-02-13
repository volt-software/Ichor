#include "Common.h"
#include "FakeLifecycleManager.h"
#include <ichor/Filter.h>

struct Interface1{};
struct Interface2{};

using namespace Ichor;
using namespace Ichor::v1;

TEST_CASE("FilterTests: PropertiesFilterEntry") {
    {
        PropertiesFilterEntry<bool, false> f{"TestProp", true};
        FakeLifecycleManager fm;
        REQUIRE(!f.matches(fm));
        fm._properties.emplace("TestProp", Ichor::v1::make_any<int>(5));
        REQUIRE(!f.matches(fm));
        fm._properties.clear();
        fm._properties.emplace("TestProp", Ichor::v1::make_any<bool>(false));
        REQUIRE(!f.matches(fm));
        fm._properties.clear();
        fm._properties.emplace("TestProp", Ichor::v1::make_any<bool>(true));
        REQUIRE(f.matches(fm));
        fm._properties.clear();
    }
    {
        PropertiesFilterEntry<bool, true> f{"TestProp", true};
        FakeLifecycleManager fm;
        REQUIRE(f.matches(fm));
        fm._properties.emplace("TestProp", Ichor::v1::make_any<int>(5));
        REQUIRE(f.matches(fm));
        fm._properties.clear();
        fm._properties.emplace("TestProp", Ichor::v1::make_any<bool>(false));
        REQUIRE(!f.matches(fm));
        fm._properties.clear();
        fm._properties.emplace("TestProp", Ichor::v1::make_any<bool>(true));
        REQUIRE(f.matches(fm));
        fm._properties.clear();
    }
}

TEST_CASE("FilterTests: DependencyPropertiesFilterEntry") {
    {
        DependencyPropertiesFilterEntry<Interface1, bool, false> f{"TestProp", true};
        FakeLifecycleManager fm;
        DependencyRegister dr;
        REQUIRE(!f.matches(fm));
        fm._dependencyRegistry = &dr;
        REQUIRE(!f.matches(fm));
        dr._registrations.emplace(
            typeNameHash<Interface1>(),
            std::make_tuple(
                Dependency{typeNameHash<Interface1>(), typeName<Interface1>(), DependencyFlags::NONE, 0},
                std::function<void(NeverNull<void*>, IService&)>{},
                std::function<void(NeverNull<void*>, IService&)>{},
                tl::optional<Properties>{})
            );
        REQUIRE(!f.matches(fm));
        dr._registrations.emplace(
            typeNameHash<Interface1>(),
            std::make_tuple(
                Dependency{typeNameHash<Interface1>(), typeName<Interface1>(), DependencyFlags::NONE, 0},
                std::function<void(NeverNull<void*>, IService&)>{},
                std::function<void(NeverNull<void*>, IService&)>{},
                Properties{})
            );
        REQUIRE(!f.matches(fm));
        dr._registrations.clear();
        dr._registrations.emplace(
            typeNameHash<Interface1>(),
            std::make_tuple(
                Dependency{typeNameHash<Interface1>(), typeName<Interface1>(), DependencyFlags::NONE, 0},
                std::function<void(NeverNull<void*>, IService&)>{},
                std::function<void(NeverNull<void*>, IService&)>{},
                Properties{{"TestProp", Ichor::v1::make_any<int>(5)}})
            );
        REQUIRE(!f.matches(fm));
        dr._registrations.clear();
        dr._registrations.emplace(
            typeNameHash<Interface1>(),
            std::make_tuple(
                Dependency{typeNameHash<Interface1>(), typeName<Interface1>(), DependencyFlags::NONE, 0},
                std::function<void(NeverNull<void*>, IService&)>{},
                std::function<void(NeverNull<void*>, IService&)>{},
                Properties{{"TestProp", Ichor::v1::make_any<bool>(false)}})
            );
        REQUIRE(!f.matches(fm));
        dr._registrations.clear();
        dr._registrations.emplace(
            typeNameHash<Interface1>(),
            std::make_tuple(
                Dependency{typeNameHash<Interface1>(), typeName<Interface1>(), DependencyFlags::NONE, 0},
                std::function<void(NeverNull<void*>, IService&)>{},
                std::function<void(NeverNull<void*>, IService&)>{},
                Properties{{"TestProp", Ichor::v1::make_any<bool>(true)}})
            );
        REQUIRE(f.matches(fm));
        dr._registrations.clear();
        dr._registrations.emplace(
            typeNameHash<Interface2>(),
            std::make_tuple(
                Dependency{typeNameHash<Interface2>(), typeName<Interface2>(), DependencyFlags::NONE, 0},
                std::function<void(NeverNull<void*>, IService&)>{},
                std::function<void(NeverNull<void*>, IService&)>{},
                Properties{{"TestProp", Ichor::v1::make_any<bool>(true)}})
            );
        REQUIRE(!f.matches(fm));
    }
    {
        DependencyPropertiesFilterEntry<Interface1, bool, true> f{"TestProp", true};
        FakeLifecycleManager fm;
        DependencyRegister dr;
        REQUIRE(f.matches(fm));
        fm._dependencyRegistry = &dr;
        REQUIRE(f.matches(fm));
        dr._registrations.emplace(
            typeNameHash<Interface1>(),
            std::make_tuple(
                Dependency{typeNameHash<Interface1>(), typeName<Interface1>(), DependencyFlags::NONE, 0},
                std::function<void(NeverNull<void*>, IService&)>{},
                std::function<void(NeverNull<void*>, IService&)>{},
                tl::optional<Properties>{})
            );
        REQUIRE(f.matches(fm));
        dr._registrations.emplace(
            typeNameHash<Interface1>(),
            std::make_tuple(
                Dependency{typeNameHash<Interface1>(), typeName<Interface1>(), DependencyFlags::NONE, 0},
                std::function<void(NeverNull<void*>, IService&)>{},
                std::function<void(NeverNull<void*>, IService&)>{},
                Properties{})
            );
        REQUIRE(f.matches(fm));
        dr._registrations.clear();
        dr._registrations.emplace(
            typeNameHash<Interface1>(),
            std::make_tuple(
                Dependency{typeNameHash<Interface1>(), typeName<Interface1>(), DependencyFlags::NONE, 0},
                std::function<void(NeverNull<void*>, IService&)>{},
                std::function<void(NeverNull<void*>, IService&)>{},
                Properties{{"TestProp", Ichor::v1::make_any<int>(5)}})
            );
        REQUIRE(f.matches(fm));
        dr._registrations.clear();
        dr._registrations.emplace(
            typeNameHash<Interface1>(),
            std::make_tuple(
                Dependency{typeNameHash<Interface1>(), typeName<Interface1>(), DependencyFlags::NONE, 0},
                std::function<void(NeverNull<void*>, IService&)>{},
                std::function<void(NeverNull<void*>, IService&)>{},
                Properties{{"TestProp", Ichor::v1::make_any<bool>(false)}})
            );
        REQUIRE(!f.matches(fm));
        dr._registrations.clear();
        dr._registrations.emplace(
            typeNameHash<Interface1>(),
            std::make_tuple(
                Dependency{typeNameHash<Interface1>(), typeName<Interface1>(), DependencyFlags::NONE, 0},
                std::function<void(NeverNull<void*>, IService&)>{},
                std::function<void(NeverNull<void*>, IService&)>{},
                Properties{{"TestProp", Ichor::v1::make_any<bool>(true)}})
            );
        REQUIRE(f.matches(fm));
        dr._registrations.clear();
        dr._registrations.emplace(
            typeNameHash<Interface2>(),
            std::make_tuple(
                Dependency{typeNameHash<Interface2>(), typeName<Interface2>(), DependencyFlags::NONE, 0},
                std::function<void(NeverNull<void*>, IService&)>{},
                std::function<void(NeverNull<void*>, IService&)>{},
                Properties{{"TestProp", Ichor::v1::make_any<bool>(true)}})
            );
        REQUIRE(f.matches(fm));
    }
}
