#include "muduo/base/CurrentThread.h"

#include <execinfo.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cxxabi.h>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstring>
#include <memory>
#include <thread>

namespace {

pid_t getTid() { return gettid(); }

void afterFork() {
  muduo::CurrentThread::t_cachedTid = 0;
  muduo::CurrentThread::t_threadName = "main";
  muduo::CurrentThread::cacheTid();
}

class ThreadNameInitializer {
public:
  ThreadNameInitializer() {
    muduo::CurrentThread::t_threadName = "main";
    muduo::CurrentThread::cacheTid();
    ::pthread_atfork(nullptr, nullptr, &afterFork);
  }
};

const ThreadNameInitializer g_threadNameInitializer;

} // namespace

namespace muduo::CurrentThread {

constinit thread_local int t_cachedTid = 0;
constinit thread_local std::array<char, 32> t_tidString{};
constinit thread_local int t_tidStringLength = 6;
constinit thread_local const char *t_threadName = "unknown";

void cacheTid() {
  if (t_cachedTid == 0) {
    t_cachedTid = getTid();

    std::array<char, 16> digits{};
    auto [ptr, ec] = std::to_chars(digits.data(), digits.data() + digits.size(),
                                   t_cachedTid);

    if (ec == std::errc{}) {
      const int digitLen = static_cast<int>(ptr - digits.data());
      const int padding = std::max(0, 5 - digitLen);
      int pos = 0;
      for (int i = 0; i < padding && pos < static_cast<int>(t_tidString.size()) - 1;
           ++i) {
        t_tidString[static_cast<size_t>(pos++)] = ' ';
      }
      if (const int copyLen =
              std::min(digitLen,
                       static_cast<int>(t_tidString.size()) - 1 - pos - 1);
          copyLen > 0) {
        std::memcpy(t_tidString.data() + pos, digits.data(),
                    static_cast<size_t>(copyLen));
        pos += copyLen;
      }
      if (pos < static_cast<int>(t_tidString.size()) - 1) {
        t_tidString[static_cast<size_t>(pos++)] = ' ';
      }
      t_tidStringLength = pos;
      t_tidString[static_cast<size_t>(t_tidStringLength)] = '\0';
      return;
    }

    // Conservative fallback without libc formatting.
    static constexpr std::string_view kUnknown = "00000 ";
    std::memcpy(t_tidString.data(), kUnknown.data(), kUnknown.size());
    t_tidStringLength = static_cast<int>(kUnknown.size());
    t_tidString[static_cast<size_t>(t_tidStringLength)] = '\0';
  }
}

void setName(const char *name) { t_threadName = name; }

bool isMainThread() { return tid() == ::getpid(); }

void sleepUsec(int64_t usec) {
  std::this_thread::sleep_for(std::chrono::microseconds(usec));
}

string stackTrace(bool demangle) {
  constexpr int kMaxFrames = 200;
  void *frames[kMaxFrames];
  const int frameCount = ::backtrace(frames, kMaxFrames);
  if (frameCount <= 0) {
    return {};
  }

  std::unique_ptr<char *, decltype(&std::free)> symbols(
      ::backtrace_symbols(frames, frameCount), &std::free);
  if (!symbols) {
    return {};
  }

  string trace;
  for (int i = 0; i < frameCount; ++i) {
    const char *line = symbols.get()[i];
    if (line == nullptr) {
      continue;
    }

    if (!demangle) {
      trace.append(line);
      trace.push_back('\n');
      continue;
    }

    const char *left = std::strchr(line, '(');
    const char *plus = left ? std::strchr(left, '+') : nullptr;
    if (!left || !plus || left >= plus) {
      trace.append(line);
      trace.push_back('\n');
      continue;
    }

    const auto mangledLen = static_cast<size_t>(plus - left - 1);
    string mangled(left + 1, mangledLen);
    int status = 0;
    std::unique_ptr<char, decltype(&std::free)> demangledName(
        abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status),
        &std::free);
    if (status == 0 && demangledName) {
      trace.append(line, static_cast<size_t>(left - line + 1));
      trace.append(demangledName.get());
      trace.append(plus);
    } else {
      trace.append(line);
    }
    trace.push_back('\n');
  }
  return trace;
}

} // namespace muduo::CurrentThread
