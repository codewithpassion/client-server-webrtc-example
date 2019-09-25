cmake_minimum_required(VERSION 3.0)
project(example-server)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -std=c++11 -Wno-unused-parameter -Wno-unused-value")

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

find_package(Threads REQUIRED)
# find_package(LibWebRTC REQUIRED)

include_directories(
    ${LIBWEBRTC_INCLUDE_PATH}
    "${LIBWEBRTC_INCLUDE_PATH}/third_party/abseil-cpp"
)

include_directories("src/")
include_directories("lib/")
include_directories(${CMAKE_CURRENT_BINARY_DIR})

file(GLOB PROJECT_CONFIGS CMakeLists.txt
                         .gitattributes
                         .gitignore
                         .gitmodules)

add_executable(server_exe src/main.cpp src/observers.h)

target_compile_definitions(server_exe PUBLIC
	WEBRTC_POSIX
)

set_target_properties(server_exe PROPERTIES OUTPUT_NAME example-server
    RUNTIME_OUTPUT_DIRECTORY /${CMAKE_BINARY_DIR}/bin/)

target_link_libraries(server_exe
    ${LIBWEBRTC_BINARY_PATH}/libwebrtc${CMAKE_STATIC_LIBRARY_SUFFIX}
    ${CMAKE_THREAD_LIBS_INIT}
    CONAN_PKG::boost
	CONAN_PKG::jsonformoderncpp
    pthread)

