#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <algorithm> 

#include "cfn-strategy.hpp"
#include "algorithm.hpp"
#include <ndn-cxx/link.hpp>

namespace nfd {
namespace fw {

CFNStrategyBase::CFNStrategyBase(Forwarder& forwarder)
  : Strategy(forwarder)
{
}

void
CFNStrategyBase::afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                                            const shared_ptr<pit::Entry>& pitEntry)
{
  Name name = interest.getName();
  Name floodingName = Name("/ndn/broadcast/flooding/");
  Name graphName = Name("/ndn/broadcast/graph/");
  Name execName = Name("/ndn/broadcast/exec/");

  std::cout << "CFN strategy received I:" << name << ". Extracted prefix: " << std::endl;

  if (hasPendingOutRecords(*pitEntry)) {
    // not a new Interest, don't forward
    return;
  }

  if(!name.compare(0, 3, floodingName)){
    std::cout << "will handle scoped flooding here" << std::endl;
    this->handleFlooding(interest);
  }

  if(!name.compare(0, 3, graphName)){
    std::cout << "will handle computation graph here" << std::endl;
    this->handleGraph(interest);
  }

  if(!name.compare(0, 3, execName)){
    std::cout << "will handle execution requests here" << std::endl;
    this->handleExec(interest, pitEntry);
  }
  //need to check what do we do about PIT entries
}

NFD_REGISTER_STRATEGY(CFNStrategy);

CFNStrategy::CFNStrategy(Forwarder& forwarder, const Name& name)
  : CFNStrategyBase(forwarder)
{
  ParsedInstanceName parsed = parseInstanceName(name);
  if (!parsed.parameters.empty()) {
    NDN_THROW(std::invalid_argument("CFNStrategy does not accept parameters"));
  }
  if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion()) {
    NDN_THROW(std::invalid_argument(
      "CFNStrategy does not support version " + to_string(*parsed.version)));
  }
  this->setInstanceName(makeInstanceName(name, getStrategyName()));
}

const Name&
CFNStrategy::getStrategyName()
{
  static Name strategyName("/localhost/nfd/strategy/cfn/%FD%01");
  return strategyName;
}


void
CFNStrategyBase::handleExec(const Interest& interest, const shared_ptr<pit::Entry>& pitEntry)
{
    std::cout << "received an execution request interest = " << interest.getName() << std::endl;    

    if(interest.hasApplicationParameters()){
      std::string parameters = std::string((char*)interest.getApplicationParameters().value());
      std::cout << "Application parameters:" << parameters << std::endl;
      // forwardingHint Format = /cfn/exec/dst_node
      uint32_t dstNode  = std::stoul(interest.getForwardingHint().at(0).name.getSubName(2,1).toUri().substr(1));
      
      if(isMineID(dstNode))
      {
        //forward to the local Python worker
      }
      else if (isMyNeighbour(dstNode))
      {
        if(isOverloaded(dstNode))
        {
          ::ndn::Link m_link;
          std::string forwardingHint = "/cfn/exec/" + std::to_string(getNodeHavingLowestLoad());
          m_link.addDelegation(0, forwardingHint);
          Interest redirctingInterest(interest.getName());
          redirctingInterest.setForwardingHint(m_link.getDelegationList());
          const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
          const fib::NextHopList& nexthops = fibEntry.getNextHops();
          auto it = nexthops.end();

          if (it != nexthops.end()) 
          {
            auto egress = FaceEndpoint(it->getFace(), 0);
            this->sendInterest(pitEntry, egress, redirctingInterest);
            return;
          }
          

        }

      }

      //return interest;

      /* 
      if(isMineID(interest.setForwardingHint())){
        //forward to the local Python worker
      }
      input_parameter = parameters.extractFirstInputParameter()
      dst_node = computationGraph.getNodeHaving(input_parameter)
      if(flooding.isMyNeighbour(dst_node)){
        if(flooding.isOverloaded(dst_node)){
          dst_node = flooding.getNeighbourWithLowestLoad();
        }
      interest.setForwardingHint(dst_node)
      return interest;

      */
    }

}

bool
CFNStrategyBase::isMineID(uint32_t NodeID)
{
  if(peerParameters[0] == NodeID)
    return true;
  else
    return false;
    
}

bool
CFNStrategyBase::isMyNeighbour(uint32_t NodeID)
{
  auto it = std::find(id.begin(), id.end(), NodeID);
  if(it != id.end())
    return true;
  else
    return false;
}

bool
CFNStrategyBase::isOverloaded(uint32_t NodeID)
{
    auto it = std::find(id.begin(), id.end(), NodeID);
    if(it != id.end())
    {
      if(cores[std::distance(std::begin(id), it)] == occupied_cores[std::distance(std::begin(id), it)]) 
        return true;  // That means the node is overlaoded 
      else
        return false; // That means the node has some free cores
    }
}

uint32_t
CFNStrategyBase::getNodeHavingLowestLoad()
{
    auto it = std::min_element(occupied_cores.begin(), occupied_cores.end());
    return id[std::distance(occupied_cores.begin(),it)];

}

void
CFNStrategyBase::handleFlooding(const Interest& interest)
{
    if(!(interest.getName().getSubName(0,3).equals("/ndn/flooding/"+to_string(peerParameters[0]))) && interest.hasApplicationParameters())
    {
      std::cout << "received interest = " << interest.getName().getSubName(0,4) << std::endl;      
      uint32_t nodeSeq = std::strtoul(interest.getName().getSubName(2,1).toUri().substr(1).c_str(),nullptr,10);
      uint32_t index;
      std::string parameters = std::string((char*)interest.getApplicationParameters().value());
      uint32_t nodeSeq_core = std::strtoul(parameters.substr(parameters.find("c")+1, parameters.find("o") - parameters.find("c")).c_str(),nullptr,10);
      uint32_t nodeSeq_occupied_cores = std::strtoul(parameters.substr(parameters.find("o")+1, parameters.find("q") - parameters.find("o")).c_str(),nullptr,10);
      uint32_t nodeSeq_queued_jobs = std::strtoul(parameters.substr(parameters.find("q")+1, parameters.find("e") - parameters.find("q")).c_str(),nullptr,10);
      auto it = std::find(id.begin(), id.end(), nodeSeq);


      if(it != id.end())
      {
        index = it - id.begin();
        cores[index] = nodeSeq_core;
        occupied_cores[index] = nodeSeq_occupied_cores;
        queued_jobs[index] = nodeSeq_queued_jobs;
        keep_alive[index] = 3;
      }
      else
      {
        id.push_back(nodeSeq);
        cores.push_back(nodeSeq_core);
        occupied_cores.push_back(nodeSeq_occupied_cores);
        queued_jobs.push_back(nodeSeq_queued_jobs);
        keep_alive.push_back(3);
      }
      
    }
}


void
CFNStrategyBase::handleGraph(const Interest& interest)
{
  if(!(interest.getName().getSubName(0,3).equals("/ndn/broadcast/"+to_string(peerParameters[0]))) && interest.hasApplicationParameters())
    {

      if(interest.getName().getSubName(3,1).equals("/update"))
      {
        std::cout << "received interest = " << interest.getName().getSubName(0,4) << std::endl;
    
        std::string parametersgraph = std::string((char*)interest.getApplicationParameters().value());
        
        
        std::string initial = "graphsize:";
        std::string last = "graphsizeend:" ;
        

        int gs = std::strtoul(parametersgraph.substr(parametersgraph.find(initial) + initial.length(), parametersgraph.find(last) - (parametersgraph.find(initial) + initial.length())).c_str(),nullptr,10);
        for(int j = 0; j < gs; j++)
        {
            std::stringstream temp_initial_task;
            temp_initial_task << "task" << j << ":";
            initial = temp_initial_task.str();
            std::stringstream temp_last_task;
            temp_last_task << "taskend" << j << ":";
            last = temp_last_task.str();
            std::string parameters = parametersgraph.substr(parametersgraph.find(initial) + initial.length(), parametersgraph.find(last) - (parametersgraph.find(initial) + initial.length()));

            objectInfo new_info;
            std::string initial = "name:";
            std::string last = "type:" ;
            //std::cout << "name = " << parameters.substr(parameters.find(initial) + initial.length(), parameters.find(last) - (parameters.find(initial) + initial.length())) << std::endl;
            new_info.name = parameters.substr(parameters.find(initial) + initial.length(), parameters.find(last) - (parameters.find(initial) + initial.length()));

            initial = "type:";
            last = "caller:";
            //std::cout << "type = " << parameters.substr(parameters.find(initial) + initial.length(), parameters.find(last) - (parameters.find(initial) + initial.length())) << std::endl;
            new_info.type = std::strtoul(parameters.substr(parameters.find(initial) + initial.length(), parameters.find(last) - (parameters.find(initial) + initial.length())).c_str(),nullptr,10);

            initial = "caller:";
            last = "inputsize:";
            new_info.caller = parameters.substr(parameters.find(initial) + initial.length(), parameters.find(last) - (parameters.find(initial) + initial.length()));
            //std::cout << "caller = " << new_info.caller << std::endl;

            initial = "inputsize:";
            last = "inputsizeend:";
            int input_size = std::strtoul(parameters.substr(parameters.find(initial) + initial.length(), parameters.find(last) - (parameters.find(initial) + initial.length())).c_str(),nullptr,10);
            //std::cout << "input size raw = " << parameters.substr(parameters.find(initial) + initial.length(), parameters.find(last) - (parameters.find(initial) + initial.length())) << std::endl;
        
            //std::cout << "input size = " << input_size << std::endl;

            for(int i = 0; i < input_size; i++)
            {
                std::stringstream temp_initial_name;
                temp_initial_name << "inputname" << i << ":"; 
                initial = temp_initial_name.str();
                std::stringstream temp_last_name;
                temp_last_name << "inputdatasize" << i << ":";
                last = temp_last_name.str();
                dataInfo new_dataInfo;
                new_dataInfo.name = parameters.substr(parameters.find(initial) + initial.length(), parameters.find(last) - (parameters.find(initial) + initial.length()));

                initial = last;
                std::stringstream temp_last_size;
                temp_last_size << "inputend" << i << ":";
                last = temp_last_size.str();
                new_dataInfo.size = std::strtoul(parameters.substr(parameters.find(initial) + initial.length(), parameters.find(last) - (parameters.find(initial) + initial.length())).c_str(),nullptr,10);

                new_info.inputs.insert(new_dataInfo);
            
            }

            initial = "outputsize:";
            last = "outputsizeend:";
            int output_size = std::strtoul(parameters.substr(parameters.find(initial) + initial.length(), parameters.find(last) - (parameters.find(initial) + initial.length())).c_str(),nullptr,10);
            //std::cout << "output size raw = " << parameters.substr(parameters.find(initial) + initial.length(), parameters.find(last) - (parameters.find(initial) + initial.length())) << std::endl;
            
            //std::cout << "output size = " << output_size << std::endl;

            for(int i = 0; i < output_size; i++)
            {
                std::stringstream temp_initial_name;
                temp_initial_name << "outputname" << i << ":";
                initial = temp_initial_name.str();
                std::stringstream temp_last_name;
                temp_last_name << "outdatasize" << i << ":";
                last = temp_last_name.str();
                dataInfo new_dataInfo;
                new_dataInfo.name = parameters.substr(parameters.find(initial) + initial.length(), parameters.find(last) - (parameters.find(initial) + initial.length()));
                //std::cout << "out name = " << new_dataInfo.name << std::endl;
                initial = last;
                std::stringstream temp_last_size;
                temp_last_size << "outend" << i << ":";
                last = temp_last_size.str();
                new_dataInfo.size = std::strtoul(parameters.substr(parameters.find(initial) + initial.length(), parameters.find(last) - (parameters.find(initial) + initial.length())).c_str(),nullptr,10);

                new_info.outputs.insert(new_dataInfo);
                
            }

            initial = "thunk:";
            last = "duration:";
            new_info.thunk = parameters.substr(parameters.find(initial) + initial.length(), parameters.find(last) - (parameters.find(initial) + initial.length()));

            //std::cout << "thunk = " << new_info.thunk << std::endl;

            initial = last;
            last = "endofparameters";

            new_info.duration = std::strtoul(parameters.substr(parameters.find(initial) + initial.length(), parameters.find(last) - (parameters.find(initial) + initial.length())).c_str(),nullptr,10);
            //std::cout << "duration = " << new_info.duration << std::endl;
            //int size1 = updatedGraph.items.size();
            updatedGraph.addNode(new_info);
            /*
            int size2 = updatedGraph.items.size();
            if (size2 > size1)
            {
              std::cout << "new size = " << size2 << std::endl;
            }
            else
            {
              std::cout << "duplicate name = " << new_info.name << std::endl;
            }
            */
        }
      }

      else if(interest.getName().getSubName(3,1).equals("/scopeupdate"))
      {
        //std::cout << "received interest = " << interest.getName().getSubName(0,4) << std::endl;
        //std::cout << "Node seq = "<< interest.getName().getSubName(2,1).toUri().substr(1) << std::endl;
        uint32_t nodeSeq = std::strtoul(interest.getName().getSubName(2,1).toUri().substr(1).c_str(),nullptr,10);
        uint32_t index;
        std::string parameters = std::string((char*)interest.getApplicationParameters().value());
        //std::cout << " para core = " << parameters.substr(parameters.find("c"), parameters.find("o") - parameters.find("c")) <<std::endl;
        uint32_t nodeSeq_core = std::strtoul(parameters.substr(parameters.find("c")+1, parameters.find("o") - parameters.find("c")).c_str(),nullptr,10);
        //std::cout << " para qcc = " << parameters.substr(parameters.find("o"), parameters.find("q") - parameters.find("o")) <<std::endl;
        uint32_t nodeSeq_occupied_cores = std::strtoul(parameters.substr(parameters.find("o")+1, parameters.find("q") - parameters.find("o")).c_str(),nullptr,10);
        //std::cout << " para jobs = " << parameters.substr(parameters.find("q"), parameters.find("e") - parameters.find("q")) <<std::endl;
        uint32_t nodeSeq_queued_jobs = std::strtoul(parameters.substr(parameters.find("q")+1, parameters.find("e") - parameters.find("q")).c_str(),nullptr,10);
        //std::cout<<"Parameters = "<< parameters<<"\n";
        //std::vector<int>::iterator it = std::find(id.begin(), id.end(), nodeSeq);
        auto it = std::find(id.begin(), id.end(), nodeSeq);
        //std::vector<int>::std::iterator it = std::find(id.begin(), id.end(), nodeSeq);


        if(it != id.end())
        {
          //index = std::distance(id.begin(), it);
          index = it - id.begin();
          cores[index] = nodeSeq_core;
          occupied_cores[index] = nodeSeq_occupied_cores;
          queued_jobs[index] = nodeSeq_queued_jobs;
          keep_alive[index] = 3;
        }
        else
        {
          id.push_back(nodeSeq);
          cores.push_back(nodeSeq_core);
          occupied_cores.push_back(nodeSeq_occupied_cores);
          queued_jobs.push_back(nodeSeq_queued_jobs);
          keep_alive.push_back(3);
        }
        
      }
    }
}

} // namespace fw
} // namespace nfd
