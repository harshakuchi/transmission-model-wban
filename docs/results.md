# Results

This report is populated from the actual CSV files produced by this project. Run `scripts/run_experiments.sh` to produce the full set, then `python3 scripts/plot_results.py` to refresh charts.

## Smoke execution

The short baseline and proposed smoke runs used NS-3.47 development revision `ns-3.47-5-g8fbdc74fa`, 5 sensors, 5 seconds, seed 7, and configured anomaly rate 0.20. Both generated 25 readings and delivered all five anomalies. Baseline scheduled/delivered 25/25 records and used 0.0105701 J. Proposed scheduled/delivered 13/13 records, used 0.00761046 J, and sent five repeated alarm frames. Energy includes broadcast reception activity at all receiving nodes.

## Full suite

The automation covers anomaly rates 0%, 5%, 10%, 20%, and 30%, both modes, and three seeds per configuration (30 actual runs, 60 s each). The table shows arithmetic means across the three seeds; energy and remaining energy are in joules.

| Anomaly rate | Mode | Mean energy | Mean PDR | Mean delay (s) | Mean throughput (bit/s) | Mean anomaly delivery |
|---:|---|---:|---:|---:|---:|---:|
| 0% | Baseline | 0.122326 | 0.997701 | 0.003014 | 1234.49 | N/A (none generated) |
| 0% | Proposed | 0.031287 | 1.000000 | 0.002989 | 315.73 | N/A (none generated) |
| 5% | Baseline | 0.122326 | 0.997701 | 0.003014 | 1234.49 | 1.000000 |
| 5% | Proposed | 0.040730 | 1.000000 | 0.003043 | 355.56 | 1.000000 |
| 10% | Baseline | 0.122326 | 0.997701 | 0.003014 | 1234.49 | 1.000000 |
| 10% | Proposed | 0.052569 | 1.000000 | 0.003058 | 409.60 | 1.000000 |
| 20% | Baseline | 0.122326 | 0.997701 | 0.003014 | 1234.49 | 0.994709 |
| 20% | Proposed | 0.074977 | 1.000000 | 0.002991 | 507.73 | 1.000000 |
| 30% | Baseline | 0.122326 | 0.997701 | 0.003014 | 1234.49 | 0.995984 |
| 30% | Proposed | 0.094426 | 1.000000 | 0.003062 | 588.80 | 1.000000 |

Compared with baseline, mean active-event energy was lower by 74.4%, 66.7%, 57.0%, 38.7%, and 22.8% for 0%, 5%, 10%, 20%, and 30% anomaly rates respectively. Baseline seed 1 had two lost records at each rate because of LR-WPAN contention; at 20% and 30%, one of those losses was an anomaly. Proposed mode delivered all anomaly records in these runs, with the cost of alarm repeat frames. At 0% the anomaly delivery metric is undefined; CSV stores zero because its denominator is zero.

`comparison_results.csv` is the source of truth; plots are derived from its rows. Per-run and per-node energy CSVs are also retained under `results/csv/`.

## Interpretation

At low rates, the proposed mode is expected to use fewer radio frames because it suppresses most routine reports. At larger anomaly rates, it sends more records and duplicate alert frames, so the energy advantage can narrow. Delivery and delay depend on the actual MAC callbacks and topology, not on assumed outcomes. Since idle energy is excluded, the energy result compares active TX/RX airtime only.
