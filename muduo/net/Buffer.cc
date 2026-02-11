#include "muduo/net/Buffer.h"

#include "muduo/net/SocketsOps.h"

#include <cerrno>
#include <sys/uio.h>

using namespace muduo::net;

ssize_t Buffer::readFd(int fd, int *savedErrno) {
  std::array<std::byte, 65536> extraBuffer;

  std::array<iovec, 2> vec;
  const size_t writable = writableBytes();
  auto writableView = this->writableSpan();
  vec.at(0).iov_base = writableView.data();
  vec.at(0).iov_len = writable;
  vec.at(1).iov_base = extraBuffer.data();
  vec.at(1).iov_len = extraBuffer.size();

  const int iovcnt = (writable < extraBuffer.size()) ? 2 : 1;
  const auto iov = std::span<const iovec>(vec).first(static_cast<size_t>(iovcnt));
  const ssize_t n = sockets::readv(fd, iov);
  if (n < 0) {
    *savedErrno = errno;
    return n;
  }

  if (static_cast<size_t>(n) <= writable) {
    writerIndex_ += static_cast<size_t>(n);
    return n;
  }

  writerIndex_ = buffer_.size();
  append(std::span<const std::byte>{extraBuffer.data(),
                                    static_cast<size_t>(n) - writable});
  return n;
}
