#pragma once

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
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
    public:
        HamonParser();

        // Parsing
        void parse_file(const std::string &path);

        void parse_line(const std::string &line);

        // Finalisation (remplit les manques, construit la topologie par défaut, etc.)
        void finalize();

        // Accès
        [[nodiscard]] int use_nodes() const { return nodes; }

        [[nodiscard]] int dim() const;

        [[nodiscard]] const std::string &get_topology() const;

        // Récupérer une vue aplatie des NodeCfg (après finalize)
        [[nodiscard]] std::vector<NodeCfg> materialize_nodes() const;

        // Affichage « dry-run »
        void print_plan(std::ostream &os = std::cout) const;

    private:
        // Helpers (purs C++17, sans dépendances)
        static std::string trim(const std::string &x);

        static bool starts_with(const std::string &s, const std::string &p);

        static std::vector<std::string> split_ws(const std::string &line);

        static std::vector<int> parse_list_ids(const std::string &src);

        static void parse_host_port(const std::string &s, std::string &host, int &port);

        static bool is_power_of_two(unsigned x);

        static int log2i(unsigned x);

        // Gestion des nœuds
        NodeCfg &ensure_node(int id);

        // Erreur contextualisée
        [[noreturn]] void bad(const std::string &msg) const;

        // État courant de parsing
        int nodes = -1; // @use
        int dimensions = -1; // @dim (auto si @use est puissance de 2)
        std::string topology = "hypercube"; // @topology
        std::string hostname; // @autoprefix host:port OU @auto host:port
        int autoPortBase = -1; // idem
        std::vector<std::optional<NodeCfg> > config;

        // Contexte parsing
        int currentNodeId = -1;
        int currentLine = 0;
    };
} // namespace Dualys
