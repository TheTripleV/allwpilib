# cscore macOS Crash Fix — Test & Verification Instructions

## Summary of the bug

On macOS, when an `AVCaptureSession` pipeline stalls (e.g. due to a slow
consumer or a transient USB hiccup), macOS emits the log message:

> Pipe has stalled, error needs to be cleared

and delivers an `AVCaptureSessionRuntimeErrorNotification`.  The previous code
logged the error but **never recovered the session** — it stayed stopped
indefinitely.  In addition, `fmt::vformat_to` calls in the logging path had no
exception handling; any `fmt::format_error` (whether from a corrupted format
string, a mismatched argument count, or a platform-specific edge case) would
propagate uncaught and call `std::terminate`.

## What was fixed

| File | Change |
|---|---|
| `cscore/src/main/native/objcpp/UsbCameraImplObjc.mm` | `sessionRuntimeError:` now dispatches `deviceDisconnect` + `deviceConnect` onto `sessionQueue` to restart the capture pipeline after any runtime error. |
| `cscore/src/main/native/objcpp/UsbCameraDelegate.mm` | Added a null-check on the `CVImageBufferRef` returned by `CMSampleBufferGetImageBuffer`; a stalled session can deliver sample buffers with no image data. |
| `cscore/src/main/native/cpp/Log.cpp` (`cs::NamedLogV`) | Wrapped `fmt::vformat_to` in a `try`/`catch` for `fmt::format_error`; the error text is substituted into the log message instead of crashing. |
| `cscore/src/main/native/cpp/Instance.cpp` (`def_log_func`) | Wrapped the `fmt::print` calls in a `try`/`catch` for `std::exception`; falls back to `fputs` on stderr so the process never terminates from a logging call. |
| `wpiutil/src/main/native/cpp/Logger.cpp` (`Logger::LogV`) | Same `try`/`catch` as `NamedLogV` — guards the shared `vformat_to` path used by `WPI_LOG` macros. |

## Automated unit tests (run on any platform)

The new tests live in `cscore/src/test/native/cpp/LogSafetyTest.cpp`.

### Build

```bash
# From the repository root, assuming a CMake build directory already exists:
cd build                        # or whatever your build directory is called
cmake --build . --target cscore_test
```

If you don't have a build directory yet:

```bash
mkdir build && cd build
cmake ..                        # add -DWITH_TESTS=ON if not already the default
cmake --build . --target cscore_test
```

### Run only the LogSafety tests

```bash
# Linux / macOS
./cscore/cscore_test --gtest_filter="LogSafetyTest.*"

# Windows (Release build)
cscore\Release\cscore_test.exe --gtest_filter="LogSafetyTest.*"
```

Expected output — all four tests pass:

```
[ RUN      ] LogSafetyTest.ValidFormat
[       OK ] LogSafetyTest.ValidFormat
[ RUN      ] LogSafetyTest.UnknownFormatSpecifierDoesNotCrash
[       OK ] LogSafetyTest.UnknownFormatSpecifierDoesNotCrash
[ RUN      ] LogSafetyTest.MismatchedArgsDoesNotCrash
[       OK ] LogSafetyTest.MismatchedArgsDoesNotCrash
[ RUN      ] LogSafetyTest.MessageWithBracesPassesThroughSafely
[       OK ] LogSafetyTest.MessageWithBracesPassesThroughSafely
[==========] 4 tests from 1 test case ran in 0.XXX ms.
[  PASSED  ] 4 tests.
```

## Manual smoke test on macOS (requires a USB camera)

The session-recovery path (`sessionRuntimeError:` → `deviceDisconnect` →
`deviceConnect`) can only be exercised with a real `AVCaptureSession`.  To
verify it end-to-end:

1. **Build** the `cscore` library and the provided `usbviewer` example on a Mac:
   ```bash
   cd build
   cmake .. -DWITH_TESTS=ON
   cmake --build . --target cscore_usbviewer
   ```

2. **Plug in** a USB camera and **start** the viewer:
   ```bash
   ./cscore/examples/usbviewer/cscore_usbviewer
   ```
   Confirm that the camera feed appears and stderr shows:
   > CS: INFO: UsbCamera: Connected to USB camera on …

3. **Simulate a pipe stall** — the simplest way is to briefly unplug and
   re-plug the USB cable while the viewer is running.  You should see:
   - The "Pipe has stalled" message from macOS in the console.
   - The cscore error log: `Capture session runtime error: …`
   - Followed shortly by a reconnection log: `Connected to USB camera on …`
   - The video feed resuming automatically — **no crash**.

   Previously the feed would stop and never come back (and could crash with
   `fmt::v11::format_error`).

4. **Verify no crash** — the process must remain alive and the stream must
   resume.  If it does, the fix is working correctly.
