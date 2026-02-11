#include "muduo/net/inspect/PerformanceInspector.h"

#include "muduo/base/CurrentThread.h"
#include "muduo/base/FileUtil.h"
#include "muduo/base/LogStream.h"
#include "muduo/base/ProcessInfo.h"

#include <format>
#include <unistd.h>

#ifdef HAVE_TCMALLOC
#include <gperftools/malloc_extension.h>
#include <gperftools/profiler.h>
#endif

namespace muduo::net {

void PerformanceInspector::registerCommands(Inspector *ins) {
  ins->add("pprof", "heap", PerformanceInspector::heap, "get heap information");
  ins->add("pprof", "growth", PerformanceInspector::growth,
           "get heap growth information");
  ins->add("pprof", "profile", PerformanceInspector::profile,
           "get cpu profiling information. blocks thread for 30 seconds.");
  ins->add("pprof", "cmdline", PerformanceInspector::cmdline, "get command line");
  ins->add("pprof", "memstats", PerformanceInspector::memstats, "get memory stats");
  ins->add("pprof", "memhistogram", PerformanceInspector::memhistogram,
           "get memory histogram");
  ins->add("pprof", "releasefreememory", PerformanceInspector::releaseFreeMemory,
           "release free memory");
}

string PerformanceInspector::heap(HttpRequest::Method, const Inspector::ArgList &) {
#ifdef HAVE_TCMALLOC
  std::string out;
  MallocExtension::instance()->GetHeapSample(&out);
  return string(out.data(), out.size());
#else
  return "tcmalloc is not enabled\n";
#endif
}

string PerformanceInspector::growth(HttpRequest::Method,
                                    const Inspector::ArgList &) {
#ifdef HAVE_TCMALLOC
  std::string out;
  MallocExtension::instance()->GetHeapGrowthStacks(&out);
  return string(out.data(), out.size());
#else
  return "tcmalloc is not enabled\n";
#endif
}

string PerformanceInspector::profile(HttpRequest::Method,
                                     const Inspector::ArgList &) {
#ifdef HAVE_TCMALLOC
  string filename = "/tmp/" + ProcessInfo::procname() + "." +
                    ProcessInfo::pidString() + "." + Timestamp::now().toString() +
                    ".profile";

  string profileData;
  if (ProfilerStart(filename.c_str())) {
    CurrentThread::sleepUsec(30 * 1000 * 1000);
    ProfilerStop();
    FileUtil::readFile(filename, 1024 * 1024, &profileData, nullptr, nullptr);
    ::unlink(filename.c_str());
  }
  return profileData;
#else
  return "tcmalloc is not enabled\n";
#endif
}

string PerformanceInspector::cmdline(HttpRequest::Method,
                                     const Inspector::ArgList &) {
  return {};
}

string PerformanceInspector::memstats(HttpRequest::Method,
                                      const Inspector::ArgList &) {
#ifdef HAVE_TCMALLOC
  char buf[64 * 1024];
  MallocExtension::instance()->GetStats(buf, sizeof(buf));
  return buf;
#else
  return "tcmalloc is not enabled\n";
#endif
}

string PerformanceInspector::memhistogram(HttpRequest::Method,
                                          const Inspector::ArgList &) {
#ifdef HAVE_TCMALLOC
  int blocks = 0;
  size_t total = 0;
  int histogram[kMallocHistogramSize] = {0};

  MallocExtension::instance()->MallocMemoryStats(&blocks, &total, histogram);

  LogStream s;
  s << "blocks " << blocks << "\ntotal " << total << "\n";
  for (int i = 0; i < kMallocHistogramSize; ++i) {
    s << i << " " << histogram[i] << "\n";
  }
  return s.buffer().toString();
#else
  return "tcmalloc is not enabled\n";
#endif
}

string PerformanceInspector::releaseFreeMemory(HttpRequest::Method,
                                               const Inspector::ArgList &) {
#ifdef HAVE_TCMALLOC
  const auto result =
      std::format("memory release rate: {}\nAll free memory released.\n",
                  MallocExtension::instance()->GetMemoryReleaseRate());
  MallocExtension::instance()->ReleaseFreeMemory();
  return result;
#else
  return "tcmalloc is not enabled\n";
#endif
}

string PerformanceInspector::symbol(HttpRequest::Method,
                                    const Inspector::ArgList &) {
  return "not implemented\n";
}

} // namespace muduo::net
