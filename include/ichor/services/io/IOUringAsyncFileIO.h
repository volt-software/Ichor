#pragma once

#include <ichor/services/io/IAsyncFileIO.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>

struct io_uring;

namespace Ichor {
    class IOUringAsyncFileIO final : public IAsyncFileIO, public AdvancedService<IOUringAsyncFileIO> {
    public:
        IOUringAsyncFileIO(DependencyRegister &reg, Properties props);

        Task<tl::expected<std::string, FileIOError>> readWholeFile(std::filesystem::path const &file) final;
        Task<tl::expected<void, FileIOError>> copyFile(std::filesystem::path const &from, std::filesystem::path const &to) final;
        Task<tl::expected<void, FileIOError>> removeFile(std::filesystem::path const &file) final;
        Task<tl::expected<void, FileIOError>> writeFile(std::filesystem::path const &file, std::string_view contents) final;
        Task<tl::expected<void, FileIOError>> appendFile(std::filesystem::path const &file, std::string_view contents) final;

        void addDependencyInstance(IIOUringQueue &, IService&) noexcept;
        void removeDependencyInstance(IIOUringQueue &, IService&) noexcept;
        void addDependencyInstance(ILogger &, IService&) noexcept;
        void removeDependencyInstance(ILogger &, IService&) noexcept;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void recursiveSubmitReadWholeFile(AsyncManualResetEvent &evt, int &res, std::string &contents, uint64_t &sizeLeft, uint64_t offset, uint64_t fileSize, int fd, uint32_t maxSubmissions);
        std::function<void(int32_t)> createNewEventHandlerReadWholeFile(AsyncManualResetEvent &evt, int &res, std::string &contents, uint64_t &sizeLeft, uint64_t fileSize, int fd, uint32_t maxSubmissions, uint64_t offset, char *buf, bool isLastInBatch);
        void recursiveSubmitCopyFile(AsyncManualResetEvent &evt, int &res, uint64_t &sizeLeft, uint64_t offset, uint64_t fileSize, int fromFd, int toFd, uint32_t maxSubmissions);
        std::function<void(int32_t)> createNewEventHandlerCopyFileReadPart(AsyncManualResetEvent &evt, int &res, uint64_t &sizeLeft, uint64_t fileSize, int fromFd, int toFd, uint32_t maxSubmissions, uint64_t offset, char *buf, bool isLastInBatch);
        std::function<void(int32_t)> createNewEventHandlerCopyFileWritePart(Ichor::AsyncManualResetEvent &evt, int &res, char *buf, bool isLast);

        IIOUringQueue *_q{};
        ILogger *_logger{};
        uint32_t _bufferSize{64*1024};
        uint32_t _bufferBatchSize{64};
        bool _shouldStop{};
    };
}
