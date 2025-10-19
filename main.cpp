#include "HamonCube.hpp"
#include "HamonNode.hpp"
#include "Hamon.hpp"
#include "Make.hpp"
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <thread>
#include <cmath>
#include <stdexcept>

using namespace Dualys;

int largest_power_of_two(const unsigned int n) {
    if (n == 0)
        return 0;
    return static_cast<int>(pow(2, floor(log2(n))));
}

std::vector<NodeConfig> generate_configs(const int node_count) {
    std::vector<NodeConfig> configs;
    configs.reserve(static_cast<std::size_t>(node_count));
    for (std::size_t i = 0; i < static_cast<std::size_t>(node_count); ++i) {
        NodeConfig cfg;
        cfg.id = static_cast<int>(i);
        cfg.role = i == 0 ? "coordinator" : "worker";
        cfg.ip_address = "127.0.0.1";
        cfg.port = 8000 + static_cast<int>(i);
        configs.push_back(cfg);
    }
    return configs;
}



void run_node_process(const int node_id, const int node_count, const std::vector<NodeConfig> &configs) {
    const HamonCube cube(node_count);
    HamonNode node(cube.getNode(static_cast<std::size_t>(node_id)), cube, configs);
    node.run();
}

// --- Orchestrateur ---

int main(const int argc, char **argv) {
    std::cout << "Orchestrator starting..." << std::endl;

    int node_count = 0;
    std::vector<NodeConfig> configs;

    // If an .hc file path is provided as the first argument, run its @phase tasks and exit.
    if (argc > 1) {
        const std::string hc_path = argv[1];
        std::cout << "Running @phase tasks from '" << hc_path << "'..." << std::endl;
        const bool ok = Make::build_from_hc(hc_path);
        return ok ? 0 : 1;
    } else {
        // 1. Detect hardware and generate default config
        const unsigned int hardware_cores = std::thread::hardware_concurrency();
        node_count = largest_power_of_two(hardware_cores > 0 ? hardware_cores : 1);

        if (node_count == 0) {
            std::cerr << "Not enough hardware cores detected to run." << std::endl;
            return 1;
        }
        std::cout << "Detected " << hardware_cores << " cores. Using " << node_count << " nodes." << std::endl;
        configs = generate_configs(node_count);
    }

    // 2. Launch child processes
    std::vector<pid_t> childPids;
    childPids.reserve(static_cast<std::size_t>(node_count));
    for (std::size_t i = 0; i < static_cast<std::size_t>(node_count); ++i) {
        const pid_t pid = fork();
        if (pid == 0) {
            // Child process
            run_node_process(static_cast<int>(i), node_count, configs);
            _exit(0);
        }
        if (pid > 0) {
            childPids.push_back(pid);
        } else {
            std::cerr << "Failed to fork process for Node " << i << std::endl;
        }
    }

    // 3. Wait for all processes to finish
    std::cout << "All " << childPids.size() << " nodes launched. Waiting for them to finish." << std::endl;
    for (const pid_t pid: childPids) {
        waitpid(pid, nullptr, 0);
    }

    std::cout << "All nodes have finished. Orchestrator shutting down." << std::endl;
    return 0;
}
