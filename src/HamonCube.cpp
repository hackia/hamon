#include "../include/HamonCube.hpp"
#include <stdexcept>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <thread>
#include <cmath>
using namespace dualys;

void HamonCube::initializeTopology() {
    for (size_t i = 0; i < static_cast<size_t>(node_count); ++i) {
        nodes[i].id = static_cast<int>(i);
        for (int d = 0; d < dimension; ++d) {
            nodes[i].neighbors.push_back(static_cast<int>(i ^ 1 << d));
        }
    }
}

int HamonCube::getDimension() const {
    return dimension;
}

const std::vector<Node> &HamonCube::getNodes() const {
    return nodes;
}

const Node &HamonCube::getNode(const size_t id) const {
    if (id >= nodes.size()) {
        throw std::out_of_range(_("Node ID is out of range."));
    }
    return nodes[id];
}

HamonCube::HamonCube(const int num_nodes) : node_count(num_nodes) {
    if (num_nodes <= 0 || (num_nodes & (num_nodes - 1)) != 0) {
        throw std::invalid_argument(_("Number of nodes must be a power of 2."));
    }
    this->dimension = static_cast<int>(log2(num_nodes));
    nodes.resize(static_cast<size_t>(node_count));
    initializeTopology();
}

int HamonCube::getNodeCount() const {
    return node_count;
}
