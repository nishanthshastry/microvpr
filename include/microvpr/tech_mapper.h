// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "microvpr/netlist.h"

namespace microvpr {

class TechMapError : public std::runtime_error {
public:
    explicit TechMapError(const std::string& msg) : std::runtime_error(msg) {}
};

// A K-feasible cut: `root`'s value can be computed as a Boolean function
// of only the nets in `leaves` (at most K of them).
struct Cut {
    std::vector<std::string> leaves; // sorted net names; the cut's boundary
    int depth = 0;                   // mapped-LUT depth if this cut is chosen as root's cover
};

// Milestone-2 technology mapper: covers a combinational .names/.latch
// netlist with K-input LUTs via cut enumeration (FlowMap-style
// depth-optimal selection), implementing the proposal's "technology
// mapping" stage.
//
// Algorithm, per node in topological order:
//   1. Enumerate K-feasible cuts by combining one cut from each fanin
//      (a node's cuts are unions of its fanins' cuts, bounded by K), kept
//      to a bounded "priority cuts" set per node (smallest depth, then
//      smallest leaf count) to avoid combinatorial blowup on high-fanin
//      gates.
//   2. Pick each node's minimum-depth cut — the FlowMap depth-optimal
//      criterion.
//   3. Starting from the primary outputs and every latch's data input,
//      walk backward selecting only the nodes actually needed and their
//      chosen cut's leaves (recursively) — this is the mapper's area
//      recovery: nodes that aren't on a covering path produce no LUT at
//      all, rather than one LUT per original gate.
//
// A fuller area-recovery re-pass (re-choosing among equal-depth cuts
// to further shrink LUT count once depth is fixed) is a documented
// follow-up; this mapper's area reduction comes from the required-node
// pruning in step 3 alone.
//
// Scope limit: a node whose structural fanins' cuts can't be unioned
// into K or fewer leaves (only possible for gates with unusually many
// fanins relative to K) has no K-feasible cut and mapping fails with
// TechMapError — real synthesized .names cells are almost always 2-6
// inputs, well within typical K, so this is not expected to bite on
// ordinary benchmarks.
class TechMapper {
public:
    TechMapper(const Netlist& netlist, int k);

    // Returns the mapped netlist: every required combinational node
    // becomes one Cell::LogicGate LUT (truth table recomputed over its
    // chosen cut's leaves), latches are carried through unchanged, and
    // primary inputs/outputs are preserved.
    Netlist map();

    // Diagnostics, valid after map() has run.
    int lut_count() const { return lut_count_; }
    int max_depth() const { return max_depth_; }

private:
    const std::vector<Cut>& cuts_of(const std::string& node);
    std::vector<Cut> combine_fanin_cuts(const Cell& gate);
    void select_required(const std::string& node, std::unordered_set<std::string>& required);
    bool eval_cone(const std::string& net, const std::unordered_map<std::string, bool>& boundary,
                   std::unordered_map<std::string, bool>& memo) const;
    std::vector<TruthTableRow> truth_table_for_cut(const std::string& root, const Cut& cut) const;

    const Netlist& netlist_;
    int k_;
    int lut_count_ = 0;
    int max_depth_ = 0;

    std::unordered_map<std::string, std::vector<Cut>> cuts_cache_;
    std::unordered_map<std::string, Cut> best_cut_; // min-depth cut per non-PI, non-latch-output node

    // A node's depth if it were realized as its own required LUT: 0 for a
    // primary input or latch output, otherwise 1 + the max depth of its
    // best cut's leaves. Populated the first time cuts_of(node) resolves
    // it, and consulted whenever a *different* node considers using this
    // one as a single boundary leaf rather than absorbing its logic (see
    // combine_fanin_cuts) — the depth of a combined cut always comes from
    // its final leaves' own node_depth_, never from an intermediate
    // fanin's cut that ends up fully folded in and never materialized.
    std::unordered_map<std::string, int> node_depth_;
};

} // namespace microvpr
