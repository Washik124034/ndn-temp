#ifndef NFD_DAEMON_FW_CFN_STRATEGY_HPP
#define NFD_DAEMON_FW_CFN_STRATEGY_HPP

#include "strategy.hpp"
#include "computation-graph.hpp"

namespace nfd {
namespace fw {

class CFNStrategyBase : public Strategy
{
public:
  void
  afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                       const shared_ptr<pit::Entry>& pitEntry) override;

protected:
  CFNStrategyBase(Forwarder& forwarder);

private:
  void
  handleExec(const Interest& interest);

  void
  handleFlooding(const Interest& interest);

  void
  handleGraph(const Interest& interest);


private:
  uint32_t peerParameters[4]; // peerParameters[0] = id, peerParameters[1] = number of cores, peerParameters[2] = number of occupied cores, peerParameters[3] = number of queued jobs
  std::vector <uint32_t> id;
  std::vector <uint32_t> cores;
  std::vector <uint32_t> occupied_cores;
  std::vector <uint32_t> queued_jobs;
  std::vector <uint32_t> keep_alive;
  uint32_t runTime = 0;

  std::string json_file;
  ComputationGraph localGraph;
  ComputationGraph updatedGraph;
};

/** \brief CFN strategy version 1
 *
 *  This strategy is used with CFN2 protocol
 *
 */
class CFNStrategy : public CFNStrategyBase
{
public:
  explicit
  CFNStrategy(Forwarder& forwarder, const Name& name = getStrategyName());

  static const Name&
  getStrategyName();

};

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_BEST_ROUTE_STRATEGY_HPP
