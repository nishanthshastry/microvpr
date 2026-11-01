// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "microvpr/blif_parser.h"
#include "microvpr/netlist_evaluator.h"
#include "microvpr/tech_mapper.h"
#include "mini_test.h"

using namespace microvpr;

static Netlist parse(const std::string& src) {
    BlifParser parser;
    return parser.parse(src);
}

static void check_equivalent(const Netlist& original, const Netlist& mapped,
                              const std::vector<std::unordered_map<std::string, bool>>& cases) {
    NetlistEvaluator orig_eval(original);
    NetlistEvaluator mapped_eval(mapped);
    for (const auto& inputs : cases) {
        auto expected = orig_eval.eval(inputs, {});
        auto actual = mapped_eval.eval(inputs, {});
        for (const std::string& po : original.primary_outputs) {
            CHECK_EQ(actual.at(po), expected.at(po));
        }
    }
}

static void test_half_adder_maps_and_matches_original() {
    std::string src = R"(
.model half_adder
.inputs a b
.outputs sum cout
.names a b sum
01 1
10 1
.names a b cout
11 1
.end
)";
    Netlist nl = parse(src);
    TechMapper mapper(nl, /*k=*/4);
    Netlist mapped = mapper.map();

    CHECK_EQ(mapper.lut_count(), 2); // sum and cout are independent, each within K=4 directly
    CHECK(mapped.primary_inputs == nl.primary_inputs);
    CHECK(mapped.primary_outputs == nl.primary_outputs);

    std::vector<std::unordered_map<std::string, bool>> cases = {
        {{"a", false}, {"b", false}}, {{"a", false}, {"b", true}}, {{"a", true}, {"b", false}}, {{"a", true}, {"b", true}},
    };
    check_equivalent(nl, mapped, cases);
}

static void test_deep_chain_collapses_into_one_lut_when_k_allows() {
    // y = ((a & b) & c) & d, three 2-input gates chained. With K=4 all
    // four leaves fit in one cut, so the whole chain should collapse to
    // a single LUT — this is the real depth-optimal cut-combination
    // behavior, not just a pass-through of already-small gates.
    std::string src = R"(
.model chain4
.inputs a b c d
.outputs y
.names a b t1
11 1
.names t1 c t2
11 1
.names t2 d y
11 1
.end
)";
    Netlist nl = parse(src);
    TechMapper mapper(nl, /*k=*/4);
    Netlist mapped = mapper.map();

    CHECK_EQ(mapper.lut_count(), 1);
    CHECK_EQ(mapper.max_depth(), 1);

    std::vector<std::unordered_map<std::string, bool>> cases;
    for (int a = 0; a <= 1; ++a)
        for (int b = 0; b <= 1; ++b)
            for (int c = 0; c <= 1; ++c)
                for (int d = 0; d <= 1; ++d)
                    cases.push_back({{"a", a != 0}, {"b", b != 0}, {"c", c != 0}, {"d", d != 0}});
    check_equivalent(nl, mapped, cases);
}

static void test_deep_chain_with_small_k_stays_multi_level() {
    // Same chain, but K=2 can't fit all four leaves in one cut, so the
    // mapper must keep it as multiple LUTs — and the result must still
    // be functionally equivalent to the original.
    std::string src = R"(
.model chain4
.inputs a b c d
.outputs y
.names a b t1
11 1
.names t1 c t2
11 1
.names t2 d y
11 1
.end
)";
    Netlist nl = parse(src);
    TechMapper mapper(nl, /*k=*/2);
    Netlist mapped = mapper.map();

    CHECK(mapper.lut_count() >= 2); // can't collapse into a single 2-input LUT

    std::vector<std::unordered_map<std::string, bool>> cases;
    for (int a = 0; a <= 1; ++a)
        for (int b = 0; b <= 1; ++b)
            for (int c = 0; c <= 1; ++c)
                for (int d = 0; d <= 1; ++d)
                    cases.push_back({{"a", a != 0}, {"b", b != 0}, {"c", c != 0}, {"d", d != 0}});
    check_equivalent(nl, mapped, cases);
}

static void test_five_independent_fanins_requires_large_enough_k() {
    std::string src = R"(
.model and5
.inputs a b c d e
.outputs y
.names a b c d e y
11111 1
.end
)";
    Netlist nl = parse(src);

    // K=3 can't cover a single gate with 5 independent fanins.
    TechMapper small_mapper(nl, /*k=*/3);
    bool threw = false;
    try {
        small_mapper.map();
    } catch (const TechMapError&) {
        threw = true;
    }
    CHECK(threw);

    // K=5 fits exactly.
    TechMapper big_mapper(nl, /*k=*/5);
    Netlist mapped = big_mapper.map();
    CHECK_EQ(big_mapper.lut_count(), 1);
    check_equivalent(nl, mapped,
                      {{{"a", true}, {"b", true}, {"c", true}, {"d", true}, {"e", true}},
                       {{"a", true}, {"b", true}, {"c", true}, {"d", true}, {"e", false}}});
}

static void test_latch_passes_through_unchanged() {
    std::string src = R"(
.model toggle
.inputs clk
.outputs q
.names q qn
0 1
1 0
.latch qn q re clk 0
.end
)";
    Netlist nl = parse(src);
    TechMapper mapper(nl, /*k=*/4);
    Netlist mapped = mapper.map();

    // Exactly one latch, carried through unchanged, plus one LUT for the
    // inverter feeding it.
    int latch_count = 0, lut_count = 0;
    for (const Cell& c : mapped.cells) {
        if (c.kind == CellKind::Latch) latch_count++;
        else lut_count++;
    }
    CHECK_EQ(latch_count, 1);
    CHECK_EQ(lut_count, 1);

    NetlistEvaluator orig_eval(nl);
    NetlistEvaluator mapped_eval(mapped);
    bool q_orig = false, q_mapped = false;
    for (int step = 0; step < 4; ++step) {
        auto r1 = orig_eval.eval({{"clk", true}}, {{"q", q_orig}});
        auto r2 = mapped_eval.eval({{"clk", true}}, {{"q", q_mapped}});
        CHECK_EQ(r2.at("q"), r1.at("q"));
        q_orig = r1.at("qn");
        q_mapped = r2.at("qn");
    }
}

int main() {
    test_half_adder_maps_and_matches_original();
    test_deep_chain_collapses_into_one_lut_when_k_allows();
    test_deep_chain_with_small_k_stays_multi_level();
    test_five_independent_fanins_requires_large_enough_k();
    test_latch_passes_through_unchanged();
    TEST_MAIN_RETURN();
}
