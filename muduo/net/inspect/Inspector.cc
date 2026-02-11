#include "muduo/net/inspect/Inspector.h"

#include "muduo/base/CurrentThread.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/http/HttpResponse.h"
#include "muduo/net/inspect/PerformanceInspector.h"
#include "muduo/net/inspect/ProcessInspector.h"
#include "muduo/net/inspect/SystemInspector.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iterator>
#include <ranges>
#include <utility>

namespace muduo::net {
namespace {
using namespace std::chrono_literals;

Inspector *&globalInspectorSlot() {
  static Inspector *instance = nullptr;
  return instance;
}

std::vector<string> splitPath(std::string_view path) {
  std::vector<string> out;
  for (auto &&part : path | std::views::split('/')) {
    if (std::ranges::empty(part)) {
      continue;
    }
    out.emplace_back(std::ranges::begin(part), std::ranges::end(part));
  }
  return out;
}

} // namespace

Inspector::Inspector(EventLoop *loop, const InetAddress &httpAddr,
                     const string &name)
    : server_(loop, httpAddr, "Inspector:" + name),
      processInspector_(std::make_unique<ProcessInspector>()),
      systemInspector_(std::make_unique<SystemInspector>()) {
  assert(CurrentThread::isMainThread());
  assert(globalInspectorSlot() == nullptr);
  globalInspectorSlot() = this;

  server_.setHttpCallback(
      [this](const HttpRequest &req, HttpResponse *resp) { onRequest(req, resp); });

  processInspector_->registerCommands(this);
  systemInspector_->registerCommands(this);
#ifdef HAVE_TCMALLOC
  performanceInspector_ = std::make_unique<PerformanceInspector>();
  performanceInspector_->registerCommands(this);
#endif

  (void)loop->runAfter(0ms, TimerCallback([this] { start(); }));
}

Inspector::~Inspector() {
  assert(CurrentThread::isMainThread());
  globalInspectorSlot() = nullptr;
}

void Inspector::add(const string &moduleName, const string &command, Callback cb,
                    const string &help) {
  std::scoped_lock lock(mutex_);
  modules_[moduleName][command] = std::move(cb);
  helps_[moduleName][command] = help;
}

void Inspector::remove(const string &moduleName, const string &command) {
  std::scoped_lock lock(mutex_);
  if (const auto it = modules_.find(moduleName); it != modules_.end()) {
    it->second.erase(command);
  }
  if (const auto it = helps_.find(moduleName); it != helps_.end()) {
    it->second.erase(command);
  }
}

void Inspector::start() { server_.start(); }

void Inspector::onRequest(const HttpRequest &req, HttpResponse *resp) {
  if (req.path() == "/") {
    string result;
    std::scoped_lock lock(mutex_);
    std::ranges::for_each(helps_, [&result](const auto &moduleEntry) {
      const auto &[moduleName, helpList] = moduleEntry;
      std::ranges::for_each(
          helpList, [&result, &moduleName](const auto &helpEntry) {
        const auto &[command, help] = helpEntry;
        result += "/";
        result += moduleName;
        result += "/";
        result += command;
        const size_t len = moduleName.size() + command.size();
        result += string(len >= 25 ? 1 : 25 - len, ' ');
        result += help;
        result += '\n';
      });
    });
    resp->setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType("text/plain");
    resp->setBody(result);
    return;
  }

  const auto parts = splitPath(req.path());
  if (parts.size() == 1 && parts.front() == "favicon.ico") {
    resp->setStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
    resp->setStatusMessage("Not Found");
    return;
  }

  bool ok = false;
  if (parts.size() >= 2) {
    const auto &moduleName = parts[0];
    const auto &command = parts[1];

    std::scoped_lock lock(mutex_);
    if (const auto moduleIt = modules_.find(moduleName);
        moduleIt != modules_.end()) {
      const auto &commands = moduleIt->second;
      if (const auto cmdIt = commands.find(command); cmdIt != commands.end() &&
          static_cast<bool>(cmdIt->second)) {
        ArgList args;
        std::ranges::copy(parts | std::views::drop(2), std::back_inserter(args));
        resp->setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/plain");
        resp->setBody(cmdIt->second(req.method(), args));
        ok = true;
      }
    }
  } else {
    muduo::logDebug("Invalid inspect path: {}", req.path());
  }

  if (!ok) {
    resp->setStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
    resp->setStatusMessage("Not Found");
  }
}

} // namespace muduo::net
