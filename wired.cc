#include "ns3/core-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/bridge-module.h"
#include "ns3/csma-module.h"
#include "ns3/tcp-westwood.h"
#include "ns3/error-model.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/gnuplot.h"
#include <string>
#include "src/fd-net-device/helper/creator-utils.h"
using namespace ns3;

typedef std::map<FlowId, FlowMonitor::FlowStats> STATS;

NS_LOG_COMPONENT_DEFINE ("Wired_Script");
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
    //Time::SetResolution (Time::S);
    double sim_time=10.0;
    std::string tcp_protocol="Vegas";
    std::string application="OnAndOff";
    CommandLine cmd;
    cmd.AddValue ("TCP_Protocol", "Transport protocol to use:" "Vegas,Veno,Westwood\n", tcp_protocol);
    cmd.AddValue ("Simulation_Time", "Simulation time in seconds\n", sim_time);
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
    //Plot declarations
    Gnuplot2dDataset throughputdata;
    Gnuplot2dDataset fairnessdata;

    // Creating Network Topology for Different Packet Sizes
    for (int i=0;i<10;i++){
        //Setting the MSS
        Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (packet_size_arr[i]));
        /*                             Topology
        node_1 ----------------router_1-----------------router_2----------------node_2
                100Mbps,20ms             10Mbps,50ms             100Mbps,20ms           
        Drop Tail Queue - A FIFO packet queue that drops tail-end packets on overflow*/
       
        PointToPointHelper node_router;
        node_router.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));//Link Capacity
        node_router.SetChannelAttribute ("Delay", StringValue ("20ms"));//Propogation Delay
        node_router.SetQueue("ns3::DropTailQueue<Packet>","MaxSize", QueueSizeValue (QueueSize("250000B")));//QueueSize Set to BandwidthDelayProduct=LinkSpeed*Delay=12.5MBps*20ms
       
        PointToPointHelper router_router;
        router_router.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
        router_router.SetChannelAttribute ("Delay", StringValue ("50ms"));
        router_router.SetQueue("ns3::DropTailQueue<Packet>","MaxSize", QueueSizeValue (QueueSize("62500B")));//QueueSize Set to BandwidthDelayProduct=LinkSpeed*Delay=1.25MBps*50ms

        //Creating dumbbell topology with 1 left(node_router) node 1 right node(node_router) and the bottle neck link denoted by router_router
        PointToPointDumbbellHelper dumb_top (1, node_router, 1, node_router, router_router);
       
        //Install the Internet stack using InstallStack Member Function of PointToPointDumbbellHelper. Aggregates TCP/IP/UDP functionality to existing nodes
        InternetStackHelper stack;
        dumb_top.InstallStack(stack);

        //Subnet allocation using AssignIPV4Addresses function of PointToPointDumbbellHelper.Allocation for left leaves,right leaves and the bottleneck link done differently
        dumb_top.AssignIpv4Addresses(Ipv4AddressHelper("10.1.1.0", "255.255.255.0"),Ipv4AddressHelper("10.2.1.0", "255.255.255.0"),Ipv4AddressHelper("10.3.1.0", "255.255.255.0"));
       
        NS_LOG_INFO("\nNetwork Setup Complete for "<<packet_size_arr[i]<<"\n");
       
        //ApplicationLayer
	if(application=="Bulk"){
		BulkSendHelper serverApp ("ns3::TcpSocketFactory", 
				        (InetSocketAddress (dumb_top.GetRightIpv4Address (0), 1000)));
		serverApp.SetAttribute ("MaxBytes", UintegerValue (7000*packet_size_arr[i]));
		serverApp.SetAttribute ("SendSize", UintegerValue(packet_size_arr[i]));
		ApplicationContainer sourceApps = serverApp.Install(dumb_top.GetLeft(0));
		sourceApps.Start (Seconds (0.0));
		sourceApps.Stop (Seconds (10.0));

		PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 1000));
		ApplicationContainer sinkApps = sink.Install (dumb_top.GetRight(0));        
		sinkApps.Start (Seconds (0.0));
		sinkApps.Stop (Seconds (10.0));
	}
	else if(application=="OnAndOff"){
		OnOffHelper serverApp ("ns3::TcpSocketFactory", 
                                (InetSocketAddress (dumb_top.GetRightIpv4Address (0), 1000)));
		serverApp.SetAttribute ("PacketSize", UintegerValue (packet_size_arr[i]));
		serverApp.SetAttribute ("DataRate",  StringValue ("100Mbps"));
		serverApp.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
		serverApp.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
		ApplicationContainer onOffApp = serverApp.Install(dumb_top.GetLeft(0));

		PacketSinkHelper clientSink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 1000));
		ApplicationContainer sinkApp = clientSink.Install (dumb_top.GetRight(0));

		onOffApp.Start(Seconds(1.0));
                //onOffApp.Stop (Seconds (5.1));
		sinkApp.Start(Seconds (0.0));
                //sinkApp.Stop (Seconds (5.1));
	}

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
            //std::cout<<"hi"<<std::endl;
           // Structure to classify a packet. source address,port,destination address,port,protocol
           Ipv4FlowClassifier::FiveTuple five_tup = classifier->FindFlow (it->first);
           //if(five_tup.sourceAddress!=dumb_top.GetLeftIpv4Address (0)) {continue;}
	   if(it->first!=1) {continue;}
           NS_LOG_INFO("Source Adress: "<<five_tup.sourceAddress<<" Source Port: "<<five_tup.sourcePort<<"\n");
           NS_LOG_INFO("Destination Adress: "<<five_tup.destinationAddress<<" Destination Port: "<<five_tup.destinationPort<<"\n");
           NS_LOG_INFO("  Tx Packets:   " << it->second.txPackets << std::endl);
           NS_LOG_INFO("  Tx Bytes:   " << it->second.txBytes << std::endl);
           NS_LOG_INFO("  Rx Packets:   " << it->second.rxPackets << std::endl);
           NS_LOG_INFO("  Rx Bytes:   " << it->second.rxBytes << std::endl);
           double time_elapsed= it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds();
           double throughput=(it->second.rxBytes*8)/time_elapsed;
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

    //Plots
    std::string title,legend1,legend2;
    title= tcp_protocol + " for WiredTCP in Application " + application + " Fairness Index vs Packet Size";
    legend1="Packet Size in Bytes";
    legend2="Jain's Fairness Index Values";
    plot_plt_file(title, fairnessdata,legend1,legend2);
    title= tcp_protocol + " for WiredTCP in Application " + application + " Throughput vs Packet Size";
    legend1="Packet Size in Bytes";
    legend2="Throughput Values(in Kbps)";
    plot_plt_file(title, throughputdata,legend1,legend2);

}
