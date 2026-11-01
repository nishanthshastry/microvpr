# microvpr

A compact FPGA CAD flow in C++: takes a gate-level netlist (BLIF) and, stage
by stage, implements it on an idealized island-style FPGA fabric —
technology mapping to K-input LUTs, simulated-annealing placement,
PathFinder-style negotiated-congestion routing, and static timing
analysis. Named as a small-scale homage to
[VPR](https://docs.verilogtorouting.org/), the academic tool it imitates.

This repo currently implements **Milestones 1 and 2** of the roadmap
below: a BLIF parser, the netlist data model, a netlist evaluator that
serves as the golden model later stages get checked against, and now
technology mapping to K-input LUTs via cut enumeration, validated by
random-simulation equivalence against the original netlist.

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

Pass a second argument to also map to K-input LUTs and run a 1000-trial
random-simulation equivalence check against the original:

```bash
./microvpr_cli ../benchmarks/and_chain4.blif 4   # 3 chained AND gates -> collapses to 1 LUT at depth 1
./microvpr_cli ../benchmarks/and_chain4.blif 2   # same circuit, K too small to collapse -> stays 3 LUTs, depth 3
```

## Roadmap

1. **BLIF parser and netlist data model**, with a simulator for the
   netlist that serves as the golden model. *(done)*
2. **Technology mapping to K-LUTs**, checked for equivalence by random
   simulation against the input netlist. *(done — see `TechMapper`)*
3. Simulated-annealing placement with incremental HPWL updates, reported
   against a random-placement baseline.
4. Routing-resource graph and PathFinder router, reporting the minimum
   channel width at which each design routes.
5. Static timing analysis and a critical-path report.
6. Stretch: bitstream emitter and a C++ model of the fabric that
   reproduces the original circuit's behavior.

## Technology mapping notes

`TechMapper` enumerates K-feasible cuts per node (unioning one cut from
each fanin, bounded to K leaves), prunes to a bounded "priority cuts" set
per node (smallest depth, then smallest size), and picks each node's
minimum-depth cut — the FlowMap depth-optimal criterion. A backward walk
from the primary outputs (and every latch's data input) then selects only
the nodes actually needed as LUT boundaries, which is where the area
recovery comes from: nodes that get fully absorbed into a larger cut
never become their own LUT. Each fanin always has the option of being
absorbed (its own cut's leaves folded in) *or* kept as a single boundary
leaf (left to become its own LUT) — without that second option, depth
would be computed from gates that get folded away and never actually
materialized, undercounting real LUT depth once multiple gates collapse
into one. Latches are carried through unchanged; their output nets act as
pseudo primary inputs for mapping purposes, and their input nets are
always required nodes. A fuller area-recovery re-pass (re-choosing among
equal-depth cuts to shrink LUT count further once depth is fixed) is a
documented follow-up — this mapper's area reduction comes from the
required-node pruning alone.

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

## License

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) and [NOTICE](NOTICE).
