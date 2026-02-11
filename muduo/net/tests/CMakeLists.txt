include(GoogleTest)

function(add_net_gtest target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE muduo_net GTest::gtest_main)
  set_target_properties(${target} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST)
  set_property(TARGET ${target} PROPERTY FOLDER "tests/net")
endfunction()

function(add_net_benchmark target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE muduo_net benchmark::benchmark_main)
  set_target_properties(${target} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  set_property(TARGET ${target} PROPERTY FOLDER "bench/net")
endfunction()

set(NET_GTEST_SPECS
  net_buffer_test Buffer_test.cc
  net_inetaddress_test InetAddress_test.cc
  net_zlibstream_test ZlibStream_test.cc
  net_eventloop_test EventLoop_test.cc
  net_eventloopthread_test EventLoopThread_test.cc
  net_eventloopthreadpool_test EventLoopThreadPool_test.cc
  net_timerqueue_test TimerQueue_test.cc
  net_socketsops_test SocketsOps_test.cc
  net_channel_test Channel_test.cc
  net_poller_test Poller_test.cc
  net_httprequest_test HttpRequest_test.cc
  net_httpserver_test HttpServer_test.cc
  net_inspector_test Inspector_test.cc
  net_echoserver_test EchoServer_test.cc
  net_echoclient_test EchoClient_test.cc
  net_tcpclient_reg_test TcpClient_reg_test.cc
  net_api_compatibility_test ApiCompatibility_test.cc
)

set(_net_specs ${NET_GTEST_SPECS})
while(_net_specs)
  list(POP_FRONT _net_specs target)
  list(POP_FRONT _net_specs source)
  add_net_gtest(${target} ${source})
endwhile()

if(TARGET muduo_protorpc_wire)
  add_executable(net_rpccodec_test RpcCodec_test.cc)
  target_link_libraries(net_rpccodec_test PRIVATE
    muduo_protorpc_wire
    muduo_protobuf_codec
    muduo_net
    GTest::gtest_main
  )
  set_target_properties(net_rpccodec_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  gtest_discover_tests(net_rpccodec_test DISCOVERY_MODE PRE_TEST)
  set_property(TARGET net_rpccodec_test PROPERTY FOLDER "tests/net")

  find_library(ABSL_CHECK_OP_LIB NAMES absl_log_internal_check_op)
  if(ABSL_CHECK_OP_LIB)
    target_link_libraries(net_rpccodec_test PRIVATE ${ABSL_CHECK_OP_LIB})
  endif()
endif()

if(benchmark_FOUND)
  add_net_benchmark(net_echo_bench Echo_bench.cc)
endif()

add_executable(net_httpserver_bench HttpServer_bench.cc)
target_link_libraries(net_httpserver_bench PRIVATE muduo_net)
set_target_properties(net_httpserver_bench PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
set_property(TARGET net_httpserver_bench PROPERTY FOLDER "bench/net")
