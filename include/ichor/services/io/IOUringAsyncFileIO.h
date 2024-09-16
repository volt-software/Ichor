#pragma once

#ifndef ICHOR_USE_LIBURING
#error "Ichor has not been compiled with io_uring support"
#endif

#include <ichor/services/io/IAsyncFileIO.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>

struct io_uring;

namespace Ichor {
    class IOUringAsyncFileIO final : public IAsyncFileIO, public AdvancedService<IOUringAsyncFileIO> {
    public:
        IOUringAsyncFileIO(DependencyRegister &reg, Properties props);

        Task<tl::expected<std::string, IOError>> readWholeFile(std::filesystem::path const &file) final;
        Task<tl::expected<void, IOError>> copyFile(std::filesystem::path const &from, std::filesystem::path const &to) final;
        Task<tl::expected<void, IOError>> removeFile(std::filesystem::path const &file) final;
        Task<tl::expected<void, IOError>> writeFile(std::filesystem::path const &file, std::string_view contents) final;
        Task<tl::expected<void, IOError>> appendFile(std::filesystem::path const &file, std::string_view contents) final;

        void addDependencyInstance(IIOUringQueue &, IService&) noexcept;
        void removeDependencyInstance(IIOUringQueue &, IService&) noexcept;
        void addDependencyInstance(ILogger &, IService&) noexcept;
        void removeDependencyInstance(ILogger &, IService&) noexcept;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void recursiveSubmitReadWholeFile(AsyncManualResetEvent &evt, int &res, std::string &contents, uint64_t &sizeLeft, uint64_t offset, uint64_t fileSize, int fd, uint32_t maxSubmissions);
        std::function<void(io_uring_cqe*)> createNewEventHandlerReadWholeFile(AsyncManualResetEvent &evt, int &res, std::string &contents, uint64_t &sizeLeft, uint64_t fileSize, int fd, uint32_t maxSubmissions, uint64_t offset, char *buf, bool isLastInBatch);
        void recursiveSubmitCopyFile(AsyncManualResetEvent &evt, int &res, uint64_t &sizeLeft, uint64_t offset, uint64_t fileSize, int fromFd, int toFd, uint32_t maxSubmissions);
        std::function<void(io_uring_cqe*)> createNewEventHandlerCopyFileReadPart(AsyncManualResetEvent &evt, int &res, uint64_t &sizeLeft, uint64_t fileSize, int fromFd, int toFd, uint32_t maxSubmissions, uint64_t offset, char *buf, bool isLastInBatch);
        std::function<void(io_uring_cqe*)> createNewEventHandlerCopyFileWritePart(Ichor::AsyncManualResetEvent &evt, int &res, char *buf, bool isLastInBatch);

        IIOUringQueue *_q{};
        ILogger *_logger{};
        uint32_t _bufferSize{64*1024};
        uint32_t _bufferBatchSize{64};
        bool _shouldStop{};
    };
}
