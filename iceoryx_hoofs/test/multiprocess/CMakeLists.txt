iox_add_executable(
    TARGET  multiprocess-test-publisher
    FILES   ./multiprocess_test_publisher.cpp
    LIBS    iceoryx_posh::iceoryx_posh
)

iox_add_executable(
    TARGET  multiprocess-test-subscriber
    FILES   ./multiprocess_test_subscriber.cpp
    LIBS    iceoryx_posh::iceoryx_posh
)

add_dependencies(multiprocess_tests multiprocess-test-publisher multiprocess-test-subscriber)
file(COPY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/test_multiprocess.py ${CMAKE_CURRENT_BINARY_DIR}/test_multiprocess.py ONLY_IF_DIFFERENT)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_multiprocess.py)
