# Serializer Example

This example shows how to create and use a JSON serializer. The serializer itself is located in the [common](../common) directory.

To implement your own, create a class that inherits from an `ISerializer<T>` where `T` is the class/struct you want to serialize.

This then forces you to implement two functions:
```c++
class MyMsg {
    int id;
};

class MySerializer final : public ISerializer<MyMsg> {
public:
    std::vector<uint8_t> serialize(MyMsg const &msg) final {
        // return a vector<uint8_t> containing the binary format of your serialization
        return std::vector<uint8_t>{};
    }

    std::optional<MyMsg> deserialize(std::vector<uint8_t> &&stream) final {
        // deserialize and return a MyMsg with the right id
        return MyMsg{};
    }
};
```
