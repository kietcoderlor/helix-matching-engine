set(HELIX_ENGINE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(helix_core STATIC
  src/version.cpp
  src/codec.cpp
  src/order_book.cpp
  src/matching_engine.cpp
  src/replay.cpp
  src/net/socket.cpp
  src/net/connection.cpp
  src/net/server.cpp
  src/engine_service.cpp
)
target_include_directories(helix_core
  PUBLIC
    ${HELIX_ENGINE_INCLUDE_DIR}
)
target_compile_features(helix_core PUBLIC cxx_std_20)

if(WIN32)
  target_sources(helix_core PRIVATE src/net/poller_wsapoll.cpp)
  target_link_libraries(helix_core PUBLIC ws2_32)
else()
  target_sources(helix_core PRIVATE src/net/poller_epoll.cpp)
endif()

add_executable(helix_engine
  src/main.cpp
)
target_link_libraries(helix_engine PRIVATE helix_core)

add_executable(helix_replay
  src/replay_main.cpp
)
target_link_libraries(helix_replay PRIVATE helix_core)

add_executable(helix_tests
  tests/test_main.cpp
  tests/test_version.cpp
  tests/test_types.cpp
  tests/test_protocol.cpp
  tests/test_order_book.cpp
  tests/test_matching.cpp
  tests/test_cancel_modify.cpp
  tests/test_tif_stp.cpp
  tests/test_replay.cpp
  tests/test_net.cpp
  tests/test_server.cpp
  tests/test_spsc.cpp
  tests/test_pipeline.cpp
  tests/test_routing.cpp
)
target_include_directories(helix_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(helix_tests PRIVATE helix_core)
target_compile_definitions(helix_tests PRIVATE
  HELIX_TAPES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tapes"
)

add_test(NAME helix_unit_tests COMMAND helix_tests)

add_test(
  NAME helix_replay_cross
  COMMAND helix_replay ${CMAKE_CURRENT_SOURCE_DIR}/tapes/cross.tape
)
