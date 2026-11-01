// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#include "microvpr/blif_parser.h"
#include "mini_test.h"

using namespace microvpr;

static void test_simple_and_gate() {
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

    CHECK_EQ(nl.model_name, std::string("and2"));
    CHECK_EQ(nl.primary_inputs.size(), 2u);
    CHECK_EQ(nl.primary_outputs.size(), 1u);
    CHECK_EQ(nl.cells.size(), 1u);
    CHECK(nl.cells[0].kind == CellKind::LogicGate);
    CHECK_EQ(nl.cells[0].inputs.size(), 2u);
    CHECK_EQ(nl.cells[0].output, std::string("y"));
    CHECK_EQ(nl.cells[0].truth_table.size(), 1u);
    CHECK_EQ(nl.cells[0].truth_table[0].input_pattern, std::string("11"));
    CHECK_EQ(nl.cells[0].truth_table[0].output_value, '1');
    CHECK(nl.driver_of.at("y") == 0);
    CHECK(nl.driver_of.at("a") == -1);
}

static void test_multiple_names_blocks() {
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
    CHECK_EQ(nl.cells.size(), 2u);
    CHECK_EQ(nl.cells[0].output, std::string("sum"));
    CHECK_EQ(nl.cells[0].truth_table.size(), 2u);
    CHECK_EQ(nl.cells[1].output, std::string("cout"));
    CHECK_EQ(nl.cells[1].truth_table.size(), 1u);
}

static void test_latch_directive() {
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
    CHECK_EQ(nl.cells.size(), 2u);
    CHECK(nl.cells[0].kind == CellKind::LogicGate);
    CHECK(nl.cells[1].kind == CellKind::Latch);
    CHECK_EQ(nl.cells[1].latch_input, std::string("qn"));
    CHECK_EQ(nl.cells[1].latch_output, std::string("q"));
    CHECK(nl.driver_of.at("q") == 1);
}

static void test_comments_and_blank_lines_ignored() {
    std::string src = R"(
# top-level comment
.model m
.inputs a
.outputs a_out

# another comment
.names a a_out
1 1
.end
)";
    BlifParser parser;
    Netlist nl = parser.parse(src);
    CHECK_EQ(nl.model_name, std::string("m"));
    CHECK_EQ(nl.cells.size(), 1u);
}

static void test_missing_end_throws() {
    std::string src = ".model m\n.inputs a\n.outputs a\n";
    BlifParser parser;
    bool threw = false;
    try {
        parser.parse(src);
    } catch (const BlifParseError&) {
        threw = true;
    }
    CHECK(threw);
}

static void test_missing_model_throws() {
    std::string src = ".inputs a\n.outputs a\n.end\n";
    BlifParser parser;
    bool threw = false;
    try {
        parser.parse(src);
    } catch (const BlifParseError&) {
        threw = true;
    }
    CHECK(threw);
}

int main() {
    test_simple_and_gate();
    test_multiple_names_blocks();
    test_latch_directive();
    test_comments_and_blank_lines_ignored();
    test_missing_end_throws();
    test_missing_model_throws();
    TEST_MAIN_RETURN();
}
