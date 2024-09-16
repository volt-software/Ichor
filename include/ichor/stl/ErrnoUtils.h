#pragma once

#include <type_traits>
#include <cstdint>
#include <cerrno>
#include <fmt/core.h>

namespace Ichor {
    enum class IOError : uint_fast16_t {
        // catch-all error
        FAILED,
        // ichor-specific
        SERVICE_QUITTING,
        // file I/O
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
        // sockets
        NOT_CONNECTED,
        NOT_A_SOCKET,
        MESSAGE_SIZE_TOO_BIG,
        CONNECTION_RESET,
        DESTINATION_ADDRESS_REQUIRED,
        ALREADY_CONNECTED,
        NETWORK_INTERFACE_OUTPUT_QUEUE_FULL,
        NO_MEMORY_AVAILABLE,
        NOT_SUPPORTED,
        KERNEL_TOO_OLD
    };

    /*
     * Function to make an errno indicator to an Ichor IOError. Returns IOError::FAILED if it doesn't recognize the number.
     */
    [[nodiscard]] static constexpr Ichor::IOError mapErrnoToError(std::remove_cvref_t<decltype(errno)> err) noexcept {
        if(err == EACCES || err == EFAULT || err == EPERM) {
            return IOError::NO_PERMISSION;
        } else if(err == EINTR) {
            return IOError::INTERRUPTED_BY_SIGNAL;
        } else if(err == EINVAL) {
            return IOError::BASENAME_CONTAINS_INVALID_CHARACTERS;
        } else if(err == ELOOP) {
            return IOError::TOO_MANY_SYMBOLIC_LINKS;
        } else if(err == EMFILE) {
            return IOError::PER_PROCESS_LIMIT_REACHED;
        } else if(err == ENAMETOOLONG) {
            return IOError::FILEPATH_TOO_LONG;
        } else if(err == ENOENT) {
            return IOError::FILE_DOES_NOT_EXIST;
        } else if(err == EISDIR) {
            return IOError::IS_DIR_SHOULD_BE_FILE;
        } else if(err == ENOTDIR) {
            return IOError::IS_FILE_SHOULD_BE_DIR;
        } else if(err == EROFS) {
            return IOError::READ_ONLY_FS;
        } else if(err == EDQUOT) {
            return IOError::USER_QUOTA_REACHED;
        } else if(err == ENOSPC) {
            return IOError::NO_SPACE_LEFT;
        } else if(err == EBADF) {
            return IOError::BAD_FILE_DESCRIPTOR;
        } else if(err == EIO) {
            return IOError::IO_ERROR;
        } else if(err == ENOTCONN) {
            return IOError::NOT_CONNECTED;
        } else if(err == ENOTSOCK) {
            return IOError::NOT_A_SOCKET;
        } else if(err == EMSGSIZE) {
            return IOError::MESSAGE_SIZE_TOO_BIG;
        } else if(err == ECONNRESET) {
            return IOError::CONNECTION_RESET;
        } else if(err == EDESTADDRREQ) {
            return IOError::DESTINATION_ADDRESS_REQUIRED;
        } else if(err == EISCONN) {
            return IOError::ALREADY_CONNECTED;
        } else if(err == ENOBUFS) {
            return IOError::NETWORK_INTERFACE_OUTPUT_QUEUE_FULL;
        } else if(err == ENOMEM) {
            return IOError::NO_MEMORY_AVAILABLE;
        } else if(err == EOPNOTSUPP) {
            return IOError::NOT_SUPPORTED;
        } else {
            return IOError::FAILED;
        }
    }
}



template <>
struct fmt::formatter<Ichor::IOError> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::IOError& change, FormatContext& ctx) {
        switch(change) {
            case Ichor::IOError::FAILED:
                return fmt::format_to(ctx.out(), "FAILED");
            case Ichor::IOError::SERVICE_QUITTING:
                return fmt::format_to(ctx.out(), "SERVICE_QUITTING");
            case Ichor::IOError::FILE_DOES_NOT_EXIST:
                return fmt::format_to(ctx.out(), "FILE_DOES_NOT_EXIST");
            case Ichor::IOError::NO_PERMISSION:
                return fmt::format_to(ctx.out(), "NO_PERMISSION");
            case Ichor::IOError::IS_DIR_SHOULD_BE_FILE:
                return fmt::format_to(ctx.out(), "IS_DIR_SHOULD_BE_FILE");
            case Ichor::IOError::IS_FILE_SHOULD_BE_DIR:
                return fmt::format_to(ctx.out(), "IS_FILE_SHOULD_BE_DIR");
            case Ichor::IOError::INTERRUPTED_BY_SIGNAL:
                return fmt::format_to(ctx.out(), "INTERRUPTED_BY_SIGNAL");
            case Ichor::IOError::BASENAME_CONTAINS_INVALID_CHARACTERS:
                return fmt::format_to(ctx.out(), "BASENAME_CONTAINS_INVALID_CHARACTERS");
            case Ichor::IOError::TOO_MANY_SYMBOLIC_LINKS:
                return fmt::format_to(ctx.out(), "TOO_MANY_SYMBOLIC_LINKS");
            case Ichor::IOError::PER_PROCESS_LIMIT_REACHED:
                return fmt::format_to(ctx.out(), "PER_PROCESS_LIMIT_REACHED");
            case Ichor::IOError::FILEPATH_TOO_LONG:
                return fmt::format_to(ctx.out(), "FILEPATH_TOO_LONG");
            case Ichor::IOError::SERVICE_STOPPED:
                return fmt::format_to(ctx.out(), "SERVICE_STOPPED");
            case Ichor::IOError::FILE_SIZE_TOO_BIG:
                return fmt::format_to(ctx.out(), "FILE_SIZE_TOO_BIG");
            case Ichor::IOError::SYSTEM_BUG:
                return fmt::format_to(ctx.out(), "SYSTEM_BUG");
            case Ichor::IOError::READ_ONLY_FS:
                return fmt::format_to(ctx.out(), "READ_ONLY_FS");
            case Ichor::IOError::USER_QUOTA_REACHED:
                return fmt::format_to(ctx.out(), "USER_QUOTA_REACHED");
            case Ichor::IOError::NO_SPACE_LEFT:
                return fmt::format_to(ctx.out(), "NO_SPACE_LEFT");
            case Ichor::IOError::IO_ERROR:
                return fmt::format_to(ctx.out(), "IO_ERROR");
            case Ichor::IOError::BAD_FILE_DESCRIPTOR:
                return fmt::format_to(ctx.out(), "BAD_FILE_DESCRIPTOR");
            case Ichor::IOError::NOT_CONNECTED:
                return fmt::format_to(ctx.out(), "NOT_CONNECTED");
            case Ichor::IOError::NOT_A_SOCKET:
                return fmt::format_to(ctx.out(), "NOT_A_SOCKET");
            case Ichor::IOError::MESSAGE_SIZE_TOO_BIG:
                return fmt::format_to(ctx.out(), "MESSAGE_SIZE_TOO_BIG");
            case Ichor::IOError::CONNECTION_RESET:
                return fmt::format_to(ctx.out(), "CONNECTION_RESET");
            case Ichor::IOError::DESTINATION_ADDRESS_REQUIRED:
                return fmt::format_to(ctx.out(), "DESTINATION_ADDRESS_REQUIRED");
            case Ichor::IOError::ALREADY_CONNECTED:
                return fmt::format_to(ctx.out(), "ALREADY_CONNECTED");
            case Ichor::IOError::NETWORK_INTERFACE_OUTPUT_QUEUE_FULL:
                return fmt::format_to(ctx.out(), "NETWORK_INTERFACE_OUTPUT_QUEUE_FULL");
            case Ichor::IOError::NO_MEMORY_AVAILABLE:
                return fmt::format_to(ctx.out(), "NO_MEMORY_AVAILABLE");
            case Ichor::IOError::NOT_SUPPORTED:
                return fmt::format_to(ctx.out(), "NOT_SUPPORTED");
            case Ichor::IOError::KERNEL_TOO_OLD:
                return fmt::format_to(ctx.out(), "KERNEL_TOO_OLD");
        }
        return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
    }
};
