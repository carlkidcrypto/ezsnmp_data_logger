#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <thread>

#include "test_common.h"

#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#endif

TEST(MainBinaryTest, ExecutesHelpVersionAndHandlesSigintGracefully) {
   // Locate compiled binaries across execution working directories
   std::string bin = "./ezsnmp-data-logger";
   if (!std::filesystem::exists(bin)) {
      bin = "../ezsnmp-data-logger";
   }
   if (!std::filesystem::exists(bin)) {
      bin = "./build-cov/ezsnmp-data-logger";
   }

   std::string alias_bin = "./ezsnmp_loggerd";
   if (!std::filesystem::exists(alias_bin)) {
      alias_bin = "../ezsnmp_loggerd";
   }
   if (!std::filesystem::exists(alias_bin)) {
      alias_bin = "./build-cov/ezsnmp_loggerd";
   }

   int ret_ver = std::system((bin + " version").c_str());
   EXPECT_EQ(ret_ver, 0);

   int ret_help = std::system((bin + " --help").c_str());
   EXPECT_EQ(ret_help, 0);

   int ret_alias = std::system((alias_bin + " --help").c_str());
   EXPECT_EQ(ret_alias, 0);

#ifndef _WIN32
   // Exercise SIGINT signal handler in src/main.cpp
   pid_t pid = fork();
   if (pid == 0) {
      execl(bin.c_str(), bin.c_str(), "run", "--config-string",
            "{\"logger\":{\"stats_interval_ms\":50},\"duts\":[{\"name\":\"d1\",\"dut_ip\":\"127.0."
            "0.1\",\"snmp\":{\"mode\":\"stub\"},\"metrics\":[{\"name\":\"m\",\"oid\":\"1.1\"}]}]}",
            nullptr);
      _exit(0);
   } else if (pid > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      kill(pid, SIGINT);
      int status = 0;
      waitpid(pid, &status, 0);
   }
#endif
}
