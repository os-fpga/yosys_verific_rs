#include <iostream>
#include <string>
#include <map>
#include <unordered_set>

struct primitives_data {
    std::map<std::string, std::unordered_set<std::string>> io_primitives =
    {
        {"genesis3", {"BOOT_CLOCK","CLK_BUF","I_BUF","I_BUF_DS","I_DDR","I_DELAY","I_SERDES","O_BUF","O_BUFT","O_BUFT_DS","O_BUF_DS","O_DDR","O_DELAY","O_SERDES","O_SERDES_CLK","PLL"}}};
    bool contains_io_prem = false;

    // Function to get the primitive names for a specific cell library
    std::unordered_set<std::string> get_primitives(const std::string &lib) {
        std::unordered_set<std::string> primitive_names;
        auto it = io_primitives.find(lib);
        if (it != io_primitives.end()) {
            primitive_names = it->second;
        }
        return primitive_names;
    }
};
