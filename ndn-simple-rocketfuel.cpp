#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

namespace ns3 {

int
main(int argc, char* argv[])
{

  Config::SetDefault("ns3::QueueBase::MaxSize", StringValue("2000p"));

  CommandLine cmd;
  cmd.Parse(argc, argv);

  AnnotatedTopologyReader topologyReader("", 25);
  //topologyReader.SetFileName("src/ndnSIM/examples/topologies/1221.r0-conv-annotated.txt");
  topologyReader.SetFileName("src/ndnSIM/examples/topologies/topo-grid-3x3.txt");
  topologyReader.Read();


  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  //ndnHelper.SetDefaultRoutes(true);
  ndnHelper.SetOldContentStore("ns3::ndn::cs::Nocache");
  ndnHelper.InstallAll();


  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/multicast");
  ndn::StrategyChoiceHelper::InstallAll("/cnn", "/localhost/nfd/strategy/multicast");
  ndn::StrategyChoiceHelper::InstallAll("/bbc", "/localhost/nfd/strategy/multicast");
  ndn::StrategyChoiceHelper::InstallAll("/nytimes", "/localhost/nfd/strategy/multicast");



  // Installing global routing interface on all nodes
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll();

  // Installing applications

  unsigned int producer_node = 4;
  int peerKey = 10001;
  int peerNumber = 100; // for using as PeerName

  for (unsigned int i =0; i<topologyReader.GetNodes().GetN(); i++)
  {
	  if(i != producer_node)

	  {
		  int randNumber = rand()%3;

		  if (randNumber == 0) // peer node
		  {
			  ndn::AppHelper consumerHelper("ns3::ndn::PeerConsumerCbr");
			  consumerHelper.SetPrefix("/prefix/");
			  std::stringstream temp_peerkey;
			  temp_peerkey << peerKey;
			  consumerHelper.SetAttribute("PeerKey", StringValue(temp_peerkey.str()));
			  std::stringstream temp_PeerName;
			  temp_PeerName << peerNumber;
			  consumerHelper.SetAttribute("PeerName", StringValue(temp_PeerName.str()));
			  consumerHelper.SetAttribute("Frequency", StringValue("3"));
			  auto appsConsumer = consumerHelper.Install(topologyReader.GetNodes().Get(i));

			  ndn::AppHelper producerHelper("ns3::ndn::PeerProducer");
			  producerHelper.SetPrefix("/prefix/peer");
			  producerHelper.SetAttribute("PeerName", StringValue(temp_PeerName.str()));
			  auto appsProducer = producerHelper.Install(topologyReader.GetNodes().Get(i));
			  ndnGlobalRoutingHelper.AddOrigin("/prefix/peer", topologyReader.GetNodes().Get(i));

			  peerKey++;
			  peerNumber++;
			  std::cout<<"node "<< i <<"is a Peer node. \n";
		  }
		  else if(randNumber == 1) // censor node
		  {
			  //continue;
			  ndn::AppHelper producerCensorHelper("ns3::ndn::ProducerCensor");
			  producerCensorHelper.SetPrefix("/prefix/file");
			  producerCensorHelper.Install(topologyReader.GetNodes().Get(i));
			  ndnGlobalRoutingHelper.AddOrigin("/prefix/file",topologyReader.GetNodes().Get(i));
			  std::cout<<"node "<< i <<"is a censor node. \n";

		  }
		  else if(randNumber == 2) // proxy node
		  {
			  ndn::AppHelper producerProxy1Helper("ns3::ndn::ProxyProducer");
			  producerProxy1Helper.SetPrefix("/cnn");
			  producerProxy1Helper.Install(topologyReader.GetNodes().Get(i));
			  ndnGlobalRoutingHelper.AddOrigin("/cnn", topologyReader.GetNodes().Get(i));

			  ndn::AppHelper producerProxy2Helper("ns3::ndn::ProxyProducer");
			  producerProxy2Helper.SetPrefix("/bbc");
			  producerProxy2Helper.Install(topologyReader.GetNodes().Get(i));
			  ndnGlobalRoutingHelper.AddOrigin("/bbc", topologyReader.GetNodes().Get(i));

			  ndn::AppHelper producerProxy3Helper("ns3::ndn::ProxyProducer");
			  producerProxy3Helper.SetPrefix("/nytimes");
			  producerProxy3Helper.Install(topologyReader.GetNodes().Get(i));
			  ndnGlobalRoutingHelper.AddOrigin("/nytimes", topologyReader.GetNodes().Get(i));
			  std::cout<<"node "<< i <<"is a proxy node. \n";
		  }
	  }

  }



  // Producer A
  ndn::AppHelper producerAMetaDataHelper("ns3::ndn::ProducerA");
  producerAMetaDataHelper.SetPrefix("/prefix/metadata");
  producerAMetaDataHelper.Install(topologyReader.GetNodes().Get(producer_node));
  ndnGlobalRoutingHelper.AddOrigin("/prefix/metadata", topologyReader.GetNodes().Get(producer_node));


  ndn::AppHelper producerAFileHelper("ns3::ndn::ProducerA");
  producerAFileHelper.SetPrefix("/prefix/file");
  producerAFileHelper.Install(topologyReader.GetNodes().Get(producer_node));
  ndnGlobalRoutingHelper.AddOrigin("/prefix/file", topologyReader.GetNodes().Get(producer_node));
  ndnGlobalRoutingHelper.AddOrigin("/prefix/file/sync", topologyReader.GetNodes().Get(producer_node));


  ndn::AppHelper consumerAHelper("ns3::ndn::ConsumerACbr");
  consumerAHelper.SetPrefix("/prefix/file/sync");  // could be a problem
  //consumerAHelper.SetAttribute("Frequency", StringValue("3"));
  consumerAHelper.Install(topologyReader.GetNodes().Get(producer_node));



  //ndn::GlobalRoutingHelper::CalculateRoutes();
  ndnGlobalRoutingHelper.CalculateRoutes();
  std::cout <<"Number of Peers = "<< peerNumber<<"\n";


  Simulator::Stop(Seconds(50.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
