#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <vector>
#include <string>
#include <boost/tokenizer.hpp>

struct CSVData
{
    std::vector<std::pair<std::string, std::string>> rows;
    std::map<std::string, std::string> map;
};

CSVData read_csv_to_map(const std::filesystem::path& csv_path)
{
    std::ifstream csv_file(csv_path);
    CSVData data;
    
    if (!csv_file.is_open())
    {
        std::cerr << "Error: Unable to open file: " << csv_path << std::endl;
        exit(EXIT_FAILURE);
    }
    
    std::string row;
    typedef boost::tokenizer<boost::escaped_list_separator<char>> Tokenizer;
    
    while (std::getline(csv_file, row))
    {
        Tokenizer tok(row);
        std::vector<std::string> tokens(tok.begin(), tok.end());
        
        if (tokens.size() >= 2)
        {
            data.rows.push_back({tokens[0], tokens[1]});
            data.map[tokens[0]] = tokens[1];
        }
    }
    
    csv_file.close();
    return data;
}

void write_second_column_to_file(const CSVData& data, 
                                  const std::filesystem::path& output_path)
{
    std::ofstream output_file(output_path);
    
    if (!output_file.is_open())
    {
        std::cerr << "Error: Unable to create output file: " << output_path << std::endl;
        exit(EXIT_FAILURE);
    }
    
    for (const auto& [col1, col2] : data.rows)
    {
        output_file << col2 << std::endl;
    }
    
    output_file.close();
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path_to_csv_file>" << std::endl;
        std::cerr << "Example: " << argv[0] << " input.csv" << std::endl;
        return EXIT_FAILURE;
    }
    
    std::filesystem::path csv_path(argv[1]);
    
    if (!std::filesystem::exists(csv_path))
    {
        std::cerr << "Error: File does not exist: " << csv_path << std::endl;
        return EXIT_FAILURE;
    }
    
    // Read CSV into map and vector (preserving order)
    auto csv_data = read_csv_to_map(csv_path);
    
    // Create output filename
    std::filesystem::path output_path = csv_path.parent_path() / "variable_names.txt";
    
    // Write second column to output file
    write_second_column_to_file(csv_data, output_path);
    
    return EXIT_SUCCESS;
}
