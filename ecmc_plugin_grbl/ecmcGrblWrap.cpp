/*************************************************************************\
* Copyright (c) 2019 European Spallation Source ERIC
* ecmc is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
*
*  ecmeGrblWrap.cpp
*
*  Created on: Jan 20, 2022
*      Author: anderssandstrom
*
\*************************************************************************/

// Needed to get headers in ecmc right...
#define ECMC_IS_PLUGIN

#include <vector>
#include <stdexcept>
#include <string>
#include "ecmcGrbl.h"
#include "ecmcGrblDefs.h"
#include "ecmcGrblWrap.h"
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
#define ECMC_PLUGIN_PORTNAME_PREFIX "PLUGIN.GRBL"

static ecmcGrbl*  grbl = NULL;
static char  portNameBuffer[ECMC_PLUGIN_MAX_PORTNAME_CHARS];

int createGrbl(char* configStr, int exeSampleTimeMs) {

  // create asynport name for new object ()
  memset(portNameBuffer, 0, ECMC_PLUGIN_MAX_PORTNAME_CHARS);
  snprintf (portNameBuffer, ECMC_PLUGIN_MAX_PORTNAME_CHARS,
            ECMC_PLUGIN_PORTNAME_PREFIX);
  try {
    grbl = new ecmcGrbl(configStr, portNameBuffer, exeSampleTimeMs);
  }
  catch(std::exception& e) {
    if(grbl) {
      delete grbl;
    }
    printf("Exception: %s. Plugin will unload.\n",e.what());
    return ECMC_PLUGIN_GRBL_GENERAL_ERROR_CODE;
  }
  
  return 0;
}

int enterRT() {
  if(grbl){
    return grbl->enterRT();
  }
  return 0;
}

int realtime(int ecmcError) {
  if(grbl){
    return grbl->grblRTexecute(ecmcError);
  }
  return 0;
}

int setExecute(int exe) {
  if(grbl){
    return grbl->setExecute(exe);
  }
  return 0;
}

int setHalt(int halt) {
  if(grbl){
    return grbl->setHalt(halt);
  }
  return 0;
}

int setResume(int resume) {
  if(grbl){
    return grbl->setResume(resume);
  }
  return 0;
}

int getBusy() {
  if(grbl){
    return grbl->getBusy();
  }
  return 0;
}

int getParserBusy() {
  if(grbl){
    return grbl->getParserBusy();
  }
  return 0;
}

int getCodeRowNum() {
  if(grbl){
    return grbl->getCodeRowNum();
  }
  return 0;
}

int setReset(int reset) {
  if(grbl){
    return grbl->setReset(reset);
  }
  return 0;
}

int getError() {
  if(grbl){
    return grbl->getError();
  }
  return 0;
}

int resetError() {
  if(grbl){
    grbl->resetError();
  }
  return 0;
}

int setAllAxesEnable(int enable) { 
  if(grbl){
    grbl->setAllAxesEnable(enable);
  }
  return 0;
}

int getAllAxesEnabled() { 
  if(grbl){
    return grbl->getAllAxesEnabled();
  }
  return 0;
}

void deleteGrbl() {
  if(grbl) {
    delete (grbl);
  }
}

/** 
 * EPICS iocsh shell command: ecmcGrblAddCommand
*/

void ecmcGrblAddCommandPrintHelp() {
  printf("\n");
  printf("       Use ecmcGrblAddCommand(<command>)\n");
  printf("          <command>                      : Grbl command.\n");
  printf("\n");
}

int ecmcGrblAddCommand(const char* command) {

  if(!command) {
    printf("Error: command.\n");
    ecmcGrblAddCommandPrintHelp();
    return asynError;
  }

  if(strcmp(command,"-h") == 0 || strcmp(command,"--help") == 0 ) {
    ecmcGrblAddCommandPrintHelp();
    return asynSuccess;
  }

  if(!grbl) {
    printf("Plugin not initialized/loaded.\n");
    return asynError;
  }

  try {
    grbl->addCommand(command);
  }
  catch(std::exception& e) {
    printf("Exception: %s. Add command failed.\n",e.what());
    return asynError;
  }
  
  return asynSuccess;
}

static const iocshArg initArg0_0 =
{ " Grbl Command", iocshArgString };

static const iocshArg *const initArgs_0[]  = { &initArg0_0};

static const iocshFuncDef    initFuncDef_0 = { "ecmcGrblAddCommand", 1, initArgs_0 };
static void initCallFunc_0(const iocshArgBuf *args) {
  ecmcGrblAddCommand(args[0].sval);
}

/** 
 * EPICS iocsh shell command: ecmcGrblLoadGCodeFile
*/

void ecmcGrblLoadFilePrintHelp() {
  printf("\n");
  printf("       Use ecmcGrblLoadGCodeFile(<filename>,<append>)\n");
  printf("          <filename>             : Filename containg g-code.\n");
  printf("          <append>               : 0: reset grbl, clear all current commands in buffer before \n"); 
  printf ("                                     loading file (default).\n");
  printf("                                 : 1: append commands in file last in buffer. (grbl is not reset)\n");
  printf("\n");
}

int ecmcGrblLoadFile(const char* filename, int append) {

  if(!filename) {
    printf("Error: filename.\n");
    ecmcGrblLoadFilePrintHelp();
    return asynError;
  }

  if(strcmp(filename,"-h") == 0 || strcmp(filename,"--help") == 0 ) {
    ecmcGrblLoadFilePrintHelp();
    return asynSuccess;
  }

  if(!grbl) {
    printf("Plugin not initialized/loaded.\n");
    return asynError;
  }

  try {
    grbl->loadGCodeFile(filename, append);
  }
  catch(std::exception& e) {
    printf("Exception: %s. Load file command failed.\n",e.what());
    return asynError;
  }
  
  return asynSuccess;
}

static const iocshArg initArg0_1 =
{ " Filename", iocshArgString };
static const iocshArg initArg1_1 =
{ " Append", iocshArgInt };

static const iocshArg *const initArgs_1[]  = { &initArg0_1,
                                               &initArg1_1};

static const iocshFuncDef    initFuncDef_1 = { "ecmcGrblLoadGCodeFile", 2, initArgs_1 };
static void initCallFunc_1(const iocshArgBuf *args) {
  ecmcGrblLoadFile(args[0].sval,args[1].ival);
}

/*
$11 - Junction deviation, mm
$12 – Arc tolerance, mm
$30 - Max spindle speed, RPM
$31 - Min spindle speed, RPM
$100, $101 and $102 – [X,Y,Z] steps/mm
$110, $111 and $112 – [X,Y,Z] Max rate, mm/min
$120, $121, $122 – [X,Y,Z] Acceleration, mm/sec^2*/

/** 
 * EPICS iocsh shell command: ecmcGrblAddConfig
*/

void ecmcGrblAddConfigPrintHelp() {
  printf("\n");
  printf("       Use ecmcGrblAddConfig(<command>)\n");
  printf("          <command>                      : Grbl command.\n");
  printf("\n");
  printf("       Supported grbl comamnds:\n");
  printf("          $11 - Junction deviation, mm\n");
  printf("          $12 – Arc tolerance, mm\n");
  printf("          $30 - Max spindle speed, RPM\n");
  printf("          $31 - Min spindle speed, RPM\n");
  printf("          $100, $101 and $102 – [X,Y,Z] steps/mm\n");
  printf("          $110, $111 and $112 – [X,Y,Z] Max rate, mm/min\n");
  printf("          $120, $121, $122 – [X,Y,Z] Acceleration, mm/sec^2\n");
  printf("\n");
  printf("       Example: Set resolution to 1 micrometer\n");
  printf("        ecmcGrblAddConfig(\"$100=1000\")  \n");
  printf("\n");
}

int ecmcGrblAddConfig(const char* command) {

  if(!command) {
    printf("Error: command.\n");
    ecmcGrblAddConfigPrintHelp();
    return asynError;
  }

  if(strcmp(command,"-h") == 0 || strcmp(command,"--help") == 0 ) {
    ecmcGrblAddConfigPrintHelp();
    return asynSuccess;
  }

  if(!grbl) {
    printf("Plugin not initialized/loaded.\n");
    return asynError;
  }

  try {
    grbl->addConfig(command);
  }
  catch(std::exception& e) {
    printf("Exception: %s. Add config failed.\n",e.what());
    return asynError;
  }
  
  return asynSuccess;
}

static const iocshArg initArg0_2 =
{ " Grbl Config", iocshArgString };

static const iocshArg *const initArgs_2[]  = { &initArg0_2};

static const iocshFuncDef    initFuncDef_2 = { "ecmcGrblAddConfig", 1, initArgs_2 };
static void initCallFunc_2(const iocshArgBuf *args) {
  ecmcGrblAddConfig(args[0].sval);
}

/** 
 * EPICS iocsh shell command: ecmcGrblLoadConfigFile
*/

void ecmcGrblLoadConfigFilePrintHelp() {
  printf("\n");
  printf("       Use ecmcGrblLoadConfigFile(<filename>,<append>)\n");
  printf("          <filename>             : Filename containg grbl configs.\n");
  printf("          <append>               : 0: clear all current configs in buffer before \n"); 
  printf ("                                     loading file (default).\n");
  printf("                                 : 1: append commands in file last in buffer. (grbl is not reset)\n");
  printf("\n");
}

int ecmcGrblLoadConfigFile(const char* filename, int append) {

  if(!filename) {
    printf("Error: filename.\n");
    ecmcGrblLoadConfigFilePrintHelp();
    return asynError;
  }

  if(strcmp(filename,"-h") == 0 || strcmp(filename,"--help") == 0 ) {
    ecmcGrblLoadConfigFilePrintHelp();
    return asynSuccess;
  }

  if(!grbl) {
    printf("Plugin not initialized/loaded.\n");
    return asynError;
  }

  try {
    grbl->loadConfigFile(filename,append);
  }
  catch(std::exception& e) {
    printf("Exception: %s. Load file command failed.\n",e.what());
    return asynError;
  }
  
  return asynSuccess;
}

static const iocshArg initArg0_3 =
{ " Filename", iocshArgString };
static const iocshArg initArg1_3 =
{ " Append", iocshArgInt };

static const iocshArg *const initArgs_3[]  = { &initArg0_3,
                                               &initArg1_3};

static const iocshFuncDef    initFuncDef_3 = { "ecmcGrblLoadConfigFile", 2, initArgs_3 };
static void initCallFunc_3(const iocshArgBuf *args) {
  ecmcGrblLoadConfigFile(args[0].sval,args[1].ival);
}

///** 
// * Register all functions
//*/
void ecmcGrblPluginDriverRegister(void) {
  iocshRegister(&initFuncDef_0,    initCallFunc_0);   // ecmcGrblAddCommand
  iocshRegister(&initFuncDef_1,    initCallFunc_1);   // ecmcGrblLoadFile
  iocshRegister(&initFuncDef_2,    initCallFunc_2);   // ecmcGrblAddConfig
  iocshRegister(&initFuncDef_3,    initCallFunc_3);   // ecmcGrblLoadConfigFile
}

epicsExportRegistrar(ecmcGrblPluginDriverRegister);
