# Viva Questions and Answers

1. **What is a WBAN?** A Wireless Body Area Network connects sensors on or near a person's body to a coordinator.
2. **Why use a WBAN?** It can collect body-related signals and relay them to a nearby monitoring system.
3. **Why is energy efficiency important?** Small wearable sensors have limited battery capacity and should operate longer between charges.
4. **What is an anomaly here?** A synthetic sensor reading outside the model's configured normal range.
5. **How is an anomaly detected?** A separate threshold-based `AnomalyDetector` evaluates each generated value.
6. **Why use threshold detection?** It is simple, transparent, reproducible, and appropriate as a baseline detector.
7. **Are the thresholds medical rules?** No. They are simulation-only example values and cannot diagnose a person.
8. **What is NS-3?** An open-source discrete-event network simulator.
9. **Why use NS-3?** It models network devices, protocol behavior, mobility, events, and traces in a reproducible simulation.
10. **Which wireless protocol is used?** IEEE 802.15.4 LR-WPAN.
11. **Why LR-WPAN?** It is a built-in low-rate, low-power wireless model suitable for a small sensor network.
12. **How are nodes positioned?** The coordinator is at the center, and sensors are evenly distributed on a 0.45 m radius ring.
13. **What are the sensors?** ECG/heart-rate proxy, SpO2, temperature, blood-pressure proxy, and motion.
14. **How is energy calculated?** By multiplying actual TX/RX frame airtime by configured TX power and fixed RX power.
15. **Does this use the NS-3 energy source framework?** The build has that framework, but no compatible LR-WPAN device energy model is wired in this simulation.
16. **What is the baseline?** Every sampled sensor record is transmitted once.
17. **What is the proposed model?** Normal readings are suppressed except periodic reports; detected anomalies are sent immediately and repeated once.
18. **How does the proposal save energy?** It reduces normal-data frames and therefore modeled active radio airtime.
19. **Why are anomalies treated differently?** They are potentially important within the simulation and should bypass routine-data suppression.
20. **How is priority represented?** An anomaly priority bit is carried in the application header; there is no configured LR-WPAN MAC priority queue.
21. **What is PDR?** Unique sensor records received divided by unique records scheduled for transmission.
22. **What is packet loss?** Scheduled unique transmissions minus unique coordinator deliveries.
23. **What is delay?** Time from the reading's generation timestamp to its coordinator reception.
24. **What is throughput?** Delivered application payload bits divided by simulation duration.
25. **What is network lifetime?** The time until the network can no longer meet its communication goal; this project estimates active energy and does not simulate lifetime termination.
26. **What is the project's innovation?** A measurable, reproducible policy comparison that combines normal-data suppression with immediate duplicated anomaly reporting.
27. **What is anomaly rate?** The configured probability of selecting each synthetic sample as abnormal.
28. **Why use multiple seeds?** To reduce dependence on one pseudo-random sequence and show run-to-run variation.
29. **How are duplicate alarm frames handled?** They count as physical frames, but a sensor/sequence pair is counted once in unique packet delivery metrics.
30. **What are the limitations?** Synthetic data, simplified body propagation, no idle-energy accounting, no MAC priority queue, and limited battery modeling.
31. **How could machine learning be added?** Replace the `AnomalyDetector` threshold function with a validated classifier while keeping data generation, radio policy, and metrics separate.
32. **How could real medical datasets be incorporated?** Obtain an appropriately licensed and de-identified dataset, transform it into simulation traces, and avoid presenting simulator output as clinical validation.
33. **What does `normalInterval` control?** How often the proposed mode emits a normal-data report relative to the sampling interval.
34. **Why repeat an anomaly frame?** The extra frame provides a simple reliability attempt; it is not feedback-based ARQ.
35. **What does the energy result omit?** Idle listening, sensing, CPU processing, and nonlinear battery behavior.
36. **How do you reproduce a run?** Use the same NS-3 revision, command-line parameters, and seed; the summary records mode, anomaly rate, and seed.
