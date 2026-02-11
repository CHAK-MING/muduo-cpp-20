#pragma once

#include "muduo/base/Types.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/http/HttpRequest.h"
#include "muduo/net/http/HttpServer.h"

#include <concepts>
#include <map>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

namespace muduo::net {

class ProcessInspector;
class PerformanceInspector;
class SystemInspector;

class Inspector {
public:
  using ArgList = std::vector<string>;
  using Callback = CallbackFunction<string(HttpRequest::Method, const ArgList &)>;

  Inspector(EventLoop *loop, const InetAddress &httpAddr, const string &name);
  ~Inspector();

  void add(const string &moduleName, const string &command, Callback cb,
           const string &help);
  template <typename F>
    requires CallbackBindable<F, Callback>
  void add(const string &moduleName, const string &command, F &&cb,
           const string &help) {
    add(moduleName, command, Callback(std::forward<F>(cb)), help);
  }
  void remove(const string &moduleName, const string &command);

private:
  using CommandList = std::map<string, Callback>;
  using HelpList = std::map<string, string>;

  void start();
  void onRequest(const HttpRequest &req, HttpResponse *resp);

  HttpServer server_;
  std::unique_ptr<ProcessInspector> processInspector_;
  std::unique_ptr<PerformanceInspector> performanceInspector_;
  std::unique_ptr<SystemInspector> systemInspector_;

  std::mutex mutex_;
  std::map<string, CommandList> modules_;
  std::map<string, HelpList> helps_;
};

} // namespace muduo::net
