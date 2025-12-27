#include "../include/Hamon.hpp"
#include "../include/Make.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>
#include <ranges>
#include <future>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>
#include <sched.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <cmath>
#include <sys/ioctl.h>

using namespace dualys;
using namespace std;

// OpenRC style status printing with terminal width detection
static void print_status(ostream &log, const string &msg, const string &status, const bool error = false) {
    winsize w{};
    int term_width = 80;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        term_width = w.ws_col;
    }

    // Colors: Stars (Green), Brackets (White), Status (Green/Red)
    const string green_star = "\033[32m*\033[0m";
    const string white_bracket_open = "\033[37m[\033[0m";
    const string white_bracket_close = "\033[37m]\033[0m";
    const string status_text = error ? "\033[31;1m" + status + "\033[0m" : "\033[32;1m" + status + "\033[0m";
    const string status_block = " " + white_bracket_open + " " + status_text + " " + white_bracket_close;
    const int msg_display_len = 3 + static_cast<int>(msg.length());
    int padding = term_width - msg_display_len - 7;
    if (padding < 1) padding = 1;

    log << " " << green_star << " " << msg;
    for (int i = 0; i < padding; ++i) log << " ";
    log << status_block << endl;
}

struct RunItem {
    std::string cmd;
    std::string desc;
    std::string stdout_path;
    std::string stderr_path;
    int id = -1;
    int node_id = -1; // -1 if not mapped
};

static int infer_logical_cpu(const std::unordered_map<int, NodeCfg> &nodes_by_id, const int node_id) {
    const auto it = nodes_by_id.find(node_id);
    if (it == nodes_by_id.end()) return -1;
    const auto &n = it->second;
    if (n.numa < 0 && n.core < 0) return -1;
    // Infer cores per NUMA from hardware_concurrency and max numa index present
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 1;
    int max_numa = -1;
    for (const auto &snd: nodes_by_id | std::views::values) if (snd.numa >= 0) max_numa = std::max(max_numa, snd.numa);
    const int numa_count = max_numa >= 0 ? max_numa + 1 : 1;
    unsigned cores_per_numa = hw / (numa_count == 0 ? 1 : static_cast<unsigned>(numa_count));
    if (cores_per_numa == 0) cores_per_numa = 1;
    const int numa = std::max(n.numa, 0);
    const int core = std::max(n.core, 0);
    const long logical = static_cast<long>(numa) * static_cast<long>(cores_per_numa) + static_cast<long>(core);
    if (logical < 0 || logical >= static_cast<long>(hw)) return -1;
    return static_cast<int>(logical);
}

static int run_with_affinity(const std::string &cmd, const std::unordered_map<int, NodeCfg> &nodes_by_id,
                             const int node_id, std::ostream &log, const std::string &out_path,
                             const std::string &err_path) {
    const int pid = fork();
    if (pid < 0) {
        print_status(log, "failed to fork cmd", "!!", true);
        return -1;
    }
    if (pid == 0) {
        // Child: set CPU affinity if available
        if (const int cpu = infer_logical_cpu(nodes_by_id, node_id); cpu >= 0) {
            cpu_set_t set;
            CPU_ZERO(&set);
            CPU_SET(static_cast<unsigned>(cpu), &set);
            if (sched_setaffinity(0, sizeof(set), &set) != 0) {
                // best-effort: continue even if it fails
                // Do not exit; proceed to execute the command without pinning
            }
        }
        // Redirect stdout/stderr to files
        if (!out_path.empty()) {
            if (const int fd = open(out_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644); fd >= 0) {
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
        }
        if (!err_path.empty()) {
            if (const int fd = open(err_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644); fd >= 0) {
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }
        execl("/bin/sh", "sh", "-c", cmd.c_str(), static_cast<char *>(nullptr));
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        print_status(log, "failed to wait pid", "!!", true);
        return -1;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
}

bool Make::build_from_hc(const string &hc_path, ostream &log) {
    HamonParser parser;
    try {
        parser.parse_file(hc_path);
        parser.finalize();
    } catch (const std::exception &e) {
        print_status(log, e.what(), "!!", true);
        return false;
    }

    std::unordered_map<int, NodeCfg> nodes_by_id;
    for (const auto &n: parser.materialize_nodes()) nodes_by_id[n.id] = n;
    vector<RunItem> compiles;
    vector<RunItem> others;
    for (const auto &jobs = parser.get_jobs(); const auto &job: jobs) {
        for (const auto &[name, task, description, target_nodes]: job.phases) {
            const std::string expanded = parser.expand_vars(task);
            const std::string d = parser.expand_vars(description.empty() ? name : description);
            // For each target node, create a run item (if no targets, -1)
            if (target_nodes.empty()) {
                RunItem it;
                it.cmd = expanded;
                it.desc = d;
                it.node_id = -1;
                if (expanded.find(" -c ") != string::npos || expanded.rfind(" -c", expanded.size() - 2) !=
                    string::npos)
                    compiles.push_back(it);
                else others.push_back(it);
            } else {
                for (int nid: target_nodes) {
                    RunItem it;
                    it.cmd = expanded;
                    it.desc = d;
                    it.node_id = nid;
                    if (expanded.find(" -c ") != string::npos || expanded.rfind(" -c", expanded.size() - 2) !=
                        string::npos)
                        compiles.push_back(it);
                    else others.push_back(it);
                }
            }
        }
    }
    if (compiles.empty() && others.empty()) {
        print_status(log, "No tasks found", "!!", true);
        return false;
    }

    // Prepare logs directories and assign IDs
    std::filesystem::create_directories("stdout");
    std::filesystem::create_directories("stderr");
    int next_id = 1;
    auto assign_meta = [&](std::vector<RunItem> &v) {
        for (auto &it: v) {
            it.id = next_id++;
            it.stdout_path = std::string("stdout/") + std::to_string(it.id) + ".log";
            it.stderr_path = std::string("stderr/") + std::to_string(it.id) + ".log";
        }
    };
    assign_meta(compiles);
    assign_meta(others);

    // Prepare overall progress
    if (const size_t total_tasks = compiles.size() + others.size(); total_tasks > 0) {
        print_status(log, "Starting build system...", "ok");
    }
    if (!compiles.empty()) {
        vector<future<int> > futures;
        futures.reserve(compiles.size());
        for (const auto &item: compiles) {
            auto &nodes_copy = nodes_by_id;
            futures.emplace_back(std::async(std::launch::async, [item, nodes_copy] {
                return run_with_affinity(item.cmd, nodes_copy, item.node_id, cout, item.stdout_path,
                                         item.stderr_path);
            }));
        }

        for (size_t i = 0; i < futures.size(); ++i) {
            if (int rc = futures[i].get(); rc != 0) {
                print_status(log, compiles[i].desc, "!!", true);
                return false;
            }
            print_status(log, compiles[i].desc, "ok");
        }
    }

    // Run the remaining tasks sequentially
    for (const auto &[cmd, desc, stdout_path, stderr_path, id, node_id]: others) {
        if (int rc = run_with_affinity(cmd, nodes_by_id, node_id, log, stdout_path, stderr_path); rc != 0) {
            print_status(log, desc, "!!", true);
            return false;
        }
        print_status(log, desc, "ok");
    }
    print_status(log, "Build completed successfully", "ok");
    return true;
}

