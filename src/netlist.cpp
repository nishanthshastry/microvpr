#include "microvpr/netlist.h"

#include <algorithm>

namespace microvpr {

bool Netlist::is_primary_input(const std::string& net) const {
    return std::find(primary_inputs.begin(), primary_inputs.end(), net) != primary_inputs.end();
}

bool Netlist::is_primary_output(const std::string& net) const {
    return std::find(primary_outputs.begin(), primary_outputs.end(), net) != primary_outputs.end();
}

} // namespace microvpr
