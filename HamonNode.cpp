#include "HamonNode.h"

using namespace Dualys;
using namespace std::chrono_literals;

std::string serialize_map(const WordCountMap &map) {
    std::stringstream ss;
    for (const auto &[fst, snd]: map) {
        ss << fst << ":" << snd << ",";
    }
    return ss.str();
}

void deserialize_and_merge_map(const std::string &str, WordCountMap &target_map) {
    std::stringstream ss(str);
    std::string segment;
    while (std::getline(ss, segment, ',')) {
        if (segment.empty()) continue;
        size_t colon_pos = segment.find(':');
        if (colon_pos != std::string::npos) {
            std::string word = segment.substr(0, colon_pos);
            const int count = std::stoi(segment.substr(colon_pos + 1));
            target_map[word] += count;
        }
    }
}

// --- Implémentation de la classe HamonNode ---
void HamonNode::print_final_results() const {
    if (topology_node.id == 0) {
        std::cout << "------------------------------------------" << std::endl;
        std::cout << "[Node 0] FINAL RESULT: Word Counts" << std::endl;
        for (const auto &[fst, snd]: local_counts) {
            std::cout << " - '" << fst << "': " << snd << std::endl;
        }
        std::cout << "------------------------------------------" << std::endl;
    }
}

bool HamonNode::close_server_socket() const {
    return close(server_fd) == 0;
}

void HamonNode::send_string(const int sock, const std::string &str) {
    const uint32_t len = htonl(str.length());
    send(sock, &len, sizeof(len), 0);
    send(sock, str.c_str(), str.length(), 0);
}

// Le constructeur prend maintenant la configuration en plus
HamonNode::HamonNode(const Node &topology_node, const HamonCube &cube, const std::vector<NodeConfig> &configs)
    : topology_node(topology_node), cube(cube), server_fd(-1), all_configs(configs) {
}

bool HamonNode::run() {
    if (!setup_server()) return false;

    // Petite pause pour s'assurer que tous les serveurs sont prêts
    std::this_thread::sleep_for(100ms);

    if (!distribute_and_map()) return false;
    if (!reduce()) return false;

    if (topology_node.id == 0) {
        print_final_results();
    }

    return close_server_socket();
}

// La tâche de comptage de mots
WordCountMap HamonNode::perform_word_count_task(const std::string &text_chunk) const {
    std::cout << "[Node " << topology_node.id << "] Starting Word Count task..." << std::endl;
    WordCountMap counts;
    std::stringstream ss(text_chunk);
    std::string word;
    while (ss >> word) {
        counts[word]++;
    }
    std::cout << "[Node " << topology_node.id << "] Word Count task finished." << std::endl;
    return counts;
}

bool HamonNode::setup_server() {
    const NodeConfig &self_config = all_configs[topology_node.id];
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(self_config.port);

    constexpr int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        perror("[Node Error] bind failed");
        return false;
    }
    if (listen(server_fd, 16) < 0) {
        perror("[Node Error] listen failed");
        return false;
    }
    std::cout << "[Node " << topology_node.id << "] Server is listening on port " << self_config.port << std::endl;
    return true;
}

bool HamonNode::distribute_and_map() {
    if (topology_node.id == 0) {
        // Logique du Coordinateur
        std::cout << "[Node 0] Reading input file and distributing tasks..." << std::endl;
        std::ifstream file("input.txt");
        if (!file.is_open()) {
            std::cerr << "[Node 0] CRITICAL ERROR: Could not open input.txt" << std::endl;
            return false;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        size_t chunk_size = content.length() / cube.getNodeCount();

        // Distribuer à chaque worker sa part du texte
        for (int i = 1; i < cube.getNodeCount(); ++i) {
            const NodeConfig &worker_config = all_configs[i];
            int worker_sock = socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in serv_addr{};
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(worker_config.port);
            inet_pton(AF_INET, worker_config.ip_address.c_str(), &serv_addr.sin_addr);

            if (connect(worker_sock, reinterpret_cast<sockaddr *>(&serv_addr), sizeof(serv_addr)) == 0) {
                size_t start = i * chunk_size;
                size_t size = i == cube.getNodeCount() - 1 ? std::string::npos : chunk_size;
                send_string(worker_sock, content.substr(start, size));
                close(worker_sock);
            } else {
                std::cerr << "[Node 0] Failed to connect to worker " << i << " to distribute task." << std::endl;
            }
        }
        // Le coordinateur fait aussi sa part du travail
        local_counts = perform_word_count_task(content.substr(0, chunk_size));
    } else {
        // Logique des Workers
        std::cout << "[Node " << topology_node.id << "] Waiting for task from coordinator..." << std::endl;
        sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        int client_socket = accept(server_fd, reinterpret_cast<sockaddr *>(&client_addr), &addrlen);
        if (client_socket < 0) {
            perror("[Node Error] accept failed");
            return false;
        }
        std::string received_chunk = receive_string(client_socket);
        local_counts = perform_word_count_task(received_chunk);
        close(client_socket);
    }
    return true;
}

std::string HamonNode::receive_string(const int client_socket) {
    uint32_t len = 0;
    read(client_socket, &len, sizeof(len));
    len = ntohl(len);
    if (len > 0 && len < 65536) {
        // Petite sécurité
        std::vector<char> buffer(len);
        read(client_socket, buffer.data(), len);
        return std::string(buffer.begin(), buffer.end());
    }
    return "";
}

std::string HamonNode::serialize_map(const WordCountMap &map) {
    std::stringstream ss;
    for (const auto &[fst, snd]: map) {
        ss << fst << ":" << snd << ",";
    }
    return ss.str();
}

bool HamonNode::reduce() {
    std::cout << "[Node " << topology_node.id << "] Starting reduce phase..." << std::endl;

    for (int d = 0; d < cube.getDimension(); ++d) {
        const auto partner_id = topology_node.id ^ 1 << d;
        const NodeConfig &partner_config = all_configs[partner_id];

        if (topology_node.id > partner_id) {
            const int client_sock = socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in serv_addr{};
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(partner_config.port);
            inet_pton(AF_INET, partner_config.ip_address.c_str(), &serv_addr.sin_addr);

            bool connected = false;
            for (int attempt = 0; attempt < 5; ++attempt) {
                if (connect(client_sock, reinterpret_cast<sockaddr *>(&serv_addr), sizeof(serv_addr)) == 0) {
                    connected = true;
                    break;
                }
                std::this_thread::sleep_for(50ms);
            }

            if (connected) {
                send_string(client_sock, serialize_map(local_counts));
            } else {
                std::cerr << "[Node " << topology_node.id << "] Reduce phase: could not connect to partner " <<
                        partner_id << std::endl;
            }
            close(client_sock);
            break; // Mon rôle dans la réduction est terminé.
        }
        // Je reçois la map de mon partenaire et je la fusionne
        sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        int client_socket = accept(server_fd, reinterpret_cast<sockaddr *>(&client_addr), &addrlen);
        if (client_socket >= 0) {
            std::string received_str_map = receive_string(client_socket);
            deserialize_and_merge_map(received_str_map, local_counts);
            close(client_socket);
        } else {
            perror("[Node Error] accept failed during reduce");
            return false;
        }
    }
    return true;
}

