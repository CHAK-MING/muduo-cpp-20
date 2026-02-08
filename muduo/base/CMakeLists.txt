set(base_SRCS
  CurrentThread.cc
  Date.cc
  Exception.cc
  FileUtil.cc
  LogFile.cc
  Logging.cc
  LogStream.cc
  AsyncLogging.cc
  ProcessInfo.cc
  Thread.cc
  ThreadPool.cc
  TimeZone.cc
  Timestamp.cc
)

add_library(muduo_base ${base_SRCS})

target_include_directories(muduo_base
  BEFORE PUBLIC
    ${PROJECT_SOURCE_DIR}
)

# For C++20, we don't need to link pthread explicitly on many platforms
# but keeping it for compatibility
if(NOT WIN32)
  target_link_libraries(muduo_base PUBLIC pthread Boost::system ZLIB::ZLIB)
else()
  target_link_libraries(muduo_base PUBLIC Boost::system ZLIB::ZLIB)
endif()

install(TARGETS muduo_base DESTINATION lib)
file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/muduo/base)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
