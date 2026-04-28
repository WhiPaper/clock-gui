cmake_minimum_required(VERSION 3.16)
project(sleepcare_core LANGUAGES C CXX)

set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(Threads   REQUIRED)

pkg_check_modules(LWS      REQUIRED IMPORTED_TARGET libwebsockets)
pkg_check_modules(OPENSSL  REQUIRED IMPORTED_TARGET openssl)
pkg_check_modules(AVAHI    REQUIRED IMPORTED_TARGET avahi-client)
pkg_check_modules(QRENCODE REQUIRED IMPORTED_TARGET libqrencode)
pkg_check_modules(CJSON    REQUIRED IMPORTED_TARGET libcjson)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.c")

add_executable(sleepcare-core ${SOURCES})

target_include_directories(sleepcare-core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../include   # sleepcare_proto.h
)

target_link_libraries(sleepcare-core PRIVATE
    PkgConfig::LWS
    PkgConfig::OPENSSL
    PkgConfig::AVAHI
    PkgConfig::QRENCODE
    PkgConfig::CJSON
    Threads::Threads
)

install(TARGETS sleepcare-core DESTINATION bin)
