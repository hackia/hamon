#pragma once

#include <vector>

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
        HamonCube();

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
        const Node &getNode(int id) const;

    private:
        /**
         * @brief Build the hypercube adjacency (neighbors for each node).
         *
         * Populates the nodes container with 8 nodes and assigns neighbors such that
         * each pair of connected nodes differs by exactly one bit in their ID.
         */
        void initializeTopology();

        /// Storage for the 8 nodes of the 3D hypercube, indexed by node ID.
        std::vector<Node> nodes;
    };
}