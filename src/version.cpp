#include "ezsnmp_data_logger/version.h"

#include "ezsnmp_data_logger/json_utils.h"

namespace ezsnmp_data_logger {

std::string version() { return "2026.11.1a0"; }

std::string ezsnmp_target_version() { return "2026.10.28a0"; }

bool has_ezsnmp_support() {
#if defined(EZSNMP_DL_HAVE_EZSNMP) && EZSNMP_DL_HAVE_EZSNMP
   return true;
#else
   return false;
#endif
}

bool has_netsnmp_support() { return has_ezsnmp_support(); }

bool has_sqlite3_support() {
#if defined(EZSNMP_DL_HAVE_SQLITE3) && EZSNMP_DL_HAVE_SQLITE3
   return true;
#else
   return false;
#endif
}

bool has_embedded_sqlite3() {
#if defined(EZSNMP_DL_EMBEDDED_SQLITE3) && EZSNMP_DL_EMBEDDED_SQLITE3
   return true;
#else
   return false;
#endif
}

bool has_psql_support() {
#if defined(EZSNMP_DL_HAVE_LIBPQ) && EZSNMP_DL_HAVE_LIBPQ
   return true;
#else
   return false;
#endif
}

std::vector<std::string> supported_swig_languages() {
   return {
       "python",
       "javascript",
       "csharp",
       "go",
   };
}

std::string build_info_json(int indent) {
   json::Value root = json::Value::object();
   root["package"] = "ezsnmp_data_logger";
   root["version"] = version();
   root["versioning_scheme"] = "CalSemVer";
   root["ezsnmp_target_version"] = ezsnmp_target_version();
   root["cpp_standard"] = "C++17";
   root["snmp_engine"] = "ezsnmp (SessionBase)";
   root["self_contained"] = true;

   json::Value caps = json::Value::object();
   caps["ezsnmp"] = has_ezsnmp_support();
   caps["stub_snmp"] = true;
   caps["csv_sink"] = true;
   caps["jsonl_sink"] = true;
   caps["sqlite3_sink"] = has_sqlite3_support();
   caps["embedded_sqlite3"] = has_embedded_sqlite3();
   caps["psql_sink"] = has_psql_support();
   caps["prometheus_sink"] = true;
   root["capabilities"] = std::move(caps);

   json::Value langs = json::Value::array();
   for (auto const& lang : supported_swig_languages()) {
      langs.push_back(lang);
   }
   root["swig_target_languages"] = std::move(langs);
   return root.dump(indent);
}

} // namespace ezsnmp_data_logger
