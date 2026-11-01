// Copyright 2026 Nishanth Shastry
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "microvpr/netlist.h"

namespace microvpr {

class BlifParseError : public std::runtime_error {
public:
    explicit BlifParseError(const std::string& msg) : std::runtime_error(msg) {}
};

// Parses a single-model BLIF file (.model/.inputs/.outputs/.names/.latch/.end).
// Multi-model files (.subckt hierarchy) are out of scope for Milestone 1;
// the flow expects a flattened netlist, matching the "gate-level netlist
// (BLIF)" input the CAD flow proposal specifies.
class BlifParser {
public:
    Netlist parse(const std::string& text);

private:
    static std::vector<std::string> split_tokens(const std::string& line);
};

} // namespace microvpr
