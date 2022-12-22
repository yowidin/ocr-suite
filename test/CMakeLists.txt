find_package(catch2 REQUIRED CONFIG)

add_executable(tests
    src/value_queue.cpp
    )

target_include_directories(tests PRIVATE
    ../include
    )

target_link_libraries(tests PRIVATE
    Catch2::Catch2WithMain
    )