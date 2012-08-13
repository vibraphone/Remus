//=============================================================================
//
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//
//=============================================================================

#include <remus/server/Server.h>

#include <boost/uuid/uuid.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <remus/Job.h>
#include <remus/JobResult.h>
#include <remus/JobStatus.h>

#include <remus/common/JobMessage.h>
#include <remus/common/JobResponse.h>

#include <remus/common/remusGlobals.h>
#include <remus/common/zmqHelper.h>

#include <remus/server/internal/uuidHelper.h>
#include <remus/server/internal/ActiveJobs.h>
#include <remus/server/internal/JobQueue.h>
#include <remus/server/internal/WorkerPool.h>


namespace remus{
namespace server{

//------------------------------------------------------------------------------
Server::Server():
  Context(1),
  ClientQueries(Context,ZMQ_ROUTER),
  WorkerQueries(Context,ZMQ_ROUTER),
  UUIDGenerator(), //use default random number generator
  QueuedJobs(new remus::server::internal::JobQueue() ),
  WorkerPool(new remus::server::internal::WorkerPool() ),
  ActiveJobs(new remus::server::internal::ActiveJobs () ),
  WorkerFactory()
  {
  //attempts to bind to a tcp socket, with a prefered port number
  //For accurate information on what the socket actually bond to,
  //check the Job Socket Info class
  this->ClientSocketInfo = zmq::bindToTCPSocket(
                             this->ClientQueries,
                             remus::BROKER_CLIENT_PORT);
  this->WorkerSocketInfo = zmq::bindToTCPSocket(
                             this->WorkerQueries,
                             remus::BROKER_WORKER_PORT);

  //give to the worker factory the endpoint information needed to connect
  //to myself. The WorkerSocketInfo host name is not what we want to
  //give to the factory as it might not
  //be the external name we are interested in
  zmq::socketInfo<zmq::proto::tcp> externalInfo("127.0.0.1",
                                                this->WorkerSocketInfo.port());
  this->WorkerFactory.addCommandLineArgument(externalInfo.endpoint());
  }

//------------------------------------------------------------------------------
Server::Server(const remus::server::WorkerFactory& factory):
  Context(1),
  ClientQueries(Context,ZMQ_ROUTER),
  WorkerQueries(Context,ZMQ_ROUTER),
  UUIDGenerator(), //use default random number generator
  QueuedJobs(new remus::server::internal::JobQueue() ),
  WorkerPool(new remus::server::internal::WorkerPool() ),
  ActiveJobs(new remus::server::internal::ActiveJobs () ),
  WorkerFactory(factory)
  {
  //attempts to bind to a tcp socket, with a prefered port number
  //For accurate information on what the socket actually bond to,
  //check the Job Socket Info class
  this->ClientSocketInfo = zmq::bindToTCPSocket(
                             this->ClientQueries,
                             remus::BROKER_CLIENT_PORT);
  this->WorkerSocketInfo = zmq::bindToTCPSocket(
                             this->WorkerQueries,
                             remus::BROKER_WORKER_PORT);

  //give to the worker factory the endpoint information needed to connect
  //to myself. The WorkerSocketInfo host name is not what we want to
  //give to the factory as it might not
  //be the external name we are interested in
  zmq::socketInfo<zmq::proto::tcp> externalInfo("127.0.0.1",
                                                this->WorkerSocketInfo.port());
  this->WorkerFactory.addCommandLineArgument(externalInfo.endpoint());
  }

//------------------------------------------------------------------------------
Server::~Server()
{

}

//------------------------------------------------------------------------------
bool Server::startBrokering()
  {
  zmq::pollitem_t items[2] = {
      { this->ClientQueries,  0, ZMQ_POLLIN, 0 },
      { this->WorkerQueries, 0, ZMQ_POLLIN, 0 } };

  //  Process messages from both sockets
  while (true)
    {
    zmq::poll (&items[0], 2, remus::HEARTBEAT_INTERVAL);
    boost::posix_time::ptime hbTime = boost::posix_time::second_clock::local_time();
    if (items[0].revents & ZMQ_POLLIN)
      {
      //we need to strip the client address from the message
      zmq::socketIdentity clientIdentity = zmq::address_recv(this->ClientQueries);

      //Note the contents of the message isn't valid
      //after the DetermineJobQueryResponse call
      remus::common::JobMessage message(this->ClientQueries);
      this->DetermineJobQueryResponse(clientIdentity,message); //NOTE: this will queue jobs
      }
    if (items[1].revents & ZMQ_POLLIN)
      {
      //a worker is registering
      //we need to strip the worker address from the message
      zmq::socketIdentity workerIdentity = zmq::address_recv(this->WorkerQueries);

      //Note the contents of the message isn't valid
      //after the DetermineWorkerResponse call
      remus::common::JobMessage message(this->WorkerQueries);
      this->DetermineWorkerResponse(workerIdentity,message);

      //refresh all jobs for a given worker with a new expiry time
      this->ActiveJobs->refreshJobs(workerIdentity);

      //refresh the worker if it is actuall in the pool instead of doing a job
      this->WorkerPool->refreshWorker(workerIdentity);
      }

    //mark all jobs whose worker haven't sent a heartbeat in time
    //as a job that failed.
    this->ActiveJobs->markExpiredJobs(hbTime);

    //purge all pending workers with jobs that haven't sent a heartbeat
    this->WorkerPool->purgeDeadWorkers(hbTime);

    //see if we have a worker in the pool for the next job in the queue,
    //otherwise as the factory to generat a new worker to handle that job
    this->FindWorkerForQueuedJob();
    }
  return true;
  }

//------------------------------------------------------------------------------
void Server::DetermineJobQueryResponse(const zmq::socketIdentity& clientIdentity,
                                  const remus::common::JobMessage& msg)
{
  //msg.dump(std::cout);
  //server response is the general response message type
  //the client can than convert it to the expected type
  remus::common::JobResponse response(clientIdentity);
  if(!msg.isValid())
    {
    response.setServiceType(remus::INVALID_SERVICE);
    response.setData(remus::INVALID_MSG);
    response.send(this->ClientQueries);
    return; //no need to continue
    }
  response.setServiceType(msg.serviceType());

  //we have a valid job, determine what to do with it
  switch(msg.serviceType())
    {
    case remus::MAKE_MESH:
      response.setData(this->queueJob(msg));
      break;
    case remus::MESH_STATUS:
      response.setData(this->meshStatus(msg));
      break;
    case remus::CAN_MESH:
      response.setData(this->canMesh(msg));
      break;
    case remus::RETRIEVE_MESH:
      response.setData(this->retrieveMesh(msg));
      break;
    case remus::SHUTDOWN:
      response.setData(this->terminateJob(msg));
      break;
    default:
      response.setData(remus::INVALID_STATUS);
    }
  response.send(this->ClientQueries);
  return;
}

//------------------------------------------------------------------------------
bool Server::canMesh(const remus::common::JobMessage& msg)
{
  //ToDo: add registration of mesh type
  //how is a generic worker going to register its type? static method?
  bool haveSupport = this->WorkerFactory.haveSupport(msg.meshType());
  haveSupport = haveSupport || this->WorkerPool->haveWaitingWorker(msg.meshType());
  return haveSupport;
}

//------------------------------------------------------------------------------
std::string Server::meshStatus(const remus::common::JobMessage& msg)
{
  remus::Job job = remus::to_Job(msg.data());
  remus::JobStatus js(job.id(),INVALID_STATUS);
  if(this->QueuedJobs->haveUUID(job.id()))
    {
    js.Status = remus::QUEUED;
    }
  else if(this->ActiveJobs->haveUUID(job.id()))
    {
    js = this->ActiveJobs->status(job.id());
    }
  return remus::to_string(js);
}

//------------------------------------------------------------------------------
std::string Server::queueJob(const remus::common::JobMessage& msg)
{
  if(this->canMesh(msg))
  {
    //generate an UUID
    boost::uuids::uuid jobUUID = this->UUIDGenerator();
    //create a new job to place on the queue
    //This call will invalidate the msg as we are going to move the data
    //to another message to await sending to the worker
    this->QueuedJobs->addJob(jobUUID,msg);
    //return the UUID

    const remus::Job validJob(jobUUID,msg.meshType());
    return remus::to_string(validJob);
  }
  return remus::INVALID_MSG;
}

//------------------------------------------------------------------------------
std::string Server::retrieveMesh(const remus::common::JobMessage& msg)
{
  //go to the active jobs list and grab the mesh result if it exists
  remus::Job job = remus::to_Job(msg.data());

  remus::JobResult result(job.id());
  if(this->ActiveJobs->haveUUID(job.id()) && this->ActiveJobs->haveResult(job.id()))
    {
    result = this->ActiveJobs->result(job.id());
    }

  //for now we remove all references from this job being active
  this->ActiveJobs->remove(job.id());

  return remus::to_string(result);
}

//------------------------------------------------------------------------------
std::string Server::terminateJob(const remus::common::JobMessage& msg)
{

  remus::Job job = remus::to_Job(msg.data());

  bool removed = this->QueuedJobs->remove(job.id());
  if(!removed)
    {
    zmq::socketIdentity worker = this->ActiveJobs->workerAddress(job.id());
    removed = this->ActiveJobs->remove(job.id());

    //send an out of band message to the worker to kill itself
    //the trick is we are going to send a job to the worker which is constructed
    //to mean that it should die.
    if(removed && worker.size() > 0)
      {
      remus::Job terminateJob(job.id(),
                              remus::MESH_TYPE(),
                              remus::to_string(remus::SHUTDOWN));
      remus::common::JobResponse response(worker);
      response.setServiceType(remus::SHUTDOWN);
      response.setData(remus::to_string(terminateJob));
      response.send(this->WorkerQueries);
      }
    }

  remus::STATUS_TYPE status = (removed) ? remus::FAILED : remus::INVALID_STATUS;
  //std::cout << "terminate status is " << status << " " << remus::to_string(status) << std::endl;
   return remus::to_string(remus::JobStatus(job.id(),status));
}

//------------------------------------------------------------------------------
void Server::DetermineWorkerResponse(const zmq::socketIdentity &workerIdentity,
                                     const remus::common::JobMessage& msg)
{
  //we have a valid job, determine what to do with it
  switch(msg.serviceType())
    {
    case remus::CAN_MESH:
      this->WorkerPool->addWorker(workerIdentity,msg.meshType());
      break;
    case remus::MAKE_MESH:
      //the worker will block while it waits for a response.
      if(!this->WorkerPool->haveWorker(workerIdentity))
        {
        this->WorkerPool->addWorker(workerIdentity,msg.meshType());
        }
      this->WorkerPool->readyForWork(workerIdentity);
      break;
    case remus::MESH_STATUS:
      //store the mesh status msg,  no response needed
      this->storeMeshStatus(msg);
      break;
    case remus::RETRIEVE_MESH:
      //we need to store the mesh result, no response needed
      this->storeMesh(msg);
      break;
    default:
      break;
    }
}

//------------------------------------------------------------------------------
void Server::storeMeshStatus(const remus::common::JobMessage& msg)
{
  //the string in the data is actually a job status object
  remus::JobStatus js = remus::to_JobStatus(msg.data(),msg.dataSize());
  this->ActiveJobs->updateStatus(js);
}

//------------------------------------------------------------------------------
void Server::storeMesh(const remus::common::JobMessage& msg)
{
  remus::JobResult jr = remus::to_JobResult(msg.data(),msg.dataSize());
  this->ActiveJobs->updateResult(jr);
}

//------------------------------------------------------------------------------
void Server::assignJobToWorker(const zmq::socketIdentity &workerIdentity,
                               const remus::Job& job )
{
  this->ActiveJobs->add( workerIdentity, job.id() );

  remus::common::JobResponse response(workerIdentity);
  response.setServiceType(remus::MAKE_MESH);
  response.setData(remus::to_string(job));
  response.send(this->WorkerQueries);
}

//see if we have a worker in the pool for the next job in the queue,
//otherwise as the factory to generate a new worker to handle that job
//------------------------------------------------------------------------------
void Server::FindWorkerForQueuedJob()
{

  //We assume that a worker could possibly handle multiple jobs but all of the same type.
  //In order to prevent allocating more workers than needed we use a set instead of a vector.
  //This results in the server only creating one worker per job type.
  //This gives the new workers the opportunity of getting assigned multiple jobs.


  typedef std::set<remus::MESH_TYPE>::const_iterator it;
  this->WorkerFactory.updateWorkerCount();
  std::set<remus::MESH_TYPE> types;


  //find all the jobs that have been marked as waiting for a worker
  //and ask if we have a worker in the poll that can mesh that job
  types = this->QueuedJobs->waitingForWorkerTypes();
  for(it type = types.begin(); type != types.end(); ++type)
    {
    if(this->WorkerPool->haveWaitingWorker(*type))
      {
      //give this job to that worker
      this->assignJobToWorker(this->WorkerPool->takeWorker(*type),
                              this->QueuedJobs->takeJob(*type));
      }
    }


  //find all jobs that queued up and check if we can assign it to an item in
  //the worker pool
  types = this->QueuedJobs->queuedJobTypes();
  for(it type = types.begin(); type != types.end(); ++type)
    {
    if(this->WorkerPool->haveWaitingWorker(*type))
      {
      //give this job to that worker
      this->assignJobToWorker(this->WorkerPool->takeWorker(*type),
                              this->QueuedJobs->takeJob(*type));
      }
    }

  //now if we have room in our worker pool for more pending workers create some
  //make sure we ask the worker pool what its limit on number of pending
  //workers is before creating more. We have to requery to get the updated
  //job types since the worker pool might have taken some.
  types = this->QueuedJobs->queuedJobTypes();
  for(it type = types.begin(); type != types.end(); ++type)
    {
    //check if we have a waiting worker, if we don't than try
    //ask the factory to create a worker of that type.
    if(this->WorkerFactory.createWorker(*type))
      {
      this->QueuedJobs->workerDispatched(*type);
      }
    }
}

}
}