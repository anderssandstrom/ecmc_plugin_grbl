/*************************************************************************\
* Copyright (c) 2019 European Spallation Source ERIC
* ecmc is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
*
*  ecmcGrbl.h
*
*  Created on: jan 12, 2022
*      Author: anderssandstrom
*
\*************************************************************************/
#ifndef ECMC_GRBL_H_
#define ECMC_GRBL_H_

#include <stdexcept>
#include "asynPortDriver.h"
#include "ecmcGrblDefs.h"

#include "inttypes.h"
#include <epicsMutex.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include <string.h>

class ecmcGrbl : public asynPortDriver {
 public:

  /** ecmc ecmcGrbl class
   * This object can throw: 
   *    - bad_alloc
   *    - invalid_argument
   *    - runtime_error
   *    - out_of_range
  */
  ecmcGrbl(char*  configStr,
           char*  portName,
           double exeSampelTimeMs);
  ~ecmcGrbl();

  void doReadWorker();
  void doMainWorker();
  void doWriteWorker();
  int  enterRT();
  int  grblRTexecute(int ecmcError);
  void addCommand(std::string command);

 private:
  void                  parseConfigStr(char *configStr);
  void                  autoEnableAtStart();
  bool                  getEcmcAxisEnabled(int axis_id);
  bool                  getAllConfiguredAxisEnabled();
  double                getEcmcAxisActPos(int axis);
  void                  syncPositionIfNotEnabled();
  static std::string    to_string(int value);
  int                   cfgDbgMode_;
  int                   cfgXAxisId_;
  int                   cfgYAxisId_;
  int                   cfgZAxisId_;
  int                   cfgSpindleAxisId_;
  int                   cfgAutoEnableAtStart_; 
  int                   destructs_;
  int                   connected_;
  int                   errorCode_;
  double                exeSampleTimeMs_;
  int                   grblInitDone_;
  std::vector<std::string> grblCommandBuffer_;
  int                   grblCommandBufferIndex_;
  epicsMutexId          grblCommandBufferMutex_;
  bool                  firstCommandWritten_;
  int                   autoEnableExecuted_;
  int                   grblExeCycles_;  
  double timeToNextExeMs_;
};

#endif  /* ECMC_GRBL_H_ */
