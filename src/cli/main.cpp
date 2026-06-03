#include "app/commands.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

std::string read_input(int argc, char** argv) {
    if (argc >= 3 && std::string_view(argv[2]) != "-") {
        const std::string arg = argv[2];
        if (!arg.empty() && (arg[0] == '{' || arg[0] == '[')) {
            return arg;
        }
        std::ifstream file(arg);
        if (!file) {
            throw std::runtime_error("failed to open input file: " + arg);
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    std::ostringstream buffer;
    buffer << std::cin.rdbuf();
    return buffer.str();
}

void print_usage() {
    std::cerr
        << "Usage: skill_rating_cli <command> [input.json|-|json]\n"
        << "\nCommands:\n"
        << "  rate          Update arbitrary teams. Input keys: rating_groups, optional ranks, weights, environment, min_delta.\n"
        << "  rate-1vs1     Update two players. Input keys: first_player, second_player, optional drawn, environment, min_delta.\n"
        << "  quality       Compute arbitrary-team match quality. Input keys: rating_groups, optional weights, environment.\n"
        << "  quality-1vs1  Compute two-player match quality. Input keys: first_player, second_player, optional environment.\n"
        << "  expose        Compute conservative exposure. Input keys: rating, optional environment.\n"
        << "  draw-probability  Compute draw probability. Input keys: first_player/second_player or two rating_groups, optional weights, environment.\n";
}

int run(int argc, char** argv) {
    if (argc < 2 || std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "-h") {
        print_usage();
        return argc < 2 ? 1 : 0;
    }
    std::cout << skill_rating::app::run_command(argv[1], read_input(argc, argv)) << '\n';
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
