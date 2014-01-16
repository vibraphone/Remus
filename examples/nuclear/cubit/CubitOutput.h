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

#ifndef _CubitOutput_h
#define _CubitOutput_h

#include <string>

#include <remus/client/JobResult.h>

class CubitOutput
{
public:
  CubitOutput(bool valid = false)
    :Valid(false)
  {}

  CubitOutput(const remus::client::JobResult& job, bool valid=true)
    : Prefix(job.Data), Valid(valid)
  {}

  CubitOutput(std::string& prefix, bool valid=true)
    : Prefix(prefix), Valid(valid)
  {}

  std::string const& getPrefix() const
  {
    return Prefix;
  }

  bool isValid() const
  { return Valid; }

  void setPrefix(std::string p, bool valid = true)
  { Prefix = p; Valid = valid; }

  operator std::string(void) const
  { return Prefix; }
private:
  std::string Prefix;
  bool Valid;
};

#endif
