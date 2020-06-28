#pragma once

struct CallbackKey {
    uint64_t id;
    uint64_t type;

    bool operator==(const CallbackKey &other) const {
        return id == other.id && type == other.type;
    }
};

namespace std {
    template <>
    struct hash<CallbackKey> {
        std::size_t operator()(const CallbackKey& k) const {
            // TODO Terrible hashing method?

            return std::hash<uint64_t>()(k.id) ^ std::hash<uint64_t>()(k.type);
        }
    };
}