#include <csignal>
#include <string>
#include <vector>

#include "ezsnmp_data_logger/cli_runner.h"

namespace {

void handle_signal(int /*sig*/) { ezsnmp_data_logger::CliRunner::request_stop(); }

} // namespace

int main(int argc, char* argv[]) {
   std::signal(SIGINT, handle_signal);
   std::signal(SIGTERM, handle_signal);

   std::vector<std::string> args;
   if (argc > 1) {
      args.reserve(static_cast<size_t>(argc - 1));
      for (int i = 1; i < argc; ++i) {
         args.emplace_back(argv[i]);
      }
   }
   return ezsnmp_data_logger::CliRunner::run(args);
}
