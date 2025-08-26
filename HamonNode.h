#pragma once

#include "HamonCube.h"
#include <map>
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace Dualys {
    using WordCountMap = std::map<std::string, int>;


    class HamonNode {
    public:
        void initializeTopology();

        HamonNode(const Node &p_topology_node, const HamonCube &p_cube, const std::vector<NodeConfig> &p_configs);

        void print_final_results() const;

        [[nodiscard]] bool close_server_socket() const;

        static void send_string(int sock, const std::string &str);

        bool run();

        static std::string serialize_map(const WordCountMap &target_map);

        static void deserialize_and_merge_map(const std::string &x, WordCountMap &map);

    private:
        [[nodiscard]] WordCountMap perform_word_count_task(const std::string &text_chunk) const;

        bool setup_server();

        bool distribute_and_map();

        static std::string receive_string(int client_socket);

        [[nodiscard]] bool reduce();

        Node topology_node;
        HamonCube cube;
        int server_fd;
        WordCountMap local_counts;
        std::vector<NodeConfig> all_configs;
    };
}
