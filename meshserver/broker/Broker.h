/*=========================================================================

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifndef __broker_h
#define __broker_h

#include <zmq.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/uuid/random_generator.hpp>

#include <meshserver/internal/meshServerGlobals.h>

namespace meshserver{
namespace internal{
//forward declaration of classes only the implementation needs
class JobMessage;
class JobQueue;
}
}

namespace meshserver{
namespace broker{
  //forward declaration of classes only the implementation needs
  namespace internal{class JobQueue;}
  
class Broker
{
public:
  Broker();
  ~Broker();
  bool startBrokering();

protected:
  //processes all job queries
  void DetermineJobResponse(const std::string &clientAddress,
                            const meshserver::internal::JobMessage& msg);

  //These methods are all to do with send responses to job messages
  bool canMesh(const meshserver::internal::JobMessage& msg);
  meshserver::STATUS_TYPE meshStatus(const meshserver::internal::JobMessage& msg);
  std::string queueJob(const meshserver::internal::JobMessage& msg);
  std::string retrieveMesh(const meshserver::internal::JobMessage& msg);

  //Methods for processing Worker queries
  void DetermineWorkerResponse();

  //Sends a single job to a worker
  void DispatchJob();
private:
  boost::uuids::random_generator UUIDGenerator;
  boost::scoped_ptr<meshserver::broker::internal::JobQueue> Jobs;

  zmq::context_t Context;
  zmq::socket_t JobQueries;
  zmq::socket_t WorkerQueries;
  zmq::socket_t Workers;
};

}
}


#endif
