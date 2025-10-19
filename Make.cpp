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
#include <future>

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
            if (smatch m; regex_search(line, m, phaseTask)) {
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
        const auto rawTasks = extract_tasks(hc_path, log);
        if (rawTasks.empty()) {
            log << "[Make] No tasks found in: " << hc_path << '\n';
            return false;
        }

        // Expand variables first
        vector<string> tasks;
        tasks.reserve(rawTasks.size());
        for (const auto &t : rawTasks) tasks.emplace_back(parser.expand_vars(t));

        // Split into compilations (-c) and the rest (e.g., linking)
        vector<string> compiles;
        vector<string> others;
        compiles.reserve(tasks.size());
        others.reserve(tasks.size());
        for (const auto &cmd : tasks) {
            if (cmd.find(" -c ") != string::npos || cmd.rfind(" -c", cmd.size()-2) != string::npos) {
                compiles.push_back(cmd);
            } else {
                others.push_back(cmd);
            }
        }

        // Run compile commands in parallel
        if (!compiles.empty()) {
            log << "[Make] Parallel compile jobs: " << compiles.size() << '\n';
            vector<future<int>> futures;
            futures.reserve(compiles.size());
            for (size_t i = 0; i < compiles.size(); ++i) {
                const string cmd = compiles[i];
                log << "[Make][C][" << (i + 1) << "/" << compiles.size() << "] $ " << cmd << '\n';
                futures.emplace_back(std::async(std::launch::async, [cmd]() {
                    return std::system(cmd.c_str());
                }));
            }
            // Wait and check
            for (size_t i = 0; i < futures.size(); ++i) {
                int rc = futures[i].get();
                if (rc != 0) {
                    log << "[Make] Compile command failed with code " << rc << '\n';
                    return false;
                }
            }
        }

        // Run the remaining tasks sequentially (e.g., linking)
        for (size_t i = 0; i < others.size(); ++i) {
            const string &cmd = others[i];
            log << "[Make][S][" << (i + 1) << "/" << others.size() << "] $ " << cmd << '\n';
            if (int rc = std::system(cmd.c_str()); rc != 0) {
                log << "[Make] Command failed with code " << rc << ": " << cmd << '\n';
                return false;
            }
        }

        log << "[Make] All tasks completed successfully." << '\n';
        return true;
    }

} // namespace Dualys
