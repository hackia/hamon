#include <gtest/gtest.h>
#include "../HamonCube.h"

using namespace Dualys;

TEST(HamonCubeTest, Handles8NodeCube) {
    constexpr int node_count = 8;
    const HamonCube cube(node_count);

    ASSERT_EQ(cube.getNodeCount(), 8);
    ASSERT_EQ(cube.getDimension(), 3);
}

TEST(HamonCubeTest, CorrectNeighborsForNode5) {
    const HamonCube cube(8);
    const auto &[id, neighbors] = cube.getNode(5);
    ASSERT_EQ(neighbors.size(), 3);
    ASSERT_EQ(id, 5);
}

TEST(HamonCubeTest, ThrowsOnInvalidNodeCount) {
    EXPECT_THROW(Dualys::HamonCube(7), std::invalid_argument);
    EXPECT_THROW(Dualys::HamonCube(0), std::invalid_argument);
    EXPECT_THROW(Dualys::HamonCube(-8), std::invalid_argument);
}
