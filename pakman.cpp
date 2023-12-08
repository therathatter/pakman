
#include <iostream>
#include <filesystem>
#include <string_view>
#include <algorithm>
#include <fstream>
#include <vector>
#include <string>

#pragma pack(push, 1)
struct pak_header_t {
    char magic[4];
    uint32_t initial_file_header_offset;
    uint32_t file_count;
};

struct pak_file_header_t {
    char filename[52];
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t data_size_2;
};
#pragma pack(pop)

void print_usage(char** argv) {
    std::cout << "pakman: Pack and unpack .PAK files from Garfield (2004)" << std::endl;
    std::cout << "pakman: Usage: " << argv[0] << " [pack/unpack] [input/output file]";
}

int main(int argc, char** argv) {
    if (argc != 3) {
        print_usage(argv);
        return 1;
    }

    std::string_view cmd = argv[1];
    std::string file = argv[2];

    if (cmd == "pack") {
        using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
        std::vector<pak_file_header_t> headers;

        std::string pak_name = file + ".pak";
        std::ofstream pak(pak_name, std::ios::binary);

        pak_header_t pak_header{ 0 };
        pak.write((char*)&pak_header, sizeof(pak_header_t));

        std::cout << "pakman: Generating file table and writing files..." << std::endl;
        int offset = sizeof(pak_header_t);
        for (const auto& dirEntry : recursive_directory_iterator(file)) {
            if (dirEntry.is_directory())
                continue;

            std::string rel_path = dirEntry.path().lexically_relative(file).string();
            std::replace(rel_path.begin(), rel_path.end(), '\\', '/');

            uint32_t size = std::filesystem::file_size(dirEntry);
            pak_file_header_t& header = headers.emplace_back();

            memset(header.filename, 0, sizeof(header.filename));
            memcpy(header.filename, rel_path.c_str(), std::min<uint32_t>(rel_path.length(), 52));
            header.data_offset = offset;
            header.data_size = size;
            header.data_size_2 = size;

            std::ifstream file(dirEntry.path(), std::ios::binary);
            pak << file.rdbuf();
            file.close();

            offset += size + 16;
            int pad = offset % 8;
            offset -= pad;

            for (int i = 0; i < 16 - pad; i++)
                pak << '\0';
        }

        std::cout << "pakman: Writing file table..." << std::endl;
        pak.write((char*)headers.data(), headers.size() * sizeof(pak_file_header_t));

        memcpy(pak_header.magic, "PACK", 4);
        pak_header.file_count = headers.size();
        pak_header.initial_file_header_offset = offset;

        std::cout << "pakman: Writing pak header..." << std::endl;
        pak.seekp(0);
        pak.write((char*)&pak_header, sizeof(pak_header_t));
        pak.close();

        std::cout << "pakman: Done! Saved to " << pak_name << "." << std::endl;
    }
    else if (cmd == "unpack") {
        std::cout << "pakman: Unpacking " << file << "..." << std::endl;

        std::ifstream pak(file.data(), std::ios::binary | std::ios::ate);
        size_t size = pak.tellg();
        pak.seekg(0);

        if (!pak.is_open()) {
            std::cout << "pakman: Failed to open " << file << "." << std::endl;
            return 1;
        }

        pak_header_t pak_header;
        pak.read((char*)&pak_header, sizeof(pak_header_t));
        
        for (uint32_t i = 0; i < pak_header.file_count; i++) {
            pak_file_header_t file_header;
            pak.seekg(pak_header.initial_file_header_offset + (i * sizeof(pak_file_header_t)));
            pak.read((char*)&file_header, sizeof(pak_file_header_t));

            std::cout << file_header.filename << " (" << i + 1 << "/" << pak_header.file_count << ")" << std::endl;

            std::filesystem::path path = file.substr(0, file.find("."));
            path /= file_header.filename;
            try {
                std::filesystem::path dir = path;
                std::filesystem::create_directories(dir.remove_filename());
            } catch(...) {}

            std::ofstream file(path, std::ios::binary);
        
            std::vector<char> data(file_header.data_size);
            pak.seekg(file_header.data_offset);
            pak.read(data.data(), file_header.data_size);
            file.write(data.data(), file_header.data_size);
        }

        pak.close();
        std::cout << "pakman: Done!" << std::endl;
    }
    else {
        print_usage(argv);
        return 1;
    }

    return 0;
}
