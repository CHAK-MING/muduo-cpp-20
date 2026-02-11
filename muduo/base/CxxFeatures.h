#pragma once

#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
#define MUDUO_HAS_CPP23_MOVE_ONLY_FUNCTION 1
#else
#define MUDUO_HAS_CPP23_MOVE_ONLY_FUNCTION 0
#endif

#if defined(__cpp_lib_byteswap) && __cpp_lib_byteswap >= 202110L
#define MUDUO_HAS_CPP23_BYTESWAP 1
#else
#define MUDUO_HAS_CPP23_BYTESWAP 0
#endif

#if defined(__cpp_lib_stacktrace) && __cpp_lib_stacktrace >= 202011L
#define MUDUO_HAS_CPP23_STACKTRACE 1
#else
#define MUDUO_HAS_CPP23_STACKTRACE 0
#endif

#if defined(__cpp_lib_unreachable) && __cpp_lib_unreachable >= 202202L
#define MUDUO_HAS_CPP23_UNREACHABLE 1
#else
#define MUDUO_HAS_CPP23_UNREACHABLE 0
#endif

#if defined(__cpp_lib_print) && __cpp_lib_print >= 202207L
#define MUDUO_HAS_CPP23_PRINT 1
#else
#define MUDUO_HAS_CPP23_PRINT 0
#endif
