#include "ezsnmp_data_logger/prometheus_sink.h"

#include <cctype>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "ezsnmp_data_logger/exceptions.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ezsnmp_data_logger {

namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
void close_socket(SocketHandle s) {
   if (s != INVALID_SOCKET) {
      closesocket(s);
   }
}
struct WinsockInitializer {
   WinsockInitializer() {
      WSADATA wsa;
      WSAStartup(MAKEWORD(2, 2), &wsa);
   }
};
void ensure_socket_subsystem() {
   static WinsockInitializer init;
   (void)init;
}
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
void close_socket(SocketHandle s) {
   if (s >= 0) {
      ::close(s);
   }
}
void ensure_socket_subsystem() {}
#endif

std::string sanitize_prom_name(std::string const& raw) {
   std::string out;
   out.reserve(raw.size());
   for (size_t i = 0; i < raw.size(); ++i) {
      unsigned char ch = static_cast<unsigned char>(raw[i]);
      if (std::isalpha(ch) || ch == '_' || ch == ':') {
         out.push_back(static_cast<char>(ch));
      } else if (i > 0 && std::isdigit(ch)) {
         out.push_back(static_cast<char>(ch));
      } else {
         out.push_back('_');
      }
   }
   return out.empty() ? "metric" : out;
}

std::string escape_prom_label(std::string const& s) {
   std::string out;
   out.reserve(s.size());
   for (char c : s) {
      if (c == '\\') {
         out += "\\\\";
      } else if (c == '"') {
         out += "\\\"";
      } else if (c == '\n') {
         out += "\\n";
      } else {
         out.push_back(c);
      }
   }
   return out;
}

} // namespace

PrometheusSink::PrometheusSink(SinkConfig const& cfg)
    : host_(cfg.prometheus_host.empty() ? "127.0.0.1" : cfg.prometheus_host),
      configured_port_(cfg.prometheus_port),
      prefix_(sanitize_prom_name(cfg.prometheus_prefix.empty() ? "ezsnmp" : cfg.prometheus_prefix)),
      listen_fd_(static_cast<intptr_t>(kInvalidSocket)) {
   start_server();
}

PrometheusSink::~PrometheusSink() { close(); }

void PrometheusSink::start_server() {
   ensure_socket_subsystem();

   SocketHandle fd = ::socket(AF_INET, SOCK_STREAM, 0);
   if (fd == kInvalidSocket) {
      throw SinkError("PrometheusSink: failed to create TCP socket");
   }

   int opt = 1;
#ifdef _WIN32
   ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char const*>(&opt), sizeof(opt));
#else
   ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

   sockaddr_in addr{};
   addr.sin_family = AF_INET;
   addr.sin_port = htons(static_cast<uint16_t>(configured_port_));
   if (host_ == "0.0.0.0" || host_ == "*") {
      addr.sin_addr.s_addr = htonl(INADDR_ANY);
   } else if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
      close_socket(fd);
      throw SinkError("PrometheusSink: invalid IPv4 bind address '" + host_ + "'");
   }

   if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
      close_socket(fd);
      throw SinkError("PrometheusSink: failed to bind " + host_ + ":" +
                      std::to_string(configured_port_));
   }

   sockaddr_in actual{};
#ifdef _WIN32
   int actual_len = sizeof(actual);
#else
   socklen_t actual_len = sizeof(actual);
#endif
   if (::getsockname(fd, reinterpret_cast<sockaddr*>(&actual), &actual_len) == 0) {
      bound_port_ = ntohs(actual.sin_port);
   } else {
      bound_port_ = configured_port_;
   }

   if (::listen(fd, 16) != 0) {
      close_socket(fd);
      throw SinkError("PrometheusSink: listen() failed");
   }

   listen_fd_ = static_cast<intptr_t>(fd);
   running_.store(true);
   server_thread_ = std::thread(&PrometheusSink::serve_loop, this);
}

void PrometheusSink::serve_loop() {
   SocketHandle lfd = static_cast<SocketHandle>(listen_fd_);
   while (running_.load()) {
#ifdef _WIN32
      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(lfd, &rfds);
      timeval tv{};
      tv.tv_sec = 0;
      tv.tv_usec = 100000;
      int ready = ::select(0, &rfds, nullptr, nullptr, &tv);
      if (ready <= 0) {
         continue;
      }
#else
      pollfd pfd{};
      pfd.fd = lfd;
      pfd.events = POLLIN;
      int ready = ::poll(&pfd, 1, 100);
      if (ready <= 0 || !(pfd.revents & POLLIN)) {
         continue;
      }
#endif
      if (!running_.load()) {
         break;
      }

      sockaddr_in client_addr{};
#ifdef _WIN32
      int client_len = sizeof(client_addr);
#else
      socklen_t client_len = sizeof(client_addr);
#endif
      SocketHandle cfd = ::accept(lfd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
      if (cfd == kInvalidSocket) {
         continue;
      }

      char req_buf[1024]{};
#ifdef _WIN32
      int n = ::recv(cfd, req_buf, sizeof(req_buf) - 1, 0);
#else
      ssize_t n = ::recv(cfd, req_buf, sizeof(req_buf) - 1, 0);
#endif
      std::string req(n > 0 ? req_buf : "", n > 0 ? static_cast<size_t>(n) : 0);

      std::string status_line = "HTTP/1.1 200 OK\r\n";
      std::string content_type = "text/plain; version=0.0.4; charset=utf-8\r\n";
      std::string body;

      if (req.rfind("GET /healthz", 0) == 0) {
         content_type = "application/json; charset=utf-8\r\n";
         body = "{\"status\":\"ok\"}\n";
      } else if (req.rfind("GET /metrics", 0) == 0 || req.rfind("GET / ", 0) == 0) {
         body = render_metrics();
      } else {
         status_line = "HTTP/1.1 404 Not Found\r\n";
         body = "Not Found\n";
      }

      std::ostringstream resp;
      resp << status_line << "Content-Type: " << content_type << "Content-Length: " << body.size()
           << "\r\nConnection: close\r\n\r\n"
           << body;
      std::string payload = resp.str();

      size_t sent_total = 0;
      while (sent_total < payload.size()) {
#ifdef _WIN32
         int sent = ::send(cfd, payload.data() + sent_total,
                           static_cast<int>(payload.size() - sent_total), 0);
#else
         ssize_t sent = ::send(cfd, payload.data() + sent_total, payload.size() - sent_total, 0);
#endif
         if (sent <= 0) {
            break;
         }
         sent_total += static_cast<size_t>(sent);
      }

      close_socket(cfd);
   }
}

void PrometheusSink::write(Batch const& batch) {
   std::lock_guard<std::mutex> lock(metrics_mutex_);
   ++batches_exported_;
   for (auto const& s : batch.samples) {
      std::string key = batch.dut_name + "\x1f" + s.metric_name + "\x1f" + s.oid;
      MetricEntry entry;
      entry.dut_name = batch.dut_name;
      entry.dut_ip = batch.dut_ip;
      entry.metric_name = s.metric_name;
      entry.oid = s.oid;
      entry.status = s.status;
      entry.ts_unix_ns = batch.ts_unix_ns;
      if (s.type_tag == TypeTag::Int64) {
         entry.value = static_cast<double>(s.i64);
         latest_metrics_[key] = std::move(entry);
      } else if (s.type_tag == TypeTag::Float64) {
         entry.value = s.f64;
         latest_metrics_[key] = std::move(entry);
      }
   }
}

std::string PrometheusSink::render_metrics() const {
   std::lock_guard<std::mutex> lock(metrics_mutex_);
   std::ostringstream out;
   out << "# HELP " << prefix_ << "_batches_exported_total Total SNMP batches exported.\n";
   out << "# TYPE " << prefix_ << "_batches_exported_total counter\n";
   out << prefix_ << "_batches_exported_total " << batches_exported_ << "\n";

   std::string gauge_name = prefix_ + "_sample_value";
   std::string status_name = prefix_ + "_sample_status";
   out << "# HELP " << gauge_name << " Latest numeric SNMP metric value polled via ezsnmp.\n";
   out << "# TYPE " << gauge_name << " gauge\n";
   out << "# HELP " << status_name << " Latest SNMP sample status code (0 = OK).\n";
   out << "# TYPE " << status_name << " gauge\n";

   for (auto const& kv : latest_metrics_) {
      auto const& m = kv.second;
      std::string labels = "{dut_name=\"" + escape_prom_label(m.dut_name) + "\",dut_ip=\"" +
                           escape_prom_label(m.dut_ip) + "\",metric_name=\"" +
                           escape_prom_label(m.metric_name) + "\",oid=\"" +
                           escape_prom_label(m.oid) + "\"}";
      out << gauge_name << labels << " " << std::setprecision(15) << m.value << "\n";
      out << status_name << labels << " " << m.status << "\n";
   }
   return out.str();
}

void PrometheusSink::flush() {}

void PrometheusSink::close() noexcept {
   try {
      bool was_running = running_.exchange(false);
      SocketHandle lfd = static_cast<SocketHandle>(listen_fd_);
      if (lfd != kInvalidSocket) {
         close_socket(lfd);
         listen_fd_ = static_cast<intptr_t>(kInvalidSocket);
      }
      if (was_running && server_thread_.joinable()) {
         server_thread_.join();
      }
   } catch (...) {
   }
}

} // namespace ezsnmp_data_logger
