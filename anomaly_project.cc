/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 SEBASTIEN DERONNE
 * Copyright (c) 2020 AGH University of Science and Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Szymon Szott <szott@kt.agh.edu.pl>
 * Based on he-wifi-network.cc by S. Deronne <sebastien.deronne@gmail.com>
 * Last update: 2021-03-04 10:15:37
 */

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/yans-wifi-channel.h"
#include <chrono>  // For high resolution clock
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/abort.h"
#include "ns3/global-value.h"
#include "ns3/mobility-model.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"

#include <iostream>
#include <vector>
#include <math.h>
#include <string>
#include <fstream>
#include <string>
#include <ctime>
#include <iomanip>
#include <sys/stat.h>

// Course: Simulation Methods (Metody symulacyjne)
// Lab exercise: 2
//
// This is a simple scenario to measure the performance of an IEEE 802.11ax Wi-Fi network 
// under varying channel conditions.
//
//   AP                    STA
//    *  <-- distance -->  *
//    |                    |
//   n0                    n1
//
// The station generates constant traffic so as to saturate the channel.
// The user can specify the distance between the AP and a station and the MCS value (0-11) used.
// The user can also specify the channel model to use.
// The simulation output is the aggregate network throughput.

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ms-lab2");

int main (int argc, char *argv[])
{
  
  // Initialize default simulation parameters
  uint32_t nWifi = 1;   //Number of transmitting stations
  uint32_t nWifiF = 1;  // Numer of transmitting stations far away
  double simulationTime = 10; //Default simulation time [s]
  int mcs = 7; // Default MCS is set to highest value
  int channelWidth = 20; //Default channel width [MHz]
  int gi = 800; //Default guard interval [ns]
  double distance = 1.0; //Distance between station and AP [m]
  double distanceF = 10.0;
  double offeredLoad = 150e6;   
 // float radius = 1.0;
std::string lossModel = "LogDistance"; //Propagation loss model  

  // Parse command line arguments
  CommandLine cmd;
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("mcs", "use a specific MCS (0-11)", mcs);
  cmd.AddValue ("distance", "Distance between the station and the AP [m]", distance);  
  cmd.AddValue ("distanceF","Distance beetwen far away station and AP [m]", distanceF);
  cmd.AddValue ("offeredLoad","Offered load", offeredLoad);
  cmd.AddValue ("lossModel", "Propagation loss model to use (Friis, LogDistance, Nakagami)", lossModel);  
  cmd.Parse (argc,argv);

  // Print simulation settings to screen
  std::cout << std::endl << "Simulating an IEEE 802.11ax network with the following settings:" << std::endl;
  std::cout << "- number of transmitting stations: " << nWifi << std::endl;  
  std::cout << "- frequency band: 5 GHz" << std::endl;  
  std::cout << "- modulation and coding scheme (MCS): " << mcs << std::endl;  
  std::cout << "- channel width: " << channelWidth << " MHz" << std::endl;  
  std::cout << "- guard interval: " << gi << " ns" << std::endl;    
  std::cout << "- distance: " << distance << " m" << std::endl;
  std::cout << "- offered load: " << offeredLoad << " Mbit/s" <<std::endl;  
  std::cout << "- loss model: " << lossModel << std::endl;  

  // Create AP and stations
  NodeContainer wifiApNode;
  wifiApNode.Create (1);
  NodeContainer wifiStaNode;
  wifiStaNode.Create (nWifi);
  NodeContainer wifiStaNodeF;
  wifiStaNodeF.Create (nWifiF);
  // Configure wireless channel
  YansWifiPhyHelper phy;
  Ptr<YansWifiChannel> channel;
  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default ();

  if (lossModel=="LogDistance") {
    phy.SetChannel (channelHelper.Create ());  
  }
  else if (lossModel=="Friis") {
    channel = channelHelper.Create ();
    channel->SetPropagationLossModel (CreateObject<FriisPropagationLossModel>());
    phy.SetChannel (channel);
  }  
  else if (lossModel=="Nakagami") {
    // Add Nakagami fading to the default log distance model
    channelHelper.AddPropagationLoss ("ns3::NakagamiPropagationLossModel");
    phy.SetChannel (channelHelper.Create ());
  }     
  else {
    NS_ABORT_MSG("Wrong propagation model selected. Valid models are: Friis, LogDistance, Nakagami\n");
  }
  


  // Create and configure Wi-Fi network
  WifiMacHelper mac;
  WifiMacHelper mac1;
  WifiHelper wifiAP;
  wifiAP.SetStandard (WIFI_STANDARD_80211a);
  WifiHelper wifiSta;
  wifiSta.SetStandard (WIFI_STANDARD_80211a);
  WifiHelper wifiStaF;
  wifiStaF.SetStandard (WIFI_STANDARD_80211a);
 
// 802.11 a
       
wifiAP.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
							    "DataMode",    StringValue ("OfdmRate54Mbps"),
							    "ControlMode", StringValue ("OfdmRate6Mbps"));
              wifiSta.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
							    "DataMode",    StringValue ("OfdmRate54Mbps"),
			                                    "ControlMode", StringValue ("OfdmRate6Mbps"));
              wifiStaF.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
							    "DataMode",    StringValue ("OfdmRate6Mbps"),
							    "ControlMode", StringValue ("OfdmRate6Mbps"));


  // 802.11 ac
/*  std::ostringstream oss;
              oss << "VhtMcs" << mcs;
              wifiAP.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (oss.str ()),
                                            "ControlMode", StringValue ("VhtMcs0"));
              wifiSta.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (oss.str ()),
                                            "ControlMode", StringValue ("VhtMcs0"));
              wifiStaF.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue ("VhtMcs0"),
                                            "ControlMode", StringValue ("VhtMcs0"));
*/  
  Ssid ssid = Ssid ("ns3-80211a");

  mac.SetType ("ns3::StaWifiMac",
                           "Ssid", SsidValue (ssid));
  
//Ssid ssid1 = Ssid ("ns3-80211a");

  //mac1.SetType ("ns3::StaWifiMac",
  //                         "Ssid", SsidValue (ssid1));

  //std::ostringstream oss;
  //oss << "HeMcs" << mcs;
  //wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (oss.str ()),
  //                              "ControlMode", StringValue (oss.str ())); //Set MCS

  //Ssid ssid = Ssid ("ns3-80211ax"); //Set SSID

  //mac.SetType ("ns3::StaWifiMac",
  //              "Ssid", SsidValue (ssid));

  // Create and configure Wi-Fi interfaces
  NetDeviceContainer staDevice;
  staDevice = wifiSta.Install (phy, mac, wifiStaNode);
  // Create and configure far away Wi-Fi interfaces
  NetDeviceContainer staDeviceF;
  staDeviceF = wifiStaF.Install (phy, mac, wifiStaNodeF);

  mac.SetType ("ns3::ApWifiMac",
                "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice;
  apDevice = wifiAP.Install (phy, mac, wifiApNode);

 // Set channel width
  //Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidth));
 
 // Set guard interval
  // Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported", BooleanValue (gi));
 
  // Set channel width and guard interval on all interfaces of all nodes
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidth));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HeConfiguration/GuardInterval", TimeValue (NanoSeconds (gi)));  

  // Configure mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  MobilityHelper mobility1;
  Ptr<ListPositionAllocator> positionAlloc1 = CreateObject<ListPositionAllocator> ();
 
  positionAlloc->Add (Vector (0.0, 0.0, 2.5)); // position of AP
  positionAlloc->Add (Vector (distance, 0.0, 1.5)); // position of station
  //positionAlloc->Add (Vector (distanceF,0.0, 1.5)); // position of far away station 
  mobility.SetPositionAllocator (positionAlloc);
  positionAlloc1->Add (Vector (distanceF,0.0, 1.5));
  mobility1.SetPositionAllocator (positionAlloc1);
  /*Ptr<UniformDiscPositionAllocator> positionAlloc = CreateObject<UniformDiscPositionAllocator> ();
  positionAlloc->SetX   (0.0); positionAlloc->SetY   (0.0); //set disc center
  positionAlloc->SetRho (radius); //area radius
  MobilityHelper mobility;
  mobility.SetPositionAllocator (positionAlloc);*/
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  
  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNode);
  mobility1.Install (wifiStaNodeF);
  for (NodeContainer::Iterator j = wifiStaNode.Begin (); j != wifiStaNode.End (); ++j) {
    Ptr<Node> object = *j;
    Ptr<MobilityModel> position = object->GetObject<MobilityModel> ();
    Vector pos = position->GetPosition ();
    std::cout << "Sta " << (uint32_t) object->GetId() << ":\tx=" << pos.x << ", y=" << pos.y << std::endl;
  }
  std::cout <<"Distant STA: "<<std::endl;
  for (NodeContainer::Iterator j = wifiStaNodeF.Begin (); j != wifiStaNodeF.End (); ++j) {
    Ptr<Node> object = *j;
    Ptr<MobilityModel> position = object->GetObject<MobilityModel> ();
    Vector pos = position->GetPosition ();
    std::cout << "Sta " << (uint32_t) object->GetId() << ":\tx=" << pos.x << ", y=" << pos.y << std::endl;
  }

  // Install an Internet stack
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);
  stack.Install (wifiStaNodeF);

  // Configure IP addressing
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterface;
  Ipv4InterfaceContainer apNodeInterface;
  Ipv4InterfaceContainer staNodeInterfaceF;

  staNodeInterface = address.Assign (staDevice);
  apNodeInterface = address.Assign (apDevice);
  staNodeInterfaceF = address.Assign (staDeviceF);

  // Install applications (traffic generators)
  ApplicationContainer sourceApplications, sinkApplications, sinkApplicationsF;
  uint32_t portNumber = 9;
  for (uint32_t index = 0; index < nWifi; ++index) //Loop over all stations (which transmit to the AP)
    {
      auto ipv4 = wifiApNode.Get (0)->GetObject<Ipv4> (); //Get destination's IP interface
      const auto address = ipv4->GetAddress (1, 0).GetLocal (); //Get destination's IP address
      InetSocketAddress sinkSocket (address, portNumber++); //Configure destination socket
      OnOffHelper onOffHelper ("ns3::UdpSocketFactory", sinkSocket); //Configure traffic generator: UDP, destination socket
      onOffHelper.SetConstantRate (DataRate ((offeredLoad)/ (nWifi+nWifiF)), 1000);  //Set data rate (150 Mb/s divided by no. of transmitting stations) and packet size [B]
      sourceApplications.Add (onOffHelper.Install (wifiStaNode.Get (index))); //Install traffic generator on station
      PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkSocket); //Configure traffic sink
      sinkApplications.Add (packetSinkHelper.Install (wifiApNode.Get (0))); //Install traffic sink on AP
    }

  for (uint32_t index = 0; index < nWifiF; ++index) //Loop over all stations (which transmit to the AP)
    {
      auto ipv4 = wifiApNode.Get (0)->GetObject<Ipv4> (); //Get destination's IP interface
      const auto address = ipv4->GetAddress (1, 0).GetLocal (); //Get destination's IP address
      InetSocketAddress sinkSocket (address, portNumber++); //Configure destination socket
      OnOffHelper onOffHelper ("ns3::UdpSocketFactory", sinkSocket); //Configure traffic generator: UDP, destination socket
      onOffHelper.SetConstantRate (DataRate ((offeredLoad)/ (nWifi+nWifiF)), 1000);  //Set data rate (150 Mb/s divided by no. of transmitting stations) and packet size [B]
      sourceApplications.Add (onOffHelper.Install (wifiStaNodeF.Get (index))); //Install traffic generator on station
      PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkSocket); //Configure traffic sink
      sinkApplicationsF.Add (packetSinkHelper.Install (wifiApNode.Get (0))); //Install traffic sink on AP
    }

  // Configure application start/stop times
  // Note: 
  // - source starts transmission at 1.0 s
  // - source stops at simulationTime+1
  // - simulationTime reflects the time when data is sent
 
  sinkApplications.Start (Seconds (0.0));
  sinkApplications.Stop (Seconds (simulationTime + 1));
  sinkApplicationsF.Start (Seconds (0.0));
  sinkApplicationsF.Stop (Seconds (simulationTime + 1)); 
  sourceApplications.Start (Seconds (1.0));
  sourceApplications.Stop (Seconds (simulationTime + 1));
  
  FlowMonitorHelper flowmon;
	Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
  // Define simulation stop time
  Simulator::Stop (Seconds (simulationTime + 1));
  
  // Print information that the simulation will be executed
  std::clog << std::endl << "Starting simulation... ";
  // Record start time
  auto start = std::chrono::high_resolution_clock::now();
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));
  // Run the simulatio

  Simulator::Run ();

  // Record stop time and count duration
  auto finish = std::chrono::high_resolution_clock::now();
  std::clog << ("done!") << std::endl;  
  std::chrono::duration<double> elapsed = finish - start;
  std::cout << "Elapsed time: " << elapsed.count() << " s\n\n";
  
  // Calculate throughput
 /* double throughput = 0;
  for (uint32_t index = 0; index < sinkApplications.GetN (); ++index) //Loop over all traffic sinks
    {
      uint64_t totalBytesThrough = DynamicCast<PacketSink> (sinkApplications.Get (index))->GetTotalRx (); //Get amount of bytes received
      throughput += ((totalBytesThrough * 8) / (simulationTime * 1000000.0)); //Mbit/s 
    }
  // Calculate throughput
  double throughputF = 0;
  for (uint32_t index = 0; index < sinkApplicationsF.GetN (); ++index) //Loop over all traffic sinks
    {
      uint64_t totalBytesThrough = DynamicCast<PacketSink> (sinkApplicationsF.Get (index))->GetTotalRx (); //Get amount of bytes received
      throughputF += ((totalBytesThrough * 8) / (simulationTime * 1000000.0)); //Mbit/s 
    } 
 

  //Print results
  std::cout << "Results: " << std::endl;
  std::cout << "- throughput in close STA: " << throughput/nWifi << " Mbit/s" << std::endl;
  std::cout << "- throughput in distant STA: " << throughputF/nWifiF << " Mbit/s" << std::endl;
*/
 // Calculate per-flow throughput and print results
  double flowThr=0;
  double flowThrClose=0;
  double flowThrDist=0;
  double totalThr=0;
  uint32_t number = 0;
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
	std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  std::cout << "Results: " << std::endl;
	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i) {
		Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
		flowThr=i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1e6;
		NS_LOG_UNCOND ("Flow " << i->first  << " (" << t.sourceAddress <<"/"<<t.sourcePort<< " -> " << t.destinationAddress << "/"<<t.destinationPort<<")\tThroughput: " <<  flowThr  << " Mbps");
    
    if (i->second.rxBytes!=0) {
      totalThr += flowThr;
    }
    if(number<nWifi){
     flowThrClose+=flowThr; 
    }
    if(number>=nWifi){
     flowThrDist+=flowThr;
   }
  number++;
	
}
std::cout << std::endl << "Total throughput Close: " << flowThrClose << " Mb/s" << std::endl << std::endl;
std::cout << std::endl << "Total throughput Distant: " << flowThrDist << " Mb/s" << std::endl << std::endl;

std::cout << std::endl << "Total throughput: " << totalThr << " Mb/s" << std::endl << std::endl;

  //Clean-up;
 Simulator::Destroy ();
  return 0;
}
