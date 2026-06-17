#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "gridModelisation.h"
#include "test_utils/utils.h"

namespace fs = std::filesystem;

class PluginTest : public ::testing::Test
{
protected:
    fs::path test_utils_dir;

    void SetUp() override
    {
        test_utils_dir = fs::path(__FILE__).parent_path() / "test_utils";
    }
};

TEST_F(PluginTest, ComputeFactorsAndReturnConstraints)
{
    std::string data_path = (test_utils_dir / "cpp_structures").string();
    Plugin plugin(data_path, nullptr);

    // Read z_dict: no sub_name header, all lines are "key : value"
    auto [z_sub_name, z_values] = read_sub_solution(test_utils_dir / "z_dict.txt");

    // z_dict.txt has no header line, so the first data line is parsed as sub_name.
    // Re-parse it as a key-value pair and include it.
    std::map<std::string, int> z_dict;
    {
        auto sep = z_sub_name.find(" : ");
        if (sep != std::string::npos)
        {
            z_dict[z_sub_name.substr(0, sep)] = static_cast<int>(std::stod(z_sub_name.substr(sep + 3)));
        }
    }
    for (const auto& [key, val] : z_values)
    {
        z_dict[key] = static_cast<int>(val);
    }

    ASSERT_FALSE(z_dict.empty()) << "z_dict should not be empty";

    // Compute sensitivity factors from investment decisions
    plugin.compute_factors_for_micro_iterations(z_dict);

    // Read F_N_values from constraints_for_micro_iterations.txt
    auto [sub_name, F_N_values] = read_sub_solution(test_utils_dir / "constraints_for_micro_iterations.txt");

    ASSERT_FALSE(sub_name.empty()) << "sub_name should not be empty";
    ASSERT_FALSE(F_N_values.empty()) << "F_N_values should not be empty";

    // Compute constraints for the sub-problem
    auto constraints = plugin.return_constraints_for_micro_iteration(sub_name, F_N_values);

    EXPECT_FALSE(constraints.empty()) << "Expected at least one violated constraint";
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
