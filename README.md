# An Anomaly-Based Low-Energy Transmission Model for Wireless Body Area Networks

## Abstract

This project models five synthetic body sensors and a coordinator using NS-3's IEEE 802.15.4 LR-WPAN module. A threshold detector labels generated samples. The baseline sends every sample; the proposed policy sends abnormal samples at once and reduces routine traffic by reporting one in every four normal samples. A second alarm frame is sent shortly after an alarm to improve its delivery chance. The simulation records real MAC indications and estimates radio energy from actual frame activity.

All readings are generated for simulation. Thresholds are configurable model examples, not clinical diagnostic rules.

## Problem and motivation

Body sensors are battery constrained, while an alert may need to reach a coordinator promptly. Sending every routine sample spends airtime and energy. This study evaluates a simple policy that reduces routine reporting while keeping abnormal readings on the fast alert path.

## Objectives

- Simulate ECG, SpO2, temperature, blood pressure, and motion sensors around a coordinator.
- Generate seeded synthetic normal and abnormal measurements at a configurable rate.
- Compare periodic baseline reporting with anomaly-aware reporting.
- Measure delivery, delay, throughput, alarm delivery, packet counts, and event-based radio energy.
- Produce reproducible CSV summaries and plots.

## Architecture and methodology

The coordinator is placed at the body center; sensor nodes are placed on a 45 cm radius ring. All nodes join one 802.15.4 PAN. Sensor readings are sampled every second by default. The sensor type cycles through five example types if `numSensors` differs from five. `WbanHeader` carries sensor ID/type, sequence, scaled reading, anomaly flag, priority, and generation timestamp.

`AnomalyDetector` is a separate threshold component. A configured Bernoulli draw selects whether a reading should be abnormal; the generated value is then placed outside its type's threshold. A normal value is drawn from the modeled normal range. These are synthetic simulation rules only.

### Baseline

Every generated reading is sent once using the same LR-WPAN PAN, packet size, sampling schedule, and channel conditions as the proposed mode.

### Proposed transmission policy

Every abnormal sample is sent immediately when generated and is sent a second time after `anomalyInterval` (0.01 s by default). Its header carries the high-priority flag. Normal samples are suppressed except for the first sample and one report per `normalInterval` window (default: every fourth sample with 1 s sampling). This saves radio frames while maintaining occasional normal telemetry.

The LR-WPAN MAC itself has no anomaly-priority queue configured in this project. Priority is explicitly marked in the application header; promptness comes from sending alarms at generation time and alarm duplication, not from claiming a MAC queue scheduler.

## Energy accounting

The available LR-WPAN module does not expose a compatible radio `DeviceEnergyModel` in this build. Energy is therefore calculated from actual transmitted MAC frames and coordinator MAC data indications. Each TX frame uses its payload plus the modeled 6-byte 802.15.4 MAC overhead at the 250 kbit/s PHY rate and the configured `txPower` (electrical power draw in watts); each received frame uses 0.0591 W. The node's initial energy budget is used to calculate total remaining energy. Idle/listening energy and battery discharge nonlinearities are excluded, so the result is radio active-event energy rather than a full battery lifetime prediction. `initialEnergy` applies separately to every node.

## Metrics

CSV summaries report energy consumed and per-node average, remaining and initial energy, generated readings, unique records transmitted and received, packet loss, PDR, average end-to-end delay, application throughput, anomaly counts and delivery ratio, anomaly delay, physical TX/RX frames, and alarm repeat count (`retransmissions`). Suppressed normal readings count as generated readings but are excluded from the offered-packet PDR denominator. Duplicate alarm deliveries are deduplicated by sensor and sequence number.

## Build and run

This workspace uses an existing NS-3 development tree at `/home/harsha/ns-3-dev`, identified as `ns-3.47-5-g8fbdc74fa`. Its LR-WPAN and energy modules are built. The implementation is standard C++20 linked against those libraries.

```bash
cmake -S . -B build -DNS3_ROOT=/home/harsha/ns-3-dev
cmake --build build -j2
./build/wban-anomaly-low-energy --mode=baseline --anomalyRate=0.10 --simulationTime=10 --seed=1 --output=results/csv/single.csv --rawOutput=results/raw/single.csv
./build/wban-anomaly-low-energy --mode=proposed --anomalyRate=0.10 --simulationTime=10 --seed=1 --output=results/csv/proposed.csv --rawOutput=results/raw/proposed.csv
```

Run the full experiment suite (five anomaly rates, both modes, seeds 1–3):

```bash
scripts/run_experiments.sh
```

Optional overrides: `NS3_ROOT`, `SIMULATION_TIME`, `SEEDS` (for example `SEEDS="1 2 3 4 5"`), and `JOBS`. The suite creates `baseline_results.csv`, `proposed_results.csv`, and `comparison_results.csv` from simulation output.

Plot the resulting CSV:

```bash
python3 scripts/plot_results.py
```

The simulation accepts `mode`, `anomalyRate`, `simulationTime`, `numSensors`, `packetSize`, `seed`, `packetInterval`, `normalInterval`, `anomalyInterval`, `initialEnergy`, `txPower`, `output`, and `rawOutput`. Pass values with `--name=value`.

## Results

See [docs/results.md](docs/results.md) for the measured smoke and experiment results. Re-run the suite to regenerate the CSV data and plots.

## Limitations

- Synthetic readings and thresholds are not medical-grade data or clinical decision rules.
- The current LR-WPAN setup uses broadcast MAC delivery and has no routing, interference model tailored to a body, or link-layer anomaly priority queue.
- Alarm duplication is the implemented delivery enhancement; it is not adaptive ARQ based on a feedback channel.
- Energy excludes idle listening, sensing, processor use, and detailed battery effects.
- MAC frame loss is represented through actual receive callbacks; the default close-in body topology tends to have high PDR.
- Throughput is application payload bits delivered per simulation second.

## Future work

Add an LR-WPAN radio state energy model, body-channel measurements, explicit coordinator acknowledgements and retries, adaptive sampling, aggregation, more realistic medical datasets under appropriate approvals, confidence-aware anomaly detection, and machine-learning detectors trained and validated separately from the test runs.

## Viva summary

The contribution is a reproducible simulation comparison: suppress routine sensor reports to reduce active radio energy, while immediately forwarding and duplicating abnormal reports. The measured results distinguish generated readings from transmitted records and use actual MAC receive events for delivery metrics. See [docs/viva_questions.md](docs/viva_questions.md) for preparation questions.
