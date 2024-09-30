#pragma once

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>

    struct Render
    {
        static std::string file(const std::string& filename)
        {
            std::ifstream file(filename, std::ios::binary);
            if (!file) {
                return "Error opening file";
            }
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }
    };