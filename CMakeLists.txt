cmake_minimum_required(VERSION 3.12)
project(clockOS_gui LANGUAGES C CXX ASM)

# 1. Policy Fix: Ensure our variables override LVGL internal options
cmake_policy(SET CMP0077 NEW)

# --- 2. LVGL Global Configuration (MUST be BEFORE add_subdirectory) ---

# Tell LVGL exactly where our config file is
set(LV_BUILD_CONF_PATH "${PROJECT_SOURCE_DIR}/lv_conf.h" CACHE STRING "" FORCE)
set(LV_CONF_SKIP_POSIX_REQD ON CACHE BOOL "" FORCE)

# Strictly disable demos and examples to avoid the Persian font error
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Optimization: Disable other heavy internal features if not needed
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)

# --- 3. Add LVGL Subdirectory ---
add_subdirectory(lvgl)

# --- 4. Define Application Target ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES})

# --- 5. Include Directories ---
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)

# --- 6. Link Libraries ---
target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
    lvgl
    gpiod
    pthread
)