// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "microvpr/blif_parser.h"
#include "microvpr/netlist_evaluator.h"
#include "mini_test.h"

using namespace microvpr;

static void test_and_gate_truth_table() {
    std::string src = R"(
.model and2
.inputs a b
.outputs y
.names a b y
11 1
.end
)";
    BlifParser parser;
    Netlist nl = parser.parse(src);
    NetlistEvaluator eval(nl);

    struct Case { bool a, b, expected; };
    std::vector<Case> cases = {{false, false, false}, {false, true, false}, {true, false, false}, {true, true, true}};
    for (const auto& c : cases) {
        auto result = eval.eval({{"a", c.a}, {"b", c.b}}, {});
        CHECK_EQ(result.at("y"), c.expected);
    }
}

static void test_half_adder() {
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
    BlifParser parser;
    Netlist nl = parser.parse(src);
    NetlistEvaluator eval(nl);

    struct Case { bool a, b, sum, cout; };
    std::vector<Case> cases = {
        {false, false, false, false},
        {false, true, true, false},
        {true, false, true, false},
        {true, true, false, true},
    };
    for (const auto& c : cases) {
        auto result = eval.eval({{"a", c.a}, {"b", c.b}}, {});
        CHECK_EQ(result.at("sum"), c.sum);
        CHECK_EQ(result.at("cout"), c.cout);
    }
}

static void test_mux_with_dont_cares() {
    std::string src = R"(
.model mux2to1
.inputs sel a b
.outputs y
.names sel a b y
01- 1
1-1 1
.end
)";
    BlifParser parser;
    Netlist nl = parser.parse(src);
    NetlistEvaluator eval(nl);

    // sel=0 selects a, sel=1 selects b.
    CHECK_EQ(eval.eval({{"sel", false}, {"a", true}, {"b", false}}, {}).at("y"), true);
    CHECK_EQ(eval.eval({{"sel", false}, {"a", false}, {"b", true}}, {}).at("y"), false);
    CHECK_EQ(eval.eval({{"sel", true}, {"a", true}, {"b", false}}, {}).at("y"), false);
    CHECK_EQ(eval.eval({{"sel", true}, {"a", false}, {"b", true}}, {}).at("y"), true);
}

static void test_latch_toggles_across_steps() {
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
    BlifParser parser;
    Netlist nl = parser.parse(src);
    NetlistEvaluator eval(nl);

    bool q = false; // initial state (matches the .latch init value of 0)
    for (int step = 0; step < 4; ++step) {
        auto result = eval.eval({{"clk", true}}, {{"q", q}});
        CHECK_EQ(result.at("q"), q);
        bool qn = result.at("qn");
        CHECK_EQ(qn, !q);
        q = qn; // commit next state, as the caller of a step-driven flow would
    }
}

static void test_missing_input_throws() {
    std::string src = R"(
.model and2
.inputs a b
.outputs y
.names a b y
11 1
.end
)";
    BlifParser parser;
    Netlist nl = parser.parse(src);
    NetlistEvaluator eval(nl);
    bool threw = false;
    try {
        eval.eval({{"a", true}}, {}); // b missing
    } catch (const NetlistEvalError&) {
        threw = true;
    }
    CHECK(threw);
}

int main() {
    test_and_gate_truth_table();
    test_half_adder();
    test_mux_with_dont_cares();
    test_latch_toggles_across_steps();
    test_missing_input_throws();
    TEST_MAIN_RETURN();
}
