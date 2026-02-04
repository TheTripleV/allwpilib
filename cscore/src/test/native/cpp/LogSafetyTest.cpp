// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

//
// Tests that a malformed format string passed to cs::NamedLogV does not
// terminate the process.  Before the fix this would throw an uncaught
// fmt::format_error and call std::terminate.
//
// The AVCaptureSession pipe-stall / recovery path can only be exercised on a
// real macOS machine with a USB camera attached.  See task.md for manual-test
// instructions.
//

#include <gtest/gtest.h>

#include <fmt/format.h>
#include <wpi/Logger.h>

#include "Log.h"

namespace cs {

class LogSafetyTest : public ::testing::Test {
 protected:
  // Capture whatever the logger callback receives so we can assert on it.
  std::string captured;
  wpi::Logger logger;

  LogSafetyTest() {
    logger.SetLogger([this](unsigned int /*level*/, const char* /*file*/,
                            unsigned int /*line*/, const char* msg) {
      captured = msg;
    });
  }
};

// A format string with a bare, valid placeholder should work normally.
TEST_F(LogSafetyTest, ValidFormat) {
  cs::NamedLogV(logger, wpi::WPI_LOG_ERROR, __FILE__, __LINE__, "TestSource",
                "{}", fmt::make_format_args(std::string_view{"hello"}));
  EXPECT_EQ(captured, "TestSource: hello");
}

// A format string with an unknown specifier (e.g. "{:Q}") would previously
// throw fmt::format_error and crash.  After the fix the exception is caught
// and the output contains the fmt error message rather than crashing.
TEST_F(LogSafetyTest, UnknownFormatSpecifierDoesNotCrash) {
  // "{:Q}" is not a valid fmt format spec — fmt will throw format_error.
  // We must use vformat_to / make_format_args so that the bad spec is not
  // checked at compile time by FMT_STRING.
  std::string arg = "value";
  cs::NamedLogV(logger, wpi::WPI_LOG_ERROR, __FILE__, __LINE__, "TestSource",
                "{:Q}", fmt::make_format_args(arg));

  // The output should contain the source name and the fmt error text,
  // NOT have thrown.
  EXPECT_NE(captured.find("TestSource"), std::string::npos);
  EXPECT_NE(captured.find("fmt error"), std::string::npos);
}

// A format string with mismatched argument count (more placeholders than
// args) is another class of runtime format_error.  Same crash path.
TEST_F(LogSafetyTest, MismatchedArgsDoesNotCrash) {
  std::string arg = "only-one";
  cs::NamedLogV(logger, wpi::WPI_LOG_ERROR, __FILE__, __LINE__, "TestSource",
                "{} {} {}", fmt::make_format_args(arg));

  EXPECT_NE(captured.find("TestSource"), std::string::npos);
  EXPECT_NE(captured.find("fmt error"), std::string::npos);
}

// An error message whose text itself contains curly braces (as macOS
// NSError descriptions can, e.g. "UserInfo={...}") must not be re-interpreted
// as a format string.  NamedLogV receives it as a pre-formatted argument, so
// this should pass through unchanged.
TEST_F(LogSafetyTest, MessageWithBracesPassesThroughSafely) {
  // Simulate what happens when NSError.description contains braces:
  // the error text arrives as a std::string argument to "... error: {}".
  std::string errorDesc =
      "The operation couldn't be completed. UserInfo={NSLocalizedDescription="
      "Pipe has stalled, error needs to be cleared}";
  cs::NamedLogV(logger, wpi::WPI_LOG_ERROR, __FILE__, __LINE__, "UsbCamera",
                "Capture session runtime error: {}",
                fmt::make_format_args(errorDesc));

  EXPECT_EQ(captured,
            "UsbCamera: Capture session runtime error: " + errorDesc);
}

}  // namespace cs
