#include "muduo/base/Exception.h"
#include "muduo/base/CurrentThread.h"

using namespace muduo;

Exception::Exception(std::string_view msg)
    : Exception(msg, StackTraceMode::Capture, std::source_location::current()) {}

Exception::Exception(std::string_view msg, std::source_location where) noexcept
    : Exception(msg, StackTraceMode::Skip, where) {}

Exception::Exception(std::string_view msg, StackTraceMode mode,
                     std::source_location where)
    : message_(msg),
      fileName_(where.file_name()),
      functionName_(where.function_name()),
      line_(where.line()) {
  if (mode == StackTraceMode::Capture) {
    stack_ = CurrentThread::stackTrace(/*demangle=*/false);
  }
}
