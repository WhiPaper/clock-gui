cmake_minimum_required(VERSION 3.12)
project(sleepcare_gui LANGUAGES C CXX ASM)

cmake_policy(SET CMP0077 NEW)
cmake_policy(SET CMP0079 NEW)

# --- LVGL Configuration ---
set(LV_BUILD_CONF_PATH "${PROJECT_SOURCE_DIR}/lv_conf.h" CACHE STRING "" FORCE)
set(LV_CONF_SKIP_POSIX_REQD     ON  CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS       OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES    OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)

# --- Dependencies ---
find_package(PkgConfig REQUIRED)
find_package(Threads   REQUIRED)

pkg_check_modules(DRM  REQUIRED IMPORTED_TARGET libdrm)

# --- LVGL ---
add_subdirectory(lvgl)

# Expose libdrm headers to lvgl.
# Required for: lv_linux_drm.h → xf86drmMode.h → drm.h
target_link_libraries(lvgl PUBLIC PkgConfig::DRM)

# --- Application ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(sleepcare-gui ${SOURCES})

target_include_directories(sleepcare-gui PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(sleepcare-gui
    PRIVATE
    lvgl
    Threads::Threads
)

install(TARGETS sleepcare-gui DESTINATION bin)