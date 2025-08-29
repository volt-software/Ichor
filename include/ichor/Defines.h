#pragma once

#if defined(__cpp_contracts)
#define ICHOR_CONTRACT_ASSERT contract_assert
#else
#define ICHOR_CONTRACT_ASSERT(expr) if constexpr (false) {  (void)!!(expr); }
#endif

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#define ICHOR_EXCEPTIONS_ENABLED 1
#else
#define ICHOR_EXCEPTIONS_ENABLED 0
#endif
