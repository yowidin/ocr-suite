find_package(catch2 REQUIRED CONFIG)

add_executable(tests
    src/value_queue.cpp
    )

target_include_directories(tests PRIVATE
    ../include
    )

target_link_libraries(tests PRIVATE
    # ${CONAN_LIBS}
    Catch2::Catch2WithMain
    )

set_target_properties(tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    )
