#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <boost/mpi.hpp>
#include "micro_iterations_logger.h"

namespace fs = std::filesystem;
namespace mpi = boost::mpi;

static mpi::environment* g_mpi_env = nullptr;
static mpi::communicator* g_world = nullptr;

class MicroIterationsLogTest : public ::testing::Test
{
protected:
    fs::path tmp_dir;

    void SetUp() override
    {
        tmp_dir = fs::path(__FILE__).parent_path().parent_path() / "tmp";
        // Clean and recreate the tmp directory for each test
        if (fs::exists(tmp_dir))
        {
            fs::remove_all(tmp_dir);
        }
        fs::create_directories(tmp_dir);
    }

    void TearDown() override
    {
        // Clean up after test
        if (fs::exists(tmp_dir))
        {
            fs::remove_all(tmp_dir);
        }
    }
};

TEST_F(MicroIterationsLogTest, ConstructorCreatesLogFile)
{
    {
        MicroIterationsLog logger(tmp_dir, false, g_world, 0);
    } // destructor flushes and joins worker thread

    std::string expected_log = "micro_iterations_proc_" + std::to_string(g_world->rank()) + ".log";
    fs::path log_path = tmp_dir / expected_log;

    ASSERT_TRUE(fs::exists(log_path)) << "Log file was not created: " << log_path;

    std::ifstream in(log_path);
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("MICRO ITERS config"), std::string::npos);
    EXPECT_NE(content.find("warm_start=0"), std::string::npos);
}

TEST_F(MicroIterationsLogTest, ConstructorWarmStartFlag)
{
    {
        MicroIterationsLog logger(tmp_dir, true, g_world, 0);
    }

    std::string expected_log = "micro_iterations_proc_" + std::to_string(g_world->rank()) + ".log";
    fs::path log_path = tmp_dir / expected_log;

    std::ifstream in(log_path);
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("warm_start=1"), std::string::npos);
}

TEST_F(MicroIterationsLogTest, ConstructorCreatesAddedConstraintsDir)
{
    {
        MicroIterationsLog logger(tmp_dir, false, g_world, 0);
    }

    if (g_world->rank() == 0)
    {
        EXPECT_TRUE(fs::exists(tmp_dir / "added_constraints"));
        EXPECT_TRUE(fs::is_directory(tmp_dir / "added_constraints"));
    }
}

TEST_F(MicroIterationsLogTest, AddMasterIterationLog)
{
    {
        MicroIterationsLog logger(tmp_dir, false, g_world, 0);
        logger.AddMasterIterationLog(1, "42.5");
        logger.AddMasterIterationLog(2, "13.0");
    }

    std::string expected_log = "micro_iterations_proc_" + std::to_string(g_world->rank()) + ".log";
    fs::path log_path = tmp_dir / expected_log;

    std::ifstream in(log_path);
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("master iteration 1 elapsed time : 42.5 ms"), std::string::npos);
    EXPECT_NE(content.find("master iteration 2 elapsed time : 13.0 ms"), std::string::npos);
}

TEST_F(MicroIterationsLogTest, AddMicroIterationLog)
{
    {
        MicroIterationsLog logger(tmp_dir, false, g_world, 0);
        logger.AddMicroIterionLog("area1.mps", 0, 1, "10.5", {"key1", "key2"});
        logger.AddMicroIterionLog("area2.mps", 1, 1, "20.3", {});
    }

    std::string expected_log = "micro_iterations_proc_" + std::to_string(g_world->rank()) + ".log";
    fs::path log_path = tmp_dir / expected_log;

    std::ifstream in(log_path);
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("area1.mps ; 1 ; 0 ; 10.5"), std::string::npos);
    EXPECT_NE(content.find("area2.mps ; 1 ; 1 ; 20.3"), std::string::npos);
}

TEST_F(MicroIterationsLogTest, AddMicroIterCount)
{
    {
        MicroIterationsLog logger(tmp_dir, false, g_world, 0);
        logger.AddMicroIterCount("area1.mps", 3);
    }

    std::string expected_log = "micro_iterations_proc_" + std::to_string(g_world->rank()) + ".log";
    fs::path log_path = tmp_dir / expected_log;

    std::ifstream in(log_path);
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("area1.mps ; 3"), std::string::npos);
}

TEST_F(MicroIterationsLogTest, DumpAddedConstraintsCreatesFile)
{
    {
        MicroIterationsLog logger(tmp_dir, false, g_world, 0);
        std::vector<std::string> constraints = {"constraint_A", "constraint_B", "constraint_C"};
        logger.DumpAddedConstraints(0, 1, "path/to/area1.mps", constraints, 0, constraints.size());
    }

    if (g_world->rank() == 0)
    {
        fs::path expected_file = tmp_dir / "added_constraints"
                                         / "micro_iter_0_master_1_area1.txt";
        ASSERT_TRUE(fs::exists(expected_file)) << "Constraint file not created: " << expected_file;

        std::ifstream in(expected_file);
        std::string content((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());

        EXPECT_NE(content.find("constraint_A"), std::string::npos);
        EXPECT_NE(content.find("constraint_B"), std::string::npos);
        EXPECT_NE(content.find("constraint_C"), std::string::npos);
    }
}

TEST_F(MicroIterationsLogTest, DumpAddedConstraintsMultipleMicroIterations)
{
    {
        MicroIterationsLog logger(tmp_dir, false, g_world, 0);
        logger.DumpAddedConstraints(0, 1, "sub1.mps", {"c1", "c2"}, 0, 2);
        logger.DumpAddedConstraints(1, 1, "sub1.mps", {"c3"}, 0, 1);
        logger.DumpAddedConstraints(0, 2, "sub1.mps", {"c4", "c5"}, 0, 2);
    }

    if (g_world->rank() == 0)
    {
        EXPECT_TRUE(fs::exists(tmp_dir / "added_constraints" / "micro_iter_0_master_1_sub1.txt"));
        EXPECT_TRUE(fs::exists(tmp_dir / "added_constraints" / "micro_iter_1_master_1_sub1.txt"));
        EXPECT_TRUE(fs::exists(tmp_dir / "added_constraints" / "micro_iter_0_master_2_sub1.txt"));

        // Verify content of the second file
        std::ifstream in(tmp_dir / "added_constraints" / "micro_iter_1_master_1_sub1.txt");
        std::string content((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
        EXPECT_NE(content.find("c3"), std::string::npos);
        EXPECT_EQ(content.find("c1"), std::string::npos); // c1 should NOT be in this file
    }
}

int main(int argc, char** argv)
{
    g_mpi_env = new mpi::environment(argc, argv);
    g_world = new mpi::communicator();

    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    delete g_world;
    delete g_mpi_env;
    return result;
}
