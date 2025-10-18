# Project Hamon

[![C++ CI](https://github.com/hackialand/hamon/actions/workflows/ci.yml/badge.svg)](https://github.com/hackialand/hamon/actions/workflows/ci.yml)

Hamon is an experimental C++ implementation of a distributed computing framework, designed to be orchestrated on a single host machine. The project simulates a mini-cluster by launching multiple logical nodes that communicate via network sockets. The underlying architecture is inspired by a hypercube topology, nicknamed the "Hamon Cube," to optimize communication patterns and task parallelization.

The implemented use case is a MapReduce task: a distributed word count. A "coordinator" node distributes chunks of a text file to "worker" nodes (the Map phase). The partial results are then efficiently aggregated by following the edges of the hypercube (the Reduce phase).

## Architecture: The "Hamon Cube"

The project relies on a hypercube communication topology, where each process (node) represents a vertex of the cube. This structure provides several advantages:

- Dynamic Scalability: The number of nodes is determined dynamically at runtime based on the number of available CPU cores, selecting the largest possible power of two.
- Efficient Communication: Nodes communicate directly with their neighbors (nodes whose binary IDs differ by only a single bit). This design enables broadcast and reduction algorithms (like summing results) to complete in logarithmic time (log(N) steps).
- Decentralization: The reduction phase does not require a central master to collect results from all workers. Instead, nodes exchange their partial results in successive pairings, distributing the communication load across the entire network.

The main program (`main.cpp`) acts as an orchestrator: it determines the number of nodes to use, generates their network configuration (IP/port), and launches each node as an independent `fork()` process.

## Project Components

- `main.cpp`: The orchestrator that configures and launches the nodes.
- `HamonCube.hpp / HamonCube.cpp`: Hypercube topology and neighbor calculation.
- `HamonNode.hpp / HamonNode.cpp`: Single node logic, TCP server, Map/Reduce phases.
- `Hamon.hpp / Hamon.cpp`: Shared types and parser entry points for the `.hc` config.
- `hamon.hc`: Sample cluster configuration using the Hamon DSL.
- `help/Hamon.md`: Detailed DSL reference and design notes.
- `input.txt`: Example input for the word-count demo.
- `CMakeLists.txt`: Project build script.

## Build and Run (CLion profile quickstart)

If you are in a CLion workspace with an active CMake profile, you can build and run using the existing profile without creating new directories. For the Debug profile provided in this repository:

```bash
# Build the main executable target
cmake --build cmake-build-debug --target hamon

# Run the executable (from repo root)
./cmake-build-debug/bin/hamon              # auto-detects cores and generates a config
./cmake-build-debug/bin/hamon hamon.hc     # or: run using the provided .hc config
```

Run tests:

```bash
cmake --build cmake-build-debug --target hamon_tests && ./cmake-build-debug/bin/hamon_tests
```

## Generic CMake build (alternative)

If you are not using the provided CLion profile, a standard out-of-source CMake build also works:

```bash
mkdir -p build && cd build
cmake ..
cmake --build . --target hamon
./hamon
```

## Usage notes

- Passing a path to a `.hc` file as the first argument makes the orchestrator load the full cluster configuration from that file. See `hamon.hc` for a safe example and `help/Hamon.md` for the full DSL.
- When run without arguments, the orchestrator picks the largest power-of-two node count based on detected hardware cores and binds nodes to 127.0.0.1 ports starting at 8000.

## License

This project is licensed under the GNU Affero General Public License v3.0. For more details, please see the `LICENSE` file.