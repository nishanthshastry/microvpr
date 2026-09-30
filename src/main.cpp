#include <fstream>
#include <iostream>
#include <sstream>

#include "microvpr/blif_parser.h"

namespace {
std::string read_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open file: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
} // namespace

// Milestone-1 CLI: parses a BLIF netlist and prints a summary (I/O counts,
// gate/latch counts). Technology mapping, placement, routing and timing
// are later milestones.
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <file.blif>\n";
        return 1;
    }
    try {
        std::string text = read_file(argv[1]);
        microvpr::BlifParser parser;
        microvpr::Netlist nl = parser.parse(text);

        int gate_count = 0, latch_count = 0;
        for (const auto& cell : nl.cells) {
            if (cell.kind == microvpr::CellKind::Latch) latch_count++;
            else gate_count++;
        }

        std::cout << "model: " << nl.model_name << "\n";
        std::cout << "primary inputs: " << nl.primary_inputs.size() << "\n";
        std::cout << "primary outputs: " << nl.primary_outputs.size() << "\n";
        std::cout << "logic gates (.names): " << gate_count << "\n";
        std::cout << "latches (.latch): " << latch_count << "\n";
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
