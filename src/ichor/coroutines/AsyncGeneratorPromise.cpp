#include <ichor/DependencyManager.h>
#include <ichor/coroutines/AsyncGeneratorPromiseBase.h>

// 0 has a specific meaning.
uint64_t thread_local Ichor::Detail::AsyncGeneratorPromiseBase::_idCounter = 1;
