# Clock GUI

A Clock GUI application built with C++17 and LVGL.

## Features
- **LVGL UI**: Modern graphical interface powered by LVGL 9.5.
- **Asynchronous Buzzer**: Non-blocking buzzer implementation via LVGL timers.
- **CMake Build System**: Easily builds across modern Linux setups.

## Prerequisites
- C++17 compatible compiler (e.g., GCC or Clang)
- CMake (>= 3.10)
- Make or Ninja

## Getting Started

### 1. Clone the Repository
Make sure to include the `--recursive` flag to clone the LVGL submodule:
```bash
git clone --recursive https://github.com/your-username/clock-gui.git
cd clock-gui
```
*(If you have already cloned without submodules, run `git submodule update --init --recursive`)*

### 2. Build the Project
Configure and build using CMake:
```bash
# Configure the build system
cmake -B build -S .

# Compile the source code
cmake --build build -j$(nproc)
```

### 3. Run the Application
```bash
./build/clockOS_gui
```

## Development
- **Code Style**: The project follows Google C++ style with some custom `.clang-format` adjustments. Run clang-format before committing.
- **Dependencies**: LVGL configuration can be found and modified in `lv_conf.h`.
