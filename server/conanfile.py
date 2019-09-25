from conans import ConanFile, CMake, tools

class GeoserveConan( ConanFile ):
    name            = "trident-webrtc-pusher"
    license         = "Proprietary"
    url             = "https://gitlab.com/openrov/trident/"
    description     = "Pushes trident video messages to webrtc server"
    requires        = (
                        # ( "orovcore/1.1.0@openrov/stable" ),
                        # ( "orovmsg/3.6.1@openrov/stable" ),
                        # ( "jsonformoderncpp/3.7.0@vthiery/stable" ),
                        # ( "libcurl/7.64.1@bincrafters/stable" ),
                        ( "jsonformoderncpp/3.7.0@vthiery/stable" ),
                        ( "boost/1.68.0@conan/stable" )
    )
    options         = { "arm": [True, False] }
    default_options = { "arm": False }
    generators      = "cmake"

    # def configure( self ):
    #     self.options["OpenSSL"].shared = False
    #     self.options["libcurl"].shared = True

    def system_requirements(self):
        package_tool = tools.SystemPackageTool()

    def build( self ):
        cmake = CMake(self)
        # Add warnings
        # cmake.definitions["CMAKE_CXX_FLAGS"]="-Wall -Wextra -Wno-class-memaccess -Wno-expansion-to-defined -Wno-unused-parameter"
        # cmake.definitions["CMAKE_INSTALL_PREFIX"] = self.package_folder + "/opt/openrov/"

        # cmake.definitions["BUILD_CPR_TESTS"] = "OFF"
        # cmake.definitions["USE_SYSTEM_CURL"] = "ON"

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

        self.copy ("systemd/*", "lib/systemd/system", keep_path=False)