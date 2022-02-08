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

typedef struct {
  bool        limitBwd;
  bool        limitFwd;
  bool        enabled;
  int         error;
  double      acceleration; // only spindle
  double      actpos;
  int         axisId;
  int         trajSource;
} ecmcAxisStatusData;

typedef struct {
  ecmcAxisStatusData  xAxis;
  ecmcAxisStatusData  yAxis;
  ecmcAxisStatusData  zAxis;
  ecmcAxisStatusData  spindleAxis;
  int error;
  int errorOld;
  bool allEnabled;
  bool allLimitsOK;
  bool allLimitsOKOld;
} ecmcStatusData;

enum grblReplyType {
  ECMC_GRBL_REPLY_START = 0,
  ECMC_GRBL_REPLY_OK = 1,
  ECMC_GRBL_REPLY_ERROR = 2,
  ECMC_GRBL_REPLY_NON_PROTOCOL = 3
};

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

  void                     doMainWorker();     // Simulated grbl main.c
  void                     doWriteWorker();    // Simulated grbl client
  void                     addCommand(std::string command);
  void                     addConfig(std::string command);
  void                     loadGCodeFile(std::string filename, int append);
  void                     loadConfigFile(std::string fileName, int append);
  int                      enterRT();
  int                      grblRTexecute(int ecmcError);              //ecmc rt thread (main)
  int                      setExecute(int exe);
  int                      setHalt(int halt);
  int                      setResume(int resume);
  int                      setReset(int reset);
  int                      getBusy();
  int                      getParserBusy();
  int                      getCodeRowNum();
  int                      setAllAxesEnable(int enable);
  int                      getError();
  void                     resetError();
  int                      getAllAxesEnabled();

 private:
  void                     parseConfigStr(char *configStr);           // constructor (iocsh thread)
  void                     readEcmcStatus(int ecmcError);             // ecmc rt thread
  void                     preExeAxes();                              // ecmc rt thread
  void                     postExeAxes();                             // ecmc rt thread
  void                     preExeAxis(ecmcAxisStatusData ecmcAxisData, int grblAxisId); //ecmc rt thread
  void                     postExeAxis(ecmcAxisStatusData ecmcAxisData, int grblAxisId); //ecmc rt thread
  void                     giveControlToEcmcIfNeeded();                //ecmc rt thread
  void                     syncAxisPosition(ecmcAxisStatusData ecmcAxisData, int grblAxisId); //ecmc rt thread
  bool                     getEcmcAxisEnabled(int ecmcAxisId);        //ecmc rt thread
  double                   getEcmcAxisActPos(int axis);               //ecmc rt thread
  int                      getEcmcAxisTrajSource(int ecmcAxisId);     //ecmc rt thread
  bool                     getEcmcAxisLimitBwd(int ecmcAxisId);       //ecmc rt thread
  bool                     getEcmcAxisLimitFwd(int ecmcAxisId);       //ecmc rt thread
  grblReplyType            grblReadReply();                           // doWriteWorker thread
  void                     grblWriteCommand(std::string command);     // doWriteWorker thread
  bool                     applyConfigsSuccess();                     // doWriteWorker thread
  bool                     WriteGCodeSuccess();                       // doWriteWorker thread
  bool                     autoEnableAxesSuccess();                   // doWriteWorker thread

  int                      cfgDbgMode_;
  int                      cfgXAxisId_;
  int                      cfgYAxisId_;
  int                      cfgZAxisId_;
  int                      cfgSpindleAxisId_;
  int                      cfgAutoEnable_;
  int                      cfgAutoStart_;
  int                      destructs_;
  int                      executeCmd_;
  int                      resetCmd_;
  int                      haltCmd_;
  int                      resumeCmd_;
  int                      errorCode_;
  int                      errorCodeOld_;
  double                   exeSampleTimeMs_;
  int                      grblInitDone_;
  std::vector<std::string> grblConfigBuffer_;
  epicsMutexId             grblConfigBufferMutex_;
  std::vector<std::string> grblCommandBuffer_;
  unsigned int             grblCommandBufferIndex_;
  epicsMutexId             grblCommandBufferMutex_;
  bool                     autoStartDone_;
  int                      grblExeCycles_;  
  double                   timeToNextExeMs_;
  bool                     writerBusy_;
  double                   spindleAcceleration_;
  int                      cfgAutoEnableTimeOutSecs_;
  int                      unrecoverableError_;
  ecmcStatusData           ecmcData_;

};

#endif  /* ECMC_GRBL_H_ */
