#pragma once

#include <filesystem>
#include <fstream>
#include <map>
#include <string>

void write_sub_solution(const std::filesystem::path& filepath,
                        const std::string& sub_name,
                        const std::map<std::string, double>& values)
{
    std::ofstream out(filepath);
    if (!sub_name.empty())
    {
        out << sub_name << "\n";
    }
    for (const auto& [key, value] : values)
    {
        out << key << " : " << value << "\n";
    }
}

std::pair<std::string, std::map<std::string, double>> read_sub_solution(
    const std::filesystem::path& filepath)
{
    std::ifstream in(filepath);
    std::string sub_name;
    std::getline(in, sub_name);

    std::map<std::string, double> values;
    std::string line;
    while (std::getline(in, line))
    {
        auto sep = line.find(" : ");
        if (sep != std::string::npos)
        {
            std::string key = line.substr(0, sep);
            double val = std::stod(line.substr(sep + 3));
            values[key] = val;
        }
    }
    return {sub_name, values};
}
