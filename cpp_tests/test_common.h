#pragma once

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

#ifdef GTEST_SKIP
#define EZSNMP_SKIP_TEST_AND_RETURN(message) \
   do {                                      \
      GTEST_SKIP() << (message);             \
   } while (0)
#else
#define EZSNMP_SKIP_TEST_AND_RETURN(message)             \
   do {                                                  \
      std::cout << "[  SKIPPED ] " << (message) << "\n"; \
      return;                                            \
   } while (0)
#endif

namespace ezsnmp_data_logger::testing_support {

class ScopedTempDir {
  public:
   explicit ScopedTempDir(std::string const& prefix = "ezsnmp_dl_test_") {
      static std::atomic<uint64_t> counter{0};
      auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count();
      path_ = std::filesystem::temp_directory_path() /
              (prefix + std::to_string(ns) + "_" + std::to_string(++counter));
      std::filesystem::create_directories(path_);
   }

   ~ScopedTempDir() {
      std::error_code ec;
      std::filesystem::remove_all(path_, ec);
   }

   std::filesystem::path const& path() const { return path_; }
   std::string str() const { return path_.string(); }

  private:
   std::filesystem::path path_;
};

} // namespace ezsnmp_data_logger::testing_support
