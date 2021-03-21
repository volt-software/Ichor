#include <execinfo.h>
#include <cstdio>
#include <exception>
#include <cstring>
#include <cstdlib>
#include <dlfcn.h>

bool reentrant;

#ifdef __SANITIZE_ADDRESS__
#warning "Overriding malloc does not work with address sanitizer. The realtime example will terminate if run."
#endif

void* malloc(size_t size) {
    // this is not thread-safe, as there is no check on if the current function call is on the same thread.
    // but this example only uses one thread.
    // this is necessary because some code paths in backtrace()/backtrace_symbols require malloc (glibc exception handling f.e.)
    if(!reentrant) {
        void *array[10];
        size_t size2;

        reentrant = true;

        size2 = backtrace(array, 10);
        char **funcNames = backtrace_symbols(array, size2);
        bool shouldTerminate = true;
//        for (size_t i = 0; i < size2; i++) {
//            printf("%li: %s\n", i, funcNames[i]); // re-enable to see which function gets reported by backtrace()
//        }
        for (size_t i = 0; i < size2; i++) {
            // the first 2 dozen memory allocations or so are used for __static_initialization_and_destruction_0 and std::locale
            if (strstr(funcNames[i], "locale") != nullptr || //exception for locale
                strstr(funcNames[i], "__cxa_allocate_exception") != nullptr || //exception for std::terminate
                strstr(funcNames[i], "runtime_error") != nullptr || //exception for std::terminate
                strstr(funcNames[i], "_IO_file_doallocate") != nullptr || //exception for printf() related functionality
                strstr(funcNames[i], "ld-linux-x86-64") != nullptr) { //exception for initialization at beginning of program
                shouldTerminate = false;
                break;
            }
        }

        free(funcNames);

        if (shouldTerminate) {
            std::terminate();
        }

        reentrant = false;
    }

    auto libc_malloc = reinterpret_cast<void *(*)(size_t)>(dlsym(RTLD_NEXT, "malloc"));
    return libc_malloc(size);
}

[[nodiscard]]
void* operator new(std::size_t size)  {
    return malloc(size);
}

[[nodiscard]]
void* operator new[](std::size_t size) {
    return malloc(size);
}

[[nodiscard]]
void* operator new(std::size_t size, std::align_val_t t) {
    return malloc(size);
}

[[nodiscard]]
void* operator new[](std::size_t size, std::align_val_t t) {
    return malloc(size);
}

void operator delete(void * p) noexcept
{
    free(p);
}

void operator delete[](void * p) noexcept
{
    free(p);
}

void operator delete(void * p, std::size_t t) noexcept
{
    free(p);
}

void operator delete[](void * p, std::size_t t) noexcept
{
    free(p);
}

void operator delete(void * p, std::size_t t, std::align_val_t v) noexcept
{
    free(p);
}

void operator delete[](void * p, std::size_t t, std::align_val_t v) noexcept
{
    free(p);
}

void operator delete(void * p, std::align_val_t v) noexcept
{
    free(p);
}

void operator delete[](void * p, std::align_val_t v) noexcept
{
    free(p);
}