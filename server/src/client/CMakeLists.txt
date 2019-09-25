cmake_minimum_required(VERSION 3.0)
project(example-client)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -std=c++14 -Wno-unused-parameter -Wno-unused-value")

find_package(Threads REQUIRED)

include_directories(
    ${LIBWEBRTC_INCLUDE_PATH}
    "${LIBWEBRTC_INCLUDE_PATH}/third_party/abseil-cpp"
	"../"
)

include_directories("src/")
include_directories(${CMAKE_CURRENT_BINARY_DIR})

file(GLOB PROJECT_CONFIGS CMakeLists.txt
                         .gitattributes
                         .gitignore
                         .gitmodules)

add_executable(client_exe
		main.cpp
		../observers.h)

target_compile_definitions(client_exe PUBLIC
	WEBRTC_POSIX
)

set_target_properties(client_exe PROPERTIES OUTPUT_NAME example-client
    RUNTIME_OUTPUT_DIRECTORY /${CMAKE_BINARY_DIR}/bin/)

target_link_libraries(client_exe
    ${LIBWEBRTC_BINARY_PATH}/libwebrtc${CMAKE_STATIC_LIBRARY_SUFFIX}
    ${CMAKE_THREAD_LIBS_INIT}
    CONAN_PKG::boost
	CONAN_PKG::jsonformoderncpp
	CONAN_PKG::websocketpp
    pthread)

