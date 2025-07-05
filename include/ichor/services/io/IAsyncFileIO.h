#pragma once

#include <ichor/coroutines/Task.h>
#include <ichor/stl/ErrnoUtils.h>
#include <string>
#include <string_view>
#include <filesystem>
#include <tl/expected.h>

namespace Ichor::v1 {
    class IAsyncFileIO {
    public:

        /*
         * Reads the entire contents of a file into a std::string.
         */
        virtual Task<tl::expected<std::string, IOError>> readWholeFile(std::filesystem::path const &file) = 0;

        /*
         * Copies the contents of one file to another. This function will also copy the permission bits of the original file to the destination file.
         * This function will overwrite the contents of to.
         * Note that if from and to both point to the same file, then the file will likely get truncated by this operation.
         */
        virtual Task<tl::expected<void, IOError>> copyFile(std::filesystem::path const &from, std::filesystem::path const &to) = 0;

        /*
         * Removes a file from the filesystem.
         * Note that there is no guarantee that the file is immediately deleted (e.g., depending on platform, other open file descriptors may prevent immediate removal).
         */
        virtual Task<tl::expected<void, IOError>> removeFile(std::filesystem::path const &file) = 0;

        /*
         * Write a string as the entire contents of a file.
         * This function will create a file if it does not exist, and will entirely replace its contents if it does.
         * Depending on the platform, this function may fail if the full directory path does not exist.
         */
        virtual Task<tl::expected<void, IOError>> writeFile(std::filesystem::path const &file, std::string_view contents) = 0;
        /*
         * Append a string to a file.
         * This function will create a file if it does not exist, and will add the given contents regardless.
         * Depending on the platform, this function may fail if the full directory path does not exist.
         */
        virtual Task<tl::expected<void, IOError>> appendFile(std::filesystem::path const &file, std::string_view contents) = 0;

    protected:
        ~IAsyncFileIO() = default;
    };
}

