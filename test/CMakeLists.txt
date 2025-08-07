find_package(Catch2 CONFIG REQUIRED)

add_executable(tests src/value_queue.cpp)

target_include_directories(tests PRIVATE ../include)

target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)

add_test(NAME tests COMMAND tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})