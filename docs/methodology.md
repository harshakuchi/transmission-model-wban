# Methodology

## Simulation steps

1. Create one coordinator and the requested number of sensor nodes.
2. Place them with constant-position mobility on a 0.45 m body-radius ring around the coordinator.
3. Install LR-WPAN devices and associate all nodes with PAN ID `0x42`.
4. Use the configured seed to generate synthetic measurements and anomaly decisions.
5. Apply `AnomalyDetector` thresholds by sensor type.
6. Apply the selected baseline or proposed transmission policy.
7. Parse `WbanHeader` at the coordinator MAC indication callback and deduplicate sequence IDs.
8. Compute packet metrics from unique generated/transmitted/received records and energy from actual TX/RX frame events.

## Synthetic sensor model

The five types cycle in this order: ECG (heart-rate proxy), SpO2, temperature, systolic blood-pressure proxy, and motion magnitude. Representative normal ranges are 65–95 bpm, 95–99.5%, 36.2–37.2 C, 105–130 mmHg, and 0–1.8 g. Abnormal values are selected beyond model thresholds: ECG below 50 or above 120, SpO2 below 92%, temperature below 35 or above 38 C, pressure below 90 or above 140, and motion above 2.5 g. These values are only simulation parameters and are not clinical rules.

The configured `anomalyRate` is the probability that each generated sample is selected as an abnormal case. With a seeded uniform random variable, observed counts can vary around the configured rate. When selected, the synthetic reading is drawn from the corresponding out-of-range interval.

## Transmission policies

Baseline transmits every sample once. Proposed mode transmits a normal report every fourth sample by default and does not forward intervening normal samples. Any detector-positive sample bypasses suppression and is sent immediately, followed by one repeat after `anomalyInterval`. A header priority bit distinguishes alarm frames. Duplicate receive records are discarded from unique delivery metrics.

## Metrics and denominators

PDR = unique sensor records received by the coordinator / unique records scheduled for transmission. Packet loss is the difference between those two values. Suppressed samples remain counted as generated readings, but they are not packet transmissions. Anomaly delivery ratio is unique anomaly records received / anomalies generated. Delay uses the application generation timestamp in `WbanHeader`. Throughput is delivered application bytes times eight divided by simulation duration.

The event energy estimate is `TX frame airtime × txPower + RX frame airtime × 0.0591 W`. Frame airtime uses the 250 kbit/s LR-WPAN PHY and includes six bytes for MAC overhead. This estimate intentionally excludes idle/listen/sensing/CPU energy; it must not be presented as a full battery discharge simulation.

## Reproducibility

Use the same seed, time, topology, packet size, and channel configuration across a baseline/proposed pair. The automation defaults to seeds 1, 2, and 3 for each configured anomaly rate. The seed and configuration are recorded in every summary row.
