# Architecture

```text
Synthetic sensor values
        |
        v
Anomaly generator ---> AnomalyDetector ---> policy selection
                                         /                  \
                               baseline: every sample    proposed: sparse normal
                                                       + immediate/repeated alarm
                                          \                /
                                           802.15.4 LR-WPAN
                                                   |
                                           coordinator MAC RX
                                                   |
                                   deduplication, metrics, CSV
                                                   |
                                    experiment aggregation/plots
```

The coordinator is device index zero. Sensor nodes are distributed evenly around it at 0.45 m. All devices join one LR-WPAN PAN and sensor frames are broadcast to the coordinator. `WbanHeader` stores sequence number, sensor ID/type, integer hundredths reading, anomaly flag, priority flag, and generation time in nanoseconds.

`AnomalyDetector` is independent of `SensorTick` and the send policy, so another detector can replace the threshold implementation without changing the radio or accounting code. Radio callbacks feed measured frame counts and unique application deliveries into `Metrics`.

The included `Energy` module exists in the NS-3 build, but this LR-WPAN model does not have a compatible device-energy helper wired here. The project therefore uses documented event-based TX/RX energy accounting rather than implying battery-model integration.
