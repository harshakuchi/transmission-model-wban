#include "ns3/core-module.h"
#include "ns3/lr-wpan-helper.h"
#include "ns3/lr-wpan-mac.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/lr-wpan-net-device.h"
#include "ns3/mac16-address.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace ns3;
using namespace ns3::lrwpan;

namespace {
constexpr uint32_t kHeaderBytes = 18;
constexpr double kBitRate = 250000.0;
constexpr double kRxPowerW = 0.0591; // approximate 3 V * 19.7 mA radio RX power
const std::array<std::string, 5> kTypes{"ECG", "SpO2", "Temperature", "BloodPressure", "Motion"};

struct Config {
    std::string mode{"baseline"};
    std::string output{"results/raw/run.csv"};
    double anomalyRate{0.10};
    double simulationTime{60.0};
    double packetInterval{1.0};
    double normalInterval{4.0};
    double anomalyInterval{0.01};
    double initialEnergy{10.0};
    double txPower{0.0522};
    std::string rawOutput{"results/raw/readings.csv"};
    std::string nodeEnergyOutput{"results/csv/node-energy.csv"};
    uint32_t numSensors{5};
    uint32_t packetSize{32};
    uint32_t seed{1};
};

class WbanHeader : public Header {
  public:
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("ns3::WbanHeader").SetParent<Header>().AddConstructor<WbanHeader>();
        return tid;
    }
    TypeId GetInstanceTypeId() const override { return GetTypeId(); }
    uint32_t GetSerializedSize() const override { return kHeaderBytes; }
    void Serialize(Buffer::Iterator i) const override {
        i.WriteHtonU32(sequence);
        i.WriteU8(sensorId);
        i.WriteU8(sensorType);
        i.WriteHtonU16(readingCenti);
        i.WriteU8(anomaly ? 1 : 0);
        i.WriteU8(priority);
        i.WriteU64(generationNs);
    }
    uint32_t Deserialize(Buffer::Iterator i) override {
        sequence = i.ReadNtohU32();
        sensorId = i.ReadU8();
        sensorType = i.ReadU8();
        readingCenti = i.ReadNtohU16();
        anomaly = i.ReadU8() != 0;
        priority = i.ReadU8();
        generationNs = i.ReadU64();
        return kHeaderBytes;
    }
    void Print(std::ostream& os) const override {
        os << "sensor=" << unsigned(sensorId) << " seq=" << sequence << " anomaly=" << anomaly;
    }
    uint32_t sequence{0};
    uint8_t sensorId{0};
    uint8_t sensorType{0};
    uint16_t readingCenti{0};
    bool anomaly{false};
    uint8_t priority{0};
    uint64_t generationNs{0};
};

class AnomalyDetector {
  public:
    static bool IsAbnormal(uint8_t type, double value) {
        switch (type) {
        case 0: return value < 50.0 || value > 120.0;                 // heart rate, bpm
        case 1: return value < 92.0;                                  // SpO2, percent
        case 2: return value < 35.0 || value > 38.0;                  // temperature, C
        case 3: return value < 90.0 || value > 140.0;                 // systolic proxy, mmHg
        default: return value > 2.5;                                  // motion magnitude proxy
        }
    }
};

struct Metrics {
    uint64_t generated{0}, anomaliesGenerated{0}, offered{0}, transmitted{0}, txFrames{0};
    uint64_t received{0}, anomaliesReceived{0}, rxFrames{0};
    double delaySum{0}, anomalyDelaySum{0}, txEnergy{0}, rxEnergy{0};
    std::vector<std::vector<uint32_t>> delivered;
    std::vector<double> txEnergyByNode, rxEnergyByNode;
    std::vector<uint64_t> rxFramesByNode;
};

Config g_config;
Metrics g_metrics;
Ptr<UniformRandomVariable> g_uniform;
uint32_t g_coordinatorId{0};
std::vector<Ptr<NetDevice>> g_devices;
std::vector<uint32_t> g_sequences;
std::vector<uint8_t> g_handles;
struct TxInfo { uint32_t sensor; uint32_t sequence; uint32_t bytes; };
std::map<std::pair<uint32_t, uint8_t>, TxInfo> g_pendingTx;
std::set<std::pair<uint32_t, uint32_t>> g_successfulRecords;
std::ofstream g_raw;

double Reading(uint8_t type, bool inject) {
    if (inject) {
        switch (type) {
        case 0: return 135.0 + g_uniform->GetValue(0.0, 18.0);
        case 1: return 82.0 + g_uniform->GetValue(0.0, 8.0);
        case 2: return 38.7 + g_uniform->GetValue(0.0, 1.2);
        case 3: return 155.0 + g_uniform->GetValue(0.0, 20.0);
        default: return 3.2 + g_uniform->GetValue(0.0, 2.0);
        }
    }
    switch (type) {
    case 0: return 65.0 + g_uniform->GetValue(0.0, 30.0);
    case 1: return 95.0 + g_uniform->GetValue(0.0, 4.5);
    case 2: return 36.2 + g_uniform->GetValue(0.0, 1.0);
    case 3: return 105.0 + g_uniform->GetValue(0.0, 25.0);
    default: return g_uniform->GetValue(0.0, 1.8);
    }
}

void SendMeasurement(uint32_t sensor, uint32_t seq, uint8_t type, double value, bool anomaly, bool repeated=false) {
    WbanHeader header;
    header.sequence = seq;
    header.sensorId = static_cast<uint8_t>(sensor);
    header.sensorType = type;
    header.readingCenti = static_cast<uint16_t>(std::max(0.0, std::min(655.35, value)) * 100.0);
    header.anomaly = anomaly;
    header.priority = anomaly ? 1 : 0;
    header.generationNs = static_cast<uint64_t>(Simulator::Now().GetNanoSeconds());
    Ptr<Packet> packet = Create<Packet>(g_config.packetSize > kHeaderBytes ? g_config.packetSize-kHeaderBytes : 0);
    packet->AddHeader(header);
    auto radio = DynamicCast<LrWpanNetDevice>(g_devices[sensor + 1]);
    McpsDataRequestParams request;
    request.m_dstPanId = 0x42;
    request.m_srcAddrMode = SHORT_ADDR;
    request.m_dstAddrMode = SHORT_ADDR;
    request.m_dstAddr = Mac16Address::GetBroadcast();
    request.m_txOptions = 0;
    request.m_msduHandle = g_handles[sensor + 1]++;
    const uint32_t nodeId = g_devices[sensor + 1]->GetNode()->GetId();
    g_pendingTx[{nodeId, request.m_msduHandle}] = {sensor, seq, packet->GetSize()};
    if (!repeated) g_metrics.offered++;
    radio->GetMac()->McpsDataRequest(request, packet);
}

void DataConfirm(uint32_t nodeId, McpsDataConfirmParams params) {
    const auto key = std::make_pair(nodeId, params.m_msduHandle);
    auto it = g_pendingTx.find(key);
    if (it == g_pendingTx.end()) return;
    if (params.m_status == MacStatus::SUCCESS) {
        const TxInfo info = it->second;
        const double airtime = (info.bytes + 6.0) * 8.0 / kBitRate;
        const double energy = g_config.txPower * airtime;
        const uint32_t nodeIndex = nodeId - g_coordinatorId;
        g_metrics.txFrames++;
        g_metrics.txEnergy += energy;
        if (nodeIndex < g_metrics.txEnergyByNode.size()) g_metrics.txEnergyByNode[nodeIndex] += energy;
        if (g_successfulRecords.insert({info.sensor, info.sequence}).second) g_metrics.transmitted++;
    }
    g_pendingTx.erase(it);
}

void SensorTick(uint32_t sensor) {
    const uint8_t type = static_cast<uint8_t>(sensor % kTypes.size());
    const bool injected = g_uniform->GetValue() < g_config.anomalyRate;
    const double value = Reading(type, injected);
    const bool anomaly = AnomalyDetector::IsAbnormal(type, value);
    const uint32_t seq = ++g_sequences[sensor];
    g_metrics.generated++;
    if (anomaly) g_metrics.anomaliesGenerated++;

    const bool send = g_config.mode == "baseline" || anomaly || ((seq - 1) % static_cast<uint32_t>(std::max(1.0, g_config.normalInterval / g_config.packetInterval)) == 0);
    if (send) {
        SendMeasurement(sensor, seq, type, value, anomaly);
        if (anomaly && g_config.mode == "proposed") {
            Simulator::Schedule(Seconds(g_config.anomalyInterval), &SendMeasurement, sensor, seq, type, value, anomaly, true);
        }
        g_raw << std::fixed << std::setprecision(6) << Simulator::Now().GetSeconds() << ',' << g_config.mode << ','
              << sensor << ',' << kTypes[type] << ',' << seq << ',' << value << ',' << (anomaly?1:0) << ',' << (send?1:0) << '\n';
    } else {
        g_raw << std::fixed << std::setprecision(6) << Simulator::Now().GetSeconds() << ',' << g_config.mode << ','
              << sensor << ',' << kTypes[type] << ',' << seq << ',' << value << ",0,0\n";
    }
    const double next = g_config.packetInterval + (sensor + 1) * 0.013;
    if (Simulator::Now().GetSeconds() + next < g_config.simulationTime) {
        Simulator::Schedule(Seconds(next), &SensorTick, sensor);
    }
}

void ReceiveMac(uint32_t receivingNode, McpsDataIndicationParams, Ptr<Packet> packet) {
    const double airtime = (packet->GetSize() + 6.0) * 8.0 / kBitRate;
    const double rxEnergy = kRxPowerW * airtime;
    g_metrics.rxFrames++;
    const uint32_t nodeIndex = receivingNode - g_coordinatorId;
    if (nodeIndex < g_metrics.rxEnergyByNode.size()) {
        g_metrics.rxEnergy += rxEnergy;
        g_metrics.rxEnergyByNode[nodeIndex] += rxEnergy;
        g_metrics.rxFramesByNode[nodeIndex]++;
    }
    if (receivingNode != g_coordinatorId) return;
    Ptr<Packet> copy = packet->Copy();
    WbanHeader header;
    if (copy->GetSize() < kHeaderBytes) return;
    copy->RemoveHeader(header);
    if (header.sensorId >= g_metrics.delivered.size()) return;
    auto& seen = g_metrics.delivered[header.sensorId];
    if (std::find(seen.begin(), seen.end(), header.sequence) != seen.end()) return;
    seen.push_back(header.sequence);
    g_metrics.received++;
    const double delay = Simulator::Now().GetSeconds() - header.generationNs / 1e9;
    g_metrics.delaySum += delay;
    if (header.anomaly) {
        g_metrics.anomaliesReceived++;
        g_metrics.anomalyDelaySum += delay;
    }
}

void WriteSummary() {
    const double elapsed = g_config.simulationTime;
    const double energy = g_metrics.txEnergy + g_metrics.rxEnergy;
    const double remaining = std::max(0.0, g_config.initialEnergy * (g_config.numSensors + 1) - energy);
    const double pdr = g_metrics.offered ? static_cast<double>(g_metrics.received) / g_metrics.offered : 0.0;
    const double loss = g_metrics.offered - g_metrics.received;
    const double anomalies = g_metrics.anomaliesGenerated ? static_cast<double>(g_metrics.anomaliesReceived) / g_metrics.anomaliesGenerated : 0.0;
    const double meanDelay = g_metrics.received ? g_metrics.delaySum / g_metrics.received : 0.0;
    const double meanAnomalyDelay = g_metrics.anomaliesReceived ? g_metrics.anomalyDelaySum / g_metrics.anomaliesReceived : 0.0;
    const double throughput = g_metrics.received * g_config.packetSize * 8.0 / elapsed;
    std::ofstream out(g_config.output);
    if (!out) { std::cerr << "Cannot write CSV: " << g_config.output << '\n'; return; }
    out << "scenario,mode,anomalyRate,seed,energyConsumed,averageEnergyConsumed,remainingEnergy,packetsGenerated,packetsOffered,packetsTransmitted,packetsReceived,packetLoss,pdr,averageDelay,throughput,anomaliesGenerated,anomaliesReceived,anomalyDeliveryRatio,anomalyDelay,txFrames,rxFrames,retransmissions,initialEnergy,initialEnergyTotal\n";
    out << "wban," << g_config.mode << ',' << g_config.anomalyRate << ',' << g_config.seed << ',' << energy << ','
        << energy / (g_config.numSensors + 1) << ',' << remaining << ',' << g_metrics.generated << ',' << g_metrics.offered << ',' << g_metrics.transmitted << ',' << g_metrics.received << ',' << loss << ',' << pdr << ','
        << meanDelay << ',' << throughput << ',' << g_metrics.anomaliesGenerated << ',' << g_metrics.anomaliesReceived << ','
        << anomalies << ',' << meanAnomalyDelay << ',' << g_metrics.txFrames << ',' << g_metrics.rxFrames << ','
        << (g_metrics.txFrames - g_metrics.transmitted) << ',' << g_config.initialEnergy << ',' << g_config.initialEnergy * (g_config.numSensors + 1) << '\n';
    std::ofstream nodeOut(g_config.nodeEnergyOutput);
    nodeOut << "nodeId,role,initialEnergy,txEnergy,rxEnergy,energyConsumed,remainingEnergy,rxFrames\n";
    for (uint32_t i = 0; i < g_metrics.txEnergyByNode.size(); ++i) {
        const double nodeEnergy = g_metrics.txEnergyByNode[i] + g_metrics.rxEnergyByNode[i];
        nodeOut << i << ',' << (i == 0 ? "coordinator" : kTypes[(i - 1) % kTypes.size()]) << ','
                << g_config.initialEnergy << ',' << g_metrics.txEnergyByNode[i] << ',' << g_metrics.rxEnergyByNode[i] << ','
                << nodeEnergy << ',' << std::max(0.0, g_config.initialEnergy - nodeEnergy) << ',' << g_metrics.rxFramesByNode[i] << '\n';
    }
    std::cout << "mode=" << g_config.mode << " generated=" << g_metrics.generated << " sent=" << g_metrics.transmitted
              << " received=" << g_metrics.received << " anomalies=" << g_metrics.anomaliesGenerated
              << " anomaly_rx=" << g_metrics.anomaliesReceived << " energy_J=" << energy << " pdr=" << pdr << '\n';
}
} // namespace

int main(int argc, char** argv) {
    CommandLine cmd(__FILE__);
    cmd.AddValue("mode", "baseline or proposed", g_config.mode);
    cmd.AddValue("anomalyRate", "Probability that a reading is generated abnormally (0..1)", g_config.anomalyRate);
    cmd.AddValue("simulationTime", "Simulation duration in seconds", g_config.simulationTime);
    cmd.AddValue("numSensors", "Number of sensor nodes (type rotates over five sensors)", g_config.numSensors);
    cmd.AddValue("packetSize", "MAC payload size in bytes, including WBAN header", g_config.packetSize);
    cmd.AddValue("seed", "Reproducibility seed", g_config.seed);
    cmd.AddValue("packetInterval", "Sensor sampling period in seconds", g_config.packetInterval);
    cmd.AddValue("normalInterval", "Proposed normal-data report interval in seconds", g_config.normalInterval);
    cmd.AddValue("anomalyInterval", "Alarm retry spacing in seconds", g_config.anomalyInterval);
    cmd.AddValue("initialEnergy", "Per-node initial energy budget in joules", g_config.initialEnergy);
    cmd.AddValue("txPower", "Transmitter electrical power draw in watts", g_config.txPower);
    cmd.AddValue("output", "CSV summary output path", g_config.output);
    cmd.AddValue("rawOutput", "Per-reading CSV output path", g_config.rawOutput);
    cmd.AddValue("nodeEnergyOutput", "Per-node energy CSV output path", g_config.nodeEnergyOutput);
    cmd.Parse(argc, argv);
    if ((g_config.mode != "baseline" && g_config.mode != "proposed") || g_config.anomalyRate < 0 || g_config.anomalyRate > 1 ||
        g_config.numSensors == 0 || g_config.packetSize < kHeaderBytes || g_config.packetInterval <= 0 || g_config.simulationTime <= 0) {
        std::cerr << "Invalid configuration\n"; return 2;
    }
    RngSeedManager::SetSeed(g_config.seed);
    g_uniform = CreateObject<UniformRandomVariable>();
    g_metrics.delivered.resize(g_config.numSensors);
    g_metrics.txEnergyByNode.assign(g_config.numSensors + 1, 0.0);
    g_metrics.rxEnergyByNode.assign(g_config.numSensors + 1, 0.0);
    g_metrics.rxFramesByNode.assign(g_config.numSensors + 1, 0);
    g_sequences.assign(g_config.numSensors, 0);
    g_handles.assign(g_config.numSensors + 1, 0);

    NodeContainer nodes;
    nodes.Create(g_config.numSensors + 1); // coordinator first
    g_coordinatorId = nodes.Get(0)->GetId();
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0.0, 0.0, 0.0));
    for (uint32_t i=0; i<g_config.numSensors; ++i) {
        const double angle = 2.0 * M_PI * i / g_config.numSensors;
        positions->Add(Vector(0.45 * std::cos(angle), 0.45 * std::sin(angle), 0.0));
    }
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    LrWpanHelper lrWpan;
    NetDeviceContainer devices = lrWpan.Install(nodes);
    lrWpan.CreateAssociatedPan(devices, 0x42);
    g_devices.resize(nodes.GetN());
    for (uint32_t i=0; i<nodes.GetN(); ++i) {
        g_devices[i] = devices.Get(i);
        auto radio = DynamicCast<LrWpanNetDevice>(g_devices[i]);
        radio->GetMac()->SetMcpsDataIndicationCallback(MakeBoundCallback(&ReceiveMac, nodes.Get(i)->GetId()));
        radio->GetMac()->SetMcpsDataConfirmCallback(MakeBoundCallback(&DataConfirm, nodes.Get(i)->GetId()));
    }
    std::filesystem::create_directories(std::filesystem::path(g_config.output).parent_path());
    std::filesystem::create_directories(std::filesystem::path(g_config.rawOutput).parent_path());
    std::filesystem::create_directories(std::filesystem::path(g_config.nodeEnergyOutput).parent_path());
    g_raw.open(g_config.rawOutput);
    if (g_raw) g_raw << "time_s,mode,sensorId,sensorType,sequence,reading,anomaly,transmitted\n";
    for (uint32_t i=0; i<g_config.numSensors; ++i) Simulator::Schedule(Seconds(0.2 + 0.11*i), &SensorTick, i);
    Simulator::Stop(Seconds(g_config.simulationTime));
    Simulator::Run();
    Simulator::Destroy();
    WriteSummary();
    return 0;
}
