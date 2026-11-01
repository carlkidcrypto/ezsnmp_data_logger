#include <algorithm>
#include <filesystem>
#include <fstream>

#include "ezsnmp_data_logger/csv_sink.h"
#include "ezsnmp_data_logger/exceptions.h"
#include "ezsnmp_data_logger/json_utils.h"
#include "ezsnmp_data_logger/jsonl_sink.h"
#include "ezsnmp_data_logger/prometheus_sink.h"
#include "ezsnmp_data_logger/psql_sink.h"
#include "ezsnmp_data_logger/sink.h"
#include "ezsnmp_data_logger/sqlite_sink.h"

#if !defined(EZSNMP_DL_HAVE_SQLITE3)
#define EZSNMP_DL_HAVE_SQLITE3 1
#endif

#if EZSNMP_DL_HAVE_SQLITE3
#include <sqlite3.h>
#endif

namespace ezsnmp_data_logger {

std::unique_ptr<ISink> create_sink(SinkConfig const& cfg) {
   if (cfg.type == "csv") {
      return std::make_unique<CsvSink>(cfg);
   }
   if (cfg.type == "jsonl") {
      return std::make_unique<JsonlSink>(cfg);
   }
   if (cfg.type == "sqlite3") {
      return std::make_unique<SqliteSink>(cfg);
   }
   if (cfg.type == "psql") {
      return std::make_unique<PsqlSink>(cfg);
   }
   if (cfg.type == "prometheus") {
      return std::make_unique<PrometheusSink>(cfg);
   }
   throw SinkError("Unknown sink type: '" + cfg.type + "'");
}

namespace {

std::vector<std::string> parse_csv_line(std::string const& line) {
   std::vector<std::string> fields;
   std::string cur;
   bool in_quotes = false;
   for (size_t i = 0; i < line.size(); ++i) {
      char c = line[i];
      if (in_quotes) {
         if (c == '"') {
            if (i + 1 < line.size() && line[i + 1] == '"') {
               cur.push_back('"');
               ++i;
            } else {
               in_quotes = false;
            }
         } else {
            cur.push_back(c);
         }
      } else {
         if (c == '"') {
            in_quotes = true;
         } else if (c == ',') {
            fields.push_back(std::move(cur));
            cur.clear();
         } else if (c != '\r') {
            cur.push_back(c);
         }
      }
   }
   fields.push_back(std::move(cur));
   return fields;
}

#if EZSNMP_DL_HAVE_SQLITE3
std::vector<InspectRow> inspect_sqlite_sink(SinkConfig const& scfg,
                                            int limit,
                                            std::string const& dut_filter,
                                            std::string const& metric_filter) {
   std::vector<InspectRow> rows;
   if (scfg.sqlite_path == ":memory:" || !std::filesystem::exists(scfg.sqlite_path)) {
      return rows;
   }

   sqlite3* db = nullptr;
   if (sqlite3_open_v2(scfg.sqlite_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
      if (db) {
         sqlite3_close(db);
      }
      return rows;
   }
   sqlite3_busy_timeout(db, 3000);

   std::string sql =
       "SELECT s.ts_unix_ns, sr.dut_name, sr.dut_ip, sr.metric_name, sr.oid, "
       "s.oid_index, s.snmp_type, s.raw_value, s.type_tag, s.i64, s.f64, s.str_val, s.status "
       "FROM samples s JOIN series sr ON s.series_id = sr.series_id WHERE 1=1";
   if (!dut_filter.empty()) {
      sql += " AND sr.dut_name = ?1";
   }
   if (!metric_filter.empty()) {
      sql += " AND sr.metric_name = ?2";
   }
   sql += " ORDER BY s.ts_unix_ns DESC LIMIT ?3;";

   sqlite3_stmt* stmt = nullptr;
   if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
      if (!dut_filter.empty()) {
         sqlite3_bind_text(stmt, 1, dut_filter.c_str(), -1, SQLITE_TRANSIENT);
      }
      if (!metric_filter.empty()) {
         sqlite3_bind_text(stmt, 2, metric_filter.c_str(), -1, SQLITE_TRANSIENT);
      }
      sqlite3_bind_int(stmt, 3, limit);

      while (sqlite3_step(stmt) == SQLITE_ROW) {
         InspectRow r;
         r.ts_unix_ns = sqlite3_column_int64(stmt, 0);
         unsigned char const* c_dut = sqlite3_column_text(stmt, 1);
         unsigned char const* c_ip = sqlite3_column_text(stmt, 2);
         unsigned char const* c_metric = sqlite3_column_text(stmt, 3);
         unsigned char const* c_oid = sqlite3_column_text(stmt, 4);
         unsigned char const* c_idx = sqlite3_column_text(stmt, 5);
         unsigned char const* c_stype = sqlite3_column_text(stmt, 6);
         unsigned char const* c_raw = sqlite3_column_text(stmt, 7);
         r.dut_name = c_dut ? reinterpret_cast<char const*>(c_dut) : "";
         r.dut_ip = c_ip ? reinterpret_cast<char const*>(c_ip) : "";
         r.metric_name = c_metric ? reinterpret_cast<char const*>(c_metric) : "";
         r.oid = c_oid ? reinterpret_cast<char const*>(c_oid) : "";
         r.oid_index = c_idx ? reinterpret_cast<char const*>(c_idx) : "";
         r.snmp_type = c_stype ? reinterpret_cast<char const*>(c_stype) : "";
         r.raw_value = c_raw ? reinterpret_cast<char const*>(c_raw) : "";
         int tcode = sqlite3_column_int(stmt, 8);
         r.type_tag = (tcode >= 0 && tcode <= 3) ? static_cast<TypeTag>(tcode) : TypeTag::Null;
         if (sqlite3_column_type(stmt, 9) != SQLITE_NULL) {
            r.i64 = sqlite3_column_int64(stmt, 9);
         }
         if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) {
            r.f64 = sqlite3_column_double(stmt, 10);
         }
         unsigned char const* c_str = sqlite3_column_text(stmt, 11);
         r.str_val = c_str ? reinterpret_cast<char const*>(c_str) : "";
         r.status = sqlite3_column_int(stmt, 12);
         r.sink_type = "sqlite3";
         rows.push_back(std::move(r));
      }
      sqlite3_finalize(stmt);
   }
   sqlite3_close(db);
   return rows;
}
#endif

std::vector<InspectRow> inspect_jsonl_sink(SinkConfig const& scfg,
                                           std::string const& dut_filter,
                                           std::string const& metric_filter) {
   std::vector<InspectRow> rows;
   if (!std::filesystem::exists(scfg.dir)) {
      return rows;
   }
   for (auto const& entry : std::filesystem::directory_iterator(scfg.dir)) {
      if (!entry.is_regular_file() || entry.path().extension() != ".jsonl") {
         continue;
      }
      std::ifstream in(entry.path());
      std::string line;
      while (std::getline(in, line)) {
         if (line.empty()) {
            continue;
         }
         try {
            json::Value j = json::Value::parse(line);
            if (!j.is_object()) {
               continue;
            }
            InspectRow r;
            r.ts_unix_ns = j.value_int64("ts_unix_ns", 0);
            r.dut_name = j.value_string("dut_name", "");
            r.dut_ip = j.value_string("dut_ip", "");
            r.metric_name = j.value_string("metric_name", "");
            if (!dut_filter.empty() && r.dut_name != dut_filter) {
               continue;
            }
            if (!metric_filter.empty() && r.metric_name != metric_filter) {
               continue;
            }
            r.oid = j.value_string("oid", "");
            r.oid_index = j.value_string("oid_index", "");
            r.snmp_type = j.value_string("snmp_type", "");
            r.raw_value = j.value_string("raw_value", "");
            int tcode = j.value_int("type_tag", 0);
            r.type_tag = (tcode >= 0 && tcode <= 3) ? static_cast<TypeTag>(tcode) : TypeTag::Null;
            r.i64 = j.value_int64("i64", 0);
            r.f64 = j.value_double("f64", 0.0);
            r.str_val = j.value_string("str_val", "");
            r.status = j.value_int("status", 0);
            r.sink_type = "jsonl";
            rows.push_back(std::move(r));
         } catch (...) {
         }
      }
   }
   return rows;
}

std::vector<InspectRow> inspect_csv_sink(SinkConfig const& scfg,
                                         std::string const& dut_filter,
                                         std::string const& metric_filter) {
   std::vector<InspectRow> rows;
   if (!std::filesystem::exists(scfg.dir)) {
      return rows;
   }
   for (auto const& entry : std::filesystem::directory_iterator(scfg.dir)) {
      if (!entry.is_regular_file() || entry.path().extension() != ".csv") {
         continue;
      }
      std::ifstream in(entry.path());
      std::string line;
      bool first = true;
      while (std::getline(in, line)) {
         if (first) {
            first = false;
            if (line.rfind("ts_unix_ns,", 0) == 0) {
               continue;
            }
         }
         if (line.empty()) {
            continue;
         }
         auto cols = parse_csv_line(line);
         try {
            InspectRow r;
            if (cols.size() >= 13) {
               r.ts_unix_ns = std::stoll(cols[0]);
               r.dut_name = cols[1];
               r.dut_ip = cols[2];
               r.metric_name = cols[3];
               r.oid = cols[4];
               r.oid_index = cols[5];
               r.snmp_type = cols[6];
               r.raw_value = cols[7];
               int tcode = std::stoi(cols[8]);
               r.type_tag =
                   (tcode >= 0 && tcode <= 3) ? static_cast<TypeTag>(tcode) : TypeTag::Null;
               r.i64 = cols[9].empty() ? 0 : std::stoll(cols[9]);
               r.f64 = cols[10].empty() ? 0.0 : std::stod(cols[10]);
               r.str_val = cols[11];
               r.status = std::stoi(cols[12]);
            } else if (cols.size() >= 10) {
               r.ts_unix_ns = std::stoll(cols[0]);
               r.dut_name = cols[1];
               r.dut_ip = cols[2];
               r.metric_name = cols[3];
               r.oid = cols[4];
               int tcode = std::stoi(cols[5]);
               r.type_tag =
                   (tcode >= 0 && tcode <= 3) ? static_cast<TypeTag>(tcode) : TypeTag::Null;
               r.i64 = cols[6].empty() ? 0 : std::stoll(cols[6]);
               r.f64 = cols[7].empty() ? 0.0 : std::stod(cols[7]);
               r.str_val = cols[8];
               r.status = std::stoi(cols[9]);
            } else {
               continue;
            }
            if (!dut_filter.empty() && r.dut_name != dut_filter) {
               continue;
            }
            if (!metric_filter.empty() && r.metric_name != metric_filter) {
               continue;
            }
            r.sink_type = "csv";
            rows.push_back(std::move(r));
         } catch (...) {
         }
      }
   }
   return rows;
}

void sort_and_truncate(std::vector<InspectRow>& rows, int limit) {
   std::sort(rows.begin(), rows.end(),
             [](InspectRow const& a, InspectRow const& b) { return a.ts_unix_ns > b.ts_unix_ns; });
   if (limit > 0 && static_cast<int>(rows.size()) > limit) {
      rows.resize(static_cast<size_t>(limit));
   }
}

} // namespace

std::vector<InspectRow> inspect_configured_sinks(LoggerConfig const& cfg,
                                                 int limit,
                                                 std::string const& dut_filter,
                                                 std::string const& metric_filter) {
   int effective_limit = limit > 0 ? limit : 50;

#if EZSNMP_DL_HAVE_SQLITE3
   std::vector<InspectRow> sqlite_rows;
   for (auto const& scfg : cfg.sinks) {
      if (scfg.enabled && scfg.type == "sqlite3") {
         auto part = inspect_sqlite_sink(scfg, effective_limit, dut_filter, metric_filter);
         sqlite_rows.insert(sqlite_rows.end(), part.begin(), part.end());
      }
   }
   if (!sqlite_rows.empty()) {
      sort_and_truncate(sqlite_rows, effective_limit);
      return sqlite_rows;
   }
#endif

   std::vector<InspectRow> jsonl_rows;
   for (auto const& scfg : cfg.sinks) {
      if (scfg.enabled && scfg.type == "jsonl") {
         auto part = inspect_jsonl_sink(scfg, dut_filter, metric_filter);
         jsonl_rows.insert(jsonl_rows.end(), part.begin(), part.end());
      }
   }
   if (!jsonl_rows.empty()) {
      sort_and_truncate(jsonl_rows, effective_limit);
      return jsonl_rows;
   }

   std::vector<InspectRow> csv_rows;
   for (auto const& scfg : cfg.sinks) {
      if (scfg.enabled && scfg.type == "csv") {
         auto part = inspect_csv_sink(scfg, dut_filter, metric_filter);
         csv_rows.insert(csv_rows.end(), part.begin(), part.end());
      }
   }
   sort_and_truncate(csv_rows, effective_limit);
   return csv_rows;
}

} // namespace ezsnmp_data_logger
