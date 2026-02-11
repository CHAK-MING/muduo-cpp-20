#include "muduo/base/Exception.h"
#include "muduo/base/CurrentThread.h"

using namespace muduo;

#if MUDUO_ENABLE_LEGACY_COMPAT
Exception::Exception(const char *what)
    : Exception(what == nullptr ? std::string_view{} : std::string_view{what},
                StackTraceMode::Capture, std::source_location::current()) {}
#endif

Exception::Exception(std::string_view what)
    : Exception(what, StackTraceMode::Capture, std::source_location::current()) {
}

Exception::Exception(std::string_view what,
                     std::source_location where) noexcept
    : Exception(what, StackTraceMode::Skip, where) {}

Exception::Exception(std::string_view what, StackTraceMode mode,
                     std::source_location where)
    : message_(what), fileName_(where.file_name()),
      functionName_(where.function_name()), line_(where.line()) {
  if (mode == StackTraceMode::Capture) {
    stack_ = CurrentThread::stackTrace(/*demangle=*/false);
  }
}
