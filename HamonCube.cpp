#include "HamonCube.h"
#include <stdexcept>

using namespace Dualys;

HamonCube::HamonCube() {
    nodes.resize(8);
    initializeTopology();
}

void HamonCube::initializeTopology() {
    for (int i = 0; i < 8; ++i) {
        nodes[i].id = i;
        nodes[i].neighbors.push_back(i ^ 1);
        nodes[i].neighbors.push_back(i ^ 2);
        nodes[i].neighbors.push_back(i ^ 4);
    }
}

const Node &HamonCube::getNode(int id) const {
    if (id < 0 || id >= nodes.size()) {
        throw std::out_of_range("Node ID is out of range.");
    }
    return nodes[id];
}
