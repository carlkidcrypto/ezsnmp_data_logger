#pragma once

#ifndef SWIG

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ezsnmp_data_logger/config.h"
#include "ezsnmp_data_logger/datatypes.h"

class SessionBase;

namespace ezsnmp_data_logger {

/**
 * @brief Abstract interface for executing SNMP requests against a single Device Under Test.
 */
class ISnmpPoller {
  public:
   virtual ~ISnmpPoller() = default;

   /**
    * @brief Execute one polling cycle across all configured metrics of the DUT.
    * @return Batch containing timestamp, DUT identity, and collected Sample entries.
    */
   virtual Batch poll() = 0;

   /**
    * @brief Perform an on-demand SNMP request (`get`, `getnext`, `walk`, `bulkget`, `bulkwalk`).
    * @param operation SNMP operation name.
    * @param oids Target OID strings.
    * @param max_repeaters Max repeaters for bulk operations.
    * @return Batch containing the returned ezsnmp Result samples.
    */
   virtual Batch request(std::string const& operation,
                         std::vector<std::string> const& oids,
                         int max_repeaters = 10) = 0;

   /**
    * @brief Perform an SNMP SET operation (`oid`, `type_char`, `value`).
    * @param oid Target OID string.
    * @param set_type Net-SNMP / ezsnmp type character (e.g. "s", "i", "u", "t", "a", "o", "x").
    * @param set_value Value string to write.
    * @return Batch containing the SET response Sample(s).
    */
   virtual Batch set_oid(std::string const& oid,
                         std::string const& set_type,
                         std::string const& set_value) = 0;
};

/**
 * @brief Deterministic/synthetic SNMP poller for testing, demos, and offline validation.
 */
class StubSnmpPoller : public ISnmpPoller {
  public:
   explicit StubSnmpPoller(DutConfig dut);
   ~StubSnmpPoller() override = default;

   Batch poll() override;
   Batch request(std::string const& operation,
                 std::vector<std::string> const& oids,
                 int max_repeaters = 10) override;
   Batch set_oid(std::string const& oid,
                 std::string const& set_type,
                 std::string const& set_value) override;

  private:
   DutConfig dut_;
   uint64_t tick_ = 0;
};

/**
 * @brief Poller backed directly by ezsnmp's C++17 `SessionBase` and `Result` layer.
 *
 * Delegates all Net-SNMP session management, SNMPv1/v2c/v3 USM authentication & privacy,
 * thread-safe MIB formatting, and GET/GETNEXT/WALK/BULKGET/BULKWALK/SET operations to
 * the sibling `ezsnmp` library (`SessionBase`), converting returned `Result` objects into
 * `ezsnmp_data_logger::Sample` records for storage in the data_logger layer.
 */
class EzSnmpPoller : public ISnmpPoller {
  public:
   explicit EzSnmpPoller(DutConfig dut);
   ~EzSnmpPoller() override;

   Batch poll() override;
   Batch request(std::string const& operation,
                 std::vector<std::string> const& oids,
                 int max_repeaters = 10) override;
   Batch set_oid(std::string const& oid,
                 std::string const& set_type,
                 std::string const& set_value) override;

  private:
   DutConfig dut_;
#if defined(EZSNMP_DL_HAVE_EZSNMP) && EZSNMP_DL_HAVE_EZSNMP
   std::unique_ptr<::SessionBase> session_;
#endif
};

/**
 * @brief Factory creating the appropriate ISnmpPoller for a DutConfig ("ezsnmp" or "stub").
 */
std::unique_ptr<ISnmpPoller> create_snmp_poller(DutConfig const& dut);

} // namespace ezsnmp_data_logger

#endif // SWIG
