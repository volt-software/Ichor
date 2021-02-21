#include <execinfo.h>
#include <cstdio>
#include <exception>
#include <cstring>
#include <cstdlib>

void* customNew(std::size_t size) {
    void *array[10];
    size_t size2;

    size2 = backtrace(array, 10);
    char** funcNames = backtrace_symbols(array, size2);
    bool shouldTerminate = true;
//    for (size_t i = 0; i < size2; i++) {
//        printf("%li: %s\n", i, funcNames[i]); // re-enable to see which function gets reported by backtrace()
//    }
    for (size_t i = 0; i < size2; i++) {
        if(strstr(funcNames[i], "locale") != nullptr) {
            shouldTerminate = false;
            break;
        }
    }

    free(funcNames);

    if(shouldTerminate) {
        std::terminate();
    }

    return malloc(size);
}

[[nodiscard]]
void* operator new(std::size_t size)  {
    return customNew(size);
}

[[nodiscard]]
void* operator new[](std::size_t size) {
    return customNew(size);
}

[[nodiscard]]
void* operator new(std::size_t size, std::align_val_t t) {
    return customNew(size);
}

[[nodiscard]]
void* operator new[](std::size_t size, std::align_val_t t) {
    return customNew(size);
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