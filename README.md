# Clock GUI

LVGL digital clock application with drowsiness alerts driven by an external detector over POSIX shared memory.

## What this project does

- Renders clock UI with LVGL.
- Reads drowsiness status through a POSIX shared memory bridge.
- Shows "일어나세요!" in LVGL and triggers buzzer beeps while drowsy.

## Integration Architecture

- External detector process writes drowsy status to POSIX shared memory.
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

## Build

```bash
cmake -B build -S .
cmake --build build -j$(nproc)
```

## Run

1. Start your detector in one terminal (it must publish the shared memory protocol used by this app).

```bash
# Example environment variable only; detector command depends on your setup.
EARSYS_SHM_NAME=/earsys_drowsy_shm <your_detector_command>
```

2. Start LVGL clock in another terminal:

```bash
EARSYS_SHM_NAME=/earsys_drowsy_shm ./build/clockOS_gui
```

## Notes

- Shared memory status code `1` means drowsy, so LVGL shows "일어나세요!" and beeps.
- If the shared memory object is not present yet, LVGL falls back to non-drowsy state.
