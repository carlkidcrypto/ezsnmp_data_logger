#pragma once

#include <string>
#include <vector>

namespace ezsnmp_data_logger {

/**
 * @brief Structured result returned when executing a CLI command programmatically.
 */
struct CliResult {
   int exit_code = 0;
   std::string stdout_text;
   std::string stderr_text;

   /**
    * @brief Serialize this CliResult to a JSON string.
    * @param indent Indentation spaces (-1 for compact).
    * @return JSON string.
    */
   std::string to_json(int indent = -1) const;
};

/**
 * @brief Command-line interface dispatcher for ezsnmp_data_logger.
 *
 * Powers both the native `ezsnmp-data-logger` C++17 CLI executable and
 * programmatic CLI execution from any SWIG target language. The GUI interacts
 * exclusively with this CLI interface and JSON configuration files.
 */
class CliRunner {
  public:
   /**
    * @brief Execute CLI arguments, writing directly to process stdout/stderr.
    * @param args Command-line arguments (excluding argv[0] program name).
    * @return Process exit code (0 on success, non-zero on error).
    */
   static int run(std::vector<std::string> const& args);

   /**
    * @brief Execute CLI arguments and capture stdout/stderr in a CliResult struct.
    * @param args Command-line arguments (excluding argv[0] program name).
    * @return Captured CliResult containing exit_code, stdout_text, and stderr_text.
    */
   static CliResult execute(std::vector<std::string> const& args);

   /**
    * @brief Request any active foreground `CliRunner::run` session to stop gracefully.
    */
   static void request_stop();
};

} // namespace ezsnmp_data_logger
