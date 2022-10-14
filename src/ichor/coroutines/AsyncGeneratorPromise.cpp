#include <ichor/DependencyManager.h>
#include <ichor/coroutines/AsyncGeneratorPromiseBase.h>

uint64_t thread_local Ichor::Detail::AsyncGeneratorPromiseBase::_idCounter = 1;
