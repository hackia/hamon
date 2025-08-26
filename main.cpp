#include "HamonCube.h"
#include "HamonNode.h" // On inclut notre nouvelle classe
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <thread>
#include <cmath>

using namespace Dualys;

// --- Fonctions de Configuration Dynamique (elles reviennent dans le main) ---

int largest_power_of_two(const unsigned int n) {
    if (n == 0) return 0;
    return static_cast<int>(pow(2, floor(log2(n))));
}

std::vector<NodeConfig> generate_configs(const int node_count) {
    std::vector<NodeConfig> configs;
    configs.reserve(node_count);
    for (int i = 0; i < node_count; ++i) {
        NodeConfig cfg;
        cfg.id = i;
        cfg.role = i == 0 ? "coordinator" : "worker";
        cfg.ip_address = "127.0.0.1";
        cfg.port = 8000 + i;
        configs.push_back(cfg);
    }
    return configs;
}

void run_node_process(const int node_id,const int node_count, const std::vector<NodeConfig>& configs) {
    const HamonCube cube(node_count);
    HamonNode node(cube.getNode(node_id), cube, configs);
    node.run();
}

// --- Orchestrateur ---

int main() {
    std::cout << "Orchestrator starting..." << std::endl;

    // 1. L'orchestrateur DÉTECTE le matériel et GÉNÈRE la config
    const unsigned int hardware_cores = std::thread::hardware_concurrency();
    const int node_count = largest_power_of_two(hardware_cores > 0 ? hardware_cores : 1);

    if (node_count == 0) {
        std::cerr << "Not enough hardware cores detected to run." << std::endl;
        return 1;
    }

    std::cout << "Detected " << hardware_cores << " cores. Using " << node_count << " nodes." << std::endl;
    const auto configs = generate_configs(node_count);

    // 2. L'orchestrateur LANCE les processus enfants
    std::vector<pid_t> childPids;
    childPids.reserve(node_count);
    for (int i = 0; i < node_count; ++i) {
        const pid_t pid = fork();
        if (pid == 0) { // Processus enfant
            // Chaque enfant reçoit la config complète
            run_node_process(i, node_count, configs);
            _exit(0);
        }
        if (pid > 0) {
            childPids.push_back(pid);
        } else {
            std::cerr << "Failed to fork process for Node " << i << std::endl;
        }
    }

    // 3. L'orchestrateur ATTEND que tout le monde ait fini
    std::cout << "All " << childPids.size() << " nodes launched. Waiting for them to finish." << std::endl;
    for (const pid_t pid: childPids) {
        waitpid(pid, nullptr, 0);
    }

    std::cout << "All nodes have finished. Orchestrator shutting down." << std::endl;
    return 0;
}