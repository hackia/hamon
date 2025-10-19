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
#include <thread>
#include <unistd.h>
#include <sys/wait.h>
#include <sched.h>

namespace Dualys {

    using namespace std;

    struct RunItem {
        std::string cmd;
        int node_id = -1; // -1 if not mapped
    };

    static int infer_logical_cpu(const std::unordered_map<int, NodeCfg> &nodes_by_id, int node_id) {
        auto it = nodes_by_id.find(node_id);
        if (it == nodes_by_id.end()) return -1;
        const auto &n = it->second;
        if (n.numa < 0 && n.core < 0) return -1;
        // Infer cores per NUMA from hardware_concurrency and max numa index present
        unsigned hw = std::thread::hardware_concurrency();
        if (hw == 0) hw = 1;
        int max_numa = -1;
        for (const auto &kv : nodes_by_id) if (kv.second.numa >= 0) max_numa = std::max(max_numa, kv.second.numa);
        int numa_count = max_numa >= 0 ? (max_numa + 1) : 1;
        unsigned cores_per_numa = hw / (numa_count == 0 ? 1 : static_cast<unsigned>(numa_count));
        if (cores_per_numa == 0) cores_per_numa = 1;
        int numa = std::max(n.numa, 0);
        int core = std::max(n.core, 0);
        long logical = static_cast<long>(numa) * static_cast<long>(cores_per_numa) + static_cast<long>(core);
        if (logical < 0 || logical >= static_cast<long>(hw)) return -1;
        return static_cast<int>(logical);
    }

    static int run_with_affinity(const std::string &cmd, const std::unordered_map<int, NodeCfg> &nodes_by_id, int node_id, std::ostream &log) {
        int cpu = infer_logical_cpu(nodes_by_id, node_id);
        pid_t pid = fork();
        if (pid < 0) {
            log << "[Make] fork() failed for: " << cmd << "\n";
            return -1;
        }
        if (pid == 0) {
            // Child: set CPU affinity if available
            if (cpu >= 0) {
                cpu_set_t set;
                CPU_ZERO(&set);
                CPU_SET(static_cast<unsigned>(cpu), &set);
                if (sched_setaffinity(0, sizeof(set), &set) != 0) {
                    // best-effort: continue even if it fails
                    // Do not exit; proceed to execute the command without pinning
                }
            }
            // Exec via /bin/sh -c to avoid manual argv parsing
            execl("/bin/sh", "sh", "-c", cmd.c_str(), static_cast<char *>(nullptr));
            _exit(127);
        }
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            log << "[Make] waitpid() failed for: " << cmd << "\n";
            return -1;
        }
        if (WIFEXITED(status)) return WEXITSTATUS(status);
        if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
        return -1;
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

        // Build run list using parsed jobs/phases and node targets
        // Build node map by id
        std::unordered_map<int, NodeCfg> nodes_by_id;
        for (const auto &n : parser.materialize_nodes()) nodes_by_id[n.id] = n;
        vector<RunItem> compiles;
        vector<RunItem> others;
        const auto &jobs = parser.get_jobs();
        for (const auto &job : jobs) {
            for (const auto &ph : job.phases) {
                const std::string expanded = parser.expand_vars(ph.task);
                // For each target node, create a run item (if no targets, -1)
                if (ph.target_nodes.empty()) {
                    RunItem it{expanded, -1};
                    if (expanded.find(" -c ") != string::npos || expanded.rfind(" -c", expanded.size() - 2) != string::npos) compiles.push_back(it); else others.push_back(it);
                } else {
                    for (int nid : ph.target_nodes) {
                        RunItem it{expanded, nid};
                        if (expanded.find(" -c ") != string::npos || expanded.rfind(" -c", expanded.size() - 2) != string::npos) compiles.push_back(it); else others.push_back(it);
                    }
                }
            }
        }
        if (compiles.empty() && others.empty()) {
            log << "[Make] No tasks found in: " << hc_path << '\n';
            return false;
        }

        // Run compile commands in parallel with affinity
        if (!compiles.empty()) {
            log << "[Make] Parallel compile jobs: " << compiles.size() << '\n';
            vector<future<int>> futures;
            futures.reserve(compiles.size());
            for (size_t i = 0; i < compiles.size(); ++i) {
                const auto item = compiles[i];
                int cpu = infer_logical_cpu(nodes_by_id, item.node_id);
                if (cpu >= 0) log << "[Make][C][" << (i + 1) << "/" << compiles.size() << "] pin cpu=" << cpu << " nid=" << item.node_id << " $ " << item.cmd << '\n';
                else log << "[Make][C][" << (i + 1) << "/" << compiles.size() << "] $ " << item.cmd << '\n';
                auto nodes_copy = nodes_by_id; // capture by value for async task
                futures.emplace_back(std::async(std::launch::async, [item, nodes_copy]() {
                    return run_with_affinity(item.cmd, nodes_copy, item.node_id, cout);
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

        // Run the remaining tasks sequentially (e.g., linking), with affinity for the first target node (if any)
        for (size_t i = 0; i < others.size(); ++i) {
            const auto &item = others[i];
            int cpu = infer_logical_cpu(nodes_by_id, item.node_id);
            if (cpu >= 0) log << "[Make][S][" << (i + 1) << "/" << others.size() << "] pin cpu=" << cpu << " nid=" << item.node_id << " $ " << item.cmd << '\n';
            else log << "[Make][S][" << (i + 1) << "/" << others.size() << "] $ " << item.cmd << '\n';
            int rc = run_with_affinity(item.cmd, nodes_by_id, item.node_id, log);
            if (rc != 0) {
                log << "[Make] Command failed with code " << rc << ": " << item.cmd << '\n';
                return false;
            }
        }

        log << "[Make] All tasks completed successfully." << '\n';
        return true;
    }

} // namespace Dualys
