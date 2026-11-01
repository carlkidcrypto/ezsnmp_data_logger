#include "ezsnmp_data_logger/version.h"

#include "ezsnmp_data_logger/json_utils.h"

namespace ezsnmp_data_logger {

std::string version() { return "0.2.0"; }

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

bool has_psql_support() {
#if defined(EZSNMP_DL_HAVE_LIBPQ) && EZSNMP_DL_HAVE_LIBPQ
   return true;
#else
   return false;
#endif
}

std::vector<std::string> supported_swig_languages() {
   return {
       "python", "javascript", "java", "csharp", "go", "ruby",  "perl5",  "php7",
       "lua",    "r",          "tcl8", "octave", "d",  "guile", "scilab", "xml",
   };
}

std::string build_info_json(int indent) {
   json::Value root = json::Value::object();
   root["package"] = "ezsnmp_data_logger";
   root["version"] = version();
   root["cpp_standard"] = "C++17";
   root["snmp_engine"] = "ezsnmp (SessionBase)";

   json::Value caps = json::Value::object();
   caps["ezsnmp"] = has_ezsnmp_support();
   caps["stub_snmp"] = true;
   caps["csv_sink"] = true;
   caps["jsonl_sink"] = true;
   caps["sqlite3_sink"] = has_sqlite3_support();
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
