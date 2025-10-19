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
#include <fcntl.h>

namespace Dualys {

    using namespace std;

    struct RunItem {
        std::string cmd;
        std::string desc;
        std::string stdout_path;
        std::string stderr_path;
        int id = -1;
        int node_id = -1; // -1 if not mapped
    };

    // One-line console progress bar with stdout/stderr paths and context
    // Format: [ stdout ] [ stderr ] [#####-----] 15% desc cmd
    static void print_progress(size_t done, size_t total, std::ostream &log,
                               const std::string &desc = std::string(),
                               const std::string &cmd = std::string(),
                               const std::string &stdout_path = std::string(),
                               const std::string &stderr_path = std::string()) {
        if (total == 0) return;
        const size_t width = 20; // reasonable default for terminal widths
        double ratio = static_cast<double>(done) / static_cast<double>(total);
        if (ratio < 0) ratio = 0;
        if (ratio > 1) ratio = 1;
        size_t filled = static_cast<size_t>(ratio * width + 0.5);

        // Build bar
        std::ostringstream bar;
        for (size_t i = 0; i < width; ++i) bar << (i < filled ? '#' : '-');
        int percent = static_cast<int>(ratio * 100.0 + 0.5);

        // Trim long desc/cmd to keep the line readable
        auto trim_to = [](std::string s, size_t n) {
            if (n == 0) return std::string();
            if (s.size() <= n) return s;
            if (n <= 3) return s.substr(0, n);
            return s.substr(0, n - 3) + "...";
        };

        const size_t max_desc = 60ul;
        const size_t max_cmd  = 80ul;
        std::string d = trim_to(desc, max_desc);
        std::string c = trim_to(cmd, max_cmd);

        log << "\r[ " << stdout_path << " ] [ " << stderr_path << " ] [" << bar.str() << "] "
            << percent << "% " << d;
        if (!c.empty()) log << " $ " << c;
        log << std::flush;
    }

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

    static int run_with_affinity(const std::string &cmd, const std::unordered_map<int, NodeCfg> &nodes_by_id, int node_id, std::ostream &log, const std::string &out_path, const std::string &err_path) {
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
            // Redirect stdout/stderr to files
            if (!out_path.empty()) {
                int fd = ::open(out_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd >= 0) { ::dup2(fd, STDOUT_FILENO); ::close(fd); }
            }
            if (!err_path.empty()) {
                int fd = ::open(err_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd >= 0) { ::dup2(fd, STDERR_FILENO); ::close(fd); }
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
            log << "[hamon] Parse error: " << e.what() << '\n';
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
                const std::string d = parser.expand_vars(ph.description.empty() ? ph.name : ph.description);
                // For each target node, create a run item (if no targets, -1)
                if (ph.target_nodes.empty()) {
                    RunItem it;
                    it.cmd = expanded;
                    it.desc = d;
                    it.node_id = -1;
                    if (expanded.find(" -c ") != string::npos || expanded.rfind(" -c", expanded.size() - 2) != string::npos) compiles.push_back(it); else others.push_back(it);
                } else {
                    for (int nid : ph.target_nodes) {
                        RunItem it;
                        it.cmd = expanded;
                        it.desc = d;
                        it.node_id = nid;
                        if (expanded.find(" -c ") != string::npos || expanded.rfind(" -c", expanded.size() - 2) != string::npos) compiles.push_back(it); else others.push_back(it);
                    }
                }
            }
        }
        if (compiles.empty() && others.empty()) {
            log << "[hamon] No tasks found in: " << hc_path << '\n';
            return false;
        }

        // Prepare logs directories and assign IDs
        std::filesystem::create_directories("stdout");
        std::filesystem::create_directories("stderr");
        int next_id = 1;
        auto assign_meta = [&](std::vector<RunItem> &v){
            for (auto &it : v) {
                it.id = next_id++;
                it.stdout_path = std::string("stdout/") + std::to_string(it.id) + ".log";
                it.stderr_path = std::string("stderr/") + std::to_string(it.id) + ".log";
            }
        };
        assign_meta(compiles);
        assign_meta(others);

        // Prepare overall progress
        const size_t total_tasks = compiles.size() + others.size();
        size_t done_tasks = 0;
        if (total_tasks > 0) {
            log << "[Make] Total tasks: " << total_tasks << " (compile: " << compiles.size() << ", other: " << others.size() << ")\n";
            print_progress(done_tasks, total_tasks, log);
        }

        // Run compile commands in parallel with affinity
        if (!compiles.empty()) {
            log << "\n[Make] Parallel compile jobs: " << compiles.size() << '\n';
            vector<future<int>> futures;
            futures.reserve(compiles.size());
            for (size_t i = 0; i < compiles.size(); ++i) {
                const auto item = compiles[i];
                int cpu = infer_logical_cpu(nodes_by_id, item.node_id);
                if (cpu >= 0) log << "[Make][C][" << (i + 1) << "/" << compiles.size() << "] pin cpu=" << cpu << " nid=" << item.node_id << " $ " << item.cmd << "\n  -> logs: " << item.stdout_path << ", " << item.stderr_path << '\n';
                else log << "[Make][C][" << (i + 1) << "/" << compiles.size() << "] $ " << item.cmd << "\n  -> logs: " << item.stdout_path << ", " << item.stderr_path << '\n';
                auto nodes_copy = nodes_by_id; // capture by value for async task
                futures.emplace_back(std::async(std::launch::async, [item, nodes_copy]() {
                    return run_with_affinity(item.cmd, nodes_copy, item.node_id, cout, item.stdout_path, item.stderr_path);
                }));
            }
            // Wait and check
            for (size_t i = 0; i < futures.size(); ++i) {
                int rc = futures[i].get();
                if (rc != 0) {
                    log << "[Make] Compile command failed with code " << rc << '\n';
                    return false;
                }
                ++done_tasks;
                print_progress(done_tasks, total_tasks, log, compiles[i].desc, compiles[i].cmd, compiles[i].stdout_path, compiles[i].stderr_path);
            }
        }

        // Run the remaining tasks sequentially (e.g., linking), with affinity for the first target node (if any)
        for (size_t i = 0; i < others.size(); ++i) {
            const auto &item = others[i];
            int cpu = infer_logical_cpu(nodes_by_id, item.node_id);
            if (cpu >= 0) log << "\n[Make][S][" << (i + 1) << "/" << others.size() << "] pin cpu=" << cpu << " nid=" << item.node_id << " $ " << item.cmd << "\n  -> logs: " << item.stdout_path << ", " << item.stderr_path << '\n';
            else log << "\n[Make][S][" << (i + 1) << "/" << others.size() << "] $ " << item.cmd << "\n  -> logs: " << item.stdout_path << ", " << item.stderr_path << '\n';
            int rc = run_with_affinity(item.cmd, nodes_by_id, item.node_id, log, item.stdout_path, item.stderr_path);
            if (rc != 0) {
                log << "[Make] Command failed with code " << rc << ": " << item.cmd << '\n';
                return false;
            }
            ++done_tasks;
            print_progress(done_tasks, total_tasks, log, item.desc, item.cmd, item.stdout_path, item.stderr_path);
        }

        log << "\n[Make] All tasks completed successfully." << '\n';
        return true;
    }

} // namespace Dualys
