#include "HamonCube.h"
#include <stdexcept>

using namespace Dualys;

void HamonCube::initializeTopology() {
    for (int i = 0; i < node_count; ++i) {
        nodes[i].id = i;
        for (int d = 0; d < dimension; ++d) {
            nodes[i].neighbors.push_back(i ^ 1 << d);
        }
    }
}

int HamonCube::getDimension() const {
    return dimension;
}

const std::vector<Node> &HamonCube::getNodes() const {
    return nodes;
}

const Node &HamonCube::getNode(const int id) const {
    if (id < 0 || id >= nodes.size()) {
        throw std::out_of_range("Node ID is out of range.");
    }
    return nodes[id];
}

HamonCube::HamonCube(const int num_nodes) : node_count(num_nodes) {
    if (num_nodes <= 0 || (num_nodes & num_nodes - 1) != 0) {
        throw std::invalid_argument("Number of nodes must be a power of 2.");
    }
    this->dimension = static_cast<int>(log2(num_nodes));
    nodes.resize(node_count);
    initializeTopology();
}

int HamonCube::getNodeCount() const {
    return node_count;
}
