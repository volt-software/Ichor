#include "CelixService.h"
#include "celix_shim_library/CelixShimLibrary.h"
#include <dlfcn.h>
#include <celix_api.h>

bool CelixService::start() {

    LOG_INFO(_logger, "CelixService started with dependency");

    _t = std::thread([this] {
        auto dlConsumerRet = dlopen("./libconsumer_exampled.so", RTLD_NOW);
        if(dlConsumerRet == nullptr) {
            auto err = dlerror();
            LOG_ERROR(_logger, "Couldn't open libconsumer_exampled.so: {}", err);
            getManager()->pushEvent<QuitEvent>(getServiceId());
            return;
        }

        auto dlProviderRet = dlopen("./libprovider_exampled.so", RTLD_NOW);
        if(dlProviderRet == nullptr) {
            auto err = dlerror();
            LOG_ERROR(_logger, "Couldn't open libprovider_exampled.so: {}", err);
            dlclose(dlConsumerRet);
            getManager()->pushEvent<QuitEvent>(getServiceId());
            return;
        }

        auto providerCtx = startCelixBundle(dlProviderRet);
        auto consumerCtx = startCelixBundle(dlConsumerRet);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        stopCelixBundle(dlConsumerRet, consumerCtx);
        stopCelixBundle(dlProviderRet, providerCtx);

        getManager()->pushEvent<QuitEvent>(getServiceId());
    });

    return true;
}
bool CelixService::stop() {

    _t.join();
    LOG_INFO(_logger, "CelixService stopped with dependency");
    return true;
}
void CelixService::addDependencyInstance(ILogger *logger) {

    _logger = logger;

    LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", logger->getServiceId(), getServiceId());
}
void CelixService::removeDependencyInstance(ILogger *logger) {

}

CelixBundleContext* CelixService::startCelixBundle(void *dlRet) {
    void *userData = nullptr;
    auto ctx = getManager()->createServiceManager<ICelixBundleContext, CelixBundleContext>(RequiredList<ILogger>, OptionalList<>);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto create = (create_function_fp)dlsym(dlRet, OSGI_FRAMEWORK_BUNDLE_ACTIVATOR_CREATE);
    if(create == nullptr) {
        create = (create_function_fp)dlsym(dlRet, OSGI_FRAMEWORK_DEPRECATED_BUNDLE_ACTIVATOR_CREATE);
    }
    auto start = (start_function_fp)dlsym(dlRet, OSGI_FRAMEWORK_BUNDLE_ACTIVATOR_START);
    if(start == nullptr) {
        start = (start_function_fp)dlsym(dlRet, OSGI_FRAMEWORK_DEPRECATED_BUNDLE_ACTIVATOR_START);
    }

    LOG_TRACE(_logger, "create/start calls");
    create(reinterpret_cast<celix_bundle_context*>(ctx), &userData);
    start(userData, reinterpret_cast<celix_bundle_context*>(ctx));
    ctx->setUserData(userData);
    return ctx;
}

void CelixService::stopCelixBundle(void *dlRet, CelixBundleContext *ctx) {
    auto stop = (stop_function_fp)dlsym(dlRet, OSGI_FRAMEWORK_BUNDLE_ACTIVATOR_STOP);
    if(stop == nullptr) {
        stop = (stop_function_fp)dlsym(dlRet, OSGI_FRAMEWORK_DEPRECATED_BUNDLE_ACTIVATOR_STOP);
    }
    auto destroy = (destroy_function_fp)dlsym(dlRet, OSGI_FRAMEWORK_BUNDLE_ACTIVATOR_DESTROY);
    if(destroy == nullptr) {
        destroy = (destroy_function_fp)dlsym(dlRet, OSGI_FRAMEWORK_DEPRECATED_BUNDLE_ACTIVATOR_DESTROY);
    }

    LOG_TRACE(_logger, "stop/destroy calls");
    stop(ctx->getUserData(), reinterpret_cast<celix_bundle_context*>(ctx));
    destroy(ctx->getUserData(), reinterpret_cast<celix_bundle_context*>(ctx));
    dlclose(dlRet);
}
