#include <ichor/services/network/ssl/openssl/OpenSSLFactory.h>

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::v1::OpenSSLFactory::start() {

    co_return {};
}

Ichor::Task<> Ichor::v1::OpenSSLFactory::stop() {
    co_return;
}
