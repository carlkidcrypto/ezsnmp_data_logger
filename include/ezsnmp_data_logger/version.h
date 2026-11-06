#pragma once

#include <string>
#include <vector>

namespace ezsnmp_data_logger {

/**
 * @brief Return the CalSemVer version string of ezsnmp_data_logger.
 * @return CalSemVer version string (e.g. "2026.11.5.1").
 */
std::string version();

/**
 * @brief Return the standardized CalSemVer alpha baseline version of ezsnmp targeted by this build.
 * @return Target ezsnmp CalSemVer string (e.g. "2026.10.28a0").
 */
std::string ezsnmp_target_version();

/**
 * @brief Validate whether a version or git tag conforms to CalSemVer
 * (YYYY.M.D[aN|bN|rcN|-alpha.N]).
 * @param ver Version or tag string to validate.
 * @return true if ver is a valid non-zero-padded CalSemVer identifier, false otherwise.
 */
bool is_valid_calsemver(std::string const& ver);

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
 * @brief Check whether SQLite3 was statically embedded via the SQLite3 C amalgamation.
 * @return true if statically embedded with zero external libsqlite3 dependency, false otherwise.
 */
bool has_embedded_sqlite3();

/**
 * @brief Check whether PostgreSQL (libpq) sink support was compiled into the binary.
 * @return true if compiled with libpq support, false otherwise.
 */
bool has_psql_support();

/**
 * @brief Return the supported SWIG target languages ("python", "javascript", "csharp", "go").
 * @return Vector of supported SWIG target language identifiers.
 */
std::vector<std::string> supported_swig_languages();

/**
 * @brief Return a JSON object string summarizing version, CalSemVer baseline, and capabilities.
 * @param indent Pretty-print indentation (-1 for compact single-line JSON).
 * @return JSON string describing build capabilities.
 */
std::string build_info_json(int indent = -1);

} // namespace ezsnmp_data_logger
