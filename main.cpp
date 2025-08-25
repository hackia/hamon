#include "HamonCube.h"
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <chrono>

using namespace Dualys;
using namespace std::chrono_literals;
constexpr int BASE_PORT = 8000;
constexpr int kListenBacklog = 3;
inline constexpr auto kServerStartupDelay = 100ms;
constexpr int kNodeCount = 8;

inline void wait_for_server_startup() noexcept
{
    std::this_thread::sleep_for(kServerStartupDelay);
}

// Fonction qui sera exécutée par chaque nœud (processus enfant)
void run_node_logic(const Node& node) {
    std::cout << "[Node " << node.id << "] Process " << getpid() << " is alive. Listening on port " << BASE_PORT + node.id << std::endl;

    // --- 1. Création du socket d'écoute (partie "serveur") ---
    const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("[Node Error] socket failed");
        exit(EXIT_FAILURE);
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(BASE_PORT + node.id);

    // Attacher le socket au port
    if (bind(server_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        perror("[Node Error] bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, kListenBacklog) < 0) {
        perror("[Node Error] listen failed");
        exit(EXIT_FAILURE);
    }
    std::cout << "[Node " << node.id << "] Is now listening for connections..." << std::endl;

    wait_for_server_startup();

    for (const int neighbor_id : node.neighbors) {
        if (neighbor_id > node.id) {
            std::cout << "[Node " << node.id << "] Attempting to connect to neighbor " << neighbor_id << "..." << std::endl;
            const int client_sock = socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in serv_addr{};
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(BASE_PORT + neighbor_id);
            inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);


            if (connect(client_sock, reinterpret_cast<sockaddr *>(&serv_addr), sizeof(serv_addr)) < 0) {
                std::cerr << "[Node Error] Connection to neighbor " << neighbor_id << " failed." << std::endl;
            } else {
                std::cout << "[Node " << node.id << "] Successfully connected to neighbor " << neighbor_id << "!" << std::endl;
                std::string msg = "Hello from Node " + std::to_string(node.id);
                send(client_sock, msg.c_str(), msg.length(), 0);
                close(client_sock);
            }
        }
    }

    std::cout << "[Node " << node.id << "] Waiting to accept connections..." << std::endl;
    for (const int neighbor_id: node.neighbors) {
        if (neighbor_id < node.id) {
            int new_socket;
            int addrlen = sizeof(address);
            if ((new_socket = accept(server_fd, reinterpret_cast<sockaddr *>(&address),
                                     reinterpret_cast<socklen_t *>(&addrlen))) < 0) {
                perror("accept");
            } else {
                char buffer[1024] = {};
                read(new_socket, buffer, 1024);
                std::cout << "[Node " << node.id << "] Received message: '" << buffer << "' from neighbor " <<
                        neighbor_id << std::endl;
                close(new_socket);
            }
        }
    }
    close(server_fd);
    std::cout << "[Node " << node.id << "] Finished." << std::endl;
}

static std::vector<pid_t> launch_nodes(const HamonCube &cube) {
    std::vector<pid_t> childPids;
    childPids.reserve(kNodeCount);

    for (int i = 0; i < kNodeCount; ++i) {
        const pid_t pid = fork();
        if (pid == 0) {
            const Node &self = cube.getNode(i);
            run_node_logic(self);
            _exit(0);
        }
        if (pid > 0) {
            childPids.push_back(pid);
        } else {
            std::cerr << "Failed to fork process for Node " << i << std::endl;
            break;
        }
    }
    return childPids;
}

static void wait_for_children(const std::vector<pid_t> &pids) {
    for (const pid_t pid: pids) {
        waitpid(pid, nullptr, 0);
    }
}

int main() {
    std::cout << "Orchestrator starting... (PID: " << getpid() << ")" << std::endl;

    const HamonCube cube;
    const auto childPids = launch_nodes(cube);

    if (childPids.size() == static_cast<size_t>(kNodeCount)) {
        std::cout << "All nodes launched. Orchestrator is waiting for them to finish." << std::endl;
    } else {
        std::cerr << "Some nodes failed to launch (" << childPids.size() << "/" << kNodeCount << ")." << std::endl;
    }
    wait_for_children(childPids);
    std::cout << "All nodes have finished. Orchestrator shutting down." << std::endl;
    return 0;
}
