#include <ichor/event_queues/IIOUringQueue.h>
#include <ichor/services/io/IOUringAsyncFileIO.h>
#include <ichor/ichor_liburing.h>
#include <fcntl.h>
#include <sys/stat.h>


Ichor::IOUringAsyncFileIO::IOUringAsyncFileIO(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
    reg.registerDependency<IIOUringQueue>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<ILogger>(this, DependencyFlags::NONE);
}


Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::IOUringAsyncFileIO::start() {
    co_return {};
}

Ichor::Task<void> Ichor::IOUringAsyncFileIO::stop() {
    _shouldStop = true; // TODO stop() currently doesn't get called if there are any open coroutines. Perhaps change the Ichor architecture for that? Hmm.
    co_return;
}

void Ichor::IOUringAsyncFileIO::addDependencyInstance(IIOUringQueue &q, IService&) noexcept {
    _q = &q;
}

void Ichor::IOUringAsyncFileIO::removeDependencyInstance(IIOUringQueue&, IService&) noexcept {
    _q = nullptr;
}

void Ichor::IOUringAsyncFileIO::addDependencyInstance(ILogger &logger, IService&) noexcept {
    _logger = &logger;
}

void Ichor::IOUringAsyncFileIO::removeDependencyInstance(ILogger&, IService&) noexcept {
    _logger = nullptr;
}

Ichor::Task<tl::expected<std::string, Ichor::IOError>> Ichor::IOUringAsyncFileIO::readWholeFile(std::filesystem::path const &file_path) {
    INTERNAL_IO_DEBUG("readWholeFile({})", file_path);

    if(_shouldStop) {
        co_return tl::unexpected(IOError::SERVICE_STOPPED);
    }

    AsyncManualResetEvent evt;

    int res{};
    auto *sqe = _q->getSqeWithData(this, [&evt, &res](io_uring_cqe *cqe) {
        INTERNAL_IO_DEBUG("openat res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
        res = cqe->res;
        evt.set();
    });
    if(file_path.is_absolute()) {
        io_uring_prep_openat(sqe, 0, file_path.c_str(), O_RDONLY | O_CLOEXEC, 0);
    } else {
        io_uring_prep_openat(sqe, AT_FDCWD, file_path.c_str(), O_RDONLY | O_CLOEXEC, 0);
    }
    co_await evt;
    INTERNAL_IO_DEBUG("readWholeFile({}) 1", file_path);

    if(_shouldStop) {
        co_return tl::unexpected(IOError::SERVICE_STOPPED);
    }

    if(res < 0) {
        co_return tl::unexpected(mapErrnoToError(-res));
    }

    int fd = res;
    struct statx x{};
    res = 0;
    evt.reset();
    sqe = _q->getSqeWithData(this, [&evt, &res](io_uring_cqe *cqe) {
        INTERNAL_IO_DEBUG("statx res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
        res = cqe->res;
        evt.set();
    });
    io_uring_prep_statx(sqe, fd, "", AT_EMPTY_PATH, STATX_SIZE, &x);
    co_await evt;
    INTERNAL_IO_DEBUG("readWholeFile({}) statx res: {} {}", file_path, res, x.stx_size);

    if(_shouldStop) {
        co_return tl::unexpected(IOError::SERVICE_STOPPED);
    }

    if(res < 0) {
        co_return tl::unexpected(mapErrnoToError(-res));
    }

    std::string contents;

    if(x.stx_size == 0) {
        INTERNAL_IO_DEBUG("readWholeFile({}) 3", file_path);
        co_return contents;
    }

    if(x.stx_size > contents.max_size()) {
        ICHOR_LOG_ERROR(_logger, "Attempt to read file with size {} that is larger than string max size {}", x.stx_size, contents.max_size());
        co_return tl::unexpected(IOError::FILE_SIZE_TOO_BIG);
    }

    uint32_t maxSubmissions = std::min(_bufferBatchSize, _q->getMaxEntriesCount());
    uint64_t size_left = x.stx_size;
    evt.reset();
    contents.reserve(static_cast<size_t>(x.stx_size));

    res = 0;
    recursiveSubmitReadWholeFile(evt, res, contents, size_left, 0, x.stx_size, fd, maxSubmissions);
    co_await evt;
    INTERNAL_IO_DEBUG("readWholeFile({}) 4", file_path);

    if(_shouldStop) {
        co_return tl::unexpected(IOError::SERVICE_STOPPED);
    }

    if(res < 0) {
        co_return tl::unexpected(mapErrnoToError(-res));
    }

    co_return contents;
}

Ichor::Task<tl::expected<void, Ichor::IOError>> Ichor::IOUringAsyncFileIO::copyFile(const std::filesystem::path &from, const std::filesystem::path &to) {
    INTERNAL_IO_DEBUG("copyFile({}, {})", from, to);

    if(_shouldStop) {
        co_return tl::unexpected(IOError::SERVICE_STOPPED);
    }

    AsyncManualResetEvent evt;

    int fromFd{-1};
    int toFd{};
    auto *sqe = _q->getSqeWithData(this, [&fromFd](io_uring_cqe *cqe) {
        INTERNAL_IO_DEBUG("openat res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
        fromFd = cqe->res;
    });
    if(from.is_absolute()) {
        io_uring_prep_openat(sqe, 0, from.c_str(), O_RDONLY | O_CLOEXEC, 0);
    } else {
        io_uring_prep_openat(sqe, AT_FDCWD, from.c_str(), O_RDONLY | O_CLOEXEC, 0);
    }
    sqe = _q->getSqeWithData(this, [&evt, &toFd](io_uring_cqe *cqe) {
        INTERNAL_IO_DEBUG("openat res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");

        if(evt.is_set()) {
            return;
        }

        toFd = cqe->res;
        evt.set();
    });
    if(to.is_absolute()) {
        io_uring_prep_openat(sqe, 0, to.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    } else {
        io_uring_prep_openat(sqe, AT_FDCWD, to.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    }
    co_await evt;
    INTERNAL_IO_DEBUG("copyFile({}, {}) 1", from, to);

    if(_shouldStop) {
        co_return tl::unexpected(IOError::SERVICE_STOPPED);
    }

    if(fromFd < 0) {
        co_return tl::unexpected(mapErrnoToError(-fromFd));
    }

    if(toFd < 0) {
        co_return tl::unexpected(mapErrnoToError(-toFd));
    }

    struct statx x{};
    int res = 0;
    evt.reset();
    sqe = _q->getSqeWithData(this, [&evt, &res](io_uring_cqe *cqe) {
        INTERNAL_IO_DEBUG("statx res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
        res = cqe->res;
        evt.set();
    });
    io_uring_prep_statx(sqe, fromFd, "", AT_EMPTY_PATH, STATX_SIZE, &x);
    co_await evt;
    INTERNAL_IO_DEBUG("copyFile({}, {}) statx res: {} {}", from, to, res, x.stx_size);

    if(_shouldStop) {
        co_return tl::unexpected(IOError::SERVICE_STOPPED);
    }

    if(res < 0) {
        co_return tl::unexpected(mapErrnoToError(-res));
    }

    if(x.stx_size == 0) {
        INTERNAL_IO_DEBUG("copyFile({}, {}) 3", from, to);
        co_return {};
    }

    uint32_t maxSubmissions = std::min(_bufferBatchSize, _q->getMaxEntriesCount());
    uint64_t size_left = x.stx_size;
    evt.reset();

    res = 0;
    recursiveSubmitCopyFile(evt, res, size_left, 0, x.stx_size, fromFd, toFd, maxSubmissions);
    co_await evt;
    INTERNAL_IO_DEBUG("copyFile({}, {}) 4", from, to);

    if(_shouldStop) {
        co_return tl::unexpected(IOError::SERVICE_STOPPED);
    }

    if(res < 0) {
        co_return tl::unexpected(mapErrnoToError(-res));
    }

    co_return {};
}

Ichor::Task<tl::expected<void, Ichor::IOError>> Ichor::IOUringAsyncFileIO::removeFile(const std::filesystem::path &file) {
    INTERNAL_IO_DEBUG("removeFile({})", file);

    if(_shouldStop) {
        co_return tl::unexpected(IOError::SERVICE_STOPPED);
    }

    AsyncManualResetEvent evt;

    int res{};
    auto *sqe = _q->getSqeWithData(this, [&evt, &res](io_uring_cqe *cqe) {
        INTERNAL_IO_DEBUG("unlinkat res: {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
        res = cqe->res;
        evt.set();
    });
    if(file.is_absolute()) {
        io_uring_prep_unlinkat(sqe, 0, file.c_str(), 0);
    } else {
        io_uring_prep_unlinkat(sqe, AT_FDCWD, file.c_str(), 0);
    }
    co_await evt;
    INTERNAL_IO_DEBUG("removeFile({}) 1", file);

    if(_shouldStop) {
        co_return tl::unexpected(IOError::SERVICE_STOPPED);
    }

    if(res < 0) {
        co_return tl::unexpected(mapErrnoToError(-res));
    }

    co_return {};
}

Ichor::Task<tl::expected<void, Ichor::IOError>> Ichor::IOUringAsyncFileIO::writeFile(const std::filesystem::path &file_path, std::string_view contents) {
    INTERNAL_IO_DEBUG("writeFile({})", file_path);

    if(_shouldStop) {
        co_return tl::unexpected(IOError::SERVICE_STOPPED);
    }

    AsyncManualResetEvent evt;

    int res{};
    auto *sqe = _q->getSqeWithData(this, [&evt, &res](io_uring_cqe *cqe) {
        INTERNAL_IO_DEBUG("openat res: {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
        res = cqe->res;
        evt.set();
    });
    if(file_path.is_absolute()) {
        io_uring_prep_openat(sqe, 0, file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    } else {
        io_uring_prep_openat(sqe, AT_FDCWD, file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    }
    co_await evt;
    INTERNAL_IO_DEBUG("writeFile({}) 1", file_path);

    if(_shouldStop) {
        co_return tl::unexpected(IOError::SERVICE_STOPPED);
    }

    if(res < 0) {
        co_return tl::unexpected(mapErrnoToError(-res));
    }

    if(_shouldStop) {
        co_return tl::unexpected(IOError::SERVICE_STOPPED);
    }

    int fd = res;
    evt.reset();
    res = 0;
    sqe = _q->getSqeWithData(this, [&evt, &res](io_uring_cqe *cqe) {
        INTERNAL_IO_DEBUG("write res: {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
        res = cqe->res;
        evt.set();
    });
    io_uring_prep_write(sqe, fd, contents.data(), static_cast<unsigned>(contents.size()), std::numeric_limits<__u64>::max());

    co_await evt;

    co_return {};
}

Ichor::Task<tl::expected<void, Ichor::IOError>> Ichor::IOUringAsyncFileIO::appendFile(const std::filesystem::path &file_path, std::string_view contents) {
    // TODO refactor writeFile and appendFile to re-use the same code.
    INTERNAL_IO_DEBUG("appendFile({})", file_path);

    if(_shouldStop) {
        co_return tl::unexpected(IOError::SERVICE_STOPPED);
    }

    AsyncManualResetEvent evt;

    int res{};
    auto *sqe = _q->getSqeWithData(this, [&evt, &res](io_uring_cqe *cqe) {
        INTERNAL_IO_DEBUG("openat res: {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
        res = cqe->res;
        evt.set();
    });
    if(file_path.is_absolute()) {
        io_uring_prep_openat(sqe, 0, file_path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    } else {
        io_uring_prep_openat(sqe, AT_FDCWD, file_path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    }
    co_await evt;
    INTERNAL_IO_DEBUG("appendFile({}) 1", file_path);

    if(_shouldStop) {
        co_return tl::unexpected(IOError::SERVICE_STOPPED);
    }

    if(res < 0) {
        co_return tl::unexpected(mapErrnoToError(-res));
    }

    if(_shouldStop) {
        co_return tl::unexpected(IOError::SERVICE_STOPPED);
    }

    int fd = res;
    evt.reset();
    res = 0;
    sqe = _q->getSqeWithData(this, [&evt, &res](io_uring_cqe *cqe) {
        INTERNAL_IO_DEBUG("write res: {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
        res = cqe->res;
        evt.set();
    });
    io_uring_prep_write(sqe, fd, contents.data(), static_cast<unsigned>(contents.size()), std::numeric_limits<__u64>::max());

    co_await evt;

    co_return {};
}

void Ichor::IOUringAsyncFileIO::recursiveSubmitReadWholeFile(AsyncManualResetEvent &evt, int &res, std::string &contents, uint64_t &sizeLeft, uint64_t offset, uint64_t fileSize, int fd, uint32_t maxSubmissions) {
    uint32_t submissions = maxSubmissions;
    while(sizeLeft > 0 && submissions > 0) {
        char* buf = new char[_bufferSize];
        bool isLastInBatch = sizeLeft < _bufferSize || submissions == 1;

        auto *sqe = _q->getSqeWithData(this, createNewEventHandlerReadWholeFile(evt, res, contents, sizeLeft, fileSize, fd, maxSubmissions, offset, buf, isLastInBatch));
        io_uring_prep_read(sqe, fd, buf, _bufferSize, offset);
        INTERNAL_IO_DEBUG("add read");

        offset += _bufferSize;
        if(sizeLeft > _bufferSize) {
            sizeLeft -= _bufferSize;
        } else {
            sizeLeft = 0;
        }

        submissions--;
    }
}

std::function<void(io_uring_cqe*)> Ichor::IOUringAsyncFileIO::createNewEventHandlerReadWholeFile(Ichor::AsyncManualResetEvent &evt, int &res, std::string &contents, uint64_t &sizeLeft, uint64_t fileSize, int fd, uint32_t maxSubmissions, uint64_t offset, char *buf, bool isLastInBatch) {
    return [this, &evt, &res, &contents, &sizeLeft, fileSize, fd, buf, offset, isLastInBatch, maxSubmissions](io_uring_cqe *cqe) {
        std::unique_ptr<char[]> uniqueBuf{buf};
        INTERNAL_IO_DEBUG("read response {} {} {} {} {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "", _shouldStop, evt.is_set(), contents.size(), fileSize);
        if(_shouldStop || res < 0) {
            if(isLastInBatch && !evt.is_set()) {
                evt.set();
            }
            return;
        }

        if(cqe->res < 0) {
            ICHOR_LOG_ERROR(_logger, "read failed with result {} res {}", cqe->res, res);
            res = cqe->res;
            if(isLastInBatch) {
                evt.set();
            }
            return;
        }

        auto readBytes = static_cast<uint32_t>(cqe->res);

        if(contents.size() >= fileSize) {
            ICHOR_LOG_ERROR(_logger, "read contents already filled? with result {} res {} contents size {} statx size {}", cqe->res, res, contents.size(), fileSize);
            res = -2;
            if(isLastInBatch) {
                evt.set();
            }
            return;
        }

        contents.append(buf, readBytes);

        if(contents.size() == fileSize) {
            if constexpr (DO_INTERNAL_DEBUG || DO_HARDENING) {
                if (!isLastInBatch) {
                    std::terminate();
                }
            }
            evt.set();
            return;
        }

        if(readBytes < _bufferSize) {
            uniqueBuf.reset();
            auto *sqe = _q->getSqeWithData(this, createNewEventHandlerReadWholeFile(evt, res, contents, sizeLeft, fileSize, fd, maxSubmissions, offset + readBytes, buf, isLastInBatch));
            io_uring_prep_read(sqe, fd, buf, _bufferSize -  readBytes, offset + readBytes);
            INTERNAL_IO_DEBUG("add read");
        }

        // contents.size() indicates we're not done but we're the last in the batch. Submit a new batch.
        if(isLastInBatch) {
            recursiveSubmitReadWholeFile(evt, res, contents, sizeLeft, offset + readBytes, fileSize, fd, maxSubmissions);
        }
    };
}

void Ichor::IOUringAsyncFileIO::recursiveSubmitCopyFile(AsyncManualResetEvent &evt, int &res, uint64_t &sizeLeft, uint64_t offset, uint64_t fileSize, int fromFd, int toFd, uint32_t maxSubmissions) {
    uint32_t submissions = maxSubmissions;
    while(sizeLeft > 0 && submissions > 0) {
        char* buf = new char[_bufferSize];
        bool isLastInBatch = sizeLeft < _bufferSize || submissions == 1;

        auto *sqe = _q->getSqeWithData(this, createNewEventHandlerCopyFileReadPart(evt, res, sizeLeft, fileSize, fromFd, toFd, maxSubmissions, offset, buf, isLastInBatch));
        io_uring_prep_read(sqe, fromFd, buf, _bufferSize, offset);
        INTERNAL_IO_DEBUG("add read");

        offset += _bufferSize;
        if(sizeLeft > _bufferSize) {
            sizeLeft -= _bufferSize;
        } else {
            sizeLeft = 0;
        }

        submissions--;
    }
}

std::function<void(io_uring_cqe*)> Ichor::IOUringAsyncFileIO::createNewEventHandlerCopyFileReadPart(Ichor::AsyncManualResetEvent &evt, int &res, uint64_t &sizeLeft, uint64_t fileSize, int fromFd, int toFd, uint32_t maxSubmissions, uint64_t offset, char *buf, bool isLastInBatch) {
    return [this, &evt, &res, &sizeLeft, fileSize, fromFd, toFd, buf, offset, isLastInBatch, maxSubmissions](io_uring_cqe *cqe) {
        std::unique_ptr<char[]> uniqueBuf{buf};
        INTERNAL_IO_DEBUG("read response {} {} {} {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "", _shouldStop, evt.is_set(), fileSize);
        if(_shouldStop || res < 0) {
            if(isLastInBatch && !evt.is_set()) {
                evt.set();
            }
            return;
        }

        if(cqe->res < 0) {
            ICHOR_LOG_ERROR(_logger, "read failed with result {} res {}", cqe->res, res);
            res = cqe->res;
            if(isLastInBatch) {
                evt.set();
            }
            return;
        }

        auto readBytes = static_cast<uint32_t>(cqe->res);
        bool isLastInWriteBatch = offset + readBytes == fileSize;

        {
            // buf will be deleted in write handler
            uniqueBuf.release();
            auto *sqe = _q->getSqeWithData(this, createNewEventHandlerCopyFileWritePart(evt, res, buf, isLastInWriteBatch));
            io_uring_prep_write(sqe, toFd, buf, readBytes, std::numeric_limits<__u64>::max());
            INTERNAL_IO_DEBUG("add write");
        }

        if(!isLastInWriteBatch && readBytes < _bufferSize) {
            char *newBuf = new char[_bufferSize];
            auto *sqe = _q->getSqeWithData(this, createNewEventHandlerCopyFileReadPart(evt, res, sizeLeft, fileSize, fromFd, toFd, maxSubmissions, offset + readBytes, newBuf, isLastInBatch));
            io_uring_prep_read(sqe, fromFd, newBuf, _bufferSize -  readBytes, offset + readBytes);
            INTERNAL_IO_DEBUG("add read");
        }

        // contents.size() indicates we're not done but we're the last in the batch. Submit a new batch.
        if(!isLastInWriteBatch && isLastInBatch) {
            recursiveSubmitCopyFile(evt, res, sizeLeft, offset + readBytes, fileSize, fromFd, toFd, maxSubmissions);
        }
    };
}

std::function<void(io_uring_cqe*)> Ichor::IOUringAsyncFileIO::createNewEventHandlerCopyFileWritePart(Ichor::AsyncManualResetEvent &evt, int &res, char *buf, bool isLastInBatch) {
    return [this, &evt, &res, buf, isLastInBatch](io_uring_cqe *cqe) {
        std::unique_ptr<char[]> uniqueBuf{buf};
        INTERNAL_IO_DEBUG("write response {} {} {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "", _shouldStop, evt.is_set());
        if(_shouldStop || res < 0) {
            if(isLastInBatch && !evt.is_set()) {
                evt.set();
            }
            return;
        }

        if(cqe->res < 0) {
            ICHOR_LOG_ERROR(_logger, "write failed with result {} res {}", cqe->res, res);
            res = cqe->res;
            if(isLastInBatch) {
                evt.set();
            }
            return;
        }

        if(isLastInBatch) {
            evt.set();
        }
    };
}
