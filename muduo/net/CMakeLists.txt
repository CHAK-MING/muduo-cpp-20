set(net_SRCS
  Acceptor.cc
  boilerplate.cc
  Buffer.cc
  Channel.cc
  Connector.cc
  EventLoop.cc
  EventLoopThread.cc
  EventLoopThreadPool.cc
  InetAddress.cc
  Poller.cc
  Socket.cc
  SocketsOps.cc
  TcpClient.cc
  TcpConnection.cc
  TcpServer.cc
  Timer.cc
  TimerQueue.cc
  ZlibStream.cc
  http/HttpContext.cc
  http/HttpResponse.cc
  http/HttpServer.cc
  inspect/Inspector.cc
  inspect/PerformanceInspector.cc
  inspect/ProcessInspector.cc
  inspect/SystemInspector.cc
  poller/DefaultPoller.cc
  poller/EPollPoller.cc
  poller/PollPoller.cc
)

add_library(muduo_net ${net_SRCS})

target_include_directories(muduo_net
  BEFORE PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(muduo_net PUBLIC muduo_base)

find_package(Protobuf QUIET)
if(Protobuf_FOUND)
  add_library(muduo_protobuf_codec protobuf/ProtobufCodecLite.cc)
  target_link_libraries(muduo_protobuf_codec
    PUBLIC
      muduo_net
      protobuf::libprotobuf
      ZLIB::ZLIB
  )
  target_include_directories(muduo_protobuf_codec
    BEFORE PUBLIC
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )
  install(TARGETS muduo_protobuf_codec
    EXPORT muduoTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  set(RPC_PROTO ${CMAKE_CURRENT_SOURCE_DIR}/protorpc/rpc.proto)
  set(RPC_PROTO_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/protorpc)
  file(MAKE_DIRECTORY ${RPC_PROTO_OUT_DIR})

  add_custom_command(
    OUTPUT ${RPC_PROTO_OUT_DIR}/rpc.pb.cc ${RPC_PROTO_OUT_DIR}/rpc.pb.h
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
      --cpp_out=${RPC_PROTO_OUT_DIR}
      -I ${CMAKE_CURRENT_SOURCE_DIR}/protorpc
      ${RPC_PROTO}
    DEPENDS ${RPC_PROTO}
    VERBATIM
  )

  add_library(muduo_protorpc_wire
    ${RPC_PROTO_OUT_DIR}/rpc.pb.cc
    protorpc/RpcCodec.cc
  )
  target_include_directories(muduo_protorpc_wire
    BEFORE PUBLIC
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
      $<BUILD_INTERFACE:${RPC_PROTO_OUT_DIR}>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )
  target_link_libraries(muduo_protorpc_wire
    PUBLIC
      muduo_protobuf_codec
      protobuf::libprotobuf
      ZLIB::ZLIB
  )

  add_library(muduo_protorpc
    protorpc/RpcChannel.cc
    protorpc/RpcServer.cc
  )
  target_include_directories(muduo_protorpc
    BEFORE PUBLIC
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
      $<BUILD_INTERFACE:${RPC_PROTO_OUT_DIR}>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )
  target_link_libraries(muduo_protorpc
    PUBLIC
      muduo_protorpc_wire
      muduo_protobuf_codec
      muduo_net
      protobuf::libprotobuf
      ZLIB::ZLIB
  )

  install(TARGETS muduo_protorpc_wire muduo_protorpc
    EXPORT muduoTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
else()
  message(STATUS "Protobuf not found, skip muduo_protobuf_codec target.")
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

install(TARGETS muduo_net
  EXPORT muduoTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
file(GLOB ROOT_HEADERS "*.h")
file(GLOB HTTP_HEADERS "http/*.h")
file(GLOB INSPECT_HEADERS "inspect/*.h")
file(GLOB PROTOBUF_HEADERS "protobuf/*.h")
file(GLOB PROTORPC_HEADERS "protorpc/*.h" "protorpc/*.proto")
install(FILES ${ROOT_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/muduo/net)
install(FILES ${HTTP_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/muduo/net/http)
install(FILES ${INSPECT_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/muduo/net/inspect)
install(FILES ${PROTOBUF_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/muduo/net/protobuf)
install(FILES ${PROTORPC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/muduo/net/protorpc)
