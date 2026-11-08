#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

#include "ezsnmp_data_logger/exceptions.h"
#include "ezsnmp_data_logger/snmp_poller.h"
#include "ezsnmp_data_logger/version.h"
#include "test_common.h"

#if defined(EZSNMP_DL_HAVE_EZSNMP) && EZSNMP_DL_HAVE_EZSNMP
#include "datatypes.h"
#include "exceptionsbase.h"
#endif

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace ezsnmp_data_logger;

#ifndef _WIN32
namespace {

// Minimal ASN.1 BER loopback SNMPv2c UDP responder for end-to-end testing of
// EzSnmpPoller -> ezsnmp::SessionBase -> Net-SNMP -> UDP -> Result -> Sample!
class LoopbackSnmpV2cAgent {
  public:
   LoopbackSnmpV2cAgent() {
      fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
      if (fd_ < 0) {
         return;
      }
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = 0;
      ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
      if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
         ::close(fd_);
         fd_ = -1;
         return;
      }
      sockaddr_in actual{};
      socklen_t len = sizeof(actual);
      if (::getsockname(fd_, reinterpret_cast<sockaddr*>(&actual), &len) == 0) {
         port_ = ntohs(actual.sin_port);
      }
      running_.store(true);
      thread_ = std::thread(&LoopbackSnmpV2cAgent::loop, this);
   }

   ~LoopbackSnmpV2cAgent() {
      running_.store(false);
      if (thread_.joinable()) {
         thread_.join();
      }
      if (fd_ >= 0) {
         ::close(fd_);
      }
   }

   int port() const { return port_; }

  private:
   static bool read_tlv(uint8_t const* buf,
                        size_t len,
                        size_t& pos,
                        uint8_t& tag,
                        size_t& val_pos,
                        size_t& val_len) {
      if (pos + 2 > len) {
         return false;
      }
      tag = buf[pos++];
      uint8_t l0 = buf[pos++];
      if ((l0 & 0x80) == 0) {
         val_len = l0;
      } else {
         size_t num_octets = l0 & 0x7F;
         if (num_octets == 0 || num_octets > 4 || pos + num_octets > len) {
            return false;
         }
         val_len = 0;
         for (size_t i = 0; i < num_octets; ++i) {
            val_len = (val_len << 8) | buf[pos++];
         }
      }
      if (pos + val_len > len) {
         return false;
      }
      val_pos = pos;
      pos += val_len;
      return true;
   }

   static std::vector<uint8_t> wrap_tlv(uint8_t tag, std::vector<uint8_t> const& body) {
      std::vector<uint8_t> out;
      out.push_back(tag);
      if (body.size() < 128) {
         out.push_back(static_cast<uint8_t>(body.size()));
      } else {
         out.push_back(0x82);
         out.push_back(static_cast<uint8_t>((body.size() >> 8) & 0xFF));
         out.push_back(static_cast<uint8_t>(body.size() & 0xFF));
      }
      out.insert(out.end(), body.begin(), body.end());
      return out;
   }

   void loop() {
      std::string sys_name = "ezsnmp-mock-dut";
      while (running_.load()) {
         pollfd pfd{};
         pfd.fd = fd_;
         pfd.events = POLLIN;
         if (::poll(&pfd, 1, 50) <= 0 || !(pfd.revents & POLLIN)) {
            continue;
         }

         uint8_t pkt[1500];
         sockaddr_in peer{};
         socklen_t peer_len = sizeof(peer);
         ssize_t n =
             ::recvfrom(fd_, pkt, sizeof(pkt), 0, reinterpret_cast<sockaddr*>(&peer), &peer_len);
         if (n <= 0) {
            continue;
         }

         size_t pos = 0;
         uint8_t msg_tag = 0;
         size_t msg_vp = 0, msg_vl = 0;
         if (!read_tlv(pkt, static_cast<size_t>(n), pos, msg_tag, msg_vp, msg_vl) ||
             msg_tag != 0x30) {
            continue;
         }

         size_t mpos = msg_vp;
         uint8_t ver_tag = 0, comm_tag = 0, pdu_tag = 0;
         size_t ver_vp = 0, ver_vl = 0, comm_vp = 0, comm_vl = 0, pdu_vp = 0, pdu_vl = 0;
         if (!read_tlv(pkt, msg_vp + msg_vl, mpos, ver_tag, ver_vp, ver_vl) ||
             !read_tlv(pkt, msg_vp + msg_vl, mpos, comm_tag, comm_vp, comm_vl) ||
             !read_tlv(pkt, msg_vp + msg_vl, mpos, pdu_tag, pdu_vp, pdu_vl)) {
            continue;
         }

         size_t ppos = pdu_vp;
         uint8_t reqid_tag = 0, err_tag = 0, idx_tag = 0, vbl_tag = 0;
         size_t reqid_vp = 0, reqid_vl = 0, err_vp = 0, err_vl = 0, idx_vp = 0, idx_vl = 0,
                vbl_vp = 0, vbl_vl = 0;
         if (!read_tlv(pkt, pdu_vp + pdu_vl, ppos, reqid_tag, reqid_vp, reqid_vl) ||
             !read_tlv(pkt, pdu_vp + pdu_vl, ppos, err_tag, err_vp, err_vl) ||
             !read_tlv(pkt, pdu_vp + pdu_vl, ppos, idx_tag, idx_vp, idx_vl) ||
             !read_tlv(pkt, pdu_vp + pdu_vl, ppos, vbl_tag, vbl_vp, vbl_vl)) {
            continue;
         }

         std::vector<uint8_t> resp_varbinds;
         size_t vb_pos = vbl_vp;
         while (vb_pos < vbl_vp + vbl_vl) {
            uint8_t vb_tag = 0;
            size_t vb_vp = 0, vb_vl = 0;
            if (!read_tlv(pkt, vbl_vp + vbl_vl, vb_pos, vb_tag, vb_vp, vb_vl)) {
               break;
            }
            size_t ipos = vb_vp;
            uint8_t oid_tag = 0, val_tag = 0;
            size_t oid_vp = 0, oid_vl = 0, val_vp = 0, val_vl = 0;
            if (!read_tlv(pkt, vb_vp + vb_vl, ipos, oid_tag, oid_vp, oid_vl) ||
                !read_tlv(pkt, vb_vp + vb_vl, ipos, val_tag, val_vp, val_vl)) {
               break;
            }

            std::vector<uint8_t> oid_bytes(pkt + oid_vp, pkt + oid_vp + oid_vl);
            std::vector<uint8_t> val_tlv;
            if (pdu_tag == 0xA3) {
               // SetRequest: echo set value and update sys_name if OCTET STRING
               if (val_tag == 0x04) {
                  sys_name.assign(reinterpret_cast<char const*>(pkt + val_vp), val_vl);
               }
               val_tlv =
                   wrap_tlv(val_tag, std::vector<uint8_t>(pkt + val_vp, pkt + val_vp + val_vl));
            } else if (!oid_bytes.empty() && oid_bytes.size() >= 2 &&
                       oid_bytes[oid_bytes.size() - 2] == 0x03) {
               // sysUpTime.0 -> INTEGER 12345 (0x3039)
               val_tlv = wrap_tlv(0x02, {0x30, 0x39});
            } else {
               // OCTET STRING sys_name
               val_tlv = wrap_tlv(0x04, std::vector<uint8_t>(sys_name.begin(), sys_name.end()));
            }

            std::vector<uint8_t> oid_tlv = wrap_tlv(0x06, oid_bytes);
            std::vector<uint8_t> single_vb = oid_tlv;
            single_vb.insert(single_vb.end(), val_tlv.begin(), val_tlv.end());
            auto wrapped_vb = wrap_tlv(0x30, single_vb);
            resp_varbinds.insert(resp_varbinds.end(), wrapped_vb.begin(), wrapped_vb.end());
         }

         std::vector<uint8_t> pdu_body;
         auto reqid_tlv =
             wrap_tlv(reqid_tag, std::vector<uint8_t>(pkt + reqid_vp, pkt + reqid_vp + reqid_vl));
         auto err0_tlv = wrap_tlv(0x02, {0x00});
         auto idx0_tlv = wrap_tlv(0x02, {0x00});
         auto vbl_tlv = wrap_tlv(0x30, resp_varbinds);
         pdu_body.insert(pdu_body.end(), reqid_tlv.begin(), reqid_tlv.end());
         pdu_body.insert(pdu_body.end(), err0_tlv.begin(), err0_tlv.end());
         pdu_body.insert(pdu_body.end(), idx0_tlv.begin(), idx0_tlv.end());
         pdu_body.insert(pdu_body.end(), vbl_tlv.begin(), vbl_tlv.end());

         auto resp_pdu = wrap_tlv(0xA2, pdu_body);
         auto ver_tlv =
             wrap_tlv(ver_tag, std::vector<uint8_t>(pkt + ver_vp, pkt + ver_vp + ver_vl));
         auto comm_tlv =
             wrap_tlv(comm_tag, std::vector<uint8_t>(pkt + comm_vp, pkt + comm_vp + comm_vl));

         std::vector<uint8_t> msg_body;
         msg_body.insert(msg_body.end(), ver_tlv.begin(), ver_tlv.end());
         msg_body.insert(msg_body.end(), comm_tlv.begin(), comm_tlv.end());
         msg_body.insert(msg_body.end(), resp_pdu.begin(), resp_pdu.end());
         auto full_msg = wrap_tlv(0x30, msg_body);

         ::sendto(fd_, full_msg.data(), full_msg.size(), 0, reinterpret_cast<sockaddr*>(&peer),
                  peer_len);
      }
   }

   int fd_ = -1;
   int port_ = 0;
   std::atomic<bool> running_{false};
   std::thread thread_;
};

} // namespace
#endif

TEST(StubPollerTest, ProducesDeterministicSamplesAndSupportsRequestAndSet) {
   DutConfig dut;
   dut.name = "stub-dut";
   dut.dut_ip = "127.0.0.1";
   dut.snmp.mode = "stub";
   dut.metrics = {
       MetricConfig{"sysUpTime", "1.3.6.1.2.1.1.3.0", "get", 10, "s", ""},
       MetricConfig{"sysName", "1.3.6.1.2.1.1.5.0", "get", 10, "s", ""},
       MetricConfig{"cpuLoad", "1.3.6.1.4.1.9999.1.0", "get", 10, "s", ""},
   };

   auto poller = create_snmp_poller(dut);
   Batch b = poller->poll();
   ASSERT_EQ(b.samples.size(), 3u);
   EXPECT_EQ(b.samples[0].type_tag, TypeTag::Int64);
   EXPECT_EQ(b.samples[1].type_tag, TypeTag::Str);
   EXPECT_EQ(b.samples[2].type_tag, TypeTag::Float64);

   Batch walk_b = poller->request("walk", {"1.3.6.1.2.1.2.2.1.2"}, 10);
   EXPECT_EQ(walk_b.samples.size(), 4u);

   Batch set_b = poller->set_oid("1.3.6.1.2.1.1.5.0", "s", "updated-host");
   ASSERT_EQ(set_b.samples.size(), 1u);
   EXPECT_EQ(set_b.samples[0].str_val, "updated-host");
}

TEST(EzSnmpPollerTest, ExecutesLiveGetAndSetThroughEzSnmpSessionBase) {
   if (!has_ezsnmp_support()) {
      EZSNMP_SKIP_TEST_AND_RETURN("ezsnmp SessionBase support not compiled into binary");
   }
#ifdef _WIN32
   EZSNMP_SKIP_TEST_AND_RETURN("POSIX loopback UDP agent helper skipped on Windows");
#else
   LoopbackSnmpV2cAgent agent;
   ASSERT_GT(agent.port(), 0);

   DutConfig dut;
   dut.name = "live-ezsnmp-dut";
   dut.dut_ip = "127.0.0.1";
   dut.snmp.mode = "ezsnmp";
   dut.snmp.version = "2c";
   dut.snmp.community = "public";
   dut.snmp.port_number = agent.port();
   dut.snmp.timeout_seconds = 1;
   dut.snmp.retries = 1;
   dut.snmp.print_oids_numerically = true;
   dut.metrics = {
       MetricConfig{"sysUpTime", "1.3.6.1.2.1.1.3.0", "get", 10, "s", ""},
       MetricConfig{"sysName", "1.3.6.1.2.1.1.5.0", "get", 10, "s", ""},
   };

   auto poller = create_snmp_poller(dut);

   // 1. Poll via ezsnmp::SessionBase::get
   Batch b1 = poller->poll();
   ASSERT_EQ(b1.samples.size(), 2u);
   EXPECT_EQ(b1.samples[0].status, 0);
   EXPECT_EQ(b1.samples[0].type_tag, TypeTag::Int64);
   EXPECT_EQ(b1.samples[0].i64, 12345);
   EXPECT_EQ(b1.samples[1].status, 0);
   EXPECT_NE(b1.samples[1].str_val.find("ezsnmp-mock-dut"), std::string::npos);

   // 2. SET via ezsnmp::SessionBase::set
   Batch set_res = poller->set_oid("1.3.6.1.2.1.1.5.0", "s", "carlkid-core-sw");
   ASSERT_EQ(set_res.samples.size(), 1u);
   EXPECT_EQ(set_res.samples[0].status, 0);

   // 3. Verify updated value via on-demand ezsnmp::SessionBase::get
   Batch req_res = poller->request("get", {"1.3.6.1.2.1.1.5.0"}, 10);
   ASSERT_EQ(req_res.samples.size(), 1u);
   EXPECT_NE(req_res.samples[0].str_val.find("carlkid-core-sw"), std::string::npos);

   // 4. Test unsupported operation throws SnmpError
   EXPECT_THROW({ (void)poller->request("unsupported_op", {"1.1"}, 10); }, SnmpError);

   // 5. Test SNMPv1 and SNMPv3 Dut constructors
   DutConfig dut_v1 = dut;
   dut_v1.snmp.version = "1";
   auto poller_v1 = create_snmp_poller(dut_v1);
   EXPECT_NE(poller_v1, nullptr);

   DutConfig dut_v3 = dut;
   dut_v3.snmp.version = "3";
   dut_v3.snmp.security_level = "authPriv";
   dut_v3.snmp.security_username = "snmpuser";
   dut_v3.snmp.auth_protocol = "SHA";
   dut_v3.snmp.auth_passphrase = "authpass123";
   dut_v3.snmp.privacy_protocol = "AES";
   dut_v3.snmp.privacy_passphrase = "privpass123";
   auto poller_v3 = create_snmp_poller(dut_v3);
   EXPECT_NE(poller_v3, nullptr);

   // 6. Test polling against an unreachable port (exercises map_ezsnmp_exception_status &
   // error_sample)
   DutConfig dut_timeout = dut;
   dut_timeout.dut_ip = "127.0.0.1";
   dut_timeout.snmp.port_number = 59998;
   dut_timeout.snmp.timeout_seconds = 1;
   dut_timeout.snmp.retries = 0;
   dut_timeout.metrics = {MetricConfig{"sysUpTime", "1.3.6.1.2.1.1.3.0", "get", 10, "s", ""},
                          MetricConfig{"sysNext", "1.3.6.1.2.1.1.1.0", "getnext", 10, "s", ""},
                          MetricConfig{"sysWalk", "1.3.6.1.2.1.1", "walk", 10, "s", ""},
                          MetricConfig{"sysBulkGet", "1.3.6.1.2.1.1.1.0", "bulkget", 10, "s", ""},
                          MetricConfig{"sysBulkWalk", "1.3.6.1.2.1.1", "bulkwalk", 10, "s", ""},
                          MetricConfig{"sysSet", "1.3.6.1.2.1.1.5.0", "set", 10, "s", "val"}};
   auto poller_timeout = create_snmp_poller(dut_timeout);
   Batch err_batch = poller_timeout->poll();
   ASSERT_EQ(err_batch.samples.size(), 6u);
   for (auto const& s : err_batch.samples) {
      EXPECT_NE(s.status, 0);
      EXPECT_EQ(s.snmp_type, "ERROR");
   }

   // 7. Test poller with empty metrics
   DutConfig dut_empty = dut;
   dut_empty.metrics.clear();
   auto poller_empty = create_snmp_poller(dut_empty);
   EXPECT_TRUE(poller_empty->poll().samples.empty());

   // 8. Test result_to_sample exhaustive conversions
   ::Result r;
   r.oid = "1.3.6.1.2.1.1.1";
   r.index = "0";
   r.type = "OCTETSTR";
   r.value = "my_val";

   r.type = "NOSUCHOBJECT";
   EXPECT_EQ(test_result_to_sample("m", r).status, 128);
   r.type = "NOSUCHINSTANCE";
   EXPECT_EQ(test_result_to_sample("m", r).status, 129);
   r.type = "ENDOFMIBVIEW";
   EXPECT_EQ(test_result_to_sample("m", r).status, 130);

   r.type = "INTEGER";
   r.converted_value = static_cast<uint32_t>(42000);
   Sample s_u32 = test_result_to_sample("m", r);
   EXPECT_EQ(s_u32.type_tag, TypeTag::Int64);
   EXPECT_EQ(s_u32.i64, 42000);

   r.type = "COUNTER64";
   r.converted_value = static_cast<uint64_t>(9999999999ULL);
   Sample s_u64 = test_result_to_sample("m", r);
   EXPECT_EQ(s_u64.type_tag, TypeTag::Int64);
   EXPECT_EQ(s_u64.i64, 9999999999LL);

   r.type = "FLOAT";
   r.converted_value = static_cast<double>(3.14159);
   Sample s_dbl = test_result_to_sample("m", r);
   EXPECT_EQ(s_dbl.type_tag, TypeTag::Float64);
   EXPECT_NEAR(s_dbl.f64, 3.14159, 1e-4);

   r.type = "OCTETSTR";
   r.converted_value = std::string("42.75");
   Sample s_fstr = test_result_to_sample("m", r);
   EXPECT_EQ(s_fstr.type_tag, TypeTag::Float64);
   EXPECT_NEAR(s_fstr.f64, 42.75, 1e-4);

   r.converted_value = std::string("regular_str");
   Sample s_str = test_result_to_sample("m", r);
   EXPECT_EQ(s_str.type_tag, TypeTag::Str);
   EXPECT_EQ(s_str.str_val, "regular_str");

   r.converted_value = std::vector<unsigned char>{0x0A, 0x1B, 0xFF};
   Sample s_hex = test_result_to_sample("m", r);
   EXPECT_EQ(s_hex.type_tag, TypeTag::Str);
   EXPECT_EQ(s_hex.str_val, "0a:1b:ff");

   // 9. Test map_ezsnmp_exception_status exhaustive mappings
   EXPECT_EQ(test_map_ezsnmp_exception(::TimeoutErrorBase("timeout")), -1);
   EXPECT_EQ(test_map_ezsnmp_exception(::ConnectionErrorBase("conn")), -2);
   EXPECT_EQ(test_map_ezsnmp_exception(::UnknownObjectIDErrorBase("unk")), -3);
   EXPECT_EQ(test_map_ezsnmp_exception(::NoSuchObjectErrorBase("nso")), 128);
   EXPECT_EQ(test_map_ezsnmp_exception(::NoSuchInstanceErrorBase("nsi")), 129);
   EXPECT_EQ(test_map_ezsnmp_exception(::NoSuchNameErrorBase("nsn")), 2);
   EXPECT_EQ(test_map_ezsnmp_exception(::ParseErrorBase("parse")), -4);
   EXPECT_EQ(test_map_ezsnmp_exception(::PacketErrorBase("packet")), -5);
   EXPECT_EQ(test_map_ezsnmp_exception(::GenericErrorBase("generic")), -6);
#endif
}
