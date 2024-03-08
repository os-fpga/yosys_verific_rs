#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

class PeripheryExtractor {
public:
    void extractAndAppend(const std::string& directoryPath) {
        std::map<std::string, std::string> peripheryMap;

        for (const auto& entry : fs::directory_iterator(directoryPath)) {
            if (entry.path().extension() == ".yaml") {
                std::ifstream file(entry.path());
                std::string line;
                std::string potentialKey; // Store the name temporarily
                bool nameFound = false;   // Flag to indicate if name has been found

                if (!file.is_open()) {
                    std::cerr << "Failed to open file: " << entry.path() << std::endl;
                    continue;
                }

                while (getline(file, line)) {
                    // Check and store the name
                    if (line.find("name:") != std::string::npos) {
                        potentialKey = line.substr(line.find_last_of(' ') + 1);
                        nameFound = true;
                    }

                    // Check the category and add the name to the map if it's periphery
                    if (nameFound && line.find("category:") != std::string::npos) {
                        std::string categoryValue = line.substr(line.find_last_of(' ') + 1);
                        if (categoryValue == "periphery") {
                            peripheryMap[potentialKey] = categoryValue;
                            break; // Assuming one relevant name-category pair per file
                        }
                    }
                }

                file.close();
            }
        }

     std::string outputFilePath = GENERATED_HEADER_OUTPUT_DIR"rs_primitive.h";
     std::cout << "Attempting to create rs_primitive.h at: " << outputFilePath << std::endl;
    std::ofstream outFile(outputFilePath);
    
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file for writing." << std::endl;
        return;
    }
        // Writing the necessary headers to the file
    outFile << "#include <iostream>\n";
    outFile << "#include <string>\n";
    outFile << "#include <map>\n";
    outFile << "#include <unordered_set>\n\n";
    outFile << "struct primitives_data {\n";
    outFile << "    std::map<std::string, std::unordered_set<std::string>> io_primitives =\n";
    outFile << "    {\n";
    outFile << "        {\"genesis3\", {";

    if (peripheryMap.empty()) {
        outFile << "No periphery entries found." << std::endl;
    } else {
        for (auto it = peripheryMap.begin(); it != peripheryMap.end(); ++it) {
            outFile << "\"" << it->first << "\"";
            if (std::next(it) != peripheryMap.end()) {
                outFile << ",";
            }
        }
    }

    outFile << "}}};\n";
    outFile << "    bool contains_io_prem = false;\n\n";

    outFile << "    // Function to get the primitive names for a specific cell library\n";
    outFile << "    std::unordered_set<std::string> get_primitives(const std::string &lib) {\n";
    outFile << "        std::unordered_set<std::string> primitive_names;\n";
    outFile << "        auto it = io_primitives.find(lib);\n";
    outFile << "        if (it != io_primitives.end()) {\n";
    outFile << "            primitive_names = it->second;\n";
    outFile << "        }\n";
    outFile << "        return primitive_names;\n";
    outFile << "    }\n";
    outFile << "};\n";

    outFile.close();
    
    }
};

int main(int argc, char* argv[]) {


    std::string directoryPath = DIRECTORY_PATH;
    PeripheryExtractor extractor;
    extractor.extractAndAppend(directoryPath);

    return 0;
}
