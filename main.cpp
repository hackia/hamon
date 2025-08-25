#include "HamonCube.h"
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>

using namespace Dualys;

void run_node_logic(const Node &node) {
    std::cout << "Node " << node.id << " (PID: " << getpid() << ") is alive." << std::endl;
    std::cout << " -> My neighbors are: ";
    for (const int neighbor_id: node.neighbors) {
        std::cout << neighbor_id << " ";
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "Orchestrator starting... (PID: " << getpid() << ")" << std::endl;

    std::vector<pid_t> child_pids;
    const HamonCube cube;
    for (int i = 0; i < 8; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            const Node &self = cube.getNode(i);
            run_node_logic(self);
            return 0;
        }
        if (pid > 0) {
            child_pids.push_back(pid);
            std::cout << "Orchestrator launched Node " << i << " with PID: " << pid << std::endl;
        } else {
            std::cerr << "Failed to fork process for Node " << i << std::endl;
            return 1;
        }
    }

    std::cout << "All nodes launched. Orchestrator is waiting for them to finish." << std::endl;
    for (const pid_t child_pid: child_pids) {
        waitpid(child_pid, nullptr, 0);
    }
    std::cout << "All nodes have finished. Orchestrator shutting down." << std::endl;

    return 0;
}
