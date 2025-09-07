#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <cmath>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <thread>

namespace Dualys {
    /**
     * @brief A vertex of the Hamon (3D) hypercube topology.
     *
     * Each node is identified by an integer ID and holds the IDs of its
     * adjacent nodes (its 1-bit-different neighbors in the hypercube).
     */
    struct Node {
        /// Zero-based identifier of the node. Intended to match its index in the container.
        int id;
        /// IDs of directly connected neighbors in the hypercube.
        std::vector<int> neighbors;
    };

    // Structure pour les données de configuration d'un nœud, lues depuis le YAML
    struct NodeConfig {
        int id;
        std::string role;
        std::string ip_address;
        int port;
    };

    /**
     * @brief Lightweight model of a 3-dimensional hypercube (8 nodes, degree 3).
     *
     * This class builds a fixed-size graph where nodes represent vertices of a 3D
     * hypercube and edges connect nodes whose IDs differ by exactly one bit.
     *
     * Design notes:
     * - The graph is initialized in the constructor and is immutable afterward.
     * - Nodes are stored in a contiguous container and addressed by their ID.
     * - The expected ID domain is [0, 7] for the 3D hypercube.
     */
    class HamonCube {
    public:
        /**
         * @brief Construct the 3D hypercube and initialize its topology.
         *
         * Initializes 8 nodes with their respective 3 neighbors (degree = 3).
         * Complexity: O(N · d) where N = 8 and d = 3 for the 3D hypercube.
         */
        explicit HamonCube(int num_nodes);

        /**
         * @brief Get the total number of nodes in the 3D hypercube.
         *
         * The number of nodes corresponds to the predefined size of the hypercube
         * (e.g., 8 nodes for a 3-dimensional hypercube).
         *
         * @return int The total number of nodes in the hypercube.
         */
        [[nodiscard]] int getNodeCount() const;

        /**
         * @brief Get the dimensionality of the hypercube.
         *
         * The dimension represents the number of binary bits used to define
         * the hypercube. For example, a 3D hypercube has a dimension of 3.
         *
         * @return int The dimensionality of the hypercube.
         */
        [[nodiscard]] int getDimension() const;

        /**
         * @brief Retrieve all nodes of the 3D hypercube.
         *
         * Provides access to the internal collection of nodes representing
         * the vertices of the hypercube.
         *
         * @return const std::vector<Node>& A constant reference to the container holding all nodes.
         *         Each node encapsulates its ID and its adjacent neighbors.
         */
        [[nodiscard]] const std::vector<Node> &getNodes() const;

        /**
         * @brief Access a node by its ID.
         *
         * @param id The identifier of the node to access. Expected range: [0, 7].
         * @return const Node& A const reference to the requested node.
         *
         * Preconditions:
         * - id must be a valid node identifier (within the constructed range).
         *
         * Remarks:
         * - Behavior for out-of-range IDs is unspecified and depends on the implementation.
         *   Callers should validate the ID before calling this function.
         */
        [[nodiscard]] const Node &getNode(size_t id) const;
    private:
        /**
         * @brief Build the hypercube adjacency (neighbors for each node).
         *
         * Populates the node's container with 8 nodes and assigns neighbors such that
         * each pair of connected nodes differs by exactly one bit in their ID.
         */
        void initializeTopology();
        int node_count;
        int dimension;
        /**
         * @brief Container holding all nodes of the 3D hypercube.
         *
         * This vector stores Node instances representing the vertices of the
         * 3-dimensional hypercube. Each node includes its ID and the IDs of its
         * adjacent nodes. The container is populated during topology initialization
         * and remains immutable afterward.
         */
        std::vector<Node> nodes;
    };
}
