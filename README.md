# Adaptive Lifespan QoS Policy for Fast DDS under DDIL Conditions

A master's thesis project comparing **static QoS** against **network-aware adaptive QoS**
(Lifespan + Deadline) for [Fast DDS](https://fast-dds.docs.eprosima.com/) publishers operating
under **DDIL** (Disconnected, Disrupted, Intermittent, Limited) network conditions.

## Overview

Two DDS applications are compared under identical simulated network stress:

| | `dds_standard` | `dds_adaptive` |
|---|---|---|
| Reliability | `RELIABLE` | `BEST_EFFORT` |
| History | `KEEP_ALL` | `KEEP_LAST(10)` |
| Lifespan | Fixed (2000ms) | Dynamic, 200ms–5000ms |
| Deadline | Fixed (1000ms) | Dynamic, 200ms–2500ms |
| Network awareness | None | Real-time via feedback channel |

The adaptive publisher runs a `NetworkMonitor` that classifies the link into one of six stages
(Optimal → Extreme Blackout) based on a weighted **Network Stress Index (NSI)** combining latency,
packet loss, jitter, and throughput. As the stage changes, the publisher live-updates its
`DataWriter`'s Lifespan (shorter under stress, to discard stale data faster) and Deadline
(longer under stress, to avoid meaningless violation reports).

See [`NetworkMonitor_explained.md`](./NetworkMonitor_explained.md) for the full mechanism.

## Architecture

```
                 DDIL_Adaptive_Topic (BEST_EFFORT)
Publisher  ───────────────────────────────────────▶  Subscriber
(NetworkMonitor,                                      (measures real
 adaptive Lifespan                                      latency/jitter/
 + Deadline)                                            loss/throughput)
     ▲                                                       │
     └──────────────── DDIL_Feedback_Topic (RELIABLE) ───────┘
```

The subscriber measures real latency, jitter, loss (via sequence-gap detection), and throughput
(sliding 2s window) from the data it actually receives, and reports these back to the publisher
over a reliable feedback channel. The publisher's `NetworkMonitor` consumes this feedback to
compute NSI and drive QoS adaptation.

## Requirements

- Linux (tested on Ubuntu)
- [Fast DDS](https://fast-dds.docs.eprosima.com/) and its dependencies
- CMake ≥ 3.10, a C++14 compiler
- Python 3 (for the DDIL network simulation script)
- `iproute2` (`tc`) for network emulation — requires `sudo`

## Build

```bash
mkdir build && cd build
cmake ..
make
```

This produces two binaries: `build/dds_standard` and `build/dds_adaptive`.

## Manual run

Each binary takes a single argument, `subscriber` or `publisher`. Run the subscriber first:

```bash
./build/dds_adaptive subscriber
# in another terminal:
./build/dds_adaptive publisher
```

## Automated DDIL simulation

`ddil_simulation.py` drives both binaries through a scripted sequence of network conditions
using `tc netem`, running each scenario `NUM_RUNS` times and writing per-run logs and a
`summary.csv` to `/tmp/dds_logs/`.

```bash
sudo python3 ddil_simulation.py
```

### Simulated stages

| Stage | Delay | Loss | Jitter | Rate | Duration |
|---|---|---|---|---|---|
| 1: Optimal | 0ms | 0% | 0ms | — | 5s |
| 2: Mild DDIL | 100ms | 5% | 15ms | 10mbit | 5s |
| 3: Moderate DDIL | 250ms | 15% | 35ms | 4mbit | 5s |
| 4: High DDIL | 450ms | 30% | 65ms | 1mbit | 5s |
| 5: Severe DDIL | 700ms | 50% | 180ms | 512kbit | 5s |
| 6: Extreme Blackout | 1200ms | 85% | 180ms | 128kbit | 5s |
| Intermittent Connectivity | toggles 100%↔0% loss, 6× | — | — | — | 9s |
| Blackout Probe | 1500ms | 99% | 200ms | 64kbit | 8s |
| Recovery | 0ms | 0% | 0ms | — | 10s |

Total: 57s per scenario run, matched to the publisher's fixed 190ms per-packet interval
(300 packets × 190ms = 57s) so that every stage carries live traffic.

## Output

- `/tmp/dds_logs/{mode}_run{N}.log` — subscriber log (per-packet latency/jitter/loss/throughput)
- `/tmp/dds_logs/{mode}_publisher_run{N}.log` — publisher log (Stage/NSI/Lifespan/Deadline transitions)
- `/tmp/dds_logs/summary.csv` — per-run delivery summary (`mode,run,received,expected,completed,drain_wait_seconds`)

Use `generate_thesis_plots.py` to turn these logs into comparison charts.

## Known limitations

- The feedback channel itself travels over the same degraded network, so under near-total
  packet loss the publisher's `NetworkMonitor` can be starved of fresh measurements and may
  lag behind real conditions.
- `RELIABLE` delivery guarantees no loss but not in-order arrival (default `DestinationOrder`
  is `BY_RECEPTION_TIMESTAMP`), so under retransmission backlog, newer samples can arrive
  before older ones.

## License

Add your preferred license here (e.g. MIT) before publishing.
