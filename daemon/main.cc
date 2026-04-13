#include "config.h"
#include "daemon.h"
#include <iostream>
#include <fstream>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>

static std::atomic<bool> g_stop{false};
static ezsnmp_logger::Daemon* g_daemon = nullptr;

static void signal_handler(int) {
    g_stop = true;
    if (g_daemon) g_daemon->stop();
}

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog
              << " --config <file> | --config-string <json>\n"
              << "       " << prog << " --help\n";
}

int main(int argc, char* argv[]) {
    std::string json_text;
    bool got_config = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--config" && i + 1 < argc) {
            std::ifstream f(argv[++i]);
            if (!f) {
                std::cerr << "Cannot open config file: " << argv[i] << "\n";
                return 1;
            }
            json_text.assign(std::istreambuf_iterator<char>(f),
                             std::istreambuf_iterator<char>());
            got_config = true;
        } else if (arg == "--config-string" && i + 1 < argc) {
            json_text = argv[++i];
            got_config = true;
        }
    }

    if (!got_config) {
        print_usage(argv[0]);
        return 1;
    }

    ezsnmp_logger::Config cfg;
    try {
        cfg = ezsnmp_logger::parse_config(json_text);
    } catch (const std::exception& e) {
        std::cerr << "Config parse error: " << e.what() << "\n";
        return 1;
    }

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    try {
        ezsnmp_logger::Daemon daemon(cfg);
        g_daemon = &daemon;

        // Stats printer thread
        std::thread stats_thread([&daemon]{
            while (!g_stop) {
                for (int i = 0; i < 10 && !g_stop; ++i)
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                if (!g_stop)
                    std::cout << daemon.stats_json() << "\n" << std::flush;
            }
        });

        daemon.run();

        g_stop = true;
        stats_thread.join();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
