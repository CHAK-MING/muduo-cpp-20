#pragma once

#include "muduo/base/ProcessInfo.h"
#include "muduo/net/inspect/Inspector.h"

#include <string_view>

namespace muduo::net {

class ProcessInspector {
public:
  void registerCommands(Inspector *ins);

  static string overview(HttpRequest::Method, const Inspector::ArgList &);
  static string pid(HttpRequest::Method, const Inspector::ArgList &);
  static string procStatus(HttpRequest::Method, const Inspector::ArgList &);
  static string openedFiles(HttpRequest::Method, const Inspector::ArgList &);
  static string threads(HttpRequest::Method, const Inspector::ArgList &);

private:
  static const string username_;
};

namespace inspect {

string uptime(Timestamp now, Timestamp start, bool showMicroseconds);
long getLong(const string &content, std::string_view key);
ProcessInfo::CpuTime getCpuTime(std::string_view data);

} // namespace inspect

} // namespace muduo::net
