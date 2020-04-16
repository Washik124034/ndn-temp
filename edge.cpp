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

#include "edge.hpp"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"
#include "ns3/random-variable-stream.h"

#include <memory>
#include <fstream>
#include <iostream>
#include <string>

#include "ns3/ptr.h"
#include "ns3/callback.h"
#include "ns3/boolean.h"
#include "ns3/integer.h"
#include "ns3/double.h"

#include "utils/ndn-ns3-packet-tag.hpp"
#include "utils/ndn-rtt-mean-deviation.hpp"

#include <ndn-cxx/lp/tags.hpp>
#include "ns3/ndnSIM/ndn-cxx/link.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/ref.hpp>

NS_LOG_COMPONENT_DEFINE("ndn.Edge");

namespace ns3
{
namespace ndn
{
using ::ndn::Delegation;
using ::ndn::DelegationList;

NS_OBJECT_ENSURE_REGISTERED(Edge);

TypeId
Edge::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::ndn::Edge")
            .SetGroupName("Ndn")
            .SetParent<App>()
            .AddConstructor<Edge>()
            .AddAttribute("EdgeID", "",
                          IntegerValue(std::numeric_limits<uint32_t>::max()),
                          MakeIntegerAccessor(&Edge::m_EdgeID), MakeIntegerChecker<uint32_t>())
            .AddAttribute(
                "Postfix",
                "Postfix that is added to the output data (e.g., for adding Edge-uniqueness)",
                StringValue("/"), MakeNameAccessor(&Edge::m_postfix), MakeNameChecker())
            .AddAttribute("PayloadSize", "Virtual payload size for Content packets", UintegerValue(1024),
                          MakeUintegerAccessor(&Edge::m_virtualPayloadSize),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("LifeTime", "LifeTime for interest packet", StringValue("2s"),
                          MakeTimeAccessor(&Edge::m_interestLifeTime), MakeTimeChecker())
            .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                          TimeValue(Seconds(0)), MakeTimeAccessor(&Edge::m_freshness),
                          MakeTimeChecker())
            .AddAttribute("StartSeq", "",
                          IntegerValue(std::numeric_limits<uint32_t>::max()),
                          MakeIntegerAccessor(&Edge::m_seq), MakeIntegerChecker<uint32_t>())
            .AddAttribute(
                "Signature",
                "Fake signature, 0 valid signature (default), other values application-specific",
                UintegerValue(0), MakeUintegerAccessor(&Edge::m_signature),
                MakeUintegerChecker<uint32_t>())
            .AddAttribute("KeyLocator",
                          "Name to be used for key locator.  If root, then key locator is not used",
                          NameValue(), MakeNameAccessor(&Edge::m_keyLocator), MakeNameChecker());
    return tid;
}

Edge::Edge()
    : m_rand(CreateObject<UniformRandomVariable>())
    , m_Utility(14)
{
    NS_LOG_FUNCTION_NOARGS();
    m_sendEvent = Simulator::Schedule(Seconds(1.0), &Edge::SendInterest, this);
}

// inherited from Application base class.
void Edge::StartApplication()
{
    NS_LOG_FUNCTION_NOARGS();
    App::StartApplication();
    m_UpdatePrefix = "/update/edge/" + std::to_string(m_EdgeID);
    FibHelper::AddRoute(GetNode(), m_UpdatePrefix, m_face, 0);
    m_OverlayUpdatePrefix = "/update/overlay";
    FibHelper::AddRoute(GetNode(), m_OverlayUpdatePrefix, m_face, 0);
    Name ForwardingHintPrefix = "/taskoffload/overlay";
    //NS_LOG_INFO(" Overlay FH = " << ForwardingHintPrefix.append(std::to_string(m_EdgeID)));
    FibHelper::AddRoute(GetNode(), ForwardingHintPrefix.append(std::to_string(m_EdgeID)), m_face, 0); //  for forwarding hint
}

void Edge::StopApplication()
{
    NS_LOG_FUNCTION_NOARGS();

    App::StopApplication();
}

void Edge::SendInterest()
{
    
    NS_LOG_FUNCTION_NOARGS();
    shared_ptr<Name> OverlayInterestName = make_shared<Name>(m_OverlayUpdatePrefix);
    OverlayInterestName->append(std::to_string(m_EdgeID) + "/" + std::to_string(Simulator::Now().GetMilliSeconds()));
    std::stringstream parameters;
    parameters << "numberofnode:" << Local_Node_ID.size() << ":endofNON";
    for(uint32_t i = 0; i < Local_Node_ID.size(); i++)
    {
        parameters << "seq" << i << ":" << "nodeid:" << Local_Node_ID[i] <<"utility:" << Local_Node_Utility[i] << "rtt:" << Local_rtt[i] << ":end" << "seqend" << i; 
    }
  
    std::string temp_parameters = parameters.str();
    const char* buff = temp_parameters.c_str();
	size_t buff_size = temp_parameters.length();

    shared_ptr<Interest> Overlayinterest = make_shared<Interest>();
    Overlayinterest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
    Overlayinterest->setName(*OverlayInterestName);
    Overlayinterest->setCanBePrefix(false);
    time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
    Overlayinterest->setInterestLifetime(interestLifeTime);
    Overlayinterest->setParameters(make_shared< ::ndn::Buffer>(buff,buff_size));
    NS_LOG_INFO("Overlay " << m_EdgeID << " Sending update = " << *OverlayInterestName);

    m_transmittedInterests(Overlayinterest, this, m_face);
    m_appLink->onReceiveInterest(*Overlayinterest);
    
    ScheduleNextPacket();
    
}

void Edge::ScheduleNextPacket()
{
    
    if (!m_sendEvent.IsRunning())
        m_sendEvent = Simulator::Schedule(Seconds(1.0), &Edge::SendInterest, this);
    
}

void Edge::OnInterest(shared_ptr<const Interest> interest)
{

    App::OnInterest(interest); // tracing inside

    NS_LOG_FUNCTION(this << interest);

    if (!m_active)
        return;

    Name dataName(interest->getName());
    //NS_LOG_INFO("Overlay " << m_EdgeID << "received " << dataName);
    if(dataName.getSubName(0,3).equals("/update/edge/" + std::to_string(m_EdgeID)))
    {
        uint32_t update_node_ID = std::strtoul(dataName.getSubName(4,1).toUri().substr(1).c_str(),nullptr,10);
        uint32_t update_node_utility = std::strtoul(dataName.getSubName(5,1).toUri().substr(1).c_str(),nullptr,10);
        int64_t update_node_rtt = Simulator::Now().GetMilliSeconds() - std::stoll (dataName.getSubName(6,1).toUri().substr(1));
        NS_LOG_INFO("Update from " << m_EdgeID << " Node " << update_node_ID << " = " << update_node_utility);

        std::string update_node_combine_ID = std::to_string(m_EdgeID) + std::to_string(update_node_ID); 
        auto it = std::find(Local_Node_ID.begin(), Local_Node_ID.end(), update_node_ID);

        if(it != Local_Node_ID.end())
        {
          uint32_t index = it - Local_Node_ID.begin();
          Local_Node_Utility[index] = update_node_utility;
          Local_rtt[index] = update_node_rtt;
        }
        else
        {
          Local_Node_ID.push_back(update_node_ID);
          Local_Node_Utility.push_back(update_node_utility);
          Local_rtt.push_back(update_node_rtt);
        }
        auto it2 = std::find(CombineID.begin(), CombineID.end(), update_node_combine_ID);

        if(it2 != CombineID.end())
        {
            uint32_t index = it2 - CombineID.begin();
            Node_Utility[index] = update_node_utility;
            rtt[index] = update_node_rtt;
            //Edge_ID[index] = m_EdgeID;
        }
        else
        {
            Node_ID.push_back(update_node_ID);
            Node_Utility.push_back(update_node_utility);
            Edge_ID.push_back(m_EdgeID);
            CombineID.push_back(update_node_combine_ID);
            rtt.push_back(update_node_rtt);
        }
        for(uint32_t i = 0; i< Node_ID.size(); i++)
        {
            NS_LOG_INFO("Edge = " << Edge_ID[i] << " Node = " << Node_ID[i] << " Utility = " << Node_Utility[i] << " RTT = " << rtt[i]);
        }
    }

    else if(dataName.getSubName(0,2).equals("/update/overlay") && interest->hasParameters())
    {
        
        uint32_t updatingEdgeID = std::strtoul(dataName.getSubName(2,1).toUri().substr(1).c_str(),nullptr,10);
        int64_t updatingEdgeRTT = Simulator::Now().GetMilliSeconds() - std::stoll (dataName.getSubName(3,1).toUri().substr(1));
        
        Block content = interest->getParameters();
		std::string interestParameters = std::string((char*)content.value());
        std::string initial = "numberofnode:";
        std::string last = ":endofNON" ;
        uint32_t update_edge_size = std::strtoul(interestParameters.substr(interestParameters.find(initial) + initial.length(), interestParameters.find(last) - (interestParameters.find(initial) + initial.length())).c_str(),nullptr,10);
        
        for(uint32_t i = 0; i < update_edge_size; i++)
        {

            initial = "seq" + std::to_string(i) + ":";
            last = "seqend" + std::to_string(i);
            
            std::string parameters_of_single_node = interestParameters.substr(interestParameters.find(initial) + initial.length(), interestParameters.find(last) - (interestParameters.find(initial) + initial.length()));
            
            initial = "nodeid:";
            last = "utility:";
            uint32_t updatingNodeID = std::strtoul(parameters_of_single_node.substr(parameters_of_single_node.find(initial) + initial.length(), parameters_of_single_node.find(last) - (parameters_of_single_node.find(initial) + initial.length())).c_str(),nullptr,10);
            
            initial = last;
            last = "rtt:";
            uint32_t updatingNodeUtility = std::strtoul(parameters_of_single_node.substr(parameters_of_single_node.find(initial) + initial.length(), parameters_of_single_node.find(last) - (parameters_of_single_node.find(initial) + initial.length())).c_str(),nullptr,10);
            

            initial = last;
            last = ":end";

            int64_t updatingNodeRTT = updatingEdgeRTT + std::stoll (parameters_of_single_node.substr(parameters_of_single_node.find(initial) + initial.length(), parameters_of_single_node.find(last) - (parameters_of_single_node.find(initial) + initial.length())));
            std::string updatingCombineID = std::to_string(updatingEdgeID) + std::to_string(updatingNodeID);
            auto it = std::find(CombineID.begin(), CombineID.end(), updatingCombineID);
            
            if(it != CombineID.end())
            {
                uint32_t index = it - CombineID.begin();
                Node_Utility[index] = updatingNodeUtility;
                rtt[index] = updatingNodeRTT;
                //Edge_ID[index] = UpdatingEdgeID;
            }
            else
            {
                Node_ID.push_back(updatingNodeID);
                Node_Utility.push_back(updatingNodeUtility);
                rtt.push_back(updatingNodeRTT);
                Edge_ID.push_back(updatingEdgeID);
                CombineID.push_back(updatingCombineID);
            }

            

        }

        for(uint32_t j = 0; j< Node_ID.size(); j++)
        {
            NS_LOG_INFO(" Overlay " << m_EdgeID << " updated : Edge = " << Edge_ID[j] << " Node = " << Node_ID[j] << " Utility = " << Node_Utility[j] << " RTT = " << rtt[j]);
        }

    }

    else if(dataName.getSubName(0,1).equals("/task"))
    {
        uint32_t taskUtility = std::stoul(dataName.getSubName(1,1).toUri().substr(1));
        NS_LOG_INFO("Task Utility = " << taskUtility);


        if(m_Utility >= taskUtility)
        {
            m_Utility -= taskUtility;
            auto data = make_shared<Data>();
            data->setName(dataName);
            data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));

            data->setContent(make_shared< ::ndn::Buffer>(m_virtualPayloadSize));

            Signature signature;
            SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

            if (m_keyLocator.size() > 0) {
                signatureInfo.setKeyLocator(m_keyLocator);
            }

            signature.setInfo(signatureInfo);
            signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

            data->setSignature(signature);

            NS_LOG_INFO(" Overlay node(" << GetNode()->GetId() << ") responding with Data: " << data->getName());

            // to create real wire encoding
            data->wireEncode();

            m_Utility += taskUtility;

            m_transmittedDatas(data, this, m_face);
            m_appLink->onReceiveData(*data);
        }

        else
        {
            auto it = std::find_if(std::begin(Node_Utility), std::end(Node_Utility), [&taskUtility](uint32_t i){return i >= taskUtility ;});
            //std::vector<int> validNodeIndices;
            bool isMatchedLocally = false;
            int matchedNodeID = -1;
            int matchedEdgeIndex = -1;
            ::ndn::Link m_link;
            
            if(it != std::end(Node_Utility))
            {
                matchedEdgeIndex = std::distance(std::begin(Node_Utility), it);
                matchedNodeID = Node_ID[matchedEdgeIndex];
                int min_delay =  rtt[matchedEdgeIndex];
                //matchedNodeIndex
                while (it != std::end(Node_Utility)) 
                {
                    //validNodeIndices.push_back(std::distance(std::begin(Node_Utility), it));
                    //int validNodeIndex = std::distance(std::begin(Node_Utility), it);
                    if(min_delay > rtt[std::distance(std::begin(Node_Utility), it)])
                    {
                        matchedEdgeIndex = std::distance(std::begin(Node_Utility), it);
                        matchedNodeID = Node_ID[matchedEdgeIndex];
                        min_delay = rtt[matchedEdgeIndex];
                    }
                    
                    it = std::find_if(std::next(it), std::end(Node_Utility), [&taskUtility](uint32_t i){return i > taskUtility;});
                }

                if(Edge_ID[matchedEdgeIndex] != m_EdgeID)
                {
                    m_link.addDelegation(0, "/taskoffload/overlay/" + std::to_string(Edge_ID[matchedEdgeIndex]));
                    NS_LOG_INFO("Overlay Redirecting to " << "/taskoffload/overlay/" + std::to_string(Edge_ID[matchedEdgeIndex]));

                }
                else // that means found match locally
                {
                    m_link.addDelegation(0, "/update/edge/" + std::to_string(Edge_ID[matchedEdgeIndex]) + "/" + std::to_string(Node_ID[matchedEdgeIndex]));
                    NS_LOG_INFO(" Overlay Redirecting to " << "/update/edge/" + std::to_string(Edge_ID[matchedEdgeIndex]) + "/" + std::to_string(Node_ID[matchedEdgeIndex]));
                
                }
                
                
                
            }

            else
            {

                m_link.addDelegation(0, "/cloud/task");
                NS_LOG_INFO(" Overlay Redirecting to " << "/cloud/task");  
            
            }
            auto redirectingInterest = make_shared<Interest>(dataName);
            redirectingInterest->setInterestLifetime(time::seconds(1));
            redirectingInterest->setForwardingHint(m_link.getDelegationList());
            NS_LOG_INFO(" Overlay task " << dataName << " " << redirectingInterest->getForwardingHint());
            m_transmittedInterests(redirectingInterest, this, m_face);
            m_appLink->onReceiveInterest(*redirectingInterest);
            
            
        }
        
    }

    
    
}

void Edge::OnData(shared_ptr<const Data> data)
{
    if (!m_active)
        return;

    App::OnData(data); // tracing inside

   // NS_LOG_FUNCTION(this << data);

    int hopCount = 0;
    auto hopCountTag = data->getTag<lp::HopCountTag>();
    if (hopCountTag != nullptr)
    { // e.g., packet came from local node's cache
        hopCount = *hopCountTag;
    }
   // NS_LOG_DEBUG("Hop count: " << hopCount);
}

} // namespace ndn
} // namespace ns3
