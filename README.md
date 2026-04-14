# Clock GUI

LVGL digital clock application with drowsiness alerts driven by a Python OpenCV detector.

## What this project does

- Renders clock UI with LVGL.
- Reads drowsiness status from EARSYS through a POSIX shared memory bridge.
- Shows "일어나세요!" in LVGL and triggers buzzer beeps while drowsy.

## Integration Architecture

- Python detector (EARSYS) writes drowsy status to POSIX shared memory.
- LVGL app reads shared memory in the main loop.
- When status is drowsy, `ClockApp` updates alert label and drives `Buzzer`.

Default shared memory name:

```text
/earsys_drowsy_shm
```

Override with environment variable:

```bash
export EARSYS_SHM_NAME=/earsys_drowsy_shm
```

## Submodule Layout

EARSYS is intended to be mounted as git submodule:

```text
external/EARSYS
```

Add it with:

```bash
git submodule add https://github.com/WhiPaper/EARSYS external/EARSYS
git submodule update --init --recursive
```

## Build

```bash
cmake -B build -S .
cmake --build build -j$(nproc)
```

## Run

1. Start detector in one terminal:

```bash
cd external/EARSYS
EARSYS_SHM_NAME=/earsys_drowsy_shm python3 main.py
```

2. Start LVGL clock in another terminal:

```bash
EARSYS_SHM_NAME=/earsys_drowsy_shm ./build/clockOS_gui
```

## Notes

- Shared memory status code `1` means drowsy, so LVGL shows "일어나세요!" and beeps.
- If the shared memory object is not present yet, LVGL falls back to non-drowsy state.
