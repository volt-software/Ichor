#include <spdlog/spdlog.h>
#include <framework/DependencyManager.h>
#include <framework/interfaces/IFrameworkLogger.h>
#include <optional_bundles/logging_bundle/SpdlogFrameworkLogger.h>
#include "test.h"
#include "../include/framework/Bundle.h"
#include "../include/framework/Framework.h"
#include "../include/framework/SerializationAdmin.h"
#include "framework/ComponentLifecycleManager.h"

#ifdef USE_RAPIDJSON
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#endif

using namespace Cppelix;


#ifdef USE_RAPIDJSON
struct TestMsg {
    uint64_t id;
    std::string val;
};

class TestMsgJsonSerializer : public ISerializer, public Bundle {
public:
    ~TestMsgJsonSerializer() final = default;
    bool start() final {
        LOG_INFO(_logger, "TestMsgSerializer started");
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "TestMsgSerializer stopped");
        return true;
    }

    void addDependencyInstance(IFrameworkLogger *logger) {
        _logger = logger;
        LOG_INFO(_logger, "Inserted logger");
    }

    void removeDependencyInstance(IFrameworkLogger *logger) {
        _logger = nullptr;
    }

    void addDependencyInstance(ISerializationAdmin *serializationAdmin) {
        _serializationAdmin = serializationAdmin;
        LOG_INFO(_logger, "Inserted serializationAdmin");
        _serializationAdmin->addSerializer(typeName<TestMsg>(), this);
    }

    void removeDependencyInstance(ISerializationAdmin *serializationAdmin) {
        _serializationAdmin = nullptr;
        LOG_INFO(_logger, "Removed serializationAdmin");
    }

    void injectDependencyManager(DependencyManager *mng) final {}

    std::vector<uint8_t> serialize(const void* obj) final {
        auto msg = static_cast<const TestMsg*>(obj);
        rapidjson::StringBuffer sb;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

        writer.StartObject();

        writer.String("id");
        writer.Uint64(msg->id);

        writer.String("val");
        writer.String(msg->val.c_str(), msg->val.size());

        writer.EndObject();
        std::string str = sb.GetString();
        std::vector<uint8_t> ret;
        ret.reserve(str.length());
        std::move(str.begin(), str.end(), std::back_inserter(ret));
        return ret;
    }
    void* deserialize(const std::vector<uint8_t> &stream) final {
        std::string s{stream.begin(), stream.end()};
        rapidjson::Document d;
        d.ParseInsitu(s.data());

        if(d.HasParseError() || !d.HasMember("id") || !d.HasMember("val")) {
            LOG_ERROR(_logger, "stream not valid json");
            return nullptr;
        }

        return new TestMsg(d["id"].GetUint64(), d["val"].GetString());
    }

private:
    IFrameworkLogger *_logger;
    ISerializationAdmin *_serializationAdmin;
};
#endif

struct ITestBundle {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class TestBundle : public ITestBundle, public Bundle {
public:
    ~TestBundle() final = default;
    bool start() final {
        LOG_INFO(_logger, "TestBundle started with dependency");
        return true;
    }

    bool stop() final {
        LOG_INFO(_logger, "TestBundle stopped with dependency");
        return true;
    }

    void addDependencyInstance(IFrameworkLogger *logger) {
        _logger = logger;
        LOG_INFO(_logger, "Inserted logger");
    }

    void removeDependencyInstance(IFrameworkLogger *logger) {
        _logger = nullptr;
    }

    void addDependencyInstance(ISerializationAdmin *serializationAdmin) {
        _serializationAdmin = serializationAdmin;
        LOG_INFO(_logger, "Inserted serializationAdmin");
        TestMsg msg{20, "five hundred"};
        auto res = _serializationAdmin->serialize<TestMsg>(msg);
        auto msg2 = _serializationAdmin->deserialize<TestMsg>(res);
        if(msg2->id != msg.id || msg2->val != msg.val) {
            LOG_ERROR(_logger, "serde incorrect!");
        } else {
            LOG_ERROR(_logger, "serde correct!");
        }
    }

    void removeDependencyInstance(ISerializationAdmin *serializationAdmin) {
        _serializationAdmin = nullptr;
        LOG_INFO(_logger, "Removed serializationAdmin");
    }

    void injectDependencyManager(DependencyManager *mng) final {}

private:
    IFrameworkLogger *_logger;
    ISerializationAdmin *_serializationAdmin;
};

int main() {
    Framework fw{{}};
    DependencyManager dm{};
    auto logMgr = dm.createComponentManager<IFrameworkLogger, SpdlogFrameworkLogger>();
    auto serAdmin = dm.createDependencyComponentManager<ISerializationAdmin, SerializationAdmin>(RequiredList<IFrameworkLogger>, OptionalList<>);
#ifdef USE_RAPIDJSON
    auto testMsgSerializer = dm.createDependencyComponentManager<ISerializer, TestMsgJsonSerializer>(RequiredList<IFrameworkLogger, ISerializationAdmin>, OptionalList<>);
#endif
    auto testMgr = dm.createDependencyComponentManager<ITestBundle, TestBundle>(RequiredList<IFrameworkLogger, ISerializationAdmin>, OptionalList<>);
    dm.start();

    return 0;
}