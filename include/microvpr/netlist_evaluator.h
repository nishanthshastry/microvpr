// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "microvpr/netlist.h"

namespace microvpr {

class NetlistEvalError : public std::runtime_error {
public:
    explicit NetlistEvalError(const std::string& msg) : std::runtime_error(msg) {}
};

// Single-step combinational evaluator over a Netlist: the "golden model"
// milestone 1 calls for, later reused to equivalence-check the LUT-mapped
// netlist (Milestone 2) by comparing outputs under random stimulus.
//
// Latches are treated as pseudo primary inputs for their current output
// (must be supplied in `latch_state`) and their input net is evaluated
// like any other combinational net, giving the next-state value the
// caller is expected to commit before the following step.
class NetlistEvaluator {
public:
    explicit NetlistEvaluator(const Netlist& netlist) : netlist_(netlist) {}

    // primary_inputs: value for every net in netlist.primary_inputs.
    // latch_state: current output value for every latch (by latch_output net name).
    // Returns the boolean value of every net that has a driver (i.e. every
    // .names output and every .latch output), plus the inputs echoed back.
    std::unordered_map<std::string, bool> eval(
        const std::unordered_map<std::string, bool>& primary_inputs,
        const std::unordered_map<std::string, bool>& latch_state) const;

private:
    bool eval_net(const std::string& net, const std::unordered_map<std::string, bool>& primary_inputs,
                   const std::unordered_map<std::string, bool>& latch_state,
                   std::unordered_map<std::string, bool>& cache,
                   std::unordered_map<std::string, bool>& in_progress) const;
    bool eval_truth_table(const Cell& cell, const std::vector<bool>& input_values) const;

    const Netlist& netlist_;
};

} // namespace microvpr
