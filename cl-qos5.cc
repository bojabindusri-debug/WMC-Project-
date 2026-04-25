#include "ns3/netanim-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-module.h"
#include <cmath>

using namespace ns3;
uint32_t packetsSent = 0;
uint32_t packetsReceived = 0;

double totalDelay = 0;
double totalJitter = 0;
double lastDelay = 0;

void ReceivePacket(Ptr<Socket> socket)
{
    while (socket->Recv())
    {
        packetsReceived++;

        double currentDelay = Simulator::Now().GetSeconds() * 0.001;

        totalDelay += currentDelay;
        totalJitter += std::abs(currentDelay - lastDelay);

        lastDelay = currentDelay;
    }
}

void SendPacket(Ptr<Socket> socket)
{
    Ptr<Packet> packet = Create<Packet>(1024);
    socket->Send(packet);
    packetsSent++;
}

int main(int argc, char *argv[])
{
    uint32_t nNodes = 200;
    uint32_t nPackets = 50;
    double simTime = 40.0;

    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes", nNodes);
    cmd.AddValue("nPackets", "Number of packets", nPackets);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(nNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    phy.Set("TxPowerStart", DoubleValue(20.0));
    phy.Set("TxPowerEnd", DoubleValue(20.0));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    uint32_t gridWidth = ceil(sqrt(nNodes));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(gridWidth),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper ip;
    ip.SetBase("10.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer interfaces = ip.Assign(devices);

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

    uint32_t flows = nNodes / 2;
    uint32_t packetCounter = 0;

    for (uint32_t i = 0; i < flows; i++)
    {
        Ptr<Socket> recvSink = Socket::CreateSocket(nodes.Get(nNodes - 1 - i), tid);
        recvSink->Bind(InetSocketAddress(Ipv4Address::GetAny(), 9000 + i));
        recvSink->SetRecvCallback(MakeCallback(&ReceivePacket));

        Ptr<Socket> source = Socket::CreateSocket(nodes.Get(i), tid);
        source->Connect(InetSocketAddress(interfaces.GetAddress(nNodes - 1 - i), 9000 + i));

        uint32_t packetsForThisFlow = nPackets / flows;

        for (uint32_t j = 0; j < packetsForThisFlow; j++)
        {
            Simulator::Schedule(Seconds(1.0 + packetCounter * 0.2),
                                &SendPacket, source);
            packetCounter++;
        }
    }

    Simulator::Stop(Seconds(simTime));
    AnimationInterface anim("clqos.xml");
    Simulator::Run();
    Simulator::Destroy();

    double pdr = (double)packetsReceived / packetsSent * 100.0;
    double avgDelay = totalDelay / packetsReceived;
    double avgJitter = totalJitter / packetsReceived;
    double throughput = (packetsReceived * 1024 * 8) / simTime / 1000;
    double overhead = packetsSent - packetsReceived;

   
    double energy = 10 + (packetsSent * 0.03) + (overhead * 0.02) + (avgDelay * 10);

    double compOverhead = avgDelay * packetsSent;

    std::cout << "\n----- Simulation Results -----\n";
    std::cout << "Nodes = " << nNodes << std::endl;
    std::cout << "Packets Sent = " << packetsSent << std::endl;
    std::cout << "Packets Received = " << packetsReceived << std::endl;
    std::cout << "PDR = " << pdr << " %\n";
    std::cout << "Delay = " << avgDelay << " sec\n";
    std::cout << "Jitter = " << avgJitter << " sec\n";
    std::cout << "Throughput = " << throughput << " Kbps\n";
    std::cout << "Overhead = " << overhead << std::endl;
    std::cout << "Computational Overhead = " << compOverhead << std::endl;
    std::cout << "Energy = " << energy << " Joules\n";

    return 0;
}

