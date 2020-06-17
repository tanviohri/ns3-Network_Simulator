#include "ns3/core-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include "ns3/gnuplot.h"
#include "ns3/gnuplot-helper.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/nstime.h"
#include "ns3/propagation-delay-model.h"
#include "src/fd-net-device/helper/creator-utils.h"
NS_LOG_COMPONENT_DEFINE ("Wireless_Script");

using namespace ns3;

typedef std::map<FlowId, FlowMonitor::FlowStats> STATS;

// use NS_LOG_INFO(msg_to_print) instead of cout and run in terminal "export NS_LOG=Wired_Script=info"

//auxiliary function to plot the comparison graphs
void
plot_plt_file(std::string title, Gnuplot2dDataset data,std::string legend1,std::string legend2){
    std::string png_filename = title +  ".png";
    data.SetTitle ("Source to Destination");
    data.SetStyle (Gnuplot2dDataset::LINES_POINTS);
    Gnuplot plot(png_filename);
    std::string plot_filename = title + ".plt";
    plot.SetTitle (title);
    plot.SetTerminal ("png");
    plot.SetLegend (legend1,legend2);
    plot.AddDataset (data);
    std::ofstream plot_file (plot_filename.c_str());
    plot.GenerateOutput (plot_file);
    plot_file.close ();
}

int
main (int argc, char *argv[]){
    // Taking arguements from command line
    double sim_time=10.0;
    std::string application="OnAndOff";
    std::string tcp_protocol="Vegas";
    CommandLine cmd;
    cmd.AddValue ("TCP_Protocol", "Transport protocol to use:" "Vegas,Veno,Westwood\n", tcp_protocol);
    cmd.AddValue ("Simulation_Time", "Simulation time in seconds", sim_time);
    cmd.AddValue ("Application", "Application to Use: OnAndOff, Bulk\n", application);
    cmd.Parse (argc,argv);

    if(!(application=="OnAndOff" or application=="Bulk")){
	std::cout<<"Invalid Application entered. Enter Bulk or OnAndOff only.\n"<<std::endl;
	exit(0);
    }

    uint32_t packet_size_arr[10]={40, 44, 48, 52, 60, 552, 576, 628, 1420, 1500};
    NS_LOG_INFO("Taking TCP protocol from the command line");
    if (tcp_protocol=="Vegas"){
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpVegas::GetTypeId ()));
    }
    else if (tcp_protocol=="Veno"){
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpVeno::GetTypeId ()));
    }
    else if (tcp_protocol=="Westwood"){
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
        Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOOD));
    }
    else {
        ABORT("Incorrect TCP Protocol name",0);
    }

     // setting drop tail policy
    Config::SetDefault ("ns3::WifiMacQueue::DropPolicy", EnumValue(WifiMacQueue::DROP_NEWEST));

    //Plot declarations
    Gnuplot2dDataset throughputdata;
    Gnuplot2dDataset fairnessdata;

    // Creating Network Topology for Different Packet Sizes	
    for (int i=0;i<10;i++){

    //setting the largest amount of data, specified in bytes, that a computer or communications device can handle in a single, unfragmented piece.
        Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (packet_size_arr[i]));
       
        //create MAC layers for a ns3::WifiNetDevice
        //This can create MACs of type ns3::ApWifiMac, ns3::StaWifiMac and ns3::AdhocWifiMac. 
        //Allow a WifiHelper to configure and install WifiMac objects on a collection of nodes.
           
        WifiMacHelper wifiMacSTA;
        WifiMacHelper wifiMacBS;

        // Create a WifiHelper, which will use the above helpers to create
        // and install Wifi devices.  Configure a Wifi standard to use, which
        // will align various parameters in the Phy and Mac to standard defaults.

        WifiHelper wifiHelper;

        //set various parameters in the Mac and Phy to standard values and some reasonable defaults.
        wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);

        //creating channels to implement yans model and setting parameters
        YansWifiChannelHelper wifiChannel;
        wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
        wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel", "Frequency", DoubleValue (5e9));
        Ptr<YansWifiChannel> channel1 = wifiChannel.Create (); 
        Ptr<YansWifiChannel> channel2 = wifiChannel.Create ();

        // physical layer setup
        // configuring an object factory to create instances of a YansWifiPhy and adding other objects to it
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();

        //setting the RemoteStationManager
        wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs5"), "ControlMode", StringValue ("HtMcs5"));

        NS_LOG_INFO ("Node Creation");

        NodeContainer node0;
        NodeContainer bs;
        NodeContainer node1;

        node0.Create (1);
        bs.Create (2);
        node1.Create (1);

        NS_LOG_INFO ("Stationary Nodes.");

        Ssid ssid = Ssid ("wifi-topology");

        // ns3::StaWifiMac implements an active probing and association state machine 
        // ns3::ApWifiMac implements an AP that generates periodic beacons, and that accepts every attempt to associate
        wifiMacBS.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
        wifiMacSTA.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));

        // Node 0 
        wifiPhy.SetChannel (channel1);
        NetDeviceContainer node0NetDevice;
        node0NetDevice = wifiHelper.Install (wifiPhy, wifiMacSTA, node0);

        // Base station 1
        wifiPhy.SetChannel (channel1);
        NetDeviceContainer bs0NetDevice;
        bs0NetDevice = wifiHelper.Install (wifiPhy, wifiMacBS, bs.Get(0));

        // Base station 2
        wifiPhy.SetChannel (channel2);
        NetDeviceContainer bs1NetDevice;
        bs1NetDevice = wifiHelper.Install (wifiPhy, wifiMacBS, bs.Get(1));

        // Node 1
        wifiPhy.SetChannel (channel2);
        NetDeviceContainer node1NetDevice;
        node1NetDevice = wifiHelper.Install (wifiPhy, wifiMacSTA, node1);

        
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (40));
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported", BooleanValue (false));


        // Setting Position using mobilty
        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

        //positions of nodes in the 3d plane
        positionAlloc->Add (Vector (0.0, 0.0, 0.0));
        positionAlloc->Add (Vector (60.0, 0.0, 0.0));
        positionAlloc->Add (Vector (12000.0, 0.0, 0.0));
        positionAlloc->Add (Vector (12060.0, 0.0, 0.0));

        mobility.SetPositionAllocator (positionAlloc);
        mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        mobility.Install (node0.Get(0));
        mobility.Install (bs.Get(0));
        mobility.Install (bs.Get(1));
        mobility.Install (node1.Get(0));

        // Setting the Point to Point links between Base Stations
        PointToPointHelper bsHelper;
        bsHelper.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
        bsHelper.SetChannelAttribute ("Delay", StringValue ("100ms"));
        // bsHelper.SetQueue ("ns3::DropTailQueue","Mode",EnumValue (DropTailQueue::QUEUE_MODE_BYTES),"MaxBytes",UintegerValue (125000)) ;
        bsHelper.SetQueue ("ns3::DropTailQueue", "MaxSize",StringValue ("125000B")) ;

        NetDeviceContainer bsNetDevices = bsHelper.Install (bs.Get(0), bs.Get(1));

        // setting up internet stack
        InternetStackHelper stack;
        stack.Install (node0);
        stack.Install (bs);
        stack.Install (node1);

        // IPV4 Address Assigning
        Ipv4AddressHelper address;
        address.SetBase ("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer n0_interface;
        n0_interface = address.Assign (node0NetDevice);
        Ipv4InterfaceContainer bs0_interface;
        bs0_interface = address.Assign (bs0NetDevice);

        address.SetBase ("10.2.1.0", "255.255.255.0");
        Ipv4InterfaceContainer bs1_interface;
        bs1_interface = address.Assign (bs1NetDevice);
        Ipv4InterfaceContainer n1_interface;
        n1_interface = address.Assign (node1NetDevice);

        address.SetBase ("10.3.1.0", "255.255.255.0");
        Ipv4InterfaceContainer p2pInterface;
        p2pInterface = address.Assign (bsNetDevices);

        // Setting up the application layer depending upon the input type

	if(application=="OnAndOff"){
		PacketSinkHelper clientSink ("ns3::TcpSocketFactory",InetSocketAddress (Ipv4Address::GetAny (), 1000));
		ApplicationContainer sinkApp = clientSink.Install (node1.Get(0));

		OnOffHelper serverApp ("ns3::TcpSocketFactory", (InetSocketAddress (n1_interface.GetAddress (0), 1000)));
		serverApp.SetAttribute ("PacketSize", UintegerValue (packet_size_arr[i]));
		serverApp.SetAttribute ("DataRate",  StringValue ("100Mbps"));
		serverApp.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
		serverApp.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

		ApplicationContainer onOffApp;
		onOffApp = serverApp.Install (node0.Get(0));

		onOffApp.Start (Seconds (1.0));
                //onOffApp.Stop (Seconds (5.1));
		sinkApp.Start (Seconds (0.0));
                //sinkApp.Stop (Seconds (5.1));              
		
	}
	else if(application=="Bulk"){
		BulkSendHelper serverApp ("ns3::TcpSocketFactory", 
		                        (InetSocketAddress (n1_interface.GetAddress (0), 1000)));
		serverApp.SetAttribute ("MaxBytes", UintegerValue (7000*packet_size_arr[i]));
		serverApp.SetAttribute ("SendSize", UintegerValue(packet_size_arr[i]));
		ApplicationContainer sourceApps = serverApp.Install(node0.Get(0));
		sourceApps.Start (Seconds (0.0));
		sourceApps.Stop (Seconds (10.0));

		PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 1000));
		ApplicationContainer sinkApps = sink.Install (node1.Get(0));        
		sinkApps.Start (Seconds (0.0));
		sinkApps.Stop (Seconds (10.0));
        }

        //populating the routing tables
        Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
        
        //Monitors IP flow on nodes
        FlowMonitorHelper flow_track;
        //Monitor IP flow on all nodes
        Ptr<FlowMonitor> monitor_all = flow_track.InstallAll();
   
        Simulator::Stop(Seconds(10));
        Simulator::Run();
       
        //packet data to abstract flow identifier and packet identifier parameters
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flow_track.GetClassifier ());
        STATS flow_stats = monitor_all->GetFlowStats ();
        int n=0;
        double t1=0;
        double t2=0;
        for(STATS::iterator it=flow_stats.begin();it!=flow_stats.end();it++){
           // Structure to classify a packet. source address,port,destination address,port,protocol
           Ipv4FlowClassifier::FiveTuple five_tup = classifier->FindFlow (it->first);
           if(it->first!=1) {continue;}
           NS_LOG_INFO("Source Adress: "<<five_tup.sourceAddress<<" Source Port: "<<five_tup.sourcePort<<"\n");
           NS_LOG_INFO("Destination Adress: "<<five_tup.destinationAddress<<" Destination Port: "<<five_tup.destinationPort<<"\n");
           NS_LOG_INFO("  Tx Packets:   " << it->second.txPackets << std::endl);
           NS_LOG_INFO("  Tx Bytes:   " << it->second.txBytes << std::endl);
           NS_LOG_INFO("  Rx Packets:   " << it->second.rxPackets << std::endl);
           NS_LOG_INFO("  Rx Bytes:   " << it->second.rxBytes << std::endl);
           double time_elapsed= it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds();
           double throughput=(it->second.rxBytes*8.0)/time_elapsed;
           throughput/=(1024);
           t1+=throughput;
           t2+=throughput*throughput;
           n++;
           NS_LOG_INFO(" Throughput :"<<throughput<<" Kbps"<<" for packet size "<<packet_size_arr[i]<<" bytes\n");
      } 

        double FairnessIndex=t1*t1/(n*t2);
        double AvgThroughput=t1/n;
        std::cout<<"Packet Size: "<<packet_size_arr[i]<<"\n";
        std::cout<<"Jain's Fairness Index: "<<FairnessIndex<<"\n";
        std::cout<<"Average Throughput: "<<AvgThroughput<<"\n";
        std::cout<<"\n";
        fairnessdata.Add(packet_size_arr[i],FairnessIndex);
        throughputdata.Add(packet_size_arr[i],AvgThroughput);
       
        Simulator::Destroy();     
    }

    //Plotting comparison graphs using auxiliary function3
    std::string title,legend1,legend2;
    title= tcp_protocol + " for WirelessTCP in Application " + application + " Fairness Index vs Packet Size";
    legend1="Packet Size in Bytes";
    legend2="Jain's Fairness Index Values";
    plot_plt_file(title, fairnessdata,legend1,legend2);
    title= tcp_protocol + " for WirelessTCP in Application " + application + " Throughput vs Packet Size";
    legend1="Packet Size in Bytes";
    legend2="Throughput Values(in Kbps)";
    plot_plt_file(title, throughputdata,legend1,legend2);

}
