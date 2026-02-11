#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpServer.h"

#include <benchmark/benchmark.h>

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

int pickPort() {
  static std::atomic<int> port{41000 + (::getpid() % 20000)};
  return port.fetch_add(1, std::memory_order_relaxed);
}

void quietOutput(const char *, int) {}
void quietFlush() {}

void prepareBenchLogging() {
  static std::once_flag once;
  std::call_once(once, [] {
    muduo::Logger::setLogLevel(muduo::Logger::LogLevel::ERROR);
    muduo::Logger::setOutput(&quietOutput);
    muduo::Logger::setFlush(&quietFlush);
  });
}

bool writeFull(int fd, std::string_view data) {
  size_t written = 0;
  while (written < data.size()) {
    const ssize_t n = ::write(fd, data.data() + written, data.size() - written);
    if (n <= 0) {
      return false;
    }
    written += static_cast<size_t>(n);
  }
  return true;
}

bool readFull(int fd, std::span<char> data) {
  size_t readn = 0;
  while (readn < data.size()) {
    const ssize_t n = ::read(fd, data.data() + readn, data.size() - readn);
    if (n <= 0) {
      return false;
    }
    readn += static_cast<size_t>(n);
  }
  return true;
}

class EchoServerHarness {
public:
  explicit EchoServerHarness(uint16_t port) : port_(port) {
    loop_ = loopThread_.startLoop();
    loop_->runInLoop([this] {
      server_ = std::make_unique<muduo::net::TcpServer>(
          loop_, muduo::net::InetAddress(port_, true), "EchoBench");
      server_->setConnectionCallback([](const muduo::net::TcpConnectionPtr &) {});
      server_->setMessageCallback([](const muduo::net::TcpConnectionPtr &conn,
                                     muduo::net::Buffer *buf, muduo::Timestamp) {
        conn->send(buf);
      });
      server_->start();
      {
        std::scoped_lock lock(mutex_);
        started_ = true;
      }
      cv_.notify_one();
    });

    std::unique_lock lock(mutex_);
    (void)cv_.wait_for(lock, std::chrono::seconds(2), [this] { return started_; });
  }

  ~EchoServerHarness() {
    if (loop_ == nullptr) {
      return;
    }
    std::mutex doneMutex;
    std::condition_variable doneCv;
    bool done = false;
    loop_->runInLoop([this, &doneMutex, &doneCv, &done] {
      server_.reset();
      loop_->quit();
      {
        std::scoped_lock lock(doneMutex);
        done = true;
      }
      doneCv.notify_one();
    });
    std::unique_lock lock(doneMutex);
    (void)doneCv.wait_for(lock, std::chrono::seconds(2), [&done] { return done; });
  }

private:
  muduo::net::EventLoopThread loopThread_;
  muduo::net::EventLoop *loop_{nullptr};
  uint16_t port_;
  std::unique_ptr<muduo::net::TcpServer> server_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool started_{false};
};

class TcpSocketClient {
public:
  explicit TcpSocketClient(uint16_t port) {
    fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd_ < 0) {
      return;
    }

    int one = 1;
    (void)::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    (void)::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
      ::close(fd_);
      fd_ = -1;
    }
  }

  ~TcpSocketClient() {
    if (fd_ >= 0) {
      ::close(fd_);
    }
  }

  [[nodiscard]] bool ok() const { return fd_ >= 0; }

  bool roundTrip(std::string_view payload, std::span<char> recvBuf) {
    return writeFull(fd_, payload) && readFull(fd_, recvBuf);
  }

private:
  int fd_{-1};
};

static void BM_EchoRoundTrip(benchmark::State &state) {
  prepareBenchLogging();

  const auto payloadSize = static_cast<size_t>(state.range(0));
  const uint16_t port = static_cast<uint16_t>(pickPort());
  EchoServerHarness server(port);
  TcpSocketClient client(port);

  if (!client.ok()) {
    state.SkipWithError("client connect failed");
    return;
  }

  std::string payload(payloadSize, 'x');
  std::vector<char> recvBuf(payloadSize);

  for (auto _ : state) {
    if (!client.roundTrip(payload, recvBuf)) {
      state.SkipWithError("echo round trip failed");
      break;
    }
    benchmark::DoNotOptimize(recvBuf);
    benchmark::ClobberMemory();
  }

  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(payloadSize) * 2);
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_EchoRoundTrip)
    ->Arg(64)
    ->Arg(256)
    ->Arg(1024)
    ->Arg(4096)
    ->Unit(benchmark::kMicrosecond);

} // namespace
