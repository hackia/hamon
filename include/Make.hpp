#pragma once
#include <libintl.h>
#include <iosfwd>
#include <string>

#ifndef I18N_GETTEXT_DEFINED
#define _(String) gettext(String)
#define I18N_GETTEXT_DEFINED
#endif
namespace dualys {
    // A very small helper to "build hamon by hamon" using a .hc script.
    // It scans @phase lines and executes the task="..." commands in order.
    // Notes:
    // - Uses HamonParser to pre-parse the file so that variable expansion ${VAR}
    //   defined via @let works before executing tasks.
    // - Job/topology directives are not orchestrated here yet; this is a local runner.
    // - Designed to be easily extended to support language extensions later.
    class Make {
    public:
        // Parse the given .hc file and execute its task commands sequentially.
        // Returns true on success (all commands returned exit code 0), false otherwise.
        static bool build_from_hc(const std::string &hc_path, std::ostream &log);
    };
} // namespace dualys
