#pragma once

#include <string>
#include <vector>

namespace ezsnmp_data_logger {

/**
 * @brief Return the semantic version string of ezsnmp_data_logger.
 * @return Semantic version string (e.g. "0.1.0").
 */
std::string version();

/**
 * @brief Check whether ezsnmp SessionBase SNMP support was compiled into the binary.
 * @return true if compiled against ezsnmp C++ SessionBase + Net-SNMP, false otherwise.
 */
bool has_ezsnmp_support();

/**
 * @brief Backward-compatible alias for has_ezsnmp_support().
 * @return true if compiled against ezsnmp + Net-SNMP, false otherwise.
 */
bool has_netsnmp_support();

/**
 * @brief Check whether SQLite3 sink support was compiled into the binary.
 * @return true if compiled with SQLite3 support, false otherwise.
 */
bool has_sqlite3_support();

/**
 * @brief Check whether PostgreSQL (libpq) sink support was compiled into the binary.
 * @return true if compiled with libpq support, false otherwise.
 */
bool has_psql_support();

/**
 * @brief Return a list of SWIG target languages supported by the interface definitions.
 * @return Vector of SWIG target language identifiers.
 */
std::vector<std::string> supported_swig_languages();

/**
 * @brief Return a JSON object string summarizing version and compiled capabilities.
 * @param indent Pretty-print indentation (-1 for compact single-line JSON).
 * @return JSON string describing build capabilities.
 */
std::string build_info_json(int indent = -1);

} // namespace ezsnmp_data_logger
