#include "Hamon.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>
using namespace Dualys;
namespace fs = std::filesystem;

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
    while (iss >> tok) tokens.push_back(std::move(tok));
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

std::vector<int> HamonParser::parse_target_selector(const std::string &selector) const {
    std::string t = trim(selector);
    if (t.empty() || t.front() != '[' || t.back() != ']') {
        bad(std::string("Invalid selector format: ") + selector);
    }
    std::string content = trim(t.substr(1, t.size() - 2));
    std::vector<int> out;
    if (content == "*" || content == "all") {
        if (nodes < 0) bad("@phase used before @use <N>");
        out.reserve(static_cast<size_t>(nodes));
        for (int i = 0; i < nodes; ++i) out.push_back(i);
        return out;
    }
    if (content == "workers") {
        if (nodes < 0) bad("@phase used before @use <N>");
        for (int i = 0; i < nodes; ++i) if (i != 0) out.push_back(i);
        return out;
    }
    // explicit list
    out = parse_list_ids(selector);
    // validate
    for (int v : out) {
        if (v < 0 || (nodes >= 0 && v >= nodes)) {
            bad(std::string("Target node id out of range: ") + std::to_string(v));
        }
    }
    return out;
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
    fs::path p = fs::absolute(path);
    if (include_depth >= include_depth_max) bad("Include depth exceeded");
    if (!fs::exists(p)) bad("Failed to open file: " + p.string());

    std::string key = p.string();
    if (include_guard.find(key) != include_guard.end()) {
        bad("Circular include detected: " + key);
    }

    std::ifstream in(p);
    if (!in.is_open()) bad("Failed to open file: " + p.string());

    // RAII guard pour stack et guard set
    struct IncludeGuardRAII {
        HamonParser *self;
        std::string key;
        fs::path path;

        IncludeGuardRAII(HamonParser *s, std::string k, fs::path pth)
            : self(s), key(std::move(k)), path(std::move(pth)) {
            self->include_guard.insert(key);
            self->file_stack.push_back(path);
            self->include_depth++;
        }

        ~IncludeGuardRAII() {
            self->include_guard.erase(key);
            if (!self->file_stack.empty()) self->file_stack.pop_back();
            self->include_depth--;
        }
    } guard(this, key, p);

    std::string line;
    currentLine = 0;
    while (std::getline(in, line)) {
        ++currentLine;
        std::string s = trim(line);
        auto cpos = s.find("//");
        if (cpos != std::string::npos) s = trim(s.substr(0, cpos));
        if (s.empty()) continue;
        parse_line(s);
    }
}

void HamonParser::parse_line(const std::string &line) {
    std::string s = trim(line);
    if (s.empty()) return;
    if (starts_with(s, "//") || starts_with(s, "#")) return;
    auto cpos = s.find("//");
    if (cpos != std::string::npos) {
        s = trim(s.substr(0, cpos));
    }
    if (s.empty()) return;
    cpos = s.find("#");
    if (cpos != std::string::npos) {
        s = trim(s.substr(0, cpos));
    }
    if (s.empty()) return;
    if (starts_with(s, "@include")) {
        std::string rest = trim(s.substr(std::string("@include").size()));
        if (rest.empty()) bad("@include expects a path");

        auto strip_quotes = [](std::string &x) {
            x = trim(x);
            if (x.size() >= 2) {
                const char a = x.front(), b = x.back();
                if ((a == '"' && b == '"') || (a == '\'' && b == '\'')) {
                    x = x.substr(1, x.size() - 2);
                }
            }
        };

        // 1) strip quotes (premier passage)
        strip_quotes(rest);
        // 2) expand ${VAR}
        rest = expand_vars(rest);
        // 3) re-trim & strip quotes (au cas où l’expansion a introduit des guillemets)
        strip_quotes(rest);

        // 4) base = dossier du fichier courant; fallback = cwd
        fs::path base = file_stack.empty() ? fs::current_path() : file_stack.back().parent_path();
        fs::path target = fs::absolute(base / rest);

        if (!fs::exists(target)) {
            bad(std::string("@include file not found: ") + target.string() +
                " (base=" + base.string() + ", rest=" + rest + ")");
        }

        parse_file(target.string());
        return;
    }

    if (starts_with(s, "@auto") || starts_with(s, "@autoprefix")) {
        auto pos = s.find(' ');
        if (pos == std::string::npos) bad("@auto expects HOST:PORT");
        std::string hp = trim(s.substr(pos + 1));
        hp = expand_vars(hp); // <<< FIX 2 : expansion avant parse_host_port
        parse_host_port(hp, hostname, autoPortBase);
        return;
    }

    if (starts_with(s, "@ip")) {
        if (currentNodeId < 0) bad("@ip used outside of @node");
        auto rest = trim(s.substr(std::string("@ip").size()));
        rest = expand_vars(rest); // <<< FIX 2 : expansion
        std::string h;
        int p = -1;
        parse_host_port(rest, h, p);
        auto &n = ensure_node(currentNodeId);
        n.host = h;
        n.port = p;
        return;
    }


    // ----- @let -----
    if (starts_with(s, "@let")) {
        std::string rest = trim(s.substr(std::string("@let").size()));
        if (rest.empty()) bad("@let expects NAME=VALUE or NAME VALUE");
        auto eq = rest.find('=');
        std::string name, value;
        if (eq == std::string::npos) {
            if (const auto toks = split_ws(rest); toks.size() == 1) {
                name = toks[0];
                value = "1";
            } else if (toks.size() >= 2) {
                name = toks[0];
                value.clear();
                for (size_t i = 1; i < toks.size(); ++i) {
                    if (i > 1) value.push_back(' ');
                    value += toks[i];
                }
            } else bad("@let invalid syntax");
        } else {
            name = trim(rest.substr(0, eq));
            value = trim(rest.substr(eq + 1));
        }
        if (name.empty()) bad("@let invalid name");
        // retire guillemets autour de value
        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (
                                      value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }
        vars[name] = expand_vars(value);
        return;
    }

    // ----- @require -----
    if (starts_with(s, "@require")) {
        std::string rest = trim(s.substr(std::string("@require").size()));
        if (rest.empty()) bad("@require expects an expression");
        if (!eval_require_expr(rest)) {
            bad(std::string("@require failed: ") + rest);
        }
        return;
    }

    // ----- Job/Phase DSL -----
    if (starts_with(s, "@job")) {
        if (currentJobIndex != -1) bad("@job inside another job");
        std::string rest = trim(s.substr(std::string("@job").size()));
        if (rest.empty()) bad("@job expects a name");
        Job j;
        j.name = rest;
        jobs.push_back(std::move(j));
        currentJobIndex = static_cast<int>(jobs.size()) - 1;
        return;
    }
    if (starts_with(s, "@input")) {
        if (currentJobIndex == -1) bad("@input used outside of @job");
        std::string rest = trim(s.substr(std::string("@input").size()));
        // strip optional quotes
        if (rest.size() >= 2 && ((rest.front() == '"' && rest.back() == '"') || (rest.front() == '\'' && rest.back() == '\''))) {
            rest = rest.substr(1, rest.size() - 2);
        }
        jobs[static_cast<size_t>(currentJobIndex)].input = expand_vars(rest);
        return;
    }
    if (starts_with(s, "@phase")) {
        if (currentJobIndex == -1) bad("@phase used outside of @job");
        std::string rest = trim(s.substr(std::string("@phase").size()));
        if (rest.empty()) bad("@phase expects a name and attributes");
        // phase name = first token
        std::string name;
        size_t i = 0;
        while (i < rest.size() && std::isspace(static_cast<unsigned char>(rest[i]))) ++i;
        size_t j = i;
        while (j < rest.size() && !std::isspace(static_cast<unsigned char>(rest[j]))) ++j;
        name = rest.substr(i, j - i);
        Phase ph;
        ph.name = name;
        // extract task="..."
        {
            static const std::regex taskRe(R"_HC(\btask\s*=\s*\"([^\"]*)\")_HC");
            std::smatch m;
            if (std::regex_search(rest, m, taskRe)) {
                if (m.size() >= 2) ph.task = m[1].str();
            }
        }
        // extract optional desc="..."
        {
            static const std::regex descRe(R"_HC(\bdesc\s*=\s*\"([^\"]*)\")_HC");
            std::smatch m;
            if (std::regex_search(rest, m, descRe)) {
                if (m.size() >= 2) ph.description = m[1].str();
            }
        }
        if (ph.task.empty()) bad("@phase missing task=\"...\"");
        // extract by=[...] or to=[...]
        {
            auto find_selector = [&](const char *key)->std::string {
                const std::string k = std::string(key) + std::string("=");
                size_t pos = rest.find(k);
                if (pos == std::string::npos) return std::string();
                pos += k.size();
                // skip spaces
                while (pos < rest.size() && std::isspace(static_cast<unsigned char>(rest[pos]))) ++pos;
                if (pos >= rest.size() || rest[pos] != '[') return std::string();
                size_t end = rest.find(']', pos);
                if (end == std::string::npos) bad(std::string("Missing closing ']' for ") + key);
                return rest.substr(pos, end - pos + 1);
            };
            std::string sel = find_selector("by");
            if (sel.empty()) sel = find_selector("to");
            if (sel.empty()) {
                // default to all nodes
                sel = "[*]";
            }
            ph.target_nodes = parse_target_selector(sel);
        }
        jobs[static_cast<size_t>(currentJobIndex)].phases.push_back(std::move(ph));
        return;
    }
    if (starts_with(s, "@end")) {
        if (currentJobIndex == -1) bad("@end outside of @job");
        currentJobIndex = -1;
        return;
    }

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
        // Accept inline attributes after the id, e.g.:
        // @node 0 @role coordinator @cpu numa=0 core=0
        std::string after = trim(s.substr(std::string("@node").size()));
        if (after.empty()) bad("@node expects a single integer id");
        // parse id (first token)
        size_t i = 0;
        while (i < after.size() && std::isspace(static_cast<unsigned char>(after[i]))) ++i;
        size_t j = i;
        while (j < after.size() && !std::isspace(static_cast<unsigned char>(after[j]))) ++j;
        if (i == j) bad("@node expects a single integer id");
        const std::string idstr = after.substr(i, j - i);
        try { currentNodeId = std::stoi(idstr); } catch (...) { bad("@node id must be integer"); }
        (void) ensure_node(currentNodeId); // garantit l'existence
        // process remaining inline directives, if any
        std::string rest = trim(after.substr(j));
        while (!rest.empty()) {
            size_t k = 0;
            while (k < rest.size() && std::isspace(static_cast<unsigned char>(rest[k]))) ++k;
            if (k >= rest.size()) break;
            if (rest[k] != '@') {
                bad(std::string("Unexpected token after @node id: ") + rest.substr(k));
            }
            size_t m = k + 1;
            // find next directive start '@' that is separated by whitespace
            for (; m < rest.size(); ++m) {
                if (rest[m] == '@' && std::isspace(static_cast<unsigned char>(rest[m - 1]))) break;
            }
            const std::string sub = trim(rest.substr(k, m - k));
            if (!sub.empty()) parse_line(sub);
            rest = trim(rest.substr(m));
        }
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
        {
            const auto toks = split_ws(s.substr(std::string("@cpu").size()));
            for (const auto &t : toks) {
                const auto eq = t.find('=');
                if (eq == std::string::npos) continue;
                auto k = trim(t.substr(0, eq));
                auto v = trim(t.substr(eq + 1));
                try {
                    if (k == "numa") numa = std::stoi(v);
                    else if (k == "core") core = std::stoi(v);
                } catch (...) { bad("Invalid @cpu value"); }
            }
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
        return;
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
    // À la fin de finalize(), après avoir rempli les voisins par défaut si besoin :
    for (int id = 0; id < nodes; ++id) {
        auto &n = *config[static_cast<std::size_t>(id)];
        // supprimer doublons
        std::sort(n.neighbors.begin(), n.neighbors.end());
        n.neighbors.erase(std::unique(n.neighbors.begin(), n.neighbors.end()), n.neighbors.end());
        // pas d’auto-voisin
        n.neighbors.erase(std::remove(n.neighbors.begin(), n.neighbors.end(), id), n.neighbors.end());
        // vérifier bornes
        for (const int v: n.neighbors) {
            if (v < 0 || v >= nodes) {
                bad("Neighbor out of range for node " + std::to_string(id) + ": " + std::to_string(v));
            }
        }
    }
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
    os << "[hamon] Cluster: " << nodes << " nodes; topology=" << topology;
    if (topology == "hypercube") os << "; dim=" << dimensions;
    os << "\n[hamon] Nodes:\n";
    for (const auto &opt: config)
        if (opt.has_value()) {
            const auto &[id, role, numa, core, host, port, neighbors] = *opt;
            os << "  • Node " << id
               << " | role=" << (role.empty() ? "<unset>" : role)
               << " | core=" << core
               << " | numa=" << numa
               << " | endpoint=" << host << ":" << port
               << " | neighbors=[";
            for (size_t i = 0; i < neighbors.size(); ++i) {
                if (i) os << ",";
                os << neighbors[i];
            }
            os << "]\n";
        }
    os << std::flush;
}

std::string HamonParser::expand_vars(const std::string &in) const {
    static const std::regex re(R"(\$\{([A-Za-z_][A-Za-z0-9_]*)\})");
    std::string out;
    out.reserve(in.size());
    std::sregex_iterator it(in.begin(), in.end(), re);
    size_t last = 0;
    for (const std::sregex_iterator end; it != end; ++it) {
        const auto &m = *it;
        out.append(in, last, static_cast<size_t>(m.position()) - last);
        std::string key = m[1].str();
        if (const auto itv = vars.find(key); itv != vars.end()) out += itv->second;
        else out += m.str();
        last = static_cast<size_t>(m.position() + m.length());
    }
    out.append(in, last, std::string::npos);
    return out;
}


// ---------- eval_require_expr ----------
bool HamonParser::is_truthy(const std::string &v) {
    if (v.empty()) return false;
    std::string s;
    s.reserve(v.size());
    for (const char c: v) s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return !(s == "0" || s == "false" || s == "no" || s == "off");
}

bool HamonParser::str_to_int(const std::string &s, long long &out) {
    char *end = nullptr;
    errno = 0;
    const long long v = std::strtoll(s.c_str(), &end, 10);
    if (errno != 0 || end == s.c_str() || *end != '\0') return false;
    out = v;
    return true;
}

bool HamonParser::eval_require_expr(const std::string &raw) const {
    const std::string s = trim(raw);
    if (s.empty()) return false;

    std::vector<std::string> tok;
    {
        bool inq = false;
        std::string cur;
        for (size_t i = 0; i < s.size(); ++i) {
            const char c = s[i];
            if (c == '"') {
                inq = !inq;
                continue;
            }
            if (!inq && std::isspace(static_cast<unsigned char>(c))) {
                if (!cur.empty()) {
                    tok.push_back(cur);
                    cur.clear();
                }
            } else {
                cur.push_back(c);
            }
        }
        if (!cur.empty()) tok.push_back(cur);
    }
    if (tok.empty()) return false;

    const auto resolve = [&](const std::string &t)-> std::string {
        const std::string e = expand_vars(t);
        return e;
    };

    if (tok.size() == 1) {
        const std::string v = resolve(tok[0]);
        return is_truthy(v);
    }
    if (tok.size() >= 3) {
        const std::string L = resolve(tok[0]);
        const std::string OP = tok[1];
        // re-colle le RHS (au cas où il y avait des espaces entre guillemets)
        std::string R;
        for (size_t i = 2; i < tok.size(); ++i) {
            if (i > 2) R.push_back(' ');
            R += tok[i];
        }
        R = resolve(R);

        long long Li, Ri;
        const bool Li_ok = str_to_int(L, Li);
        const bool Ri_ok = str_to_int(R, Ri);

        if (OP == "==") return L == R;
        if (OP == "!=") return L != R;
        if (OP == ">") return Li_ok && Ri_ok ? Li > Ri : false;
        if (OP == "<") return Li_ok && Ri_ok ? Li < Ri : false;
        if (OP == ">=") return Li_ok && Ri_ok ? Li >= Ri : false;
        if (OP == "<=") return Li_ok && Ri_ok ? Li <= Ri : false;
        return false;
    }
    return false;
}
