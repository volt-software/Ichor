#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <framework/Service.h>

namespace Cppelix {

    template <class T>
    class ISerializeMessage {

    };

    class IGet {

    };

    class IHttp {

    };

    class Serialization : public IHttp, public Service {
    public:
        Serialization();
        ~Serialization() final = default;

        static constexpr std::string_view version = "1.0.0";

        template <class T>
        std::vector<uint8_t> serialize(const T &obj);
        template <class T>
        T deserialize(const std::vector<uint8_t> &bytes);

        bool start() final;
        bool stop() final;
    private:
        std::unordered_map<std::string, ISerialization> _serializers;
    };
}

#endif //USE_SPDLOG