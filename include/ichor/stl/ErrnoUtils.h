#pragma once

#include <type_traits>
#include <cstdint>
#include <cerrno>
#include <fmt/core.h>

namespace Ichor {
    enum class FileIOError : uint_fast16_t {
        FAILED,
        FILE_DOES_NOT_EXIST,
        NO_PERMISSION,
        IS_DIR_SHOULD_BE_FILE,
        IS_FILE_SHOULD_BE_DIR,
        INTERRUPTED_BY_SIGNAL,
        BASENAME_CONTAINS_INVALID_CHARACTERS,
        TOO_MANY_SYMBOLIC_LINKS,
        PER_PROCESS_LIMIT_REACHED,
        FILEPATH_TOO_LONG,
        SERVICE_STOPPED,
        FILE_SIZE_TOO_BIG,
        SYSTEM_BUG,
        READ_ONLY_FS,
        USER_QUOTA_REACHED,
        NO_SPACE_LEFT,
        IO_ERROR,
        BAD_FILE_DESCRIPTOR,
        NOT_CONNECTED,
        NOT_A_SOCKET
    };

    /*
     * Function to make an errno indicator to an Ichor FileIOError. Returns FileIOError::FAILED if it doesn't recognize the number.
     */
    [[nodiscard]] static constexpr Ichor::FileIOError mapErrnoToError(std::remove_cvref_t<decltype(errno)> err) noexcept {
        if(err == EACCES || err == EFAULT || err == EPERM) {
            return FileIOError::NO_PERMISSION;
        } else if(err == EINTR) {
            return FileIOError::INTERRUPTED_BY_SIGNAL;
        } else if(err == EINVAL) {
            return FileIOError::BASENAME_CONTAINS_INVALID_CHARACTERS;
        } else if(err == ELOOP) {
            return FileIOError::TOO_MANY_SYMBOLIC_LINKS;
        } else if(err == EMFILE) {
            return FileIOError::PER_PROCESS_LIMIT_REACHED;
        } else if(err == ENAMETOOLONG) {
            return FileIOError::FILEPATH_TOO_LONG;
        } else if(err == ENOENT) {
            return FileIOError::FILE_DOES_NOT_EXIST;
        } else if(err == EISDIR) {
            return FileIOError::IS_DIR_SHOULD_BE_FILE;
        } else if(err == ENOTDIR) {
            return FileIOError::IS_FILE_SHOULD_BE_DIR;
        } else if(err == EROFS) {
            return FileIOError::READ_ONLY_FS;
        } else if(err == EDQUOT) {
            return FileIOError::USER_QUOTA_REACHED;
        } else if(err == ENOSPC) {
            return FileIOError::NO_SPACE_LEFT;
        } else if(err == EBADF) {
            return FileIOError::BAD_FILE_DESCRIPTOR;
        } else if(err == EIO) {
            return FileIOError::IO_ERROR;
        } else if(err == ENOTCONN) {
            return FileIOError::NOT_CONNECTED;
        } else if(err == ENOTSOCK) {
            return FileIOError::NOT_A_SOCKET;
        } else {
            return FileIOError::FAILED;
        }
    }
}



template <>
struct fmt::formatter<Ichor::FileIOError> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::FileIOError& change, FormatContext& ctx) {
        switch(change) {
            case Ichor::FileIOError::FAILED:
                return fmt::format_to(ctx.out(), "FAILED");
            case Ichor::FileIOError::FILE_DOES_NOT_EXIST:
                return fmt::format_to(ctx.out(), "FILE_DOES_NOT_EXIST");
            case Ichor::FileIOError::NO_PERMISSION:
                return fmt::format_to(ctx.out(), "NO_PERMISSION");
            case Ichor::FileIOError::IS_DIR_SHOULD_BE_FILE:
                return fmt::format_to(ctx.out(), "IS_DIR_SHOULD_BE_FILE");
            case Ichor::FileIOError::IS_FILE_SHOULD_BE_DIR:
                return fmt::format_to(ctx.out(), "IS_FILE_SHOULD_BE_DIR");
            case Ichor::FileIOError::INTERRUPTED_BY_SIGNAL:
                return fmt::format_to(ctx.out(), "INTERRUPTED_BY_SIGNAL");
            case Ichor::FileIOError::BASENAME_CONTAINS_INVALID_CHARACTERS:
                return fmt::format_to(ctx.out(), "BASENAME_CONTAINS_INVALID_CHARACTERS");
            case Ichor::FileIOError::TOO_MANY_SYMBOLIC_LINKS:
                return fmt::format_to(ctx.out(), "TOO_MANY_SYMBOLIC_LINKS");
            case Ichor::FileIOError::PER_PROCESS_LIMIT_REACHED:
                return fmt::format_to(ctx.out(), "PER_PROCESS_LIMIT_REACHED");
            case Ichor::FileIOError::FILEPATH_TOO_LONG:
                return fmt::format_to(ctx.out(), "FILEPATH_TOO_LONG");
            case Ichor::FileIOError::SERVICE_STOPPED:
                return fmt::format_to(ctx.out(), "SERVICE_STOPPED");
            case Ichor::FileIOError::FILE_SIZE_TOO_BIG:
                return fmt::format_to(ctx.out(), "FILE_SIZE_TOO_BIG");
            case Ichor::FileIOError::SYSTEM_BUG:
                return fmt::format_to(ctx.out(), "SYSTEM_BUG");
            case Ichor::FileIOError::READ_ONLY_FS:
                return fmt::format_to(ctx.out(), "READ_ONLY_FS");
            case Ichor::FileIOError::USER_QUOTA_REACHED:
                return fmt::format_to(ctx.out(), "USER_QUOTA_REACHED");
            case Ichor::FileIOError::NO_SPACE_LEFT:
                return fmt::format_to(ctx.out(), "NO_SPACE_LEFT");
            case Ichor::FileIOError::IO_ERROR:
                return fmt::format_to(ctx.out(), "IO_ERROR");
            case Ichor::FileIOError::BAD_FILE_DESCRIPTOR:
                return fmt::format_to(ctx.out(), "BAD_FILE_DESCRIPTOR");
            case Ichor::FileIOError::NOT_CONNECTED:
                return fmt::format_to(ctx.out(), "NOT_CONNECTED");
            case Ichor::FileIOError::NOT_A_SOCKET:
                return fmt::format_to(ctx.out(), "NOT_A_SOCKET");
        }
        return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
    }
};
