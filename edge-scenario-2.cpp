/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

namespace ns3
{
int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    AnnotatedTopologyReader topologyReader("", 25);
    topologyReader.SetFileName("src/ndnSIM/examples/topologies/edge-topology.txt");
    topologyReader.Read();

    // Install NDN stack on all nodes
    ndn::StackHelper ndnHelper;
    ndnHelper.InstallAll();

    ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
    ndnGlobalRoutingHelper.InstallAll();

    // Choosing forwarding strategy
    ndn::StrategyChoiceHelper::InstallAll("/update/edge", "/localhost/nfd/strategy/multicast");
    ndn::StrategyChoiceHelper::InstallAll("/update/overlay", "/localhost/nfd/strategy/multicast");
    ndn::StrategyChoiceHelper::InstallAll("/taskoffload/overlay/2", "/localhost/nfd/strategy/best-route");
    ndn::StrategyChoiceHelper::InstallAll("/taskoffload/edge/2/2", "/localhost/nfd/strategy/best-route");
    ndn::StrategyChoiceHelper::InstallAll("/task", "/localhost/nfd/strategy/best-route");
    ndn::StrategyChoiceHelper::InstallAll("/cloud", "/localhost/nfd/strategy/best-route");
    // Installing applications
    int number_of_edge = 2;
    int number_of_nodes_in_edges[number_of_edge] = {2,3};
    for (int i = 1; i <= number_of_edge; i++)
    {
        
        Ptr<Node> overLay = Names::Find<Node>("Over" + to_string(i));
        ndn::AppHelper edge("ns3::ndn::Edge");
        edge.SetAttribute("EdgeID", StringValue(to_string(i)));
        edge.Install(overLay);
        ndnGlobalRoutingHelper.AddOrigin("/update/edge/"+to_string(i), overLay);
        ndnGlobalRoutingHelper.AddOrigin("/update/overlay", overLay);
        ndnGlobalRoutingHelper.AddOrigin("/taskoffload/overlay/" + to_string(i), overLay);
        
        for(int j = 1; j <= number_of_nodes_in_edges[i-1]; j++)
        {
            Ptr<Node> edge = Names::Find<Node>("Edge" + std::to_string(i) + "_" + std::to_string(j) );
            ndn::AppHelper localedge("ns3::ndn::LocalEdge");
            localedge.SetAttribute("EdgeID", StringValue(std::to_string(i)));
            localedge.SetAttribute("NodeID", StringValue(std::to_string(j)));
            localedge.Install(edge);
            ndnGlobalRoutingHelper.AddOrigin("/update/edge/"+std::to_string(i), edge);
            ndnGlobalRoutingHelper.AddOrigin("/taskoffload/edge/"+std::to_string(i)+'/' + std::to_string(j), edge);
        }
        
    }
    Ptr<Node> user1_1 = Names::Find<Node>("User1_1");
    ndn::AppHelper user1("ns3::ndn::UserApp");
    
    user1.Install(user1_1);


    Ptr<Node> cloud1 = Names::Find<Node>("Cloud1");
    ndn::AppHelper cloud1node("ns3::ndn::Producer");
    cloud1node.SetPrefix("/cloud");
    cloud1node.SetAttribute("PayloadSize", StringValue("1024"));
    cloud1node.Install(cloud1);
    
    
    ndn::GlobalRoutingHelper::CalculateRoutes();
    Simulator::Stop(Seconds(5.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
} // namespace ns3

int main(int argc, char *argv[])
{
    return ns3::main(argc, argv);
}
