#pragma once

#include <spdlog/spdlog.h>
#include <framework/DependencyManager.h>
#include <optional_bundles/logging_bundle/Logger.h>
#include "framework/Service.h"
#include "framework/ServiceLifecycleManager.h"
#include "framework/CommunicationChannel.h"
#include "celix_shim_library/CelixShimLibrary.h"

using namespace Cppelix;


struct ICelixService : virtual public IService {
    static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};
};

class CelixService final : public ICelixService, public Service {
public:
    ~CelixService() final = default;
    bool start() final;

    bool stop() final;

    void addDependencyInstance(ILogger *logger);

    void removeDependencyInstance(ILogger *logger);

    CelixBundleContext* startCelixBundle(void *dlRet);
    void stopCelixBundle(void *dlRet, CelixBundleContext *ctx);

private:
    ILogger *_logger;
    std::thread _t;
};