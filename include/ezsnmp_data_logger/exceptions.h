#pragma once

#include <stdexcept>
#include <string>

namespace ezsnmp_data_logger {

/**
 * @brief Base exception class for all ezsnmp_data_logger errors.
 *
 * All domain-specific exceptions in the C++17 core inherit from LoggerError so
 * SWIG bindings across every target language can catch errors uniformly.
 */
class LoggerError : public std::runtime_error {
  public:
   /**
    * @brief Construct a LoggerError with an explanatory message.
    * @param message Human-readable error message.
    */
   explicit LoggerError(std::string const& message) : std::runtime_error(message) {}
};

/**
 * @brief Exception thrown when JSON configuration parsing or validation fails.
 */
class ConfigError : public LoggerError {
  public:
   /**
    * @brief Construct a ConfigError with an explanatory message.
    * @param message Human-readable configuration error message.
    */
   explicit ConfigError(std::string const& message) : LoggerError(message) {}
};

/**
 * @brief Exception thrown when an SNMP session or polling operation fails fatally.
 */
class SnmpError : public LoggerError {
  public:
   /**
    * @brief Construct an SnmpError with an explanatory message.
    * @param message Human-readable SNMP error message.
    */
   explicit SnmpError(std::string const& message) : LoggerError(message) {}
};

/**
 * @brief Exception thrown when a storage sink initialization, write, or query fails.
 */
class SinkError : public LoggerError {
  public:
   /**
    * @brief Construct a SinkError with an explanatory message.
    * @param message Human-readable sink error message.
    */
   explicit SinkError(std::string const& message) : LoggerError(message) {}
};

/**
 * @brief Exception thrown when CLI command arguments or execution fail.
 */
class CliError : public LoggerError {
  public:
   /**
    * @brief Construct a CliError with an explanatory message.
    * @param message Human-readable CLI error message.
    */
   explicit CliError(std::string const& message) : LoggerError(message) {}
};

} // namespace ezsnmp_data_logger
