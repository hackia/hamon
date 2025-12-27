#pragma once
#include <libintl.h>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <regex>

#ifndef I18N_GETTEXT_DEFINED
#define _(String) gettext(String)
#define I18N_GETTEXT_DEFINED
#endif

namespace dualys {
    struct NodeCfg {
        int id = -1;
        std::string role; // "worker" | "coordinator" | "custom:..."
        int numa = -1; // -1 = auto
        int core = -1; // -1 = auto
        std::string host; // e.g., 127.0.0.1
        int port = -1; // e.g., 8000
        std::vector<int> neighbors; // logical neighbors (ids)
    };

    struct Phase {
        std::string name;
        std::string task; // La commande à exécuter
        std::string description; // Description optionnelle à afficher dans la progress bar
        std::vector<int> target_nodes; // IDs des nœuds concernés
    };

    struct Job {
        std::string name;
        std::string input; // valeur brute pour l'instant
        std::vector<Phase> phases; // phases ordonnées
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

        std::string expand_vars(const std::string &in) const; // remplace ${VAR}
        bool eval_require_expr(const std::string &raw) const; // évalue @require

        // Jobs access
        [[nodiscard]] const std::vector<Job>& get_jobs() const { return jobs; }

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

        static bool is_truthy(const std::string &v);

        static bool str_to_int(const std::string &s, long long &out);

        // Erreur contextualisée
        [[noreturn]] void bad(const std::string &msg) const;

        // Helpers for jobs
        std::vector<int> parse_target_selector(const std::string &selector) const; // parses [*], [workers], [0,2]

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
        std::unordered_map<std::string, std::string> vars; // @let
        std::vector<std::filesystem::path> file_stack; // pile des fichiers
        std::unordered_set<std::string> include_guard; // chemins absolus visités
        int include_depth = 0;
        const int include_depth_max = 32;

        // Job parsing context
        std::vector<Job> jobs;
        int currentJobIndex = -1; // -1 = outside any job
    };
} // namespace Dualys
