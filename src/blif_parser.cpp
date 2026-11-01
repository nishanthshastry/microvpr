// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "microvpr/blif_parser.h"

#include <sstream>

namespace microvpr {

std::vector<std::string> BlifParser::split_tokens(const std::string& line) {
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

Netlist BlifParser::parse(const std::string& text) {
    Netlist nl;
    bool saw_model = false;
    bool saw_end = false;

    // State for the .names block currently being accumulated, if any.
    bool in_names = false;
    int names_cell_index = -1;

    std::istringstream lines(text);
    std::string raw_line;
    int line_no = 0;

    auto finish_names_if_open = [&]() {
        in_names = false;
        names_cell_index = -1;
    };

    while (std::getline(lines, raw_line)) {
        line_no++;

        // Strip comments (BLIF uses '#') and trailing whitespace.
        size_t hash = raw_line.find('#');
        std::string line = (hash == std::string::npos) ? raw_line : raw_line.substr(0, hash);

        // Line continuation: a trailing backslash joins with the next line.
        // Not handling multi-line continuation in Milestone 1; long .names
        // tables are just written one row per line, which needs no
        // continuation support.

        if (line.find_first_not_of(" \t\r") == std::string::npos) continue; // blank

        std::vector<std::string> tokens = split_tokens(line);
        if (tokens.empty()) continue;

        bool starts_new_directive = !tokens[0].empty() && tokens[0][0] == '.';
        if (in_names && starts_new_directive && tokens[0] != ".names") {
            // A new directive (.latch, .outputs, .end, ...) ends the
            // previous .names truth table; plain rows never start with '.'.
            finish_names_if_open();
        }

        if (tokens[0] == ".model") {
            if (saw_model) throw BlifParseError("line " + std::to_string(line_no) + ": multiple .model blocks are not supported (flatten to one model)");
            if (tokens.size() < 2) throw BlifParseError("line " + std::to_string(line_no) + ": .model requires a name");
            nl.model_name = tokens[1];
            saw_model = true;
        } else if (tokens[0] == ".inputs") {
            for (size_t i = 1; i < tokens.size(); ++i) {
                nl.primary_inputs.push_back(tokens[i]);
                nl.driver_of[tokens[i]] = -1;
            }
        } else if (tokens[0] == ".outputs") {
            for (size_t i = 1; i < tokens.size(); ++i) nl.primary_outputs.push_back(tokens[i]);
        } else if (tokens[0] == ".names") {
            if (tokens.size() < 2) throw BlifParseError("line " + std::to_string(line_no) + ": .names requires at least an output net");
            Cell cell;
            cell.kind = CellKind::LogicGate;
            cell.name = "g" + std::to_string(nl.cells.size());
            for (size_t i = 1; i + 1 < tokens.size(); ++i) cell.inputs.push_back(tokens[i]);
            cell.output = tokens.back();
            nl.cells.push_back(cell);
            names_cell_index = static_cast<int>(nl.cells.size() - 1);
            nl.driver_of[cell.output] = names_cell_index;
            in_names = true;
        } else if (tokens[0] == ".latch") {
            if (tokens.size() < 3) throw BlifParseError("line " + std::to_string(line_no) + ": .latch requires input and output nets");
            Cell cell;
            cell.kind = CellKind::Latch;
            cell.name = "lat" + std::to_string(nl.cells.size());
            cell.latch_input = tokens[1];
            cell.latch_output = tokens[2];
            cell.output = tokens[2];
            nl.cells.push_back(cell);
            nl.driver_of[cell.latch_output] = static_cast<int>(nl.cells.size() - 1);
        } else if (tokens[0] == ".end") {
            saw_end = true;
            finish_names_if_open();
            break;
        } else if (in_names) {
            // Truth-table row: "<pattern> <bit>" for cells with inputs, or
            // just "<bit>" for a zero-input (constant) .names.
            TruthTableRow row;
            if (tokens.size() == 1) {
                row.input_pattern = "";
                row.output_value = tokens[0].empty() ? '0' : tokens[0][0];
            } else if (tokens.size() >= 2) {
                row.input_pattern = tokens[0];
                row.output_value = tokens[1][0];
            }
            nl.cells[static_cast<size_t>(names_cell_index)].truth_table.push_back(row);
        } else {
            throw BlifParseError("line " + std::to_string(line_no) + ": unrecognized directive '" + tokens[0] + "'");
        }
    }

    if (!saw_model) throw BlifParseError("missing .model directive");
    if (!saw_end) throw BlifParseError("missing .end directive");

    return nl;
}

} // namespace microvpr
