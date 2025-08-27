#include "Hamon.h"

using namespace Dualys;

// ------------ Helpers ------------
std::string HamonParser::trim(const std::string &x) {
    auto start = x.begin();
    while (start != x.end() && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }
    auto rend = x.rbegin();
    while (rend != x.rend() && std::isspace(static_cast<unsigned char>(*rend))) {
        ++rend;
    }
    return std::string(start, rend.base());
}

bool HamonParser::starts_with(const std::string &s, const std::string &p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

std::vector<std::string> HamonParser::split_ws(const std::string &line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

std::vector<int> HamonParser::parse_list_ids(const std::string &src) {
    std::vector<int> result;
    const auto t = trim(src);
    if (t.empty() || t.front() != '[' || t.back() != ']') {
        throw new std::runtime_error(std::string("Invalid list format: ") + src);
    }
    const auto content = t.substr(1, t.size() - 2);
    if (content.empty()) { return result; }
    std::stringstream ss(content);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        if (item.empty()) {
            continue;
        }
        try {
            result.push_back(std::stoi(item));
        } catch (std::runtime_error &) {
            throw new std::runtime_error(std::string("Invalid number in list: ") + item);
        }
    }
    return result;
}

void HamonParser::parse_host_port(const std::string &s, std::string &host, int &port) {
    const auto pos = s.find(':');
    if (pos == std::string::npos) {
        throw new std::runtime_error(std::string("Invalid host:port format: ") + s);
    }
    host = trim(s.substr(0, pos));
    try { port = std::stoi(trim(s.substr(pos + 1))); } catch (...) {
        throw new std::runtime_error(std::string("Invalid port number in: ") + s);
    }
}

bool HamonParser::is_power_of_two(const unsigned x) {
    return x && !(x & (x - 1));
}

int HamonParser::log2i(unsigned x) {
    int r = 0;
    while (x >>= 1) ++r;
    return r;
}

// ------------ Core ------------

HamonParser::HamonParser() = default;

NodeCfg &HamonParser::ensure_node(const int id) {
    if (id < 0 || (nodes >= 0 && id >= nodes)) {
        bad("Invalid node ID: " + std::to_string(id));
    }
    const auto idx = static_cast<std::size_t>(id);
    if (idx >= config.size()) config.resize(idx + 1);
    auto &slot = config[idx];
    if (!slot.has_value()) {
        NodeCfg n;
        n.id = id;
        n.role = "worker"; // par défaut (node 0 coord. en finalize)
        n.numa = -1;
        n.core = -1;
        n.host = ""; // défini via @autoprefix sinon 127.0.0.1
        n.port = -1; // défini via @autoprefix sinon 8000 + id
        slot = n;
    }
    return *slot;
}

[[noreturn]] void HamonParser::bad(const std::string &msg) const {
    throw std::runtime_error("[HamonDSL] line " + std::to_string(currentLine) + ": " + msg);
}

void HamonParser::parse_file(const std::string &path) {
    std::ifstream in(path);
    if (!in.is_open()) bad("Failed to open file: " + path);
    std::string line;
    currentLine = 0;
    while (std::getline(in, line)) {
        ++currentLine;
        parse_line(line);
    }
}

void HamonParser::parse_line(const std::string &line) {
    const std::string s = trim(line);
    if (s.empty()) return;
    if (starts_with(s, "//") || starts_with(s, "#")) return;

    // --- Directives globales ---
    if (starts_with(s, "@use")) {
        const auto toks = split_ws(s);
        if (toks.size() != 2) bad("@use expects 1 integer");
        try { nodes = std::stoi(toks[1]); } catch (...) { bad("@use expects integer"); }
        if (nodes <= 0) bad("@use must be > 0");
        return;
    }
    if (starts_with(s, "@dim")) {
        const auto toks = split_ws(s);
        if (toks.size() != 2) bad("@dim expects 1 integer");
        try { dimensions = std::stoi(toks[1]); } catch (...) { bad("@dim expects integer"); }
        if (dimensions <= 0) bad("@dim must be > 0");
        return;
    }
    if (starts_with(s, "@topology")) {
        const auto rest = trim(s.substr(std::string("@topology").size()));
        if (rest.empty()) bad("@topology expects a value (e.g., hypercube)");
        topology = trim(rest);
        return;
    }
    if (starts_with(s, "@autoprefix") || starts_with(s, "@auto")) {
        const auto pos = s.find(' ');
        if (pos == std::string::npos) bad("@autoprefix/@auto expects HOST:PORT");
        const std::string hp = trim(s.substr(pos + 1));
        parse_host_port(hp, hostname, autoPortBase);
        return;
    }

    // --- Début bloc node ---
    if (starts_with(s, "@node")) {
        const auto toks = split_ws(s);
        if (toks.size() != 2) bad("@node expects a single integer id");
        try { currentNodeId = std::stoi(toks[1]); } catch (...) { bad("@node id must be integer"); }
        (void) ensure_node(currentNodeId); // garantit l'existence
        return;
    }

    // --- Attributs de node (nécessitent un @node courant) ---
    if (starts_with(s, "@role")) {
        if (currentNodeId < 0) bad("@role used outside of @node");
        const auto rest = trim(s.substr(std::string("@role").size()));
        if (rest.empty()) bad("@role expects a value");
        ensure_node(currentNodeId).role = rest;
        return;
    }

    if (starts_with(s, "@cpu")) {
        if (currentNodeId < 0) bad("@cpu used outside of @node");
        // format: @cpu numa=I core=J (ordre libre)
        int numa = ensure_node(currentNodeId).numa;
        int core = ensure_node(currentNodeId).core;
        for (const auto toks = split_ws(s.substr(std::string("@cpu").size())); auto &t: toks) {
            const auto eq = t.find('=');
            if (eq == std::string::npos) continue;
            auto k = trim(t.substr(0, eq));
            auto v = trim(t.substr(eq + 1));
            try {
                if (k == "numa") numa = std::stoi(v);
                else if (k == "core") core = std::stoi(v);
            } catch (...) { bad("Invalid @cpu value"); }
        }
        auto &n = ensure_node(currentNodeId);
        n.numa = numa;
        n.core = core;
        return;
    }

    if (starts_with(s, "@ip")) {
        if (currentNodeId < 0) bad("@ip used outside of @node");
        const auto rest = trim(s.substr(std::string("@ip").size()));
        if (rest.empty()) bad("@ip expects HOST:PORT");
        std::string h;
        int p = -1;
        parse_host_port(rest, h, p);
        auto &n = ensure_node(currentNodeId);
        n.host = h;
        n.port = p;
        return;
    }

    if (starts_with(s, "@neighbors")) {
        if (currentNodeId < 0) bad("@neighbors used outside of @node");
        const auto rest = trim(s.substr(std::string("@neighbors").size()));
        if (rest.empty()) bad("@neighbors expects [list]");
        ensure_node(currentNodeId).neighbors = parse_list_ids(rest);
    }

    bad("Unknown directive: " + s);
}

void HamonParser::finalize() {
    // 1) Valider @use
    if (nodes < 0) bad("Missing @use <N>");

    if (static_cast<int>(config.size()) < nodes) config.resize(static_cast<std::size_t>(nodes));
    for (int i = 0; i < nodes; ++i) {
        if (!config[static_cast<std::size_t>(i)].has_value()) {
            NodeCfg n;
            n.id = i;
            n.role = "worker";
            n.numa = -1;
            n.core = -1;
            n.host = "";
            n.port = -1;
            config[static_cast<std::size_t>(i)] = n;
        }
    }

    // 3) Calculer la dimension si hypercube et non fourni
    if (topology == "hypercube") {
        if (dimensions < 0) {
            if (!is_power_of_two(static_cast<unsigned>(nodes))) {
                bad("@use must be a power of two or provide @dim explicitly for hypercube");
            }
            dimensions = log2i(static_cast<unsigned>(nodes));
        } else {
            // cohérence 2^dim == nodes
            if (const int expect = 1 << dimensions; expect != nodes) {
                bad("@dim inconsistent with @use for hypercube");
            }
        }
    }

    // 4) Rôles + endpoints par défaut
    for (int id = 0; id < nodes; ++id) {
        auto &n = *config[static_cast<std::size_t>(id)];
        if (id == 0 && (n.role.empty() || n.role == "worker")) n.role = "coordinator";
        if (n.host.empty()) {
            if (!hostname.empty() && autoPortBase >= 0) {
                n.host = hostname;
            } else {
                n.host = "127.0.0.1";
            }
        }
        if (n.port < 0) {
            if (autoPortBase >= 0) n.port = autoPortBase + id;
            else n.port = 8000 + id;
        }
    }

    // 5) Voisins par défaut (hypercube)
    if (topology == "hypercube") {
        for (int id = 0; id < nodes; ++id) {
            auto &n = *config[static_cast<std::size_t>(id)];
            if (!n.neighbors.empty()) continue; // déjà fournis
            n.neighbors.reserve(static_cast<std::size_t>(dimensions));
            for (int d = 0; d < dimensions; ++d) {
                if (int nei = id ^ 1 << d; nei >= 0 && nei < nodes) {
                    n.neighbors.push_back(nei);
                }
            }
        }
    }
    // pour d’autres topologies, on laisse @neighbors custom s’ils sont fournis (sinon isolé)
}

int HamonParser::dim() const {
    return dimensions;
}

const std::string &HamonParser::get_topology() const {
    return topology;
}

std::vector<NodeCfg> HamonParser::materialize_nodes() const {
    std::vector<NodeCfg> out;
    out.reserve(config.size());
    for (const auto &opt: config) if (opt.has_value()) out.push_back(*opt);
    return out;
}

void HamonParser::print_plan(std::ostream &os) const {
    os << "Cluster: " << nodes << " nodes, topology=" << topology;
    if (topology == "hypercube") os << " (dim=" << dimensions << ")";
    os << "\n\nNodes:\n";
    for (const auto &opt: config)
        if (opt.has_value()) {
            const auto &[id, role, numa, core, host, port, neighbors] = *opt;
            os << " - Node " << id
                    << ": role=" << (role.empty() ? "<unset>" : role)
                    << ", cpu_core=" << core
                    << ", numa_node=" << numa
                    << ", endpoint=" << host << ":" << port
                    << ", neighbors=[";
            for (size_t i = 0; i < neighbors.size(); ++i) {
                os << neighbors[i] << (i + 1 < neighbors.size() ? "," : "");
            }
            os << "]\n";
        }
    os << std::flush;
}
