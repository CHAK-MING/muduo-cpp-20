#include "muduo/base/LogFile.h"

#include "muduo/base/FileUtil.h"
#include "muduo/base/ProcessInfo.h"

#include <cassert>
#include <chrono>
#include <format>
#include <mutex>
#include <utility>

using namespace muduo;

LogFile::LogFile(string basename, std::int64_t rollSize, bool threadSafe,
                 int flushInterval, int checkEveryN)
    : basename_(std::move(basename)), rollSize_(rollSize),
      flushInterval_(flushInterval), checkEveryN_(checkEveryN),
      mutex_(threadSafe ? std::make_unique<std::mutex>() : nullptr) {
  assert(std::string_view(basename_).find('/') == std::string_view::npos);
  rollFile();
}

LogFile::~LogFile() = default;

void LogFile::append(const char *logline, int len) {
  std::unique_lock<std::mutex> lock;
  if (mutex_) {
    lock = std::unique_lock<std::mutex>(*mutex_);
  }
  append_unlocked(logline, len);
}

void LogFile::append(std::string_view logline) {
  append(logline.data(), static_cast<int>(logline.size()));
}

void LogFile::flush() {
  std::unique_lock<std::mutex> lock;
  if (mutex_) {
    lock = std::unique_lock<std::mutex>(*mutex_);
  }
  file_->flush();
}

void LogFile::append_unlocked(const char *logline, int len) {
  file_->append(logline, static_cast<size_t>(len));

  if (file_->writtenBytes() > rollSize_) {
    rollFile();
  } else {
    ++count_;
    if (count_ >= checkEveryN_) {
      count_ = 0;
      auto now = std::chrono::system_clock::now();
      auto thisPeriod = std::chrono::floor<std::chrono::days>(now);
      auto thisPeriodTp =
          std::chrono::time_point_cast<std::chrono::system_clock::duration>(
              thisPeriod);

      if (thisPeriodTp != startOfPeriod_) {
        rollFile();
      } else if (now - lastFlush_ > std::chrono::seconds(flushInterval_)) {
        lastFlush_ = now;
        file_->flush();
      }
    }
  }
}

bool LogFile::rollFile() {
  auto now = std::chrono::system_clock::now();
  string filename = getLogFileName(basename_, &now);

  auto start = std::chrono::floor<std::chrono::days>(now);
  auto startTp =
      std::chrono::time_point_cast<std::chrono::system_clock::duration>(start);

  if (now > lastRoll_) {
    lastRoll_ = now;
    lastFlush_ = now;
    startOfPeriod_ = startTp;
    file_ = std::make_unique<FileUtil::AppendFile>(std::string_view(filename));
    return true;
  }
  return false;
}

string LogFile::getLogFileName(const string &basename,
                               std::chrono::system_clock::time_point *now) {
  *now = std::chrono::system_clock::now();
  const auto nowSec = std::chrono::floor<std::chrono::seconds>(*now);

  const auto pid = ProcessInfo::pid();
  const auto host = ProcessInfo::hostname();

  const string stamp = std::format(".{:%Y%m%d-%H%M%S}.", nowSec);
  return std::format("{}{}{}.{}.log", basename, stamp, host, pid);
}
