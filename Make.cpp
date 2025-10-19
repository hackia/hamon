#include "Make.hpp"
#include "Hamon.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace Dualys {

    using namespace std;

    static bool file_exists(const std::string &path) {
        std::error_code ec;
        return std::filesystem::exists(path, ec);
    }

    vector<string> Make::extract_tasks(const string &hc_path, ostream &log) {
        vector<string> tasks;
        if (!file_exists(hc_path)) {
            log << "[Make] File not found: " << hc_path << '\n';
            return tasks;
        }
        ifstream in(hc_path);
        if (!in) {
            log << "[Make] Cannot open file: " << hc_path << '\n';
            return tasks;
        }
        string line;
        // Very simple extraction: match task="..." on @phase lines.
        // Supports quotes with no escaping for now.
        const regex phaseTask(R"_HC(^\s*@phase\b.*\btask\s*=\s*\"([^\"]*)\")_HC");
        while (getline(in, line)) {
            smatch m;
            if (regex_search(line, m, phaseTask)) {
                if (m.size() >= 2) {
                    tasks.emplace_back(m[1].str());
                }
            }
        }
        return tasks;
    }

    bool Make::build_from_hc(const string &hc_path) {
        return build_from_hc(hc_path, std::cout);
    }

    bool Make::build_from_hc(const string &hc_path, ostream &log) {
        // Pre-parse with HamonParser to allow variable expansion from @let and friends
        HamonParser parser;
        try {
            parser.parse_file(hc_path);
            parser.finalize();
        } catch (const std::exception &e) {
            log << "[Make] Parse error: " << e.what() << '\n';
            return false;
        }
        auto tasks = extract_tasks(hc_path, log);
        if (tasks.empty()) {
            log << "[Make] No tasks found in: " << hc_path << '\n';
            return false;
        }
        // Execute sequentially, locally.
        for (size_t i = 0; i < tasks.size(); ++i) {
            string cmd = parser.expand_vars(tasks[i]);
            log << "[Make] (" << (i + 1) << "/" << tasks.size() << ") $ " << cmd << '\n';
            int rc = std::system(cmd.c_str());
            if (rc != 0) {
                log << "[Make] Command failed with code " << rc << ": " << cmd << '\n';
                return false;
            }
        }
        log << "[Make] All tasks completed successfully." << '\n';
        return true;
    }

} // namespace Dualys
