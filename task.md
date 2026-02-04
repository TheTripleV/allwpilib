# cscore macOS Crash Fix — Test & Verification Instructions

## Root cause

The crash is in `UvcControlImpl.mm`, `sendControlRequest:`, the `default:`
case of the IOKit error switch (line ~487).  The format string was:

```
"ControlRequest failed (KR=sys:sub:code) = {:02Xh}:{:03Xh}:{:04Xh}"
```

The trailing `h` on each specifier is a C `printf` length modifier.  It does
not exist in fmt.  When fmt parses `{:02Xh}` it reaches `h`, does not
recognise it as a valid format type, and throws
`fmt::format_error("unknown format specifier")`.  Nothing caught the
exception, so `std::terminate` was called.

The `UVCERROR` macro does **not** wrap its format string with `FMT_STRING`,
so the compiler never validated the specifiers.

### Why "Pipe has stalled" appears first

`"Pipe has stalled, error needs to be cleared"` is logged by the
`kIOUSBPipeStalled` case in the same switch (line ~481) — it is a cscore
log message, not a macOS system message.  A stalled pipe is not cleared
before the next control request is attempted, so that next request fails
with a *different* IOKit error code, falls into `default:`, and hits the
bad format string.

## What was fixed

| File | Change |
|---|---|
| `cscore/src/main/native/objcpp/UvcControlImpl.mm` | Removed the `h` suffix: `{:02Xh}` → `{:02X}` (likewise for the other two specifiers).  This is the direct fix for the crash. |
| `cscore/src/main/native/cpp/Log.cpp` (`cs::NamedLogV`) | `fmt::vformat_to` is now wrapped in `try`/`catch` for `fmt::format_error`.  Any future typo like this surfaces as `<fmt error: …>` in the log instead of terminating. |
| `wpiutil/src/main/native/cpp/Logger.cpp` (`Logger::LogV`) | Same `try`/`catch` — guards the `vformat_to` path used by `WPI_LOG` macros. |
| `cscore/src/main/native/objcpp/UsbCameraImplObjc.mm` | `sessionRuntimeError:` now dispatches `deviceDisconnect` + `deviceConnect` onto `sessionQueue` so the AVCaptureSession recovers after a runtime error instead of staying stopped. |
| `cscore/src/main/native/objcpp/UsbCameraDelegate.mm` | Null-check on `CVImageBufferRef` from `CMSampleBufferGetImageBuffer`; a stalled session can deliver sample buffers with no image data. |

## Automated unit tests (run on any platform)

The tests live in `cscore/src/test/native/cpp/LogSafetyTest.cpp`.

### Build

```bash
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
[ RUN      ] LogSafetyTest.CorrectUvcErrorFormat
[       OK ] LogSafetyTest.CorrectUvcErrorFormat
[ RUN      ] LogSafetyTest.BrokenUvcErrorFormatDoesNotCrash
[       OK ] LogSafetyTest.BrokenUvcErrorFormatDoesNotCrash
[ RUN      ] LogSafetyTest.UnknownFormatSpecifierDoesNotCrash
[       OK ] LogSafetyTest.UnknownFormatSpecifierDoesNotCrash
[ RUN      ] LogSafetyTest.MismatchedArgsDoesNotCrash
[       OK ] LogSafetyTest.MismatchedArgsDoesNotCrash
[==========] 4 tests from 1 test case ran in 0.XXX ms.
[  PASSED  ] 4 tests.
```

### How to reproduce the crash from the tests

To verify the crash existed before the fix, revert only the two
`try`/`catch` additions:

* `cscore/src/main/native/cpp/Log.cpp` — remove the `try`/`catch` around
  `fmt::vformat_to`, leaving the bare call.
* `wpiutil/src/main/native/cpp/Logger.cpp` — same.

Then rebuild and run `LogSafetyTest.BrokenUvcErrorFormatDoesNotCrash`.  The
test process will terminate with:

```
libc++abi: terminating due to uncaught exception of type
fmt::v11::format_error: unknown format specifier
```

That is the exact crash the user reported.  Restore the `try`/`catch` and
the test passes.

## Manual smoke test on macOS (requires a USB camera)

The `sessionRuntimeError:` recovery path and the UVC pipe-stall flow can
only be exercised with a real USB camera.

1. **Build** on a Mac:
   ```bash
   cd build
   cmake .. -DWITH_TESTS=ON
   cmake --build . --target cscore_usbviewer
   ```

2. **Plug in** a USB camera and start the viewer:
   ```bash
   ./cscore/examples/usbviewer/cscore_usbviewer
   ```

3. **Trigger a pipe stall** — briefly unplug and re-plug the USB cable.
   You should see the reconnection log (`Connected to USB camera on …`)
   and the feed resume.  No crash.
