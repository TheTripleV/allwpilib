// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

//
// Regression tests for the UVC pipe-stall crash.
//
// The crash was caused by the format string in UvcControlImpl.mm
// sendControlRequest: default case:
//   "{:02Xh}:{:03Xh}:{:04Xh}"
// The trailing 'h' on each specifier is a printf length modifier; it does not
// exist in fmt.  fmt parses 'h' as the format type, does not recognise it, and
// throws fmt::format_error("unknown format specifier").  Because the UVCERROR
// macro does not wrap with FMT_STRING the bad spec was never caught at compile
// time.
//
// Two fixes were applied:
//   1. The specifiers were corrected to {:02X} / {:03X} / {:04X}.
//   2. cs::NamedLogV (and wpi::Logger::LogV) now catch fmt::format_error so
//      that any future typo of this kind surfaces in the log instead of
//      terminating the process.
//

#include <cstdint>

#include <gtest/gtest.h>

#include <fmt/format.h>
#include <wpi/Logger.h>

#include "Log.h"

namespace cs {

class LogSafetyTest : public ::testing::Test {
 protected:
  std::string captured;
  wpi::Logger logger;

  LogSafetyTest() {
    logger.SetLogger([this](unsigned int /*level*/, const char* /*file*/,
                            unsigned int /*line*/, const char* msg) {
      captured = msg;
    });
  }
};

// The corrected format string must produce well-formed hex output.
TEST_F(LogSafetyTest, CorrectUvcErrorFormat) {
  uint32_t sys = 0x3, sub = 0x1ff, code = 0xabc;
  cs::NamedLogV(logger, wpi::WPI_LOG_ERROR, __FILE__, __LINE__, "UvcControl",
                "ControlRequest failed (KR=sys:sub:code) = {:02X}:{:03X}:{:04X}",
                fmt::make_format_args(sys, sub, code));
  EXPECT_EQ(captured,
            "UvcControl: ControlRequest failed (KR=sys:sub:code) = 03:1FF:0ABC");
}

// The original (broken) format string with the trailing 'h' must not crash.
// NamedLogV catches the format_error and substitutes an error marker.
TEST_F(LogSafetyTest, BrokenUvcErrorFormatDoesNotCrash) {
  uint32_t sys = 0x3, sub = 0x1ff, code = 0xabc;
  // Reproduce the exact pre-fix format string verbatim.
  cs::NamedLogV(logger, wpi::WPI_LOG_ERROR, __FILE__, __LINE__, "UvcControl",
                "ControlRequest failed (KR=sys:sub:code) = {:02Xh}:{:03Xh}:{:04Xh}",
                fmt::make_format_args(sys, sub, code));

  EXPECT_NE(captured.find("UvcControl"), std::string::npos);
  EXPECT_NE(captured.find("fmt error"), std::string::npos);
}

// Generic safety net: any other unknown specifier is also caught.
TEST_F(LogSafetyTest, UnknownFormatSpecifierDoesNotCrash) {
  std::string arg = "value";
  cs::NamedLogV(logger, wpi::WPI_LOG_ERROR, __FILE__, __LINE__, "TestSource",
                "{:Q}", fmt::make_format_args(arg));

  EXPECT_NE(captured.find("TestSource"), std::string::npos);
  EXPECT_NE(captured.find("fmt error"), std::string::npos);
}

// Argument-count mismatch is also caught.
TEST_F(LogSafetyTest, MismatchedArgsDoesNotCrash) {
  std::string arg = "only-one";
  cs::NamedLogV(logger, wpi::WPI_LOG_ERROR, __FILE__, __LINE__, "TestSource",
                "{} {} {}", fmt::make_format_args(arg));

  EXPECT_NE(captured.find("TestSource"), std::string::npos);
  EXPECT_NE(captured.find("fmt error"), std::string::npos);
}

}  // namespace cs
