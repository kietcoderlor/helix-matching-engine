set(HELIX_ENGINE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(helix_core STATIC
  src/version.cpp
  src/codec.cpp
  src/order_book.cpp
)
target_include_directories(helix_core
  PUBLIC
    ${HELIX_ENGINE_INCLUDE_DIR}
)
target_compile_features(helix_core PUBLIC cxx_std_20)

add_executable(helix_engine
  src/main.cpp
)
target_link_libraries(helix_engine PRIVATE helix_core)

add_executable(helix_tests
  tests/test_main.cpp
  tests/test_version.cpp
  tests/test_types.cpp
  tests/test_protocol.cpp
  tests/test_order_book.cpp
)
target_include_directories(helix_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(helix_tests PRIVATE helix_core)

add_test(NAME helix_unit_tests COMMAND helix_tests)
