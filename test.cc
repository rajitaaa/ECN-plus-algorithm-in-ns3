
#include <iostream>
#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/traffic-control-module.h"
#include "ns3/log.h"
#include "ns3/random-variable-stream.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/callback.h"

using namespace ns3;
Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
std::string dir = "results/gfc-dumbbell/";
double stopTime = 10;

void
CheckQueueSize (Ptr<QueueDisc> queue)
{
  uint32_t qSize = queue->GetCurrentSize ().GetValue ();

  // check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.001), &CheckQueueSize, queue);

  std::ofstream fPlotQueue (std::stringstream (dir + "queue-size.plotme").str ().c_str (), std::ios::out | std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds () << " " << qSize << std::endl;
  fPlotQueue.close ();
}

static void
CwndChangeA (uint32_t oldCwnd, uint32_t newCwnd)
{
  std::ofstream fPlotQueue (dir + "cwndTraces/A.plotme", std::ios::out | std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds () << " " << newCwnd/1446.0 << std::endl;
  fPlotQueue.close ();
}
static void
DropAtQueue (Ptr<OutputStreamWrapper> stream, Ptr<const QueueDiscItem> item)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " 1" << std::endl;
}

static void
MarkAtQueue (Ptr<OutputStreamWrapper> stream, Ptr<const QueueDiscItem> item, const char* reason)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " 1" << std::endl;
}

void
TraceCwnd (uint32_t node, uint32_t cwndWindow,
           Callback <void, uint32_t, uint32_t> CwndTrace)
{
  Config::ConnectWithoutContext ("/NodeList/" + std::to_string (node) + "/$ns3::TcpL4Protocol/SocketList/" + std::to_string (cwndWindow) + "/CongestionWindow", CwndTrace);
}

void InstallPacketSink (Ptr<Node> node, uint16_t port)
{
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (node);
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (stopTime));
}

void InstallBulkSend (Ptr<Node> node, Ipv4Address address, uint16_t port, 
                      uint32_t nodeId, uint32_t cwndWindow,
                      Callback <void, uint32_t, uint32_t> CwndTrace)
{
  BulkSendHelper source ("ns3::TcpSocketFactory", 
                         InetSocketAddress (address, port));

  source.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApps = source.Install (node);
  Time timeToStart = Seconds (uv->GetValue (0, 1));
  sourceApps.Start (timeToStart);
  Simulator::Schedule (timeToStart + Seconds (0.001), &TraceCwnd, nodeId, cwndWindow, CwndTrace);
  sourceApps.Stop (Seconds (stopTime));
}

int main (int argc, char *argv[])
{
  uint32_t stream = 1;
  std::string transport_prot = "TcpNewReno";
  std::string queue_disc_type = "RedQueueDisc";
  bool useEcn = true;
  uint32_t dataSize = 1446;
  uint32_t delAckCount = 2;

  time_t rawtime;
  struct tm * timeinfo;
  char buffer[80];

  time (&rawtime);
  timeinfo = localtime(&rawtime);

  strftime(buffer,sizeof(buffer),"%d-%m-%Y-%I-%M-%S",timeinfo);
  std::string currentTime (buffer);

  CommandLine cmd;
  cmd.AddValue ("stream", "Seed value for random variable", stream);
  cmd.AddValue ("transport_prot", "Transport protocol to use: TcpNewReno, "
                "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat, "
                "TcpLp", transport_prot);
  cmd.AddValue ("queue_disc_type", "Queue disc type for gateway (e.g. ns3::CoDelQueueDisc)", queue_disc_type);
  cmd.AddValue ("useEcn", "Use ECN", useEcn);
  cmd.AddValue ("dataSize", "Data packet size", dataSize);
  cmd.AddValue ("delAckCount", "Delayed ack count", delAckCount);
  cmd.AddValue ("stopTime", "Stop time for applications / simulation time will be stopTime", stopTime);
  cmd.Parse (argc,argv);

  uv->SetStream (stream);
  transport_prot = std::string ("ns3::") + transport_prot;
  queue_disc_type = std::string ("ns3::") + queue_disc_type;

  TypeId qdTid;
  NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (queue_disc_type, &qdTid), "TypeId " << queue_disc_type << " not found");

  // Select TCP variant
  if (transport_prot.compare ("ns3::TcpWestwoodPlus") == 0)
    {
      // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
      // the default protocol type in ns3::TcpWestwood is WESTWOOD
      Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    }
  else
    {
      TypeId tcpTid;
      NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (transport_prot, &tcpTid), "TypeId " << transport_prot << " not found");
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName (transport_prot)));
    }

  Config::SetDefault ("ns3::TcpL4Protocol::RecoveryType", TypeIdValue (TypeId::LookupByName ("ns3::TcpPrrRecovery")));

  NodeContainer leftNodes, rightNodes, routers;
  routers.Create (2);
  leftNodes.Create (5);
  rightNodes.Create (5);

  // Create the point-to-point link helpers
  PointToPointHelper pointToPointRouter;
  pointToPointRouter.SetDeviceAttribute  ("DataRate", StringValue ("5Mbps"));
  pointToPointRouter.SetChannelAttribute ("Delay", StringValue ("0.00075ms"));
  NetDeviceContainer r1r2ND = pointToPointRouter.Install (routers.Get (0), routers.Get (1));

  std::vector <NetDeviceContainer> leftToRouter;
  std::vector <NetDeviceContainer> routerToRight;
  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute    ("DataRate", StringValue ("5Mbps"));

  // Node 1
  pointToPointLeaf.SetChannelAttribute   ("Delay", StringValue ("0.00025ms"));
  leftToRouter.push_back (pointToPointLeaf.Install (leftNodes.Get (0), routers.Get (0)));
  routerToRight.push_back (pointToPointLeaf.Install (routers.Get (1), rightNodes.Get (0)));

  // Node 2
  pointToPointLeaf.SetChannelAttribute   ("Delay", StringValue ("0.0001ms"));
  leftToRouter.push_back (pointToPointLeaf.Install (leftNodes.Get (1), routers.Get (0)));
  routerToRight.push_back (pointToPointLeaf.Install (routers.Get (1), rightNodes.Get (1)));

  // Node 3
  pointToPointLeaf.SetChannelAttribute   ("Delay", StringValue ("0.00005ms"));
  leftToRouter.push_back (pointToPointLeaf.Install (leftNodes.Get (2), routers.Get (0)));
  routerToRight.push_back (pointToPointLeaf.Install (routers.Get (1), rightNodes.Get (2)));

  // Node 4
  pointToPointLeaf.SetChannelAttribute   ("Delay", StringValue ("0.000025ms"));
  leftToRouter.push_back (pointToPointLeaf.Install (leftNodes.Get (3), routers.Get (0)));
  routerToRight.push_back (pointToPointLeaf.Install (routers.Get (1), rightNodes.Get (3)));

  // Node 5
  pointToPointLeaf.SetChannelAttribute   ("Delay", StringValue ("0.000005ms"));
  leftToRouter.push_back (pointToPointLeaf.Install (leftNodes.Get (4), routers.Get (0)));
  routerToRight.push_back (pointToPointLeaf.Install (routers.Get (1), rightNodes.Get (4)));

  // Install Stack
  InternetStackHelper stack;
  stack.Install (routers);
  stack.Install (leftNodes);
  stack.Install (rightNodes);

  Ipv4AddressHelper ipAddresses ("10.0.0.0", "255.255.255.0");

  Ipv4InterfaceContainer r1r2IPAddress = ipAddresses.Assign (r1r2ND);        ipAddresses.NewNetwork ();

  std::vector <Ipv4InterfaceContainer> leftToRouterIPAddress;
  leftToRouterIPAddress.push_back (ipAddresses.Assign (leftToRouter [0]));   ipAddresses.NewNetwork ();
  leftToRouterIPAddress.push_back (ipAddresses.Assign (leftToRouter [1]));   ipAddresses.NewNetwork ();
  leftToRouterIPAddress.push_back (ipAddresses.Assign (leftToRouter [2]));   ipAddresses.NewNetwork ();
  leftToRouterIPAddress.push_back (ipAddresses.Assign (leftToRouter [3]));   ipAddresses.NewNetwork ();
  leftToRouterIPAddress.push_back (ipAddresses.Assign (leftToRouter [4]));   ipAddresses.NewNetwork ();

  std::vector <Ipv4InterfaceContainer> routerToRightIPAddress;
  routerToRightIPAddress.push_back (ipAddresses.Assign (routerToRight [0]));   ipAddresses.NewNetwork ();
  routerToRightIPAddress.push_back (ipAddresses.Assign (routerToRight [1]));   ipAddresses.NewNetwork ();
  routerToRightIPAddress.push_back (ipAddresses.Assign (routerToRight [2]));   ipAddresses.NewNetwork ();
  routerToRightIPAddress.push_back (ipAddresses.Assign (routerToRight [3]));   ipAddresses.NewNetwork ();
  routerToRightIPAddress.push_back (ipAddresses.Assign (routerToRight [4]));

  dir += (currentTime + "/");
  std::string dirToSave = "mkdir -p " + dir;
  system (dirToSave.c_str ());
  system ((dirToSave + "/pcap/").c_str ());
  system ((dirToSave + "/cwndTraces/").c_str ());
  system ((dirToSave + "/markTraces/").c_str ());
  system ((dirToSave + "/queueTraces/").c_str ());
  system (("cp -R PlotScripts-gfc-dumbbell/* " + dir + "/pcap/").c_str ());

  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 20));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 20));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (delAckCount));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (dataSize));
  Config::SetDefault ("ns3::TcpSocketBase::EcnMode",StringValue("ClassicEcn"));
  Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (useEcn));
  Config::SetDefault ("ns3::RedQueueDisc::ARED", BooleanValue (true));
  Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (true));
  Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (1500));
  Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (50));
  Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (50));
  Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (1));
  Config::SetDefault ("ns3::RedQueueDisc::UseHardDrop", BooleanValue (false));
  Config::SetDefault (queue_disc_type + "::MaxSize", QueueSizeValue (QueueSize ("666p")));

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> streamWrapper;

  TrafficControlHelper tch;
  tch.SetRootQueueDisc (queue_disc_type);
  QueueDiscContainer qd;
  tch.Uninstall (routers.Get (0)->GetDevice (0));
  qd.Add (tch.Install (routers.Get (0)->GetDevice (0)).Get (0));
  Simulator::ScheduleNow (&CheckQueueSize, qd.Get (0));
  streamWrapper = asciiTraceHelper.CreateFileStream (dir + "/queueTraces/drop-0.plotme");
  qd.Get (0)->TraceConnectWithoutContext ("Drop", MakeBoundCallback (&DropAtQueue, streamWrapper));
  streamWrapper = asciiTraceHelper.CreateFileStream (dir + "/queueTraces/mark-0.plotme");
  qd.Get (0)->TraceConnectWithoutContext ("Mark", MakeBoundCallback (&MarkAtQueue, streamWrapper));

  uint16_t port = 50000;
  InstallPacketSink (rightNodes.Get (0), port);     

  InstallBulkSend (leftNodes.Get (0), routerToRightIPAddress [0].GetAddress (1), port, 2, 0, MakeCallback (&CwndChangeA));
 
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  pointToPointLeaf.EnablePcapAll (dir + "pcap/N", true);

  Simulator::Stop (Seconds (stopTime));
  Simulator::Run ();
  std::ofstream myfile;
  myfile.open (dir + "queueStats.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  myfile << std::endl;
  myfile << "Stat for Queue 1";
  myfile << qd.Get (0)->GetStats ();
  myfile.close ();

  myfile.open (dir + "config.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  myfile << "useEcn " << useEcn << "\n";
  myfile << "queue_disc_type " << queue_disc_type << "\n";
  myfile << "stream  " << stream << "\n";
  myfile << "transport_prot " << transport_prot << "\n";
  myfile << "dataSize " << dataSize << "\n";
  myfile << "delAckCount " << delAckCount << "\n";
  myfile << "stopTime " << stopTime << "\n";
  myfile.close ();

  Simulator::Destroy ();
  return 0;
}

