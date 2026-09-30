#include "microvpr/netlist_evaluator.h"

namespace microvpr {

bool NetlistEvaluator::eval_truth_table(const Cell& cell, const std::vector<bool>& input_values) const {
    for (const TruthTableRow& row : cell.truth_table) {
        bool matches = true;
        for (size_t i = 0; i < row.input_pattern.size() && matches; ++i) {
            char p = row.input_pattern[i];
            if (p == '-') continue;
            bool expected = (p == '1');
            if (input_values[i] != expected) matches = false;
        }
        if (matches) return row.output_value == '1';
    }
    // BLIF convention: an unmatched pattern falls to the implicit off-set,
    // i.e. 0. A .names with an empty truth table (no rows at all) is the
    // constant-0 driver by the same rule.
    return false;
}

bool NetlistEvaluator::eval_net(const std::string& net,
                                 const std::unordered_map<std::string, bool>& primary_inputs,
                                 const std::unordered_map<std::string, bool>& latch_state,
                                 std::unordered_map<std::string, bool>& cache,
                                 std::unordered_map<std::string, bool>& in_progress) const {
    auto cached = cache.find(net);
    if (cached != cache.end()) return cached->second;

    if (netlist_.is_primary_input(net)) {
        auto it = primary_inputs.find(net);
        if (it == primary_inputs.end()) throw NetlistEvalError("no value supplied for primary input '" + net + "'");
        cache[net] = it->second;
        return it->second;
    }

    auto driver_it = netlist_.driver_of.find(net);
    if (driver_it == netlist_.driver_of.end() || driver_it->second < 0) {
        throw NetlistEvalError("net '" + net + "' has no driver in this netlist");
    }
    const Cell& cell = netlist_.cells[static_cast<size_t>(driver_it->second)];

    if (cell.kind == CellKind::Latch) {
        // The latch's *current* output is external state supplied by the
        // caller, not something derived from this evaluation step.
        auto it = latch_state.find(cell.latch_output);
        if (it == latch_state.end()) {
            throw NetlistEvalError("no state value supplied for latch output '" + cell.latch_output + "'");
        }
        cache[net] = it->second;
        return it->second;
    }

    if (in_progress[net]) throw NetlistEvalError("combinational loop detected through net '" + net + "'");
    in_progress[net] = true;

    std::vector<bool> input_values;
    input_values.reserve(cell.inputs.size());
    for (const std::string& in_net : cell.inputs) {
        input_values.push_back(eval_net(in_net, primary_inputs, latch_state, cache, in_progress));
    }
    bool result = eval_truth_table(cell, input_values);

    in_progress[net] = false;
    cache[net] = result;
    return result;
}

std::unordered_map<std::string, bool> NetlistEvaluator::eval(
    const std::unordered_map<std::string, bool>& primary_inputs,
    const std::unordered_map<std::string, bool>& latch_state) const {
    std::unordered_map<std::string, bool> cache;
    std::unordered_map<std::string, bool> in_progress;

    for (const std::string& net : netlist_.primary_inputs) {
        eval_net(net, primary_inputs, latch_state, cache, in_progress);
    }
    for (const Cell& cell : netlist_.cells) {
        eval_net(cell.output, primary_inputs, latch_state, cache, in_progress);
    }
    // Latch *input* nets are the next-state values the caller needs; make
    // sure they're evaluated and present even though they aren't a cell
    // output themselves.
    for (const Cell& cell : netlist_.cells) {
        if (cell.kind == CellKind::Latch) {
            eval_net(cell.latch_input, primary_inputs, latch_state, cache, in_progress);
        }
    }
    return cache;
}

} // namespace microvpr
