#include <gtest/gtest.h>
#include "../include/HamonNode.hpp"

using namespace dualys;

TEST(HamonNodeLogicTest, Serialization)
{
    // ARRANGE
    WordCountMap counts;
    counts["hello"] = 2;
    counts["world"] = 1;

    // ACT
    const std::string serialized = HamonNode::serialize_map(counts);

    // ASSERT
    // Le résultat peut être dans n'importe quel ordre, donc on vérifie les deux cas
    const bool case1 = serialized == "hello:2,world:1,";
    const bool case2 = serialized == "world:1,hello:2,";
    EXPECT_TRUE(case1 || case2);
}

TEST(HamonNodeLogicTest, DeserializationAndMerge)
{
    // ARRANGE
    dualys::WordCountMap counts;
    counts["existing"] = 5;
    counts["another"] = 3;
    const std::string data_to_merge = "new:10,existing:2,";

    // ACT
    HamonNode::deserialize_and_merge_map(data_to_merge, counts);
    // ASSERT
    EXPECT_EQ(counts["existing"], 7); // 5 + 2
    EXPECT_EQ(counts["another"], 3);  // N'a pas changé
    EXPECT_EQ(counts["new"], 10);     // A été ajouté
    EXPECT_EQ(counts.size(), 3);
}
