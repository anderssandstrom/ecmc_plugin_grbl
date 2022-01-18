/*************************************************************************\
* Copyright (c) 2019 European Spallation Source ERIC
* ecmc is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
*
*  ecmcFFTWrap.cpp
*
*  Created on: Mar 22, 2020
*      Author: anderssandstrom
*
\*************************************************************************/

// Needed to get headers in ecmc right...
#define ECMC_IS_PLUGIN

#include <vector>
#include <stdexcept>
#include <string>
#include "ecmcSocketCANWrap.h"
#include "ecmcSocketCAN.h"
#include "ecmcSocketCANDefs.h"
#include <epicsTypes.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsTimer.h>
#include <epicsMutex.h>
#include <epicsExport.h>
#include <epicsEvent.h>

#include <iocsh.h>


#define ECMC_PLUGIN_MAX_PORTNAME_CHARS 64
#define ECMC_PLUGIN_PORTNAME_PREFIX "PLUGIN.CAN"

static ecmcSocketCAN*  can = NULL;
static char            portNameBuffer[ECMC_PLUGIN_MAX_PORTNAME_CHARS];

int createSocketCAN(char* configStr, int exeSampleTimeMs) {

  // create new ecmcFFT object

  // create asynport name for new object ()
  memset(portNameBuffer, 0, ECMC_PLUGIN_MAX_PORTNAME_CHARS);
  snprintf (portNameBuffer, ECMC_PLUGIN_MAX_PORTNAME_CHARS,
            ECMC_PLUGIN_PORTNAME_PREFIX);
  try {
    can = new ecmcSocketCAN(configStr, portNameBuffer, exeSampleTimeMs);
  }
  catch(std::exception& e) {
    if(can) {
      delete can;
    }
    printf("Exception: %s. Plugin will unload.\n",e.what());
    return ECMC_PLUGIN_SOCKETCAN_ERROR_CODE;
  }
  
  return 0;
}

int connectSocketCAN() {
  if(can){
      try {       
        can->connectExternal();
    }
    catch(std::exception& e) {
      printf("Exception: %s.\n",e.what());
      return ECMC_PLUGIN_SOCKETCAN_ERROR_CODE;
    }
  }
  else {
    return ECMC_PLUGIN_SOCKETCAN_ERROR_CODE;
  }
  return 0;
}

int getSocketCANConnectd() {
  if(can){
    try {       
      return can->getConnected();      
    }
    catch(std::exception& e) {
      printf("Exception: %s.\n",e.what());
      return 0;
    }
  }
  return 0;
}

int getlastWritesError() {
  if(can){
    try {       
      return can->getlastWritesError();
    }
    catch(std::exception& e) {
      printf("Exception: %s.\n",e.what());
      return 1;
    }
  }
  return 1;
}

int execute() {
  if(can){
    can->execute();
  }
  return 0;
}

int addWriteSocketCAN( double canId,
                       double len,
                       double data0,
                       double data1,
                       double data2,
                       double data3,
                       double data4,
                       double data5,
                       double data6,
                       double data7) {
  if(can){
    try {       
      return can->addWriteCAN((uint32_t) canId,
                              (uint8_t) len,
                              (uint8_t) data0,
                              (uint8_t) data1,
                              (uint8_t) data2,
                              (uint8_t) data3,
                              (uint8_t) data4,
                              (uint8_t) data5,
                              (uint8_t) data6,
                              (uint8_t) data7);      
    }
    catch(std::exception& e) {
      printf("Exception: %s.\n",e.what());
      return ECMC_PLUGIN_SOCKETCAN_ERROR_CODE;
    }
  }
  return ECMC_PLUGIN_SOCKETCAN_ERROR_CODE;
}

void deleteSocketCAN() {
  if(can) {
    delete (can);
  }
}


/** 
 * EPICS iocsh shell command: ecmcCANOpenAddMaster
*/

void ecmcCANOpenAddMasterPrintHelp() {
  printf("\n");
  printf("       Use ecmcCANOpenAddMaster(<name>, <node id>,....)\n");
  printf("          <name>                         : Name of master device.\n");
  printf("          <node id>                      : CANOpen node id of master.\n");
  printf("          <LSS sample time ms>           : Sample time for LSS.\n");
  printf("          <Sync sample time ms>          : Sample time for SYNC.\n");
  printf("          <NMT Heartbeat sample time ms> : Sample time for NMT Heartbeat.\n");
  printf("\n");
}

int ecmcCANOpenAddMaster(const char* name,
                         int nodeId,
                         int lssSampleTimeMs,
                         int syncSampleTimeMs,
                         int heartSampleTimeMs) {

  if(!name) {
    printf("Error: name.\n");
    ecmcCANOpenAddMasterPrintHelp();
    return asynError;
  }

  if(strcmp(name,"-h") == 0 || strcmp(name,"--help") == 0 ) {
    ecmcCANOpenAddMasterPrintHelp();
    return asynSuccess;
  }

  if(!can) {
    printf("Plugin not initialized/loaded.\n");
    return asynError;
  }

  try {
    can->addMaster((uint32_t)nodeId,
                   name,
                   lssSampleTimeMs,
                   syncSampleTimeMs,
                   heartSampleTimeMs);
  }
  catch(std::exception& e) {
    printf("Exception: %s. Add master failed.\n",e.what());
    return asynError;
  }
  
  return asynSuccess;
}

static const iocshArg initArg0_0 =
{ "Name", iocshArgString };
static const iocshArg initArg1_0 =
{ "Node Id", iocshArgInt };
static const iocshArg initArg2_0 =
{ "LSS sample time ms", iocshArgInt };
static const iocshArg initArg3_0 =
{ "Sync sample time ms", iocshArgInt };
static const iocshArg initArg4_0 =
{ "NMT Heart sample time ms", iocshArgInt };

static const iocshArg *const initArgs_0[]  = { &initArg0_0, 
                                               &initArg1_0,
                                               &initArg2_0,
                                               &initArg3_0,
                                               &initArg4_0};

static const iocshFuncDef    initFuncDef_0 = { "ecmcCANOpenAddMaster", 5, initArgs_0 };
static void initCallFunc_0(const iocshArgBuf *args) {
  ecmcCANOpenAddMaster(args[0].sval,
                       args[1].ival,
                       args[2].ival,
                       args[3].ival,
                       args[4].ival);
}

/** 
 * EPICS iocsh shell command: ecmcCANOpenAddDevice
*/

void ecmcCANOpenAddDevicePrintHelp() {
  printf("\n");
  printf("       Use ecmcCANOpenAddDevice(<name>, <node id>)\n");
  printf("          <name>                     : Name of device.\n");
  printf("          <node id>                  : CANOpen node id of device.\n");
  printf("          <NMT Heartbeat timeout ms> : Timeout for NMT Heartbeat.\n");
  printf("\n");
}

int ecmcCANOpenAddDevice(const char* name, int nodeId,int heartTimeOutMs) {
  if(!name) {
    printf("Error: name.\n");
    ecmcCANOpenAddDevicePrintHelp();
    return asynError;
  }

  if(strcmp(name,"-h") == 0 || strcmp(name,"--help") == 0 ) {
    ecmcCANOpenAddDevicePrintHelp();
    return asynSuccess;
  }

  if(!can) {
    printf("Plugin not initialized/loaded.\n");
    return asynError;
  }

  if(heartTimeOutMs < 0) {
    printf("Invalid NMT heartbeat timeout.\n");
    return asynError;
  }

  try {
    can->addDevice((uint32_t)nodeId,name,heartTimeOutMs);
  }
  catch(std::exception& e) {
    printf("Exception: %s. Add device failed.\n",e.what());
    return asynError;
  }

  return asynSuccess;
}

static const iocshArg initArg0_1 =
{ "Name", iocshArgString };
static const iocshArg initArg1_1 =
{ "Node Id", iocshArgInt };
static const iocshArg initArg2_1 =
{ "NMT Heart timeout ms", iocshArgInt };

static const iocshArg *const initArgs_1[]  = { &initArg0_1, 
                                               &initArg1_1,
                                               &initArg2_1};

static const iocshFuncDef    initFuncDef_1 = { "ecmcCANOpenAddDevice", 3, initArgs_1 };
static void initCallFunc_1(const iocshArgBuf *args) {
  ecmcCANOpenAddDevice(args[0].sval, args[1].ival, args[2].ival);
}

/** 
 * EPICS iocsh shell command: ecmcCANOpenAddSDO
*/

void ecmcCANOpenAddSDOPrintHelp() {
  printf("\n");
  printf("  Use ecmcCANOpenAddSDO(<name>, <node id>,.....)\n");
  printf("          <name>            : Name of master device.\n");
  printf("          <node id>         : CANOpen node id of device/master.\n");
  printf("          <cob id tx>       : CANOpen cob id of Tx of slave SDO.\n");
  printf("          <cob id rx>       : CANOpen cob id of Rx of slave SDO.\n");
  printf("          <dir>             : Direction 1=write and 2=read.\n");
  printf("          <ODIndex>         : OD index of SDO.\n");
  printf("          <ODSubIndex>      : OD sub index of SDO.\n");
  printf("          <ODSize>          : OS Size.\n");
  printf("          <readSampleTimeMs>: Sample time for read in ms (write is always on demand).\n");
  printf("\n");
}

int ecmcCANOpenAddSDO(const char* name, 
                      int nodeId, 
                      int cobIdTx,
                      int cobIdRx, 
                      int dir,
                      int ODIndex, 
                      int ODSubIndex,
                      int ODSize,
                      int readSampleTimeMs) {
  if(!name) {
    printf("Error: name.\n");
    ecmcCANOpenAddSDOPrintHelp();
    return asynError;
  }

  if(strcmp(name,"-h") == 0 || strcmp(name,"--help") == 0 ) {
    ecmcCANOpenAddSDOPrintHelp();
    return asynSuccess;
  }

  if(cobIdRx < 0) {
    printf("Error: invalid cobIdRx.\n");
    ecmcCANOpenAddSDOPrintHelp();
    return asynError;
  }

  if(cobIdTx < 0) {
    printf("Error: invalid cobIdTx.\n");
    ecmcCANOpenAddSDOPrintHelp();
    return asynError;
  }

  if(dir > 2 || dir <= 0) {
    printf("Error: invalid dir.\n");
    ecmcCANOpenAddSDOPrintHelp();
    return asynError;
  }

  if(ODIndex < 0) {
    printf("Error: invalid ODIndex.\n");
    ecmcCANOpenAddSDOPrintHelp();
    return asynError;
  }

  if(ODSubIndex < 0) {
    printf("Error: invalid ODSubIndex.\n");
    ecmcCANOpenAddSDOPrintHelp();
    return asynError;
  }

  if(ODSize < 0) {
    printf("Error: invalid ODSize.\n");
    ecmcCANOpenAddSDOPrintHelp();
    return asynError;
  }

  if(readSampleTimeMs < 0) {
    printf("Error: invalid readSampleTimeMs.\n");
    ecmcCANOpenAddSDOPrintHelp();
    return asynError;
  }

  ecmc_can_direction tempDir = DIR_READ;
  if(dir == 1) {
    tempDir = DIR_WRITE;
  }

  try {
    can->addSDO((uint32_t)nodeId,
                 cobIdTx,
                 cobIdRx,
                 tempDir,
                 ODIndex,
                 ODSubIndex,
                 ODSize,
                 readSampleTimeMs,
                 name);


  }
  catch(std::exception& e) {
    printf("Exception: %s. Add PDO failed.\n",e.what());
    return asynError;
  }
  return asynSuccess;
}

static const iocshArg initArg0_2 =
{ "Name", iocshArgString };
static const iocshArg initArg1_2 =
{ "Node Id", iocshArgInt };
static const iocshArg initArg2_2 =
{ "COB id TX", iocshArgInt };
static const iocshArg initArg3_2 =
{ "COB id RX", iocshArgInt };
static const iocshArg initArg4_2 =
{ "Direction", iocshArgInt };
static const iocshArg initArg5_2 =
{ "OD Index", iocshArgInt };
static const iocshArg initArg6_2 =
{ "OD sub index", iocshArgInt };
static const iocshArg initArg7_2 =
{ "OD size", iocshArgInt };
static const iocshArg initArg8_2 =
{ "Read sample time ms", iocshArgInt };

static const iocshArg *const initArgs_2[]  = { &initArg0_2, 
                                               &initArg1_2,
                                               &initArg2_2,
                                               &initArg3_2,
                                               &initArg4_2,
                                               &initArg5_2,
                                               &initArg6_2,
                                               &initArg7_2,
                                               &initArg8_2};

static const iocshFuncDef    initFuncDef_2 = { "ecmcCANOpenAddSDO", 9, initArgs_2 };
static void initCallFunc_2(const iocshArgBuf *args) {
  ecmcCANOpenAddSDO(args[0].sval,
                    args[1].ival,
                    args[2].ival,
                    args[3].ival,
                    args[4].ival,
                    args[5].ival,
                    args[6].ival,
                    args[7].ival,
                    args[8].ival);
}

/** 
 * EPICS iocsh shell command: ecmcCANOpenAddPDO
*/
void ecmcCANOpenAddPDOPrintHelp() {
  printf("\n");
  printf("     Use \"ecmcCANOpenAddPDO(<name>, <node id>\n");
  printf("          <name>            : Name of master device.\n");
  printf("          <node id>         : CANOpen node id of device/master.\n");
  printf("          <cob id>          : CANOpen cob id of PDO.\n");
  printf("          <dir>             : Direction 1=write and 2=read.\n");
  printf("          <ODSize>          : Size of PDO (max 8 bytes).\n");
  printf("          <readTimeoutMs>   : Readtimeout in ms.\n");
  printf("          <writeCycleMs>    : Cycle time for write (if <= 0 then only write on change).\n");
  printf("\n");
}

int ecmcCANOpenAddPDO(const char* name, 
                      int nodeId, 
                      int cobId, 
                      int dir, 
                      int ODSize,
                      int readTimeoutMs,
                      int writeCycleMs) {
  if(!name) {
    printf("Error: name.\n");
    ecmcCANOpenAddPDOPrintHelp();
    return asynError;
  }

  if(strcmp(name,"-h") == 0 || strcmp(name,"--help") == 0 ) {
    ecmcCANOpenAddPDOPrintHelp();
    return asynSuccess;
  }

  if(dir > 2 || dir <= 0) {
    printf("Error: invalid dir.\n");
    ecmcCANOpenAddPDOPrintHelp();
    return asynError;
  }

  if(ODSize < 0) {
    printf("Error: invalid ODSize.\n");
    ecmcCANOpenAddPDOPrintHelp();
    return asynError;
  }

  if(readTimeoutMs < 0) {
    printf("Error: invalid readTimeoutMs.\n");
    ecmcCANOpenAddPDOPrintHelp();
    return asynError;
  }

  if(writeCycleMs < 0) {
    printf("Error: invalid writeCycleMs.\n");
    ecmcCANOpenAddPDOPrintHelp();
    return asynError;
  }

  ecmc_can_direction tempDir = DIR_READ;
  if(dir == 1) {
    tempDir = DIR_WRITE;
  }

  try {
    can->addPDO((uint32_t)nodeId,
                 cobId,
                 tempDir,
                 ODSize,
                 readTimeoutMs,
                 writeCycleMs,name);
  }
  catch(std::exception& e) {
    printf("Exception: %s. Add PDO failed.\n",e.what());
    return asynError;
  }
  return asynSuccess;
}

static const iocshArg initArg0_3 =
{ "Name", iocshArgString };
static const iocshArg initArg1_3 =
{ "Node Id", iocshArgInt };
static const iocshArg initArg2_3 =
{ "COB Id", iocshArgInt };
static const iocshArg initArg3_3 =
{ "Direction", iocshArgInt };
static const iocshArg initArg4_3 =
{ "ODSize", iocshArgInt };
static const iocshArg initArg5_3 =
{ "Read Timeout ms", iocshArgInt };
static const iocshArg initArg6_3 =
{ "Write cycle ms", iocshArgInt };

static const iocshArg *const initArgs_3[]  = { &initArg0_3,
                                               &initArg1_3,
                                               &initArg2_3,
                                               &initArg3_3,
                                               &initArg4_3,
                                               &initArg5_3,
                                               &initArg6_3
                                               };

static const iocshFuncDef    initFuncDef_3 = { "ecmcCANOpenAddPDO", 7, initArgs_3 };
static void initCallFunc_3(const iocshArgBuf *args) {
  ecmcCANOpenAddPDO(args[0].sval,
                    args[1].ival,
                    args[2].ival,
                    args[3].ival,
                    args[4].ival,
                    args[5].ival,
                    args[6].ival);
}

/** 
 * Register all functions
*/
void ecmcCANPluginDriverRegister(void) {
  iocshRegister(&initFuncDef_0,    initCallFunc_0);   // ecmcCANOpenAddMaster
  iocshRegister(&initFuncDef_1,    initCallFunc_1);   // ecmcCANOpenAddDevice
  iocshRegister(&initFuncDef_2,    initCallFunc_2);   // ecmcCANOpenAddSDO
  iocshRegister(&initFuncDef_3,    initCallFunc_3);   // ecmcCANOpenAddPDO
}

epicsExportRegistrar(ecmcCANPluginDriverRegister);
