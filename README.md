# Project Hamon

[![C++ CI](https://github.com/hackialand/hamon/actions/workflows/ci.yml/badge.svg)](https://github.com/hackialand/hamon/actions/workflows/ci.yml)

Hamon is an experimental C++ implementation of a distributed computing framework, designed to be orchestrated on a single host machine. The project simulates a mini-cluster by launching multiple logical nodes that communicate via network sockets. The underlying architecture is inspired by a **hypercube topology**, nicknamed the "Hamon Cube," to optimize communication patterns and task parallelization.

The implemented use case is a **MapReduce** task: a distributed word count. A "coordinator" node distributes chunks of a text file to "worker" nodes (the **Map** phase). The partial results are then efficiently aggregated by following the edges of the hypercube (the **Reduce** phase).

## Architecture: The "Hamon Cube"

The project relies on a hypercube communication topology, where each process (node) represents a vertex of the cube. This structure provides several advantages:

* **Dynamic Scalability**: The number of nodes is determined dynamically at runtime based on the number of available CPU cores, selecting the largest possible power of two.
* **Efficient Communication**: Nodes communicate directly with their neighbors (nodes whose binary IDs differ by only a single bit). This design enables broadcast and reduction algorithms (like summing results) to complete in logarithmic time (`log(N)` steps).
* **Decentralization**: The reduction phase does not require a central master to collect results from all workers. Instead, nodes exchange their partial results in successive pairings, distributing the communication load across the entire network.

The main program (`main.cpp`) acts as an **orchestrator**: it determines the number of nodes to use, generates their network configuration (IP/port), and launches each node as an independent `fork()` process.

## Project Components

* `main.cpp`: The orchestrator that configures and launches the nodes.
* `HamonCube.h / .cpp`: Defines the hypercube topology, including the logic for calculating neighbors for each node.
* `HamonNode.h / .cpp`: Implements the logic for an individual node, including its TCP server, task management (Map), and the reduction phase (Reduce).
* `input.txt`: An example input file containing the text to be analyzed.
* `CMakeLists.txt`: The build script for the project.

## How to Build and Run

The project uses `CMake` for compilation.

**Prerequisites:**

* A C++17 compiler or newer (the project is set to C++26 but should be compatible).
* CMake (version 4.0 or higher).
* A build toolchain (like `make` or `ninja`).

**Build Steps:**

```bash
# 1. Create a build directory
mkdir build && cd build

# 2. Configure the project with CMake
cmake ..

# 3. Compile the project
make
```

This will create an executable named `hamon` in the `build` directory.

**Execution:**

To run the simulation, simply execute the `hamon` binary from the project's root directory (this is necessary so it can find `input.txt`).

```bash
# Make sure you are in the project's root directory
./build/hamon
```

The orchestrator will detect your machine's core count, launch the appropriate number of nodes, and display the logs from each node in the console. The final word count result will be printed by Node 0.

## License

This project is licensed under the **GNU Affero General Public License v3.0**. For more details, please see the `LICENSE` file.