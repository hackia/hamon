#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>
#include <string>
#include <vector>

#include "../Hamon.h"

using namespace Dualys;

namespace {
    struct TmpFile {
        std::string path;

        explicit TmpFile(std::string p) : path(std::move(p)) {
        }

        ~TmpFile() { std::remove(path.c_str()); }
    };
} // namespace

TEST(Hamon, ParseMinimalHypercube4) {
    HamonParser p;
    std::string dsl =
            "@use 4\n"
            "@auto 127.0.0.1:9000\n";

    TmpFile tf("scenario1.hc");
    {
        std::ofstream o(tf.path);
        o << dsl;
    }
    p.parse_file(tf.path);
    p.finalize();

    EXPECT_EQ(p.use_nodes(), 4);
    EXPECT_EQ(p.get_topology(), "hypercube");
    EXPECT_EQ(p.dim(), 2); // 2^2 == 4

    auto nodes = p.materialize_nodes();
    ASSERT_EQ(static_cast<int>(nodes.size()), 4);

    // Node 0 -> coordinator, ports 9000 + id
    EXPECT_EQ(nodes[0].id, 0);
    EXPECT_EQ(nodes[0].role, "coordinator");
    EXPECT_EQ(nodes[0].host, "127.0.0.1");
    EXPECT_EQ(nodes[0].port, 9000);

    // Node 3
    EXPECT_EQ(nodes[3].id, 3);
    EXPECT_EQ(nodes[3].role, "worker");
    EXPECT_EQ(nodes[3].host, "127.0.0.1");
    EXPECT_EQ(nodes[3].port, 9003);

    // Hypercube neighbors (2D): 0 <-> (1,2), 3 <-> (1,2)
    EXPECT_EQ(static_cast<int>(nodes[0].neighbors.size()), 2);
    EXPECT_TRUE(
        (nodes[0].neighbors[0] == 1 && nodes[0].neighbors[1] == 2) ||
        (nodes[0].neighbors[0] == 2 && nodes[0].neighbors[1] == 1)
    );

    EXPECT_EQ(static_cast<int>(nodes[3].neighbors.size()), 2);
    EXPECT_TRUE(
        (nodes[3].neighbors[0] == 1 && nodes[3].neighbors[1] == 2) ||
        (nodes[3].neighbors[0] == 2 && nodes[3].neighbors[1] == 1)
    );
}

TEST(Hamon, ExplicitDimOk) {
    HamonParser p;
    std::string dsl =
            "@use 8\n"
            "@dim 3\n" // 2^3 = 8, ok
            "@autoprefix 10.0.0.1:8000\n";
    TmpFile tf("scenario2.hc");
    {
        std::ofstream o(tf.path);
        o << dsl;
    }
    p.parse_file(tf.path);
    p.finalize();

    EXPECT_EQ(p.use_nodes(), 8);
    EXPECT_EQ(p.dim(), 3);
    auto nodes = p.materialize_nodes();
    ASSERT_EQ(static_cast<int>(nodes.size()), 8);
    EXPECT_EQ(nodes[7].host, "10.0.0.1");
    EXPECT_EQ(nodes[7].port, 8007);
}

TEST(Hamon, ExplicitDimMismatchThrows) {
    HamonParser p;
    std::string dsl =
            "@use 6\n"
            "@dim 3\n"; // 2^3=8 != 6 -> incohérent pour hypercube
    TmpFile tf("scenario3.hc");
    {
        std::ofstream o(tf.path);
        o << dsl;
    }
    p.parse_file(tf.path);
    EXPECT_THROW(p.finalize(), std::runtime_error);
}

TEST(Hamon, NeighborsOverride) {
    HamonParser p;
    std::string dsl =
            "@use 4\n"
            "@auto 127.0.0.1:7000\n"
            "@node 1\n"
            "@neighbors [0,3]\n";
    TmpFile tf("scenario4.hc");
    {
        std::ofstream o(tf.path);
        o << dsl;
    }
    p.parse_file(tf.path);
    p.finalize();
    auto nodes = p.materialize_nodes();



    std::cerr << "Node 1 neighbors:";
    for (int v: nodes[1].neighbors) std::cerr << " " << v;
    std::cerr << "\n";




    ASSERT_EQ(static_cast<int>(nodes.size()), 4);

    // on vérifie le contenu sans supposer l'ordre
    std::vector<int> got = nodes[1].neighbors;
    std::sort(got.begin(), got.end());
    std::vector<int> expected{0, 3};
    std::sort(expected.begin(), expected.end());


    std::cerr << "Node 1 neighbors:";
    for (int v: nodes[1].neighbors) std::cerr << " " << v;
    std::cerr << "\n";



    ASSERT_EQ(got.size(), expected.size()) << "Neighbors count differs";
    for (size_t i = 0; i < expected.size(); ++i) {

        std::cerr << "Node 1 neighbors:";
        for (int v: nodes[1].neighbors) std::cerr << " " << v;
        std::cerr << "\n";

        std::cerr << "Node 1 neighbors:";
        for (int v: nodes[1].neighbors) std::cerr << " " << v;
        std::cerr << "\n";

        EXPECT_EQ(got[i], expected[i]) << "Mismatch at index " << i;
    }
}


TEST(Hamon, CpuParsing) {
    HamonParser p;
    std::string dsl =
            "@use 2\n"
            "@node 0\n"
            "@cpu numa=1 core=12\n"
            "@node 1\n"
            "@cpu core=3 numa=0\n"; // ordre inversé accepté
    TmpFile tf("scenario5.hc");
    {
        std::ofstream o(tf.path);
        o << dsl;
    }
    p.parse_file(tf.path);
    p.finalize();
    auto nodes = p.materialize_nodes();
    EXPECT_EQ(nodes[0].numa, 1);
    EXPECT_EQ(nodes[0].core, 12);
    EXPECT_EQ(nodes[1].numa, 0);
    EXPECT_EQ(nodes[1].core, 3);
}

TEST(Hamon, IpParsingExplicit) {
    HamonParser p;
    std::string dsl =
            "@use 2\n"
            "@node 1\n"
            "@ip 192.168.10.5:5555\n";
    TmpFile tf("scenario6.hc");
    {
        std::ofstream o(tf.path);
        o << dsl;
    }
    p.parse_file(tf.path);
    p.finalize();
    auto nodes = p.materialize_nodes();
    EXPECT_EQ(nodes[1].host, "192.168.10.5");
    EXPECT_EQ(nodes[1].port, 5555);
    // node 0: defaults
    EXPECT_EQ(nodes[0].host, "127.0.0.1");
    EXPECT_EQ(nodes[0].port, 8000);
}

TEST(Hamon, MissingUseThrows) {
    HamonParser p;
    std::string dsl =
            "@auto 127.0.0.1:9000\n"
            "@node 0\n"
            "@role coordinator\n";
    TmpFile tf("scenario7.hc");
    {
        std::ofstream o(tf.path);
        o << dsl;
    }
    p.parse_file(tf.path);
    EXPECT_THROW(p.finalize(), std::runtime_error);
}

TEST(Hamon, BadDirectiveThrowsEarly) {
    HamonParser p;
    std::string dsl =
            "@use 4\n"
            "@dim foo   // <- erreur\n";
    TmpFile tf("scenario8.hc");
    {
        std::ofstream o(tf.path);
        o << dsl;
    }
    // L'erreur se produit à la lecture (parse_line) avant finalize()
    EXPECT_THROW(p.parse_file(tf.path), std::runtime_error);
}
