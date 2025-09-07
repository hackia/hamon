#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>
#include <string>
#include <vector>
#include "../Hamon.hpp"

using namespace Dualys;

namespace
{
    struct TmpFile
    {
        std::string path;

        explicit TmpFile(std::string p) : path(std::move(p))
        {
        }

        ~TmpFile() { std::remove(path.c_str()); }
    };
} // namespace

TEST(Hamon, NeighborsOutsideNodeThrows)
{
    HamonParser p;
    std::string dsl =
        "@use 4\n"
        "@neighbors [0,3]\n"; // pas de @node ouvert
    TmpFile tf("scenario_neighbors_outside.hc");
    {
        std::ofstream o(tf.path);
        o << dsl;
    }
    EXPECT_THROW(p.parse_file(tf.path), std::runtime_error);
}

TEST(Hamon, NeighborOutOfRangeThrows)
{
    HamonParser p;
    std::string dsl =
        "@use 4\n"
        "@node 2\n"
        "@neighbors [0,99]\n";
    TmpFile tf("scenario_neighbors_oor.hc");
    {
        std::ofstream o(tf.path);
        o << dsl;
    }
    p.parse_file(tf.path);
    EXPECT_THROW(p.finalize(), std::runtime_error);
}

TEST(Hamon, NeighborSelfLoopRemovedAndDeduped)
{
    HamonParser p;
    std::string dsl =
        "@use 4\n"
        "@node 2\n"
        "@neighbors [2,1,1]\n"; // self + doublon
    TmpFile tf("scenario_neighbors_selfdup.hc");
    {
        std::ofstream o(tf.path);
        o << dsl;
    }
    p.parse_file(tf.path);
    p.finalize();
    auto nodes = p.materialize_nodes();
    std::vector<int> got = nodes[2].neighbors;
    std::ranges::sort(got);
    std::vector expected{1}; // self retiré, doublon éliminé
    EXPECT_EQ(got, expected);
}

TEST(Hamon, ParseMinimalHypercube4)
{
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
        (nodes[0].neighbors[0] == 2 && nodes[0].neighbors[1] == 1));

    EXPECT_EQ(static_cast<int>(nodes[3].neighbors.size()), 2);
    EXPECT_TRUE(
        (nodes[3].neighbors[0] == 1 && nodes[3].neighbors[1] == 2) ||
        (nodes[3].neighbors[0] == 2 && nodes[3].neighbors[1] == 1));
}

TEST(Hamon, ExplicitDimOk)
{
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

TEST(Hamon, ExplicitDimMismatchThrows)
{
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

TEST(Hamon, NeighborsOverride)
{
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
    for (int v : nodes[1].neighbors)
        std::cerr << " " << v;
    std::cerr << "\n";

    ASSERT_EQ(static_cast<int>(nodes.size()), 4);

    // on vérifie le contenu sans supposer l'ordre
    std::vector<int> got = nodes[1].neighbors;
    std::ranges::sort(got);
    std::vector expected{0, 3};
    std::ranges::sort(expected);

    std::cerr << "Node 1 neighbors:";
    for (int v : nodes[1].neighbors)
        std::cerr << " " << v;
    std::cerr << "\n";

    ASSERT_EQ(got.size(), expected.size()) << "Neighbors count differs";
    for (size_t i = 0; i < expected.size(); ++i)
    {
        std::cerr << "Node 1 neighbors:";
        for (int v : nodes[1].neighbors)
            std::cerr << " " << v;
        std::cerr << "\n";

        std::cerr << "Node 1 neighbors:";
        for (int v : nodes[1].neighbors)
            std::cerr << " " << v;
        std::cerr << "\n";

        EXPECT_EQ(got[i], expected[i]) << "Mismatch at index " << i;
    }
}

TEST(Hamon, CpuParsing)
{
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

TEST(Hamon, IpParsingExplicit)
{
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

TEST(Hamon, MissingUseThrows)
{
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

TEST(Hamon, BadDirectiveThrowsEarly)
{
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

// --- INCLUDE de base ---
TEST(Hamon, IncludeBasic)
{
    HamonParser p;

    // fichier inclus
    TmpFile inc("inc1.hc");
    {
        std::ofstream o(inc.path);
        o << "@use 2\n"
             "@auto 127.0.0.1:6000\n";
    }

    // fichier principal
    TmpFile mainf("main1.hc");
    {
        std::ofstream o(mainf.path);
        o << "@include \"" << inc.path << "\"\n";
    }

    p.parse_file(mainf.path);
    p.finalize();

    EXPECT_EQ(p.use_nodes(), 2);
    auto nodes = p.materialize_nodes();
    ASSERT_EQ(static_cast<int>(nodes.size()), 2);
    EXPECT_EQ(nodes[1].port, 6001);
}
TEST(Hamon, IncludeWithVarsAndRelative)
{
    HamonParser p;

    namespace fs = std::filesystem;
    fs::path cwd = fs::current_path();
    fs::path subdir = cwd / "sub";
    fs::create_directories(subdir);
    ASSERT_TRUE(fs::exists(subdir));

    TmpFile inc((subdir / "inc2.hc").string());
    {
        std::ofstream o(inc.path);
        o << "@use 4\n";
    }
    ASSERT_TRUE(fs::exists(inc.path));

    TmpFile mainf("main2.hc");
    {
        std::ofstream o(mainf.path);
        o << "@let DIR=" << subdir.string() << "\n"
                                               "@include \"${DIR}/inc2.hc\"\n"
                                               "@auto 10.0.0.1:7000\n";
    }

    p.parse_file(mainf.path);
    p.finalize();

    EXPECT_EQ(p.use_nodes(), 4);
    auto nodes = p.materialize_nodes();
    ASSERT_EQ((int)nodes.size(), 4);
    EXPECT_EQ(nodes[3].port, 7003);
}

TEST(Hamon, LetExpansionInAutoAndIp)
{
    HamonParser p;
    TmpFile f("let_auto.hc");
    {
        std::ofstream o(f.path);
        o << "@use 2\n"
             "@let BASE=127.0.0.1:9000\n"
             "@auto ${BASE}\n"
             "@node 1\n"
             "@let HOST=192.168.0.50\n"
             "@let PORT=5555\n"
             "@ip ${HOST}:${PORT}\n";
    }
    p.parse_file(f.path);
    p.finalize();
    auto nodes = p.materialize_nodes();
    EXPECT_EQ(nodes[0].port, 9000);
    EXPECT_EQ(nodes[1].host, "192.168.0.50");
    EXPECT_EQ(nodes[1].port, 5555);
}

// --- REQUIRE: succès simple (variable truthy) ---
TEST(Hamon, RequireTruthyVar)
{
    HamonParser p;
    TmpFile f("req1.hc");
    {
        std::ofstream o(f.path);
        o << "@let ENABLE=1\n"
             "@require ${ENABLE}\n"
             "@use 2\n";
    }
    EXPECT_NO_THROW(p.parse_file(f.path));
    EXPECT_NO_THROW(p.finalize());
}

// --- REQUIRE: échec simple ---
TEST(Hamon, RequireFail)
{
    HamonParser p;
    TmpFile f("req2.hc");
    {
        std::ofstream o(f.path);
        o << "@let ENABLE=0\n"
             "@require ${ENABLE}\n"
             "@use 2\n";
    }
    EXPECT_THROW(p.parse_file(f.path), std::runtime_error);
}

// --- REQUIRE: comparaisons ---
TEST(Hamon, RequireComparisons)
{
    HamonParser p;
    TmpFile f("req3.hc");
    {
        std::ofstream o(f.path);
        o << "@let N=4\n"
             "@require ${N} == 4\n"
             "@require ${N} >= 2\n"
             "@require ${N} <  10\n"
             "@use 4\n";
    }
    EXPECT_NO_THROW(p.parse_file(f.path));
    EXPECT_NO_THROW(p.finalize());
}

// --- INCLUDE: boucle détectée ---
TEST(Hamon, IncludeCircularDetected)
{
    HamonParser p;
    TmpFile a("a.hc");
    TmpFile b("b.hc");
    {
        std::ofstream oa(a.path);
        oa << "@include \"" << b.path << "\"\n";
    }
    {
        std::ofstream ob(b.path);
        ob << "@include \"" << a.path << "\"\n";
    }
    EXPECT_THROW(p.parse_file(a.path), std::runtime_error);
}
