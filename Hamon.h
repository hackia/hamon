#pragma once

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Dualys {
    struct NodeCfg {
        int id = -1;
        std::string role; // "worker" | "coordinator" | "custom:..."
        int numa = -1; // -1 = auto
        int core = -1; // -1 = auto
        std::string host; // e.g., 127.0.0.1
        int port = -1; // e.g., 8000
        std::vector<int> neighbors; // logical neighbors (ids)
    };


    class HamonParser {
        HamonParser();

        static std::string trim(const std::string &x);

        static bool starts_with(const std::string &s, const std::string &p);

        static std::vector<std::string> split_ws(const std::string &line);

        static std::vector<int> parse_list_ids(const std::string &src);

        static void parse_host_port(const std::string &s, std::string &host, int &port);

        static bool is_power_of_two(unsigned x);

        static int log2i(unsigned x);

        NodeCfg &ensure_node(int id);

        static void parse_line(const std::string & line);

        static void parse_file(const std::string &path);

        void finalize();

        static void bad(const std::string &msg);

        int nodes = -1; // number of nodes to use (@use)
        int dimensions = -1; // hypercube dimensions (optional; auto if power-of-two useN)
        std::string topology = "hypercube"; // default topology
        std::string hostname = ""; // from @auto
        int autoPortBase = -1; // from @auto
        std::vector<std::optional<NodeCfg> > config;
    };
}
