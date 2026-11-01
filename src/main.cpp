// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

#include "microvpr/blif_parser.h"
#include "microvpr/netlist_evaluator.h"
#include "microvpr/tech_mapper.h"

namespace {
std::string read_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open file: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Milestone-2's validation ask: check the mapped netlist against the
// original via random simulation rather than just trusting the mapper.
// Latches are stepped in lockstep on both sides so sequential designs
// are covered too, not just purely combinational ones.
bool check_equivalence_by_random_simulation(const microvpr::Netlist& original, const microvpr::Netlist& mapped,
                                             int trials, unsigned seed) {
    microvpr::NetlistEvaluator orig_eval(original);
    microvpr::NetlistEvaluator mapped_eval(mapped);
    std::mt19937 rng(seed);
    std::bernoulli_distribution bit;

    std::unordered_map<std::string, bool> orig_state, mapped_state;
    for (const auto& cell : original.cells) {
        if (cell.kind == microvpr::CellKind::Latch) {
            orig_state[cell.latch_output] = false;
            mapped_state[cell.latch_output] = false;
        }
    }

    for (int t = 0; t < trials; ++t) {
        std::unordered_map<std::string, bool> inputs;
        for (const std::string& pi : original.primary_inputs) inputs[pi] = bit(rng);

        auto expected = orig_eval.eval(inputs, orig_state);
        auto actual = mapped_eval.eval(inputs, mapped_state);

        for (const std::string& po : original.primary_outputs) {
            if (expected.at(po) != actual.at(po)) {
                std::cerr << "  mismatch at trial " << t << " on output '" << po << "'\n";
                return false;
            }
        }
        for (const auto& cell : original.cells) {
            if (cell.kind != microvpr::CellKind::Latch) continue;
            orig_state[cell.latch_output] = expected.at(cell.latch_input);
            mapped_state[cell.latch_output] = actual.at(cell.latch_input);
        }
    }
    return true;
}
} // namespace

// CLI covering Milestones 1-2: parses a BLIF netlist and prints a summary.
// With an optional K argument, also runs technology mapping to K-input
// LUTs and checks the result against the original via random simulation.
int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        std::cerr << "usage: " << argv[0] << " <file.blif> [K]\n";
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

        if (argc == 3) {
            int k = std::stoi(argv[2]);
            std::cout << "\nmapping to " << k << "-input LUTs...\n";
            microvpr::TechMapper mapper(nl, k);
            microvpr::Netlist mapped = mapper.map();

            std::cout << "LUT count: " << mapper.lut_count() << "\n";
            std::cout << "LUT depth: " << mapper.max_depth() << "\n";

            std::cout << "checking equivalence against the original netlist (1000 random trials)...\n";
            bool ok = check_equivalence_by_random_simulation(nl, mapped, /*trials=*/1000, /*seed=*/12345);
            std::cout << (ok ? "equivalence check: PASS\n" : "equivalence check: FAIL\n");
            if (!ok) return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
