// 802.11 DCF simulation reference code
// OK for ns-3.37

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/gnuplot.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include <cmath>

#define PI 3.14159265

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MyDcfExample");

int
main(int argc, char* argv[])
{
    // default values
    uint32_t nWifi = 20;
    uint32_t cwmin = 3;
    uint32_t cwmax = 255;
    std::string arrivalInterval = "0.001";
    uint32_t payloadSize = 1900; // bytes
    double simulationTime = 3; // seconds

    // configure command line arguments
    CommandLine cmd;
    cmd.AddValue("nWifi", "Number of devices", nWifi);
    cmd.AddValue("cwmin", "Minimum contention window size", cwmin);
    cmd.AddValue("cwmax", "Maximum contention window size", cwmax);
    cmd.AddValue("arrivalInterval", "STA packet arrival interval", arrivalInterval);
    cmd.AddValue("payloadSize", "Payload size", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time", simulationTime);
    cmd.Parse(argc, argv);

    std::cout << "Arguments: " << std::endl;
    std::cout << "\tnWifi = " << nWifi << std::endl;
    std::cout << "\tcwmin = " << cwmin << std::endl;
    std::cout << "\tcwmax = " << cwmax << std::endl;
    std::cout << "\tarrivalInterval = " << arrivalInterval << " s" << std::endl;
    std::cout << "\tpayloadSize = " << payloadSize << " bytes" << std::endl;
    std::cout << "\tsimulationTime = " << simulationTime << " s" << std::endl;
    std::cout << std::endl;

    // create station node objects and 1 access point node object
    NodeContainer wifiStaNode;
    wifiStaNode.Create(nWifi);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // physical layer framework to use: Yans
    // create a channel helper and phy helper, and then create the channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper();
    phy.SetChannel(channel.Create());

    // create a mac helper, which is reused across STA and AP configurations
    WifiMacHelper mac;

    // create a wifi helper, which will use the above helpers to create and install Wifi devices.
    WifiHelper wifi;
    // configure a Wifi standard to use, which will align various parameters
    // in the Phy and Mac to standard defaults.
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager(
        "ns3::ConstantRateWifiManager",
        "DataMode",
        StringValue("OfdmRate54Mbps"),
        "ControlMode",
        StringValue("OfdmRate6Mbps"));

    // declare NetDeviceContainers to hold the container returned by the helper
    NetDeviceContainer staDevice;
    NetDeviceContainer apDevice;

    // perform the installation
    Ssid ssid = Ssid("ns3-80211a");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    staDevice = wifi.Install(phy, mac, wifiStaNode);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    apDevice = wifi.Install(phy, mac, wifiApNode);

//    // set contention window size, overriding previous values
//    Ptr<NetDevice> netDev;
//    for (NetDeviceContainer::Iterator i = staDevice.Begin(); i != staDevice.End(); ++i)
//    {
//        netDev = (*i);
//        Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice>(netDev);
//        Ptr<Txop> devTxop = dev->GetMac()->GetTxop();
//        devTxop->SetMaxCw(cwmax);
//        devTxop->SetMinCw(cwmin);
//    }
//    for (NetDeviceContainer::Iterator i = apDevice.Begin(); i != apDevice.End(); ++i)
//    {
//        netDev = (*i);
//        Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice>(netDev);
//        Ptr<Txop> devTxop = dev->GetMac()->GetTxop();
//        devTxop->SetMaxCw(cwmax);
//        devTxop->SetMinCw(cwmin);
//    }

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Txop/MinCw",
                UintegerValue(cwmin));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Txop/MaxCw",
                UintegerValue(cwmax));

    // configure mobility
    // set mobility model as constant position
    // AP: at the center of the circle
    // STAs: on the circle
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP
    float rho = 0.01;
    for (double i = 0; i < nWifi; i++)
    {
        double theta = i * 2 * PI / nWifi;
        positionAlloc->Add(Vector(rho * cos(theta), rho * sin(theta), 0.0)); // STA
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode); // AP
    mobility.Install(wifiStaNode); // STA

    // add internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staNodeInterface;
    Ipv4InterfaceContainer apNodeInterface;
    staNodeInterface = address.Assign(staDevice);
    apNodeInterface = address.Assign(apDevice);

    // UDP flow applications
    ApplicationContainer serverApp; // for AP
    ApplicationContainer clientApp[nWifi]; // for STAs
    uint16_t port = 8000;
    UdpServerHelper server(port);
    serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime + 1));
    UdpClientHelper client(apNodeInterface.GetAddress(0), port);
    // record attributes to be set in each UDP client application after it is created
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(Time(arrivalInterval)));
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));
    for (uint32_t i = 0; i <= nWifi - 1; i++)
    {
        clientApp[i] = client.Install(wifiStaNode.Get(i));
        clientApp[i].Start(Seconds(1.0));
        clientApp[i].Stop(Seconds(simulationTime + 1));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simulationTime + 1));

    Simulator::Run();

    uint64_t totalPacketsThrough =
        DynamicCast<UdpServer>(serverApp.Get(0))->GetReceived();
    std::cout << "Total packets received by AP: " << totalPacketsThrough << std::endl;
    double throughput =
        totalPacketsThrough * payloadSize * 8 / (simulationTime * 1000000.0);
    std::cout << "Throughput: " << throughput << " Mbit/s" << std::endl;

    Simulator::Destroy();

    return 0;
}