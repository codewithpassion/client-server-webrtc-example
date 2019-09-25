from conans import ConanFile, CMake, tools

class WebRTC( ConanFile ):
    name            = "client-server-webrtc-example"
    license         = "MIT"
    url             = "https://github.com/codewithpassion/client-server-webrtc-example"
    description     = "Very basic webrtc DataChannel example"
    requires        = (
                        ( "jsonformoderncpp/3.7.0@vthiery/stable" ),
                        ( "websocketpp/0.8.1@bincrafters/stable" ),
                        ( "boost/1.68.0@conan/stable" )
    )
    options         = { "arm": [True, False] }
    default_options = { "arm": False }
    generators      = "cmake"


    def system_requirements(self):
        package_tool = tools.SystemPackageTool()

    def build( self ):
        cmake = CMake(self)
        # Add warnings

        cmake.definitions["LIBWEBRTC_INCLUDE_PATH"] = "~/workspace/webrtc-m74/src"
        cmake.definitions["LIBWEBRTC_INCLUDE_PATH:PATH"] = "~/workspace/webrtc-m74/src"

        cmake.definitions["LIBWEBRTC_BINARY_PATH:PATH"] = "~/workspace/webrtc-m74/src/out/build/obj"
        if self.options.arm:
            cmake.definitions["LIBWEBRTC_BINARY_PATH:PATH"] = "~/workspace/webrtc-m74/src/out/build-m74/obj"
            cmake.definitions["CMAKE_CROSSCOMPILING"] = "ON"
            cmake.definitions["CMAKE_SYSTEM_PROCESSOR"] = "armv7"
            cmake.definitions["CMAKE_SYSTEM_NAME"] = "Linux"
            cmake.definitions["CMAKE_C_COMPILER"] = "arm-linux-gnueabihf-gcc"
            cmake.definitions["CMAKE_CXX_COMPILER"] = "arm-linux-gnueabihf-g++"
            cmake.definitions["CMAKE_FIND_ROOT_PATH_MODE_PROGRAM"] = "NEVER"
            cmake.definitions["CMAKE_FIND_ROOT_PATH_MODE_INCLUDE"] ="ONLY"
            cmake.definitions["CMAKE_FIND_ROOT_PATH_MODE_LIBRARY"] ="ONLY"
            cmake.definitions["CMAKE_FIND_ROOT_PATH_MODE_PACKAGE"] ="ONLY"

        cmake.configure()
        cmake.build()

    def package( self ):
        cmake = CMake(self)
        cmake.install()
