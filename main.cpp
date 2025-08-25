#include "HamonCube.h"
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
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
constexpr int kListenBacklog = 8;
inline constexpr auto kServerStartupDelay = 100ms;
constexpr int kNodeCount = 8;

inline void wait_for_server_startup() noexcept {
    std::this_thread::sleep_for(kServerStartupDelay);
}

void send_value(const int sock, const uint32_t value) {
    const uint32_t network_value = htonl(value);
    send(sock, &network_value, sizeof(network_value), 0);
}

unsigned int receive_value(const int sock) {
    int network_value = 0;
    read(sock, &network_value, sizeof(network_value));
    return ntohl(network_value);
}

void run_node_logic(const Node &node) {
    const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(BASE_PORT + node.id);
    if (bind(server_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0 || listen(
            server_fd, kListenBacklog) < 0) {
        perror("[Node Error] Server setup failed");
        exit(EXIT_FAILURE);
    }

    wait_for_server_startup();

    unsigned int local_sum = node.id;
    std::cout << "[Node " << node.id << "] Starting with value: " << local_sum << std::endl;

    for (int d = 0; d < 3; ++d) {
        const auto partner_id = node.id ^ 1 << d;

        if (node.id > partner_id) {
            const int client_sock = socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in serv_addr{};
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(BASE_PORT + partner_id);
            inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

            if (connect(client_sock, reinterpret_cast<sockaddr *>(&serv_addr), sizeof(serv_addr)) == 0) {
                std::cout << "[Node " << node.id << "] Round " << d << ": Sending value " << local_sum << " to partner "
                        << partner_id << std::endl;
                send_value(client_sock, local_sum);
                close(client_sock);
            } else {
                perror("[Node Error] Connection failed");
            }
            break;
        }
        int new_socket;
        int addrlen = sizeof(address);
        if ((new_socket = accept(server_fd, reinterpret_cast<sockaddr *>(&address),
                                 reinterpret_cast<socklen_t *>(&addrlen))) >= 0) {
            const unsigned int received_value = receive_value(new_socket);
            local_sum += received_value;
            std::cout << "[Node " << node.id << "] Round " << d << ": Received value " << received_value <<
                    " from partner " << partner_id << ". New sum: " << local_sum << std::endl;
            close(new_socket);
        } else {
            perror("accept");
        }
    }
    if (node.id == 0) {
        std::cout << "------------------------------------------" << std::endl;
        std::cout << "[Node 0] FINAL RESULT: The total sum is " << local_sum << std::endl;
        std::cout << "------------------------------------------" << std::endl;
    }
    close(server_fd);
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
