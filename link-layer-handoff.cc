/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 *
 *
 * Author: Fahim Masud Choudhury <fahim.cuet@gmail.com>
 *         Hasanuzzaman Noor     <zaman.cuet@gmail.com>
 *         Md. Monowar Hossain   <murad0904045@gmail.com>
 */

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mesh-helper.h"
#include "ns3/flow-monitor-module.h"
#include <iomanip>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <ns3/flow-monitor-helper.h>
#include "ns3/gnuplot.h"
#include "ns3/netanim-module.h"
#include "src/point-to-point/helper/point-to-point-helper.h"
#include "src/csma/helper/csma-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "src/network/model/packet-metadata.h"
#include "mesh.h"

#include <iostream>
#include <sstream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("infrastructure-mesh");
void ThroughputMonitor (FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon,Gnuplot2dDataset DataSet);
//NS_LOG_COMPONENT_DEFINE ("TestMeshScript");

// Method for setting mobility using (x,y) position for the nodes

static void
SetPosition (Ptr<Node> node, double x, double y)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  Vector pos = mobility->GetPosition ();
  pos.x = x;
  pos.y = y;
  mobility->SetPosition (pos);
}

class MeshTest
{
public:
  /// Init test
  MeshTest ();
  /// Configure test from command line arguments
  void Configure (int argc, char ** argv);
  /// Run test
  int Run ();
private:
  int m_backbone;
  int m_gw;
  int m_mr;
  int m_ap;
  int m_sta;
  double m_step;
  double m_randomStart;
  double m_totalTime;
  double m_packetInterval;
  uint16_t m_packetSize;
  uint32_t m_nIfaces;
  bool m_chan;
  bool m_pcap;
  std::string m_stack;
  std::string m_root;

  /// NodeContainer for individual nodes
  NodeContainer nc_sta1, nc_sta2;
  NodeContainer nc_ap1, nc_ap2, nc_ap3;
  NodeContainer nc_mr1, nc_mr2;
  NodeContainer nc_gw1, nc_gw2;
  NodeContainer nc_bb1;

  // NodeContainer for categorical nodes
  NodeContainer nc_sta, nc_ap;

  // NodeContainer for connected nodes
  NodeContainer nc_sta1Ap1, nc_sta2Ap2, nc_sta1Ap1Ap3;
  NodeContainer nc_ap1Mr1, nc_ap2Mr2, nc_ap1Ap3Mr1;
  NodeContainer nc_mr1Gw1, nc_mr2Gw2;
  NodeContainer nc_gw1Bb1, nc_gw2Bb1;


  // List of categorical NetDevice Container
  NetDeviceContainer de_sta1, de_sta2;
  NetDeviceContainer de_ap1, de_ap2, de_ap3;

  // List of WiFi NetDevice Container
  NetDeviceContainer de_wifi_sta1Ap1Ap3;
  NetDeviceContainer de_wifi_sta2Ap2;

  // List of mesh NetDevice Container
  NetDeviceContainer de_mesh_mr1Gw1;
  NetDeviceContainer de_mesh_mr2Gw2;

  // List of CSMA NetDevice Container
  NetDeviceContainer de_csma_ap1Ap3Mr1;
  NetDeviceContainer de_csma_ap2Mr2;

  // List of p2p NetDevice Container
  NetDeviceContainer de_p2p_gw1Bb1;
  NetDeviceContainer de_p2p_gw2Bb1;

  // List of interface container
  Ipv4InterfaceContainer if_wifi_sta1Ap1Ap3;
  Ipv4InterfaceContainer if_wifi_sta2Ap2;

  Ipv4InterfaceContainer if_mesh_mr1Gw1;
  Ipv4InterfaceContainer if_mesh_mr2Gw2;

  Ipv4InterfaceContainer if_csma_ap1Ap3Mr1;
  Ipv4InterfaceContainer if_csma_ap2Mr2;

  Ipv4InterfaceContainer if_p2p_gw1Bb1;
  Ipv4InterfaceContainer if_p2p_gw2Bb1;

  // Helper
  MeshHelper meshHelper;
  PointToPointHelper p2pHelper;
  CsmaHelper csmaHelper;
  Ipv4AddressHelper address;

private:
  /// Create nodes and setup their mobility
  void CreateNodes ();
  /// Install internet m_stack on nodes
  void InstallInternetStack ();
  /// Setup mobility
  void SetupMobility ();
  /// Install applications
  void InstallUdpApplication ();
  /// Print mesh devices diagnostics
  void Report ();
};

MeshTest::MeshTest () :
m_backbone (1),
m_gw (2),
m_mr (2),
m_ap (3),
m_sta (2),
m_step (30.0),
m_randomStart (0.1),
m_totalTime (100.0),
m_packetInterval (1.0),
m_packetSize (1024),
m_nIfaces (1),
m_chan (true),
m_pcap (false),
m_stack ("ns3::Dot11sStack"),
m_root ("ff:ff:ff:ff:ff:ff") { }

void
MeshTest::Configure (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.AddValue ("step", "Size of edge in our grid, meters. [100 m]", m_step);
  /*
   * As soon as starting node means that it sends a beacon,
   * simultaneous start is not good.
   */
  cmd.AddValue ("start", "Maximum random start delay, seconds. [0.1 s]", m_randomStart);
  cmd.AddValue ("time", "Simulation time, seconds [100 s]", m_totalTime);
  cmd.AddValue ("packet-interval", "Interval between packets in UDP ping, seconds [0.001 s]", m_packetInterval);
  cmd.AddValue ("packet-size", "Size of packets in UDP ping", m_packetSize);
  cmd.AddValue ("interfaces", "Number of radio interfaces used by each mesh point. [1]", m_nIfaces);
  cmd.AddValue ("channels", "Use different frequency channels for different interfaces. [0]", m_chan);
  cmd.AddValue ("pcap", "Enable PCAP traces on interfaces. [0]", m_pcap);
  cmd.AddValue ("stack", "Type of protocol stack. ns3::Dot11sStack by default", m_stack);
  cmd.AddValue ("root", "Mac address of root mesh point in HWMP", m_root);

  cmd.Parse (argc, argv);
}

void
MeshTest::CreateNodes ()
{
  // Create individual nodes in their node container
  nc_sta1.Create (1);
  nc_sta2.Create (1);
  nc_ap1.Create (1);
  nc_ap2.Create (1);
  nc_ap3.Create (1);
  nc_mr1.Create (1);
  nc_mr2.Create (1);
  nc_gw1.Create (1);
  nc_gw2.Create (1);
  nc_bb1.Create (1);

  // Create categorical Node Container
  nc_sta = NodeContainer (nc_sta1, nc_sta2);
  nc_ap = NodeContainer (nc_ap1, nc_ap2, nc_ap3);

  // Create connected nodes in their node container
  nc_sta1Ap1Ap3 = NodeContainer (nc_sta1, nc_ap1, nc_ap3);
  nc_sta2Ap2 = NodeContainer (nc_sta2, nc_ap2);

  nc_ap1Ap3Mr1 = NodeContainer (nc_ap1, nc_ap3, nc_mr1);
  nc_ap2Mr2 = NodeContainer (nc_ap2, nc_mr2);

  nc_mr1Gw1 = NodeContainer (nc_mr1, nc_gw1);
  nc_mr2Gw2 = NodeContainer (nc_mr2, nc_gw2);

  nc_gw1Bb1 = NodeContainer (nc_gw1, nc_bb1);
  nc_gw2Bb1 = NodeContainer (nc_gw2, nc_bb1);


  // Create p2p links between backbone (bb1) and gateways (gw1, gw2)
  p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2pHelper.SetChannelAttribute ("Delay", StringValue ("10ms"));
  de_p2p_gw1Bb1 = p2pHelper.Install (nc_gw1Bb1);
  de_p2p_gw2Bb1 = p2pHelper.Install (nc_gw2Bb1);

  // Create CSMA connection between MRs (mr1, mr2) and APs (ap1, ap2)
  csmaHelper.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csmaHelper.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  de_csma_ap1Ap3Mr1 = csmaHelper.Install (nc_ap1Ap3Mr1);
  de_csma_ap2Mr2 = csmaHelper.Install (nc_ap2Mr2);


  // Configure YansWifiChannel
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  // ---------------------- Setup Mesh Network-----------------------------------

  /*
   * Create mesh helper and set stack installer to it
   * Stack installer creates all needed protocols and install them to
   * mesh point device
   */
  meshHelper = MeshHelper::Default ();
  if (!Mac48Address (m_root.c_str ()).IsBroadcast ())
    {
      meshHelper.SetStackInstaller (m_stack, "Root", Mac48AddressValue (Mac48Address (m_root.c_str ())));
    }
  else
    {
      //If root is not set, we do not use "Root" attribute, because it
      //is specified only for 11s
      meshHelper.SetStackInstaller (m_stack);
    }
  if (m_chan)
    {
      meshHelper.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
    }
  else
    {
      meshHelper.SetSpreadInterfaceChannels (MeshHelper::ZERO_CHANNEL);
    }
  meshHelper.SetMacType ("RandomStart", TimeValue (Seconds (m_randomStart)));
  // Set number of interfaces - default is single-interface mesh point
  meshHelper.SetNumberOfInterfaces (m_nIfaces);
  // Install protocols and return container if MeshPointDevices
  de_mesh_mr1Gw1 = meshHelper.Install (wifiPhy, nc_mr1Gw1);
  de_mesh_mr2Gw2 = meshHelper.Install (wifiPhy, nc_mr2Gw2);

  
  // -------------------------Setup WiFi for network 1--------------------------------
  WifiHelper wifi1 = WifiHelper::Default ();
  wifi1.SetRemoteStationManager ("ns3::AarfWifiManager");

  NqosWifiMacHelper mac1 = NqosWifiMacHelper::Default ();

  // STA1 and AP1, AP3 are initialized for network 1
  Ssid ssid1 = Ssid ("WiFi Network 1");
  mac1.SetType ("ns3::StaWifiMac",
                "Ssid", SsidValue (ssid1),
                "ActiveProbing", BooleanValue (false));

  de_sta1 = wifi1.Install (wifiPhy, mac1, nc_sta1);

  // Setup AP1 and AP3 for network 1
  mac1.SetType ("ns3::ApWifiMac",
                "Ssid", SsidValue (ssid1),
                "BeaconGeneration",BooleanValue (true));

  de_ap1 = wifi1.Install (wifiPhy, mac1, nc_ap1);
  de_ap3 = wifi1.Install (wifiPhy, mac1, nc_ap3);


  // ---------------------------Setup WiFi for network 2-------------------------------
  WifiHelper wifi2 = WifiHelper::Default ();
  wifi2.SetRemoteStationManager ("ns3::AarfWifiManager");

  NqosWifiMacHelper mac2 = NqosWifiMacHelper::Default ();

  // STA2 and AP2 are initialized for network 2
  Ssid ssid2 = Ssid ("network-2");
  mac2.SetType ("ns3::StaWifiMac",
                "Ssid", SsidValue (ssid2),
                "ActiveProbing", BooleanValue (false));

  de_sta2 = wifi2.Install (wifiPhy, mac2, nc_sta2);

  // Setup AP2 for network 2
  mac2.SetType ("ns3::ApWifiMac",
                "Ssid", SsidValue (ssid2),
                "BeaconGeneration",BooleanValue (true));

  de_ap2 = wifi2.Install (wifiPhy, mac2, nc_ap2);

  // Setup WiFi for network 3
  WifiHelper wifi3 = WifiHelper::Default ();
  wifi3.SetRemoteStationManager ("ns3::AarfWifiManager");

  NqosWifiMacHelper mac3 = NqosWifiMacHelper::Default ();

  // Net Device container for STA1 and AP1, AP3 in WiFi network 1
  de_wifi_sta1Ap1Ap3.Add (de_sta1);
  de_wifi_sta1Ap1Ap3.Add (de_ap1);
  de_wifi_sta1Ap1Ap3.Add (de_ap3);

  // Net Device container for STA2 and AP2 in WiFi network 2
  de_wifi_sta2Ap2.Add (de_sta2);
  de_wifi_sta2Ap2.Add (de_ap2);
  wifiPhy.EnablePcapAll("wifi-infrastructure-mesh");
}

void
MeshTest::SetupMobility ()
{
  // -------------------------------Setup mobility for the nodes---------------------
  MobilityHelper fixedMobility;
  fixedMobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue (0.0),
                                      "MinY", DoubleValue (0.0),
                                      "DeltaX", DoubleValue (m_step),
                                      "DeltaY", DoubleValue (m_step),
                                      "GridWidth", UintegerValue (5),
                                      "LayoutType", StringValue ("RowFirst"));
  fixedMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  // ------------------------Setup fixed position for the network nodes---------------- 
  fixedMobility.Install (nc_sta1);
  fixedMobility.Install (nc_ap1);
  fixedMobility.Install (nc_mr1);
  fixedMobility.Install (nc_gw1);
  fixedMobility.Install (nc_bb1);
  fixedMobility.Install (nc_sta2);
  fixedMobility.Install (nc_ap2);
  fixedMobility.Install (nc_mr2);
  fixedMobility.Install (nc_gw2);
  fixedMobility.Install (nc_ap3);

  // Position for AP3 and Backbone
  SetPosition (nc_ap3.Get (0), 30.0, -100.0);
  SetPosition (nc_bb1.Get (0), 120.0, 15.0);

  // ------------------------Setup mobility for the STA1 node--------------------------
  double startTime = 20.0;

  for (int sta1_x = 0, sta1_y = 0; sta1_y >= -150; sta1_x -= 1, sta1_y -= 5)
    {
      // Change position of STA1 after startTime
      Simulator::Schedule (Seconds (startTime), &SetPosition, nc_sta1.Get (0), sta1_x, sta1_y);
      // Change position of STA2 after startTime
      //Simulator::Schedule (Seconds (startTime), &SetPosition, nc_sta2.Get (0), 0.0, sta2_y);
      //sta2_y--;
      startTime++;
    }

}

void
MeshTest::InstallInternetStack ()
{

  InternetStackHelper internetStackHelper;
  OlsrHelper routingProtocol;
  internetStackHelper.SetRoutingHelper (routingProtocol);

  // ---------------------Setup internet stack on the nodes------------------------------
  internetStackHelper.Install (nc_sta1);
  internetStackHelper.Install (nc_sta2);
  internetStackHelper.Install (nc_ap1);
  internetStackHelper.Install (nc_ap2);
  internetStackHelper.Install (nc_ap3);
  internetStackHelper.Install (nc_mr1);
  internetStackHelper.Install (nc_mr2);
  internetStackHelper.Install (nc_gw1);
  internetStackHelper.Install (nc_gw2);
  internetStackHelper.Install (nc_bb1);

  // -------------------- IP address for Network 1 (left)---------------------------
  address.SetBase ("10.1.1.0", "255.255.255.0");
  if_wifi_sta1Ap1Ap3 = address.Assign (de_wifi_sta1Ap1Ap3);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  if_csma_ap1Ap3Mr1 = address.Assign (de_csma_ap1Ap3Mr1);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  if_mesh_mr1Gw1 = address.Assign (de_mesh_mr1Gw1);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  if_p2p_gw1Bb1 = address.Assign (de_p2p_gw1Bb1);



  // -------------------- IP address for Network 2 (right)--------------------------
  address.SetBase ("20.1.1.0", "255.255.255.0");
  if_wifi_sta2Ap2 = address.Assign (de_wifi_sta2Ap2);

  address.SetBase ("20.1.2.0", "255.255.255.0");
  if_csma_ap2Mr2 = address.Assign (de_csma_ap2Mr2);

  address.SetBase ("20.1.3.0", "255.255.255.0");
  if_mesh_mr2Gw2 = address.Assign (de_mesh_mr2Gw2);

  address.SetBase ("20.1.4.0", "255.255.255.0");
  if_p2p_gw2Bb1 = address.Assign (de_p2p_gw2Bb1);

}

void
MeshTest::InstallUdpApplication ()
{
  // ------------------Server is set on STA2 in network 2 (right)--------------------
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nc_sta2.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (m_totalTime));

  // ------------------Client is set on STA1 in network 1 (left)---------------------
  UdpEchoClientHelper echoClient (if_wifi_sta2Ap2.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue ((uint32_t) (m_totalTime * (1 / m_packetInterval))));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (m_packetInterval)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (m_packetSize));
  ApplicationContainer clientApps = echoClient.Install (nc_sta1.Get (0));
  clientApps.Start (Seconds (0.0));
  clientApps.Stop (Seconds (m_totalTime));
}

int
MeshTest::Run ()
{

  CreateNodes ();
  InstallInternetStack ();
  SetupMobility ();
  InstallUdpApplication ();
  //Gnuplot parameters
      
    std::string fileNameWithNoExtension = "FlowVSThroughput_tt_";
    std::string graphicsFileName        = fileNameWithNoExtension + ".png";
    std::string plotFileName            = fileNameWithNoExtension + ".plt";
    std::string plotTitle               = "Flow vs Throughput";
    std::string dataTitle               = "Throughput";

    // Instantiate the plot and set its title.
    Gnuplot gnuplot (graphicsFileName);
    gnuplot.SetTitle (plotTitle);

    // Make the graphics file, which the plot file will be when it
    // is used with Gnuplot, be a PNG file.
    gnuplot.SetTerminal ("png");

    // Set the labels for each axis.
    gnuplot.SetLegend ("Flow", "Throughput");

     
   Gnuplot2dDataset dataset;
   dataset.SetTitle (dataTitle);
   dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  //flowMonitor declaration
  FlowMonitorHelper fmHelper;
  Ptr<FlowMonitor> allMon = fmHelper.InstallAll();
  // call the flow monitor function
  ThroughputMonitor(&fmHelper, allMon, dataset); 

  //Simulator::Schedule (Seconds (m_totalTime), &MeshTest::Report, this);
  Simulator::Stop (Seconds (m_totalTime));

  // Enable graphical interface for netanim
  AnimationInterface animation ("link-layer-handoff.xml");
  animation.EnablePacketMetadata (false);
  csmaHelper.EnablePcapAll("csma-infrastructure-mesh");
  p2pHelper.EnablePcapAll("p2p-infrastructure-mesh");
  //meshHelper.EnablePcapAll("mesh-infrastructure-mesh");
 
  Simulator::Run ();
  //Gnuplot ...continued
  gnuplot.AddDataset (dataset);
  // Open the plot file.
  std::ofstream plotFile (plotFileName.c_str());
  // Write the plot file.
  gnuplot.GenerateOutput (plotFile);
  // Close the plot file.
  plotFile.close ();
  Simulator::Destroy ();

  return 0;
}

int
main (int argc, char *argv[])
{
  ns3::PacketMetadata::Enable ();
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  MeshTest t;
  t.Configure (argc, argv);
  return t.Run ();
}


void ThroughputMonitor (FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon,Gnuplot2dDataset DataSet)
  {
        double localThrou=0;
    std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats();
    Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier());
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin (); stats != flowStats.end (); ++stats)
    {
      Ipv4FlowClassifier::FiveTuple fiveTuple = classing->FindFlow (stats->first);
      std::cout<<"Flow ID     : " << stats->first <<" ; "<< fiveTuple.sourceAddress <<" -----> "<<fiveTuple.destinationAddress<<std::endl;
      std::cout<<"Tx Packets = " << stats->second.txPackets<<std::endl;
      std::cout<<"Rx Packets = " << stats->second.rxPackets<<std::endl;
            std::cout<<"Duration    : "<<(stats->second.timeLastRxPacket.GetSeconds()-stats->second.timeFirstTxPacket.GetSeconds())<<std::endl;
      std::cout<<"Last Received Packet  : "<< stats->second.timeLastRxPacket.GetSeconds()<<" Seconds"<<std::endl;
      std::cout<<"Throughput: " << stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds()-stats->second.timeFirstTxPacket.GetSeconds())/1024/1024  << " Mbps"<<std::endl;
            localThrou=(stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds()-stats->second.timeFirstTxPacket.GetSeconds())/1024/1024);
      // updata gnuplot data
            DataSet.Add((double)Simulator::Now().GetSeconds(),(double) localThrou);
      std::cout<<"---------------------------------------------------------------------------"<<std::endl;
    }
      Simulator::Schedule(Seconds(1),&ThroughputMonitor, fmhelper, flowMon,DataSet);
   //if(flowToXml)
      {
  flowMon->SerializeToXmlFile ("ThroughputMonitor.xml", true, true);
      }

  }
