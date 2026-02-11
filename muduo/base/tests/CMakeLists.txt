include(GoogleTest)

function(add_base_gtest target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE muduo_base GTest::gtest_main)
  set_target_properties(${target} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST)
  set_property(TARGET ${target} PROPERTY FOLDER "tests/base")
endfunction()

function(add_base_benchmark target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE muduo_base benchmark::benchmark_main)
  set_target_properties(${target} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  set_property(TARGET ${target} PROPERTY FOLDER "bench/base")
endfunction()

set(BASE_GTEST_SPECS
  blockingqueue_test BlockingQueue_test.cc
  boundedblockingqueue_test BoundedBlockingQueue_test.cc
  date_test Date_unittest.cc
  timestamp_test Timestamp_unittest.cc
  thread_test Thread_test.cc
  threadpool_test ThreadPool_test.cc
  fileutil_test FileUtil_test.cc
  logstream_test LogStream_test.cc
  exception_test Exception_test.cc
  currentthread_test CurrentThread_test.cc
  singleton_test Singleton_test.cc
  threadlocalsingleton_test ThreadLocalSingleton_test.cc
  weakcallback_test WeakCallback_test.cc
  processinfo_test ProcessInfo_test.cc
  timezone_test TimeZone_unittest.cc
  logging_test Logging_test.cc
  logfile_test LogFile_test.cc
  asynclogging_test AsyncLogging_test.cc
  fork_test Fork_test.cc
  types_test Types_test.cc
  stringpiece_test StringPiece_test.cc
  gzipfile_test GzipFile_test.cc
)

set(_base_specs ${BASE_GTEST_SPECS})
while(_base_specs)
  list(POP_FRONT _base_specs target)
  list(POP_FRONT _base_specs source)
  add_base_gtest(${target} ${source})
endwhile()

if(benchmark_FOUND)
  set(BASE_BENCH_SPECS
    logstream_bench LogStream_bench.cc
    logging_bench Logging_bench.cc
    threadpool_bench ThreadPool_bench.cc
    blockingqueue_bench BlockingQueue_bench.cc
    blockingqueue_bench2 BlockingQueue_bench2.cc
    boundedblockingqueue_impl_bench BoundedBlockingQueue_impl_bench.cc
    thread_bench Thread_bench.cc
  )

  set(_base_bench_specs ${BASE_BENCH_SPECS})
  while(_base_bench_specs)
    list(POP_FRONT _base_bench_specs target)
    list(POP_FRONT _base_bench_specs source)
    add_base_benchmark(${target} ${source})
  endwhile()
endif()
