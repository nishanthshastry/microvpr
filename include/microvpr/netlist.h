#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace microvpr {

// Gate-level netlist data model shared by every stage of the flow
// (technology mapping, placement, routing, timing). Built once by the
// BLIF parser and never mutated in place — each stage produces a new
// derived structure (mapped netlist, placement, routing) that still
// refers back to this one for correctness checks.

enum class CellKind {
    PrimaryInput,
    PrimaryOutput,
    Latch,     // .latch: single-bit DFF, one input, one output, no reset value modeled yet
    LogicGate, // .names: truth-table-defined combinational gate
};

// A .names truth table row: an input pattern over {'0','1','-'} and the
// single-bit output value it produces (BLIF's single-output-per-.names
// convention).
struct TruthTableRow {
    std::string input_pattern; // length == cell.inputs.size()
    char output_value = '1';   // '0' or '1'
};

struct Cell {
    std::string name; // BLIF has no per-gate name; synthesized as "g<index>" or the .latch/.names identity
    CellKind kind = CellKind::LogicGate;

    std::vector<std::string> inputs;  // net names, in BLIF-declared order
    std::string output;               // net name (PrimaryInput cells have no output net; they *are* the net)

    // Populated for CellKind::LogicGate.
    std::vector<TruthTableRow> truth_table;

    // Populated for CellKind::Latch.
    std::string latch_input;
    std::string latch_output;
};

struct Netlist {
    std::string model_name;
    std::vector<std::string> primary_inputs;
    std::vector<std::string> primary_outputs;
    std::vector<Cell> cells; // logic gates and latches only; PI/PO are listed above, not here

    // net name -> index of the cell that drives it (logic gate or latch),
    // or -1 if the net is a primary input (undriven within this netlist).
    std::unordered_map<std::string, int> driver_of;

    bool is_primary_input(const std::string& net) const;
    bool is_primary_output(const std::string& net) const;
};

} // namespace microvpr
