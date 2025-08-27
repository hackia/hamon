#include "Hamon.h"

std::string Dualys::HamonParser::trim(const std::string &x) {
    auto start = x.begin();
    while (start != x.end() && std::isspace(*start)) {
        ++start;
    }
    auto end = x.rbegin();
    while (end != x.rend() && std::isspace(*end)) {
        ++end;
    }
    return std::string(start, end.base()).c_str();
}

bool Dualys::HamonParser::starts_with(const std::string &s, const std::string &p) {
    return s.size() >= p.size() && s.substr(0, p.size()) == p;
}

std::vector<std::string> Dualys::HamonParser::split_ws(const std::string &line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<int> Dualys::HamonParser::parse_list_ids(const std::string &src) {
    std::vector<int> result;
    auto trimmed = trim(src);
    if (trimmed.empty() || trimmed[0] != '[' || trimmed.back() != ']') {
        bad("Invalid list format: " + src);
    }
    std::string content = trimmed.substr(1, trimmed.size() - 2);
    if (content.empty()) {
        return result;
    }
    std::stringstream ss(content);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        if (!item.empty()) {
            try {
                result.push_back(std::stoi(item));
            } catch (const std::exception &) {
                bad("Invalid number in list: " + item);
            }
        }
    }
    return result;
}

void Dualys::HamonParser::parse_host_port(const std::string &s, std::string &host, int &port) {
    auto pos = s.find(':');
    if (pos == std::string::npos) {
        bad("Invalid host:port format: " + s);
    }
    host = trim(s.substr(0, pos));
    try {
        port = std::stoi(trim(s.substr(pos + 1)));
    } catch (const std::exception &) {
        bad("Invalid port number in: " + s);
    }
}

bool Dualys::HamonParser::is_power_of_two(unsigned x) {
    return x && !(x & (x - 1));
}

int Dualys::HamonParser::log2i(unsigned x) {
    int result = 0;
    while (x >>= 1) {
        ++result;
    }
    return result;
}

Dualys::NodeCfg &Dualys::HamonParser::ensure_node(int id) {
    if (id < 0 || id >= nodes) {
        bad("Invalid node ID: " + std::to_string(id));
    }
    const std::size_t idx = static_cast<std::size_t>(id);

    if (idx >= config.size()) {
        config.resize(idx + 1);
    }

    auto& opt = config[idx];
    if (!opt.has_value()) {
        opt.emplace(NodeCfg{
            .id = id,
            .role = "worker",
            .numa = -1,
            .core = -1,
            .host = "127.0.0.1",
            .port = 8000 + id,
            .neighbors = {}
        });
    }

    return *opt;
}

void Dualys::HamonParser::parse_line(const std::string &line) {

}

void Dualys::HamonParser::parse_file(const std::string &path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    std::string line;
    while (std::getline(in, line)) {
        parse_line(line);
    }
    in.close();
}
