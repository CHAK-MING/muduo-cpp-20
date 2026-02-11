#pragma once

#include "muduo/net/inspect/Inspector.h"

namespace muduo::net {

class SystemInspector {
public:
  void registerCommands(Inspector *ins);

  static string overview(HttpRequest::Method, const Inspector::ArgList &);
  static string loadavg(HttpRequest::Method, const Inspector::ArgList &);
  static string version(HttpRequest::Method, const Inspector::ArgList &);
  static string cpuinfo(HttpRequest::Method, const Inspector::ArgList &);
  static string meminfo(HttpRequest::Method, const Inspector::ArgList &);
  static string stat(HttpRequest::Method, const Inspector::ArgList &);
};

} // namespace muduo::net
