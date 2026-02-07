#include "muduo/base/Exception.h"
#include "muduo/base/CurrentThread.h"

using namespace muduo;

Exception::Exception(std::string_view msg)
    : message_(msg),
      stack_(CurrentThread::stackTrace(/*demangle=*/false)) {}
