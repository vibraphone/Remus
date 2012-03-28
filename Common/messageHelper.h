/*=========================================================================
  
  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.
  
=========================================================================*/

#ifndef __messageHelper_h
#define __messageHelper_h


#include <boost/uuid/uuid.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp> //needed to get to_string
#include <string>

#include "jobMessage.h"

//The purpose of this header is to limit the number of files that
//need to include boost, plus give better names to the conversion from and too
//boost::uuid

namespace meshserver
{

//------------------------------------------------------------------------------
inline std::string UUIDToString(const boost::uuids::uuid& id)
{
  //call the boost to_string method in uuid_io
  return to_string(id);
}

//------------------------------------------------------------------------------
inline boost::uuids::uuid JobMessageToUUID(const meshserver::JobMessage& msg)
{
  //take the contents of the msg and convert it to an uuid
  //no type checking will be done to make sure this is valid for now
  boost::uuids::string_generator gen;
  const std::string sId(msg.data(),16);
  return gen(sId);
}

}

#endif // __messageHelper_h