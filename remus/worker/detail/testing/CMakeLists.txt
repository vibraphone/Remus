#=============================================================================
#
#  Copyright (c) Kitware, Inc.
#  All rights reserved.
#  See LICENSE.txt for details.
#
#  This software is distributed WITHOUT ANY WARRANTY; without even
#  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
#  PURPOSE.  See the above copyright notice for more information.
#
#=============================================================================

#MessageRouter and JobQueue aren't exported classes, and don't
#have any symbols, so we need to compile them into our unit test executable
set(srcs
  ../MessageRouter.cxx
  ../JobQueue.cxx
  )

set(unit_tests
  UnitTestMessageRouter.cxx
  UnitTestWorkerJobQueue.cxx
  )

remus_unit_tests(SOURCES ${unit_tests}
                 EXTRA_SOURCES ${srcs}
                 LIBRARIES RemusWorker
                           ${ZeroMQ_LIBRARIES}
                           ${Boost_LIBRARIES}
                           ${CMAKE_THREAD_LIBS_INIT})

#both of these tests need to be serially since they bind to ports/channels
set_tests_properties(UnitTestMessageRouter
                     UnitTestWorkerJobQueue
                     PROPERTIES RUN_SERIAL TRUE)

#both of these tests need to be run after UnitTestWorker, because UnitTestWorker
#dummy server can't currently handle binding to the non default tcp/ip port,
#and on some OS if these tests are run first, the OS won't release the port
#for a couple of seconds after the test has finished.
set_tests_properties(UnitTestMessageRouter
                     UnitTestWorkerJobQueue
                     PROPERTIES DEPENDS UnitTestWorker)
