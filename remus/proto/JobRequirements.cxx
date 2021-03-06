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

#include <remus/proto/JobRequirements.h>

#include <remus/common/ConditionalStorage.h>
#include <remus/proto/conversionHelpers.h>

#include <boost/make_shared.hpp>

namespace remus{
namespace proto{

struct JobRequirements::InternalImpl
{
  template<typename T>
  explicit InternalImpl(const T& t)
  {
    remus::common::ConditionalStorage temp(t);
    this->Storage.swap(temp);
    this->Size = this->Storage.size();
    this->Data = this->Storage.data();
  }

  InternalImpl(const char* d, std::size_t s):
    Size(s),
    Data(d),
    Storage()
  {
  }

  std::size_t size() const { return Size; }
  const char* data() const { return Data; }

private:
  //store the size of the data being held
  std::size_t Size;

  //points to the zero copy or data in the conditional storage
  const char* Data;

  //Storage is an optional allocation that is used when we need to copy data
  remus::common::ConditionalStorage Storage;
};

//------------------------------------------------------------------------------
JobRequirements::JobRequirements():
  SourceType(),
  FormatType(),
  MeshType(),
  WorkerName(),
  Tag(),
  Implementation(boost::make_shared<InternalImpl>(
                 static_cast<char*>(NULL),std::size_t(0)))
{
}

//------------------------------------------------------------------------------
JobRequirements::JobRequirements(remus::common::ContentFormat::Type ftype,
                                 remus::common::MeshIOType mtype,
                                 const std::string& wname,
                                 const remus::common::FileHandle& reqs_file ):
  SourceType(remus::common::ContentSource::File),
  FormatType(ftype),
  MeshType(mtype),
  WorkerName(wname),
  Tag(),
  Implementation(boost::make_shared<InternalImpl>(reqs_file))
{
}


//------------------------------------------------------------------------------
JobRequirements::JobRequirements(remus::common::ContentFormat::Type ftype,
                                 remus::common::MeshIOType mtype,
                                 const std::string& wname,
                                 const std::string& reqs ):
  SourceType(remus::common::ContentSource::Memory),
  FormatType(ftype),
  MeshType(mtype),
  WorkerName(wname),
  Tag(),
  Implementation(boost::make_shared<InternalImpl>(reqs))
{
}

//------------------------------------------------------------------------------
JobRequirements::JobRequirements(remus::common::ContentFormat::Type ftype,
                                 remus::common::MeshIOType mtype,
                                 const std::string& wname,
                                 const char* reqs,
                                 std::size_t reqs_size ):
  SourceType(remus::common::ContentSource::Memory),
  FormatType(ftype),
  MeshType(mtype),
  WorkerName(wname),
  Tag(),
  Implementation(boost::make_shared<InternalImpl>(reqs,reqs_size))
{
}


//------------------------------------------------------------------------------
bool JobRequirements::hasRequirements() const
{
  return this->Implementation->size() > 0;
}

//------------------------------------------------------------------------------
std::size_t JobRequirements::requirementsSize() const
{
  return this->Implementation->size();
}

//------------------------------------------------------------------------------
const char* JobRequirements::requirements() const
{
  return this->Implementation->data();
}

//------------------------------------------------------------------------------
 bool JobRequirements::operator<(const JobRequirements& other) const
{
  //the sort order as follows.
  //first comes mesh input & output type, this allows us to group all
  //requirements for a given input & output type together
  //than comes source type, allowing all File sources to come before
  //all Memory sources
  //than comes format type, allowing all user defined to come before
  //XML, JSON and BSOn
  //than comes worker name
  //than comes tag
  if ( !(this->meshTypes() == other.meshTypes()) )
  { return (this->meshTypes() < other.meshTypes()); }

  else if (this->sourceType() != other.sourceType())
  { return (this->sourceType() < other.sourceType()); }

  else if (this->formatType() != other.formatType())
  { return (this->formatType() < other.formatType()); }

  else if (this->workerName() != other.workerName())
  { return (this->workerName() < other.workerName()); }

  else if (this->tag() != other.tag())
  { return (this->tag() < other.tag()); }

 return false; //both objects are equal
}

//------------------------------------------------------------------------------
 bool JobRequirements::operator==(const JobRequirements& other) const
 {
  return ((this->meshTypes() == other.meshTypes()) &&
          (this->sourceType() == other.sourceType()) &&
          (this->formatType() == other.formatType()) &&
          (this->workerName() == other.workerName()) &&
          (this->tag() == other.tag()));
 }

//------------------------------------------------------------------------------
void JobRequirements::serialize(std::ostream& buffer) const
{
  buffer << this->sourceType() << std::endl;
  buffer << this->formatType() << std::endl;
  buffer << this->meshTypes() << std::endl;

  buffer << this->workerName().size() << std::endl;
  remus::internal::writeString( buffer, this->workerName());

  buffer << this->tag().size() << std::endl;
  remus::internal::writeString( buffer, this->tag());

  buffer << this->requirementsSize() << std::endl;
  remus::internal::writeString( buffer,
                                this->requirements(),
                                this->requirementsSize() );
}

//------------------------------------------------------------------------------
JobRequirements::JobRequirements(std::istream& buffer)
{
  int stype=0, ftype=0, workerNameSize=0, tagSize=0, contentsSize=0;

  //read in the source and format types
  buffer >> stype;
  buffer >> ftype;
  buffer >> this->MeshType;

  this->SourceType = static_cast<remus::common::ContentSource::Type>(stype);
  this->FormatType = static_cast<remus::common::ContentFormat::Type>(ftype);

  buffer >> workerNameSize;
  this->WorkerName = remus::internal::extractString(buffer,workerNameSize);

  buffer >> tagSize;
  this->Tag = remus::internal::extractString(buffer,tagSize);

  //read in the contents, todo do this with less temp objects and copies
  buffer >> contentsSize;

  std::vector<char> contents(contentsSize);

  //enables us to use less copies for faster read of large data
  remus::internal::extractVector(buffer,contents);

  this->Implementation = boost::make_shared<InternalImpl>(contents);
}

//------------------------------------------------------------------------------
JobRequirementsSet::JobRequirementsSet():
Container()
{
}

//------------------------------------------------------------------------------
JobRequirementsSet::JobRequirementsSet(const ContainerType& container):
Container(container)
{
}

//------------------------------------------------------------------------------
void JobRequirementsSet::serialize(std::ostream& buffer) const
{
  buffer << this->Container.size() << std::endl;
  typedef JobRequirementsSet::ContainerType::const_iterator IteratorType;
  for(IteratorType i = this->Container.begin();
      i != this->Container.end(); ++i)
    {
    buffer << *i << std::endl;
    }
}

//------------------------------------------------------------------------------
JobRequirementsSet::JobRequirementsSet(std::istream& buffer)
{
  std::size_t csize = 0;
  buffer >> csize;
  for(std::size_t i = 0; i < csize; ++i)
    {
    JobRequirements reqs;
    buffer >> reqs;
    this->Container.insert(reqs);
    }
}


}
}