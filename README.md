# microvpr

A compact FPGA CAD flow in C++: takes a gate-level netlist (BLIF) and, stage
by stage, implements it on an idealized island-style FPGA fabric —
technology mapping to K-input LUTs, simulated-annealing placement,
PathFinder-style negotiated-congestion routing, and static timing
analysis. Named as a small-scale homage to
[VPR](https://docs.verilogtorouting.org/), the academic tool it imitates.

This repo currently implements **Milestone 1** of the roadmap below: a
BLIF parser, the netlist data model, and a netlist evaluator that serves
as the golden model later stages get checked against.

## Target architecture

An island-style grid of logic blocks, each holding one K-input LUT and
one flip-flop, with I/O pads on the perimeter and uniform routing
channels of fixed width W. K, grid size, and W are parameters, meant to
be swept in experiments.

## Build & test

```bash
mkdir build && cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

Run the CLI on a sample netlist (parses and prints a summary):

```bash
./microvpr_cli ../benchmarks/half_adder.blif
```

## Roadmap

1. **BLIF parser and netlist data model**, with a simulator for the
   netlist that serves as the golden model. *(this repo, so far)*
2. Technology mapping to K-LUTs, checked for equivalence by random
   simulation against the input netlist.
3. Simulated-annealing placement with incremental HPWL updates, reported
   against a random-placement baseline.
4. Routing-resource graph and PathFinder router, reporting the minimum
   channel width at which each design routes.
5. Static timing analysis and a critical-path report.
6. Stretch: bitstream emitter and a C++ model of the fabric that
   reproduces the original circuit's behavior.

## Algorithms per stage

| Stage | Algorithm | Objective |
|---|---|---|
| Technology mapping | Cut enumeration, depth-optimal selection (FlowMap-style), area recovery | Minimize LUT depth, then LUT count |
| Placement | Simulated annealing with swap moves, adaptive cooling | Minimize total HPWL |
| Routing | PathFinder: repeated A* with history/present-sharing costs | Zero overuse, then minimum delay |
| Timing | Forward/backward traversal of the timing graph | Critical path delay and slack |

## Metrics

LUT count and logic depth, total HPWL before/after routing, minimum
routable channel width and router iterations, critical path delay under a
simple delay model, and per-stage runtime — reported per benchmark
circuit (the MCNC set common in the VPR literature is the intended
benchmark source).

## AI extension (planned)

ML-guided placement: a graph neural network over the netlist predicts
congestion or final wirelength, used as a cost term or to rank candidate
moves during annealing. Compared against simulated annealing with HPWL
alone; success is final HPWL, routability, and runtime at equal quality.

## Reference tools

[Verilog-to-Routing (VTR)](https://verilogtorouting.org/),
[Yosys](https://yosyshq.net/yosys/),
[ABC](https://github.com/berkeley-abc/abc),
[OpenROAD](https://theopenroadproject.org/).
