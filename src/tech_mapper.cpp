// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "microvpr/tech_mapper.h"

#include <algorithm>
#include <functional>
#include <set>

namespace microvpr {

namespace {
// Pragmatic bounds to keep cut enumeration polynomial on high-fanin
// gates: only the best few cuts per fanin are combined, and the total
// number of combinations tried per node is capped. Real synthesized
// .names cells are almost always small (2-6 inputs), so these bounds
// are not expected to matter on ordinary benchmarks.
constexpr size_t kMaxCutsPerFaninInCombine = 4;
constexpr int kMaxCombosPerNode = 256;
constexpr size_t kMaxCutsKeptPerNode = 8; // "priority cuts"
} // namespace

TechMapper::TechMapper(const Netlist& netlist, int k) : netlist_(netlist), k_(k) {
    if (k_ < 1) throw TechMapError("K must be at least 1");
}

std::vector<Cut> TechMapper::combine_fanin_cuts(const Cell& gate) {
    if (gate.inputs.empty()) {
        return {Cut{{}, 0}}; // constant .names cell: a 0-leaf LUT, available at depth 0
    }

    // Per fanin, the candidate *leaf sets* it can contribute: either one
    // of its own cuts' leaf sets (absorbing that fanin's logic into this
    // combined cut), or just the fanin itself as a single boundary leaf
    // (leaving it to be realized as its own separate LUT). The trivial
    // option is what makes the depth formula below correct — without it,
    // a node that gets fully absorbed into a larger cut would wrongly
    // keep contributing the depth it *would have had* as a standalone
    // LUT, even though it's never actually materialized as one.
    std::vector<std::vector<std::vector<std::string>>> fanin_leaf_options;
    fanin_leaf_options.reserve(gate.inputs.size());
    for (const std::string& in : gate.inputs) {
        const std::vector<Cut>& c = cuts_of(in);
        std::vector<std::vector<std::string>> options;
        size_t limit = std::min(c.size(), kMaxCutsPerFaninInCombine - 1);
        for (size_t i = 0; i < limit; ++i) options.push_back(c[i].leaves);
        options.push_back({in}); // trivial: use `in` itself as a boundary leaf
        fanin_leaf_options.push_back(std::move(options));
    }

    std::vector<Cut> out;
    int combos_tried = 0;
    std::vector<size_t> idx(fanin_leaf_options.size(), 0);

    std::function<void(size_t)> recurse = [&](size_t pos) {
        if (combos_tried >= kMaxCombosPerNode) return;
        if (pos == fanin_leaf_options.size()) {
            std::set<std::string> leafset;
            for (size_t i = 0; i < fanin_leaf_options.size(); ++i) {
                const auto& leaves = fanin_leaf_options[i][idx[i]];
                leafset.insert(leaves.begin(), leaves.end());
            }
            combos_tried++;
            if (static_cast<int>(leafset.size()) <= k_) {
                int depth = 0;
                for (const std::string& leaf : leafset) depth = std::max(depth, node_depth_.at(leaf));
                Cut cut;
                cut.leaves.assign(leafset.begin(), leafset.end());
                cut.depth = depth + 1;
                out.push_back(std::move(cut));
            }
            return;
        }
        for (size_t i = 0; i < fanin_leaf_options[pos].size(); ++i) {
            idx[pos] = i;
            recurse(pos + 1);
            if (combos_tried >= kMaxCombosPerNode) return;
        }
    };
    recurse(0);
    return out;
}

const std::vector<Cut>& TechMapper::cuts_of(const std::string& node) {
    auto cached = cuts_cache_.find(node);
    if (cached != cuts_cache_.end()) return cached->second;

    bool is_leaf_terminal = netlist_.is_primary_input(node);
    if (!is_leaf_terminal) {
        auto drv = netlist_.driver_of.find(node);
        if (drv != netlist_.driver_of.end() && drv->second >= 0 &&
            netlist_.cells[static_cast<size_t>(drv->second)].kind == CellKind::Latch) {
            is_leaf_terminal = true; // latch output: pseudo primary input for mapping purposes
        }
    }

    std::vector<Cut> result;
    if (is_leaf_terminal) {
        result.push_back(Cut{{node}, 0});
    } else {
        auto drv = netlist_.driver_of.find(node);
        if (drv == netlist_.driver_of.end() || drv->second < 0) {
            throw TechMapError("net '" + node + "' has no driver and is not a primary input");
        }
        const Cell& gate = netlist_.cells[static_cast<size_t>(drv->second)];
        if (gate.kind != CellKind::LogicGate) {
            throw TechMapError("net '" + node + "' is driven by an unsupported cell kind for mapping");
        }
        result = combine_fanin_cuts(gate);
        if (result.empty()) {
            throw TechMapError("no K-feasible cut found for net '" + node + "' (gate has too many fanins for K=" +
                                std::to_string(k_) + ")");
        }
    }

    // Priority-cut pruning: dedupe identical leaf sets (keep the lower
    // depth), sort by (depth, size) ascending, keep a bounded count.
    std::unordered_map<std::string, Cut> by_key;
    for (Cut& c : result) {
        std::sort(c.leaves.begin(), c.leaves.end());
        std::string key;
        for (const std::string& l : c.leaves) key += l + "|";
        auto it = by_key.find(key);
        if (it == by_key.end() || c.depth < it->second.depth) by_key[key] = c;
    }
    std::vector<Cut> pruned;
    pruned.reserve(by_key.size());
    for (auto& [key, c] : by_key) {
        (void)key;
        pruned.push_back(c);
    }
    std::sort(pruned.begin(), pruned.end(), [](const Cut& a, const Cut& b) {
        if (a.depth != b.depth) return a.depth < b.depth;
        return a.leaves.size() < b.leaves.size();
    });
    if (pruned.size() > kMaxCutsKeptPerNode) pruned.resize(kMaxCutsKeptPerNode);

    // node_depth_ must be set before this call returns: a parent gate
    // combining this node as a fanin may need it (either because this
    // node bottoms out a fully-absorbed cut, or via the trivial
    // single-leaf option in combine_fanin_cuts) right after cuts_of()
    // returns, before it ever sees the pruned cut list itself.
    node_depth_[node] = is_leaf_terminal ? 0 : pruned.front().depth;

    auto [it, inserted] = cuts_cache_.emplace(node, std::move(pruned));
    (void)inserted;
    return it->second;
}

bool TechMapper::eval_cone(const std::string& net, const std::unordered_map<std::string, bool>& boundary,
                            std::unordered_map<std::string, bool>& memo) const {
    auto b = boundary.find(net);
    if (b != boundary.end()) return b->second;
    auto m = memo.find(net);
    if (m != memo.end()) return m->second;

    auto drv = netlist_.driver_of.find(net);
    if (drv == netlist_.driver_of.end() || drv->second < 0) {
        throw TechMapError("eval_cone: net '" + net + "' has no driver and is outside the cut boundary");
    }
    const Cell& cell = netlist_.cells[static_cast<size_t>(drv->second)];
    if (cell.kind == CellKind::Latch) {
        throw TechMapError("eval_cone: reached latch output '" + net + "' outside the cut boundary");
    }

    std::vector<bool> input_values;
    input_values.reserve(cell.inputs.size());
    for (const std::string& in : cell.inputs) input_values.push_back(eval_cone(in, boundary, memo));

    bool result = false;
    for (const TruthTableRow& row : cell.truth_table) {
        bool matches = true;
        for (size_t i = 0; i < row.input_pattern.size() && matches; ++i) {
            char p = row.input_pattern[i];
            if (p == '-') continue;
            if (input_values[i] != (p == '1')) matches = false;
        }
        if (matches) {
            result = row.output_value == '1';
            break;
        }
    }
    memo[net] = result;
    return result;
}

std::vector<TruthTableRow> TechMapper::truth_table_for_cut(const std::string& root, const Cut& cut) const {
    size_t n = cut.leaves.size();
    std::vector<TruthTableRow> rows;
    size_t combos = (n == 0) ? 1 : (size_t{1} << n);
    for (size_t mask = 0; mask < combos; ++mask) {
        std::unordered_map<std::string, bool> boundary;
        std::string pattern(n, '0');
        for (size_t i = 0; i < n; ++i) {
            bool bit = ((mask >> i) & 1u) != 0;
            boundary[cut.leaves[i]] = bit;
            pattern[i] = bit ? '1' : '0';
        }
        std::unordered_map<std::string, bool> memo;
        if (eval_cone(root, boundary, memo)) rows.push_back(TruthTableRow{pattern, '1'});
        // Off-set rows (output 0) are omitted, matching the implicit-0
        // convention NetlistEvaluator already uses for unmatched patterns.
    }
    return rows;
}

void TechMapper::select_required(const std::string& node, std::unordered_set<std::string>& required) {
    if (required.count(node)) return;

    if (netlist_.is_primary_input(node)) {
        required.insert(node);
        return;
    }
    auto drv = netlist_.driver_of.find(node);
    if (drv != netlist_.driver_of.end() && drv->second >= 0 &&
        netlist_.cells[static_cast<size_t>(drv->second)].kind == CellKind::Latch) {
        required.insert(node); // latch output: boundary, no LUT needed for it itself
        return;
    }

    required.insert(node);
    const std::vector<Cut>& cs = cuts_of(node); // sorted ascending by (depth, size)
    const Cut& chosen = cs.front();              // minimum-depth cut: the FlowMap criterion
    best_cut_[node] = chosen;
    for (const std::string& leaf : chosen.leaves) select_required(leaf, required);
}

Netlist TechMapper::map() {
    Netlist out;
    out.model_name = netlist_.model_name + "_k" + std::to_string(k_);
    out.primary_inputs = netlist_.primary_inputs;
    out.primary_outputs = netlist_.primary_outputs;

    std::unordered_set<std::string> required;
    for (const std::string& po : netlist_.primary_outputs) select_required(po, required);
    for (const Cell& cell : netlist_.cells) {
        if (cell.kind == CellKind::Latch) select_required(cell.latch_input, required);
    }

    for (const Cell& cell : netlist_.cells) {
        if (cell.kind != CellKind::Latch) continue;
        Cell latch = cell;
        latch.name = "lat" + std::to_string(out.cells.size());
        out.cells.push_back(latch);
        out.driver_of[latch.latch_output] = static_cast<int>(out.cells.size() - 1);
    }
    for (const std::string& pi : out.primary_inputs) out.driver_of[pi] = -1;

    lut_count_ = 0;
    max_depth_ = 0;
    for (const auto& [node, cut] : best_cut_) {
        if (!required.count(node)) continue;
        Cell lut;
        lut.kind = CellKind::LogicGate;
        lut.name = "lut" + std::to_string(out.cells.size());
        lut.inputs = cut.leaves;
        lut.output = node;
        lut.truth_table = truth_table_for_cut(node, cut);
        out.cells.push_back(lut);
        out.driver_of[node] = static_cast<int>(out.cells.size() - 1);
        lut_count_++;
        max_depth_ = std::max(max_depth_, cut.depth);
    }

    return out;
}

} // namespace microvpr
