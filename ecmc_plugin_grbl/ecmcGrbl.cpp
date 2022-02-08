/*************************************************************************\
* Copyright (c) 2019 European Spallation Source ERIC
* ecmc is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
*
*  ecmcGrbl.cpp
*
*  Created on: Mar 22, 2020
*      Author: anderssandstrom
*      Credits to  https://github.com/sgreg/dynamic-loading 
*
\*************************************************************************/

// Needed to get headers in ecmc right...
#define ECMC_IS_PLUGIN

#include <sstream>
#include "ecmcGrbl.h"
#include "ecmcPluginClient.h"
#include "ecmcAsynPortDriver.h"
#include "ecmcAsynPortDriverUtils.h"
#include "epicsThread.h"
#include "ecmcMotion.h"
#include <iostream>
#include <fstream>

extern "C" {
#include "grbl.h"
}

// Global vars
int enableDebugPrintouts = 0;
int stepperInterruptEnable = 0;

system_t sys;
int32_t sys_position[N_AXIS];         // Real-time machine (aka home) position vector in steps.
int32_t sys_probe_position[N_AXIS];   // Last probe position in machine coordinates and steps.
volatile uint8_t sys_probe_state;     // Probing state value.  Used to coordinate the probing cycle with stepper ISR.
volatile uint8_t sys_rt_exec_state;   // Global realtime executor bitflag variable for state management. See EXEC bitmasks.
volatile uint8_t sys_rt_exec_alarm;   // Global realtime executor bitflag variable for setting various alarms.
volatile uint8_t sys_rt_exec_motion_override; // Global realtime executor bitflag variable for motion-based overrides.
volatile uint8_t sys_rt_exec_accessory_override; // Global realtime executor bitflag variable for spindle/coolant overrides.

#ifdef DEBUG
  volatile uint8_t sys_rt_exec_debug;
#endif

// Thread that writes commands to grbl
void f_worker_write(void *obj) {
  if(!obj) {
    printf("%s/%s:%d: GRBL: ERROR: Worker read thread ecmcGrbl object NULL..\n",
            __FILE__, __FUNCTION__, __LINE__);
    return;
  }
  ecmcGrbl * grblObj = (ecmcGrbl*)obj;
  grblObj->doWriteWorker();
}

// Start worker for socket connect()
void f_worker_main(void *obj) {
  if(!obj) {
    printf("%s/%s:%d: GRBL: ERROR: Worker main thread ecmcGrbl object NULL..\n",
            __FILE__, __FUNCTION__, __LINE__);
    return;
  }
  ecmcGrbl * grblObj = (ecmcGrbl*)obj;
  grblObj->doMainWorker();
}

/** ecmc ecmcGrbl class
 * This object can throw: 
*/
ecmcGrbl::ecmcGrbl(char* configStr,
                   char* portName,
                   double exeSampleTimeMs)
         : asynPortDriver(portName,
                   1, /* maxAddr */
                   asynInt32Mask | asynFloat64Mask | asynFloat32ArrayMask |
                   asynFloat64ArrayMask | asynEnumMask | asynDrvUserMask |
                   asynOctetMask | asynInt8ArrayMask | asynInt16ArrayMask |
                   asynInt32ArrayMask | asynUInt32DigitalMask, /* Interface mask */
                   asynInt32Mask | asynFloat64Mask | asynFloat32ArrayMask |
                   asynFloat64ArrayMask | asynEnumMask | asynDrvUserMask |
                   asynOctetMask | asynInt8ArrayMask | asynInt16ArrayMask |
                   asynInt32ArrayMask | asynUInt32DigitalMask, /* Interrupt mask */
                   ASYN_CANBLOCK , /*NOT ASYN_MULTI_DEVICE*/
                   1, /* Autoconnect */
                   0, /* Default priority */
                   0) /* Default stack size */

                   {

  // Init  
  cfgDbgMode_           = 0;
  cfgAutoStart_         = 0;
  destructs_            = 0;
  executeCmd_           = 0;
  resetCmd_             = 0;
  haltCmd_              = 0;
  resumeCmd_            = 0;
  errorCode_            = 0;
  errorCodeOld_         = 0;
  exeSampleTimeMs_      = exeSampleTimeMs;
  cfgXAxisId_           = -1;
  cfgYAxisId_           = -1;
  cfgZAxisId_           = -1;
  cfgSpindleAxisId_     = -1;
  cfgAutoEnable_        = 0;
  grblInitDone_         = 0;
  autoStartDone_        = 0;
  timeToNextExeMs_      = 0;
  writerBusy_           = 0;
  spindleAcceleration_  = 0;
  unrecoverableError_   = 0;
  cfgAutoEnableTimeOutSecs_ = ECMC_PLUGIN_AUTO_ENABLE_TIME_OUT_SEC;
  autoEnableTimeOutCounter_ = 0;
  grblCommandBufferIndex_ = 0;
  grblCommandBuffer_.clear();
  grblConfigBuffer_.clear();
  memset(&ecmcData_,0,sizeof(ecmcStatusData));

  if(!(grblConfigBufferMutex_ = epicsMutexCreate())) {
    throw std::runtime_error("GRBL: ERROR: Failed create mutex config buffer.");
  }
  
  if(!(grblCommandBufferMutex_ = epicsMutexCreate())) {
    throw std::runtime_error("GRBL: ERROR: Failed create mutex for command buffer.");
  }
  
  
  parseConfigStr(configStr); // Assigns all configs
  
  
  ecmcData_.xAxis.axisId       = cfgXAxisId_;
  ecmcData_.yAxis.axisId       = cfgYAxisId_;
  ecmcData_.zAxis.axisId       = cfgZAxisId_;
  ecmcData_.spindleAxis.axisId = cfgSpindleAxisId_;

  // global varaible in grbl  
  enableDebugPrintouts = cfgDbgMode_;

  //Check atleast one valid axis
  if(cfgXAxisId_<0 && cfgXAxisId_<0 && cfgXAxisId_<0 && cfgSpindleAxisId_<0) {
    throw std::out_of_range("GRBL: ERROR: No valid axis choosen.");
  }

    // Create worker thread for main grbl loop
  std::string threadname = "ecmc.grbl.main";
  if(epicsThreadCreate(threadname.c_str(), 0, 32768, f_worker_main, this) == NULL) {
    throw std::runtime_error("GRBL: ERROR: Failed create worker thread for main().");
  }

  // Create worker thread for write socket
  threadname = "ecmc.grbl.write";
  if(epicsThreadCreate(threadname.c_str(), 0, 32768, f_worker_write, this) == NULL) {
    throw std::runtime_error("GRBL: ERROR: Failed create worker thread for write().");
  }

  // wait for grblInitDone_!
  if(cfgDbgMode_) {
    printf("GRBL: INFO: Waiting for grbl init..");
  }

  while(!grblInitDone_) {      
    delay_ms(100);
    if(cfgDbgMode_) {
      printf(".");
    }
  }
  delay_ms(100);  
}

ecmcGrbl::~ecmcGrbl() {
  // kill worker
  destructs_ = 1;  // maybe need todo in other way..
}

void ecmcGrbl::parseConfigStr(char *configStr) {

  // check config parameters
  if (configStr && configStr[0]) {    
    char *pOptions = strdup(configStr);
    char *pThisOption = pOptions;
    char *pNextOption = pOptions;
    
    while (pNextOption && pNextOption[0]) {
      pNextOption = strchr(pNextOption, ';');
      if (pNextOption) {
        *pNextOption = '\0'; /* Terminate */
        pNextOption++;       /* Jump to (possible) next */
      }
      
      // ECMC_PLUGIN_DBG_PRINT_OPTION_CMD (1/0)
      if (!strncmp(pThisOption, ECMC_PLUGIN_DBG_PRINT_OPTION_CMD, strlen(ECMC_PLUGIN_DBG_PRINT_OPTION_CMD))) {
        pThisOption += strlen(ECMC_PLUGIN_DBG_PRINT_OPTION_CMD);
        cfgDbgMode_ = atoi(pThisOption);
      }
      
      // ECMC_PLUGIN_X_AXIS_ID_OPTION_CMD (1..ECMC_MAX_AXES)
      if (!strncmp(pThisOption, ECMC_PLUGIN_X_AXIS_ID_OPTION_CMD, strlen(ECMC_PLUGIN_X_AXIS_ID_OPTION_CMD))) {
        pThisOption += strlen(ECMC_PLUGIN_X_AXIS_ID_OPTION_CMD);
        cfgXAxisId_ = atoi(pThisOption);
      }

      // ECMC_PLUGIN_Y_AXIS_ID_OPTION_CMD (1..ECMC_MAX_AXES)
      if (!strncmp(pThisOption, ECMC_PLUGIN_Y_AXIS_ID_OPTION_CMD, strlen(ECMC_PLUGIN_Y_AXIS_ID_OPTION_CMD))) {
        pThisOption += strlen(ECMC_PLUGIN_Y_AXIS_ID_OPTION_CMD);
        cfgYAxisId_ = atoi(pThisOption);
      }

      // ECMC_PLUGIN_Z_AXIS_ID_OPTION_CMD (1..ECMC_MAX_AXES)
      if (!strncmp(pThisOption, ECMC_PLUGIN_Z_AXIS_ID_OPTION_CMD, strlen(ECMC_PLUGIN_Z_AXIS_ID_OPTION_CMD))) {
        pThisOption += strlen(ECMC_PLUGIN_Z_AXIS_ID_OPTION_CMD);
        cfgZAxisId_ = atoi(pThisOption);
      }

      // ECMC_PLUGIN_SPINDLE_AXIS_ID_OPTION_CMD (1..ECMC_MAX_AXES)
      if (!strncmp(pThisOption, ECMC_PLUGIN_SPINDLE_AXIS_ID_OPTION_CMD, strlen(ECMC_PLUGIN_SPINDLE_AXIS_ID_OPTION_CMD))) {
        pThisOption += strlen(ECMC_PLUGIN_SPINDLE_AXIS_ID_OPTION_CMD);
        cfgSpindleAxisId_ = atoi(pThisOption);
      }

      // ECMC_PLUGIN_AUTO_ENABLE_AT_START_OPTION_CMD
      if (!strncmp(pThisOption, ECMC_PLUGIN_AUTO_ENABLE_AT_START_OPTION_CMD, strlen(ECMC_PLUGIN_AUTO_ENABLE_AT_START_OPTION_CMD))) {
        pThisOption += strlen(ECMC_PLUGIN_AUTO_ENABLE_AT_START_OPTION_CMD);
        cfgAutoEnable_ = atoi(pThisOption);
      }

      // ECMC_PLUGIN_AUTO_START_OPTION_CMD
      if (!strncmp(pThisOption, ECMC_PLUGIN_AUTO_START_OPTION_CMD, strlen(ECMC_PLUGIN_AUTO_START_OPTION_CMD))) {
        pThisOption += strlen(ECMC_PLUGIN_AUTO_START_OPTION_CMD);
        cfgAutoStart_ = atoi(pThisOption);
      }

      pThisOption = pNextOption;
    }    
    free(pOptions);
  }
}

// Write socket worker
void ecmcGrbl::doWriteWorker() {
  // simulate serial connection here (need mutex)
  std::string reply = "";
  if(cfgDbgMode_){
    printf("%s:%s:%d\n",__FILE__,__FUNCTION__,__LINE__);
  }
  
  for(;;) {
    // Wait for grbl startup string before send comamnds"['$' for help]" 
    // basically flush buffer
  
    if(cfgDbgMode_){
      printf("GRBL: INFO: Wait for startup\n");
    }

    while (grblReadReply() != ECMC_GRBL_REPLY_START) {
        delay_ms(2);
    }
  
    if(cfgDbgMode_){
      printf("GRBL: INFO: Ready for commands\n");
    }
  
    if(cfgDbgMode_){
      printf("GRBL: INFO: Wait for IOC state RUN \n");
    }
    // wait for epics state
    while(getEcmcEpicsIOCState()!=16) {
      delay_ms(2);
    }
    delay_ms(2);
  
    if(cfgDbgMode_){
      printf("GRBL: INFO: Configuration start\n");
    }

    // Apply configs
    int index = 0;
    while(index < grblConfigBuffer_.size()) {
      epicsMutexLock(grblConfigBufferMutex_);
      std::string commandRaw = grblConfigBuffer_[index];
      epicsMutexUnlock(grblConfigBufferMutex_);
      std::string command = commandRaw.substr(0, commandRaw.find(ECMC_CONFIG_FILE_COMMENT_CHAR));

      if(command.length() == 0) {
        continue;
      }

      //Write command (will block untill written)
      grblWriteCommand(command);
      
      // will block untill answer
      grblReplyType replyStat = grblReadReply();

      if(replyStat != ECMC_GRBL_REPLY_OK) {
        errorCode_ = ECMC_PLUGIN_CONFIG_ERROR_CODE;
        printf("GRBL: ERROR: Plugin suspended due to configuration failed on command %s\n",command.c_str());
        printf("GRBL: ERROR: Restart of IOC needed.\n");
        unrecoverableError_ = 1;
        setExecute(0);
        return;  //kill thread
      }

      index++;
    }
    
    if(cfgDbgMode_){
      printf("GRBL: INFO: Configuration ready\n");
    }

    if(cfgDbgMode_){
      printf("GRBL: INFO: Start load g-code\n");
    }

    // GRBL ready, now we can send g-code comamnds
    for(;;) {
      if( (grblCommandBuffer_.size() > grblCommandBufferIndex_) &&         
          executeCmd_ && ecmcData_.allEnabled && grblInitDone_) {
        epicsMutexLock(grblCommandBufferMutex_);
        std::string commandRaw = grblCommandBuffer_[grblCommandBufferIndex_];
        epicsMutexUnlock(grblCommandBufferMutex_);
        std::string command = commandRaw.substr(0, commandRaw.find(ECMC_CONFIG_FILE_COMMENT_CHAR));
        if(command.length() == 0) {
          continue;
        }
  
        //Write command (will block untill written)
        grblWriteCommand(command);
        
        // will block untill answer
        grblReplyType replyStat = grblReadReply();
  
        if(replyStat != ECMC_GRBL_REPLY_OK) {
          errorCode_ = ECMC_PLUGIN_GRBL_COMMAND_ERROR_CODE;
          // stop motion
          setExecute(0);
          setReset(0);
          setReset(1);
          setReset(0);
          grblCommandBufferIndex_ = 0;
          break;  // for loop
        }
        
        grblCommandBufferIndex_++;
      }
      else {
        if( (( grblCommandBufferIndex_ >= grblCommandBuffer_.size()) || !executeCmd_ ) &&
              grblInitDone_) {
          writerBusy_ = 0;
        }
  
        // Wait for right condition to start
        delay_ms(5);
      }
    }    
  }
}

void ecmcGrbl::grblWriteCommand(std::string command) {

  // wait for grbl            
  while(serial_get_rx_buffer_available() <= strlen(command.c_str())+1) {
    delay_ms(1);
  }
  if(cfgDbgMode_){
    printf("GRBL: INFO: Write command (command[%d] = %s)\n",
           grblCommandBufferIndex_,
           command.c_str());
  }
  
  ecmc_write_command_serial(strdup(command.c_str()));      
}

 grblReplyType ecmcGrbl::grblReadReply() {
  std::string reply = "";

  // Wait for reply!
  for(;;) {
    while(serial_get_tx_buffer_count()==0) {
      delay_ms(1);      
    }
    char c = ecmc_get_char_from_grbl_tx_buffer();
    reply += c;
    if(c == '\n'&& reply.length() > 1) {
      if(reply.find(ECMC_PLUGIN_GRBL_GRBL_OK_STRING) != std::string::npos) {            
        if(cfgDbgMode_){
          printf("GRBL: INFO: Reply OK\n");
        }
        return ECMC_GRBL_REPLY_OK;        
      } else if(reply.find(ECMC_PLUGIN_GRBL_GRBL_ERR_STRING) != std::string::npos) {
        if(cfgDbgMode_){
          printf("GRBL: ERROR: Reply ERROR\n");
        }
        return ECMC_GRBL_REPLY_ERROR;
      } else if(reply.find(ECMC_PLUGIN_GRBL_GRBL_STARTUP_STRING) != std::string::npos ) {
        if(cfgDbgMode_){
          printf("GRBL: INFO: Ready for commands: %s\n",reply.c_str());
        }
        return ECMC_GRBL_REPLY_START;
      } else {
        // keep waiting (no break)            
        if(cfgDbgMode_){
          printf("GRBL: INFO: Reply non protocol related: %s\n",reply.c_str());
        }
        return ECMC_GRBL_REPLY_NON_PROTOCOL;
      }
    }
  }
  return ECMC_GRBL_REPLY_NON_PROTOCOL;
}

// Main grbl worker (copied from grbl main.c)
void ecmcGrbl::doMainWorker() {
  if(cfgDbgMode_){
    printf("%s:%s:%d\n",__FILE__,__FUNCTION__,__LINE__);
  }
  
  // Initialize system upon power-up.
  serial_init();             // Setup serial baud rate and interrupts
  ecmc_init_file();          // create and clear file (simulated eeprom)
  settings_restore(0b1111);  // restore all to defaults
  settings_init();           // Load Grbl settings from EEPROM
  stepper_init();            // Configure stepper pins and interrupt timers
  system_init();             // Configure pinout pins and pin-change interrupt
  memset(sys_position,0,sizeof(sys_position)); // Clear machine position.
  //sei(); // Enable interrupts
  // Initialize system state.
  #ifdef FORCE_INITIALIZATION_ALARM
    // Force Grbl into an ALARM state upon a power-cycle or hard reset.
    sys.state = STATE_ALARM;
  #else
    sys.state = STATE_IDLE;
  #endif
  
  // Check for power-up and set system alarm if homing is enabled to force homing cycle
  // by setting Grbl's alarm state. Alarm locks out all g-code commands, including the
  // startup scripts, but allows access to settings and internal commands. Only a homing
  // cycle '$H' or kill alarm locks '$X' will disable the alarm.
  // NOTE: The startup script will run after successful completion of the homing cycle, but
  // not after disabling the alarm locks. Prevents motion startup blocks from crashing into
  // things uncontrollably. Very bad.
  #ifdef HOMING_INIT_LOCK
    if (bit_istrue(settings.flags,BITFLAG_HOMING_ENABLE)) { sys.state = STATE_ALARM; }
  #endif
   // Grbl initialization loop upon power-up or a system abort. For the latter, all processes
  // will return to this loop to be cleanly re-initialized.
  for(;;) {
    if(destructs_) {
      return;
    }
    // Reset system variables.
    uint8_t prior_state = sys.state;
    memset(&sys, 0, sizeof(system_t)); // Clear system struct variable.
    sys.state = prior_state;
    sys.f_override = DEFAULT_FEED_OVERRIDE;  // Set to 100%
    sys.r_override = DEFAULT_RAPID_OVERRIDE; // Set to 100%
    sys.spindle_speed_ovr = DEFAULT_SPINDLE_SPEED_OVERRIDE; // Set to 100%
		memset(sys_probe_position,0,sizeof(sys_probe_position)); // Clear probe position.
    sys_probe_state = 0;
    sys_rt_exec_state = 0;
    sys_rt_exec_alarm = 0;
    sys_rt_exec_motion_override = 0;
    sys_rt_exec_accessory_override = 0;

    // Reset Grbl primary systems.
    serial_reset_read_buffer(); // Clear serial read buffer
    gc_init(); // Set g-code parser to default state
    spindle_init();
    coolant_init();    
    limits_init(); //Why is this function not working...
    probe_init();
    plan_reset(); // Clear block buffer and planner variables
    st_reset(); // Clear stepper subsystem variables.

    // Sync cleared gcode and planner positions to current system position.
    plan_sync_position();
    gc_sync_position();

    // Print welcome message. Indicates an initialization has occured at power-up or with a reset.
    report_init_message();

    // ready for commands through serial interface
    grblInitDone_ = 1;
    protocol_main_loop();
    if(destructs_) {
      return;
    }
    if(cfgDbgMode_){
      printf("GRBL: INFO: Resetting (after protocol_main_loop())...\n");
    }
    delay_ms(1);
  }
}

int ecmcGrbl::setAllAxesEnable(int enable) {

  if(ecmcData_.xAxis.axisId >= 0) {
     setAxisEnable(ecmcData_.xAxis.axisId, enable);
  }
  if(ecmcData_.yAxis.axisId >= 0 ) {
    setAxisEnable(ecmcData_.yAxis.axisId, enable);
  }
  if(ecmcData_.zAxis.axisId >= 0 ) {
    setAxisEnable(ecmcData_.zAxis.axisId, enable);
  }
  if(ecmcData_.spindleAxis.axisId >= 0) {
    setAxisEnable(ecmcData_.spindleAxis.axisId, enable);
  }
  return 0;
}

int ecmcGrbl::getAllAxesEnabled() {
  return ecmcData_.allEnabled;
}  

void  ecmcGrbl::autoEnableAxis(ecmcAxisStatusData ecmcAxisData) {
  
  if(!cfgAutoEnable_ || getEcmcEpicsIOCState()!=16 || errorCode_ || ecmcAxisData.axisId < 0) {
    return;
  }
  
  if(ecmcAxisData.enabled) {
    return;
  }

  setAxisEnable(ecmcAxisData.axisId,1);
}

bool ecmcGrbl::getEcmcAxisLimitBwd(int ecmcAxisId) {
  int lim = 0;
  getAxisLimitSwitchBwd(ecmcAxisId,&lim);
  return lim == 1;
}

bool ecmcGrbl::getEcmcAxisLimitFwd(int ecmcAxisId) {
  int lim = 0;
  getAxisLimitSwitchFwd(ecmcAxisId,&lim);
  return lim == 1;
}

bool  ecmcGrbl::getEcmcAxisEnabled(int ecmcAxisId) {
  int ena=0;
  getAxisEnabled(ecmcAxisId, &ena);
  return ena;
}

int  ecmcGrbl::getEcmcAxisTrajSource(int ecmcAxisId) {
  int source = ECMC_DATA_SOURCE_INTERNAL;
  getAxisTrajSource(ecmcAxisId,&source);
  return source;
}

double  ecmcGrbl::getEcmcAxisActPos(int axis) {
  double pos=0;
  getAxisEncPosAct(axis,
                   &pos);
  return pos;
}

void ecmcGrbl::preExeAxes() {

  preExeAxis(ecmcData_.xAxis,X_AXIS);
  preExeAxis(ecmcData_.yAxis,Y_AXIS);
  preExeAxis(ecmcData_.zAxis,Z_AXIS);

  // Kill everything if limit switch violation
  giveControlToEcmcIfNeeded();

  //spindle
  autoEnableAxis(ecmcData_.spindleAxis);
  
  if(ecmcData_.allEnabled) {
    autoEnableTimeOutCounter_ = 0;
  } else {
    if(cfgAutoEnable_ && !errorCode_) {
      if(autoEnableTimeOutCounter_ >= cfgAutoEnableTimeOutSecs_/exeSampleTimeMs_*1000) {
        errorCode_ = ECMC_PLUGIN_AUTO_ENABLE_TIMEOUT_ERROR_CODE;
        if(errorCode_ != errorCodeOld_) {
          printf("GRBL: ERROR: Auto enable timeout 0x%x\n",errorCode_);
        }
        setExecute(0);
        setAllAxesEnable(0);
      } else {
        autoEnableTimeOutCounter_++;
      }
    }
  }
}

void ecmcGrbl::preExeAxis(ecmcAxisStatusData ecmcAxisData, int grblAxisId) {
  if(ecmcAxisData.axisId < 0) {
    return;
  }
  
  syncAxisPosition(ecmcAxisData, grblAxisId);
  autoEnableAxis(ecmcAxisData);  
}

void ecmcGrbl::giveControlToEcmcIfNeeded() {

  // Give total control to ecmc at negative edge of any limit switch
  if( (!ecmcData_.allLimitsOK && ecmcData_.allLimitsOKOld) ||
     (!ecmcData_.errorOld && ecmcData_.error) ) {

    if(ecmcData_.xAxis.axisId >= 0) {
      if(ecmcData_.xAxis.trajSource == ECMC_DATA_SOURCE_EXTERNAL) {
        setAxisTrajSource(ecmcData_.xAxis.axisId,ECMC_DATA_SOURCE_INTERNAL);
        stopMotion(ecmcData_.xAxis.axisId,0);
      }
    }

    if(ecmcData_.yAxis.axisId >= 0) {
      if(ecmcData_.yAxis.trajSource == ECMC_DATA_SOURCE_EXTERNAL) {
        setAxisTrajSource(ecmcData_.yAxis.axisId,ECMC_DATA_SOURCE_INTERNAL);
        stopMotion(ecmcData_.yAxis.axisId,0);
      }
    }

    if(ecmcData_.zAxis.axisId >= 0) {
      if(ecmcData_.zAxis.trajSource == ECMC_DATA_SOURCE_EXTERNAL) {
        setAxisTrajSource(ecmcData_.zAxis.axisId,ECMC_DATA_SOURCE_INTERNAL);
        stopMotion(ecmcData_.zAxis.axisId,0);
      }
    }

    // Stop spindle
    if(ecmcData_.spindleAxis.axisId >= 0) {
      setAxisTargetVel(ecmcData_.spindleAxis.axisId, 0);
      stopMotion(ecmcData_.spindleAxis.axisId,0);
    }

    // Halt grbl and stop motion (even though should be handled by ecmc)
    setExecute(0);
    setHalt(0);
    setHalt(1);
    
    errorCode_ = ECMC_PLUGIN_LIMIT_SWITCH_VIOLATION_ERROR_CODE;

    // Also reset for safety (avoid autoenable)
    cfgAutoStart_         = 0; 
  }
}

void ecmcGrbl::syncAxisPosition(ecmcAxisStatusData ecmcAxisData, int grblAxisId) {
  
  // sync positions when not enabled
  if(!ecmcAxisData.enabled || ecmcAxisData.trajSource == ECMC_DATA_SOURCE_INTERNAL) {
    sys_position[grblAxisId] = (int32_t)(double(settings.steps_per_mm[grblAxisId])*ecmcAxisData.actpos);
    plan_sync_position();
    gc_sync_position();
  }
}

// prepare for rt here  
int ecmcGrbl::enterRT() {
  // readback spindleAcceleration_
  if(cfgSpindleAxisId_ >= 0) {
    double acc = 0;
    int errorCode = getAxisAcceleration(cfgSpindleAxisId_,
                                        &acc);
    if(errorCode) {
      errorCode_ = errorCode;            
      return errorCode;
    }

    if(acc <= 0) {
      errorCode_ = ECMC_PLUGIN_SPINDLE_ACC_ERROR_CODE;
      return errorCode_;
    }

    ecmcData_.spindleAxis.acceleration = acc;                 
  }
  return 0;
}

// Get data from ecmc and check enabled and limits
void   ecmcGrbl::readEcmcStatus(int ecmcError) {

  ecmcData_.errorOld       = ecmcData_.error;
  ecmcData_.error          = ecmcError;
  ecmcData_.allLimitsOKOld = ecmcData_.allLimitsOK;
  ecmcData_.allEnabled     = true;
  ecmcData_.allLimitsOK    = true;

  if(ecmcData_.xAxis.axisId >= 0) {
     ecmcData_.xAxis.enabled    = getEcmcAxisEnabled(cfgXAxisId_);
     ecmcData_.xAxis.limitBwd   = getEcmcAxisLimitBwd(cfgXAxisId_);
     ecmcData_.xAxis.limitFwd   = getEcmcAxisLimitFwd(cfgXAxisId_);
     ecmcData_.xAxis.error      = getAxisError(cfgXAxisId_);
     ecmcData_.xAxis.actpos     = getEcmcAxisActPos(cfgXAxisId_);
     ecmcData_.xAxis.trajSource = getEcmcAxisTrajSource(cfgXAxisId_);
     ecmcData_.allEnabled       = ecmcData_.allEnabled &&
                                  ecmcData_.xAxis.enabled;
     ecmcData_.allLimitsOK      = ecmcData_.allLimitsOK &&
                                  ecmcData_.xAxis.limitBwd && 
                                  ecmcData_.xAxis.limitFwd;
  }

  if(ecmcData_.yAxis.axisId >= 0) {
     ecmcData_.yAxis.enabled    = getEcmcAxisEnabled(cfgYAxisId_);
     ecmcData_.yAxis.limitBwd   = getEcmcAxisLimitBwd(cfgYAxisId_);
     ecmcData_.yAxis.limitFwd   = getEcmcAxisLimitFwd(cfgYAxisId_);
     ecmcData_.yAxis.error      = getAxisError(cfgYAxisId_);
     ecmcData_.yAxis.actpos     = getEcmcAxisActPos(cfgYAxisId_);
     ecmcData_.yAxis.trajSource = getEcmcAxisTrajSource(cfgYAxisId_);
     ecmcData_.allEnabled       = ecmcData_.allEnabled &&
                                  ecmcData_.yAxis.enabled;
     ecmcData_.allLimitsOK      = ecmcData_.allLimitsOK &&
                                  ecmcData_.yAxis.limitBwd && 
                                  ecmcData_.yAxis.limitFwd;
  }

  if(ecmcData_.zAxis.axisId >= 0) {
     ecmcData_.zAxis.enabled    = getEcmcAxisEnabled(cfgZAxisId_);
     ecmcData_.zAxis.limitBwd   = getEcmcAxisLimitBwd(cfgZAxisId_);
     ecmcData_.zAxis.limitFwd   = getEcmcAxisLimitFwd(cfgZAxisId_);
     ecmcData_.zAxis.error      = getAxisError(cfgZAxisId_);
     ecmcData_.zAxis.actpos     = getEcmcAxisActPos(cfgZAxisId_);
     ecmcData_.zAxis.trajSource = getEcmcAxisTrajSource(cfgZAxisId_);
     ecmcData_.allEnabled       = ecmcData_.allEnabled &&
                                  ecmcData_.zAxis.enabled;
     ecmcData_.allLimitsOK      = ecmcData_.allLimitsOK &&
                                  ecmcData_.zAxis.limitBwd && 
                                  ecmcData_.zAxis.limitFwd;
  }

  if(ecmcData_.spindleAxis.axisId >= 0) {
     ecmcData_.spindleAxis.enabled    = getEcmcAxisEnabled(cfgSpindleAxisId_);
     ecmcData_.spindleAxis.limitBwd   = getEcmcAxisLimitBwd(cfgSpindleAxisId_);
     ecmcData_.spindleAxis.limitFwd   = getEcmcAxisLimitFwd(cfgSpindleAxisId_);
     ecmcData_.spindleAxis.error      = getAxisError(cfgSpindleAxisId_);
     ecmcData_.spindleAxis.actpos     = getEcmcAxisActPos(cfgSpindleAxisId_);
     ecmcData_.spindleAxis.trajSource = getEcmcAxisTrajSource(cfgSpindleAxisId_);    
     ecmcData_.allEnabled             = ecmcData_.allEnabled &&
                                        ecmcData_.spindleAxis.enabled;
     ecmcData_.allLimitsOK            = ecmcData_.allLimitsOK &&
                                        ecmcData_.spindleAxis.limitBwd && 
                                        ecmcData_.spindleAxis.limitFwd;
  }
}

// grb realtime thread!!!  
int  ecmcGrbl::grblRTexecute(int ecmcError) {

  if(getEcmcEpicsIOCState()!=16 || !grblInitDone_ || unrecoverableError_) {
    return 0;
  }
  
  // Read all ecmc data
  readEcmcStatus(ecmcError);
  
  // Error handling
  if((ecmcData_.errorOld == 0 && ecmcData_.error > 0) ||
     (errorCode_ > 0 && errorCodeOld_ == 0)) {
    setHalt(0);
    setHalt(1);

    //if(ecmcData_.error != errorCode_) {  // ecmc error then reset
    //  if(ecmcData_.error > 0 && ecmcData_.errorOld == 0) {
    //    setReset(0);
    //    setReset(1);
    //  }
    //}

    // Stop spindle
    if(ecmcData_.spindleAxis.axisId >= 0) {
      setAxisTargetVel(ecmcData_.spindleAxis.axisId, 0);
      stopMotion(ecmcData_.spindleAxis.axisId,0);
    }

    setExecute(0);
    printf("GRBL: ERROR: ecmc 0x%x, plugin 0x%x\n",ecmcError,errorCode_);    
    errorCodeOld_ = errorCode_;

    giveControlToEcmcIfNeeded();
    return errorCode_;
  }

  // auto start
  if (!autoStartDone_) {
    if(cfgAutoStart_) {
      setExecute(1);
      autoStartDone_ = 1;
    }
  }

  errorCodeOld_ = errorCode_;
  
  preExeAxes();

  double sampleRateMs = 0.0;
  if(grblInitDone_ && ecmcData_.allEnabled) {
    while(timeToNextExeMs_ < exeSampleTimeMs_ && sampleRateMs >= 0) {      
      sampleRateMs = ecmc_grbl_main_rt_thread();
      if(sampleRateMs > 0){
        timeToNextExeMs_ += sampleRateMs;
      } else {
        timeToNextExeMs_ = 0;  // reset since no more steps..
      }
    }
    if(sampleRateMs >= 0){
      timeToNextExeMs_-= exeSampleTimeMs_;
    }
  }
  //update setpoints
  postExeAxes();
  return errorCode_;
}

void ecmcGrbl::postExeAxis(ecmcAxisStatusData ecmcAxisData, int grblAxisId) {
  if(ecmcAxisData.axisId >= 0) {
    setAxisExtSetPos(ecmcAxisData.axisId,double(sys_position[grblAxisId])/double(settings.steps_per_mm[grblAxisId]));
  }
}

void ecmcGrbl::postExeAxes() {
  postExeAxis(ecmcData_.xAxis,X_AXIS);
  postExeAxis(ecmcData_.yAxis,Y_AXIS);
  postExeAxis(ecmcData_.zAxis,Z_AXIS);

  
  if(ecmcData_.spindleAxis.axisId >= 0) {
    setAxisTargetVel(ecmcData_.spindleAxis.axisId,(double)sys.spindle_speed);
    if(sys.spindle_speed!=0) {
      moveVelocity(cfgSpindleAxisId_,
                   (double)sys.spindle_speed,
                   ecmcData_.spindleAxis.acceleration,
                   ecmcData_.spindleAxis.acceleration);      
    }
  }
}

// trigg start of g-code
int ecmcGrbl::setExecute(int exe) {
  if(!exe) {    
    autoEnableTimeOutCounter_ = 0;
  }

  if(!executeCmd_ && exe) {
    grblCommandBufferIndex_ = 0;
    writerBusy_ = 1;
  }
  executeCmd_ = exe;
  return 0;
}

int ecmcGrbl::setHalt(int halt) {
  if(!haltCmd_ && halt) {
    system_set_exec_state_flag(EXEC_FEED_HOLD);
  }
  haltCmd_ = halt;
  return 0;
}

int ecmcGrbl::setResume(int resume) {
  if(!resumeCmd_ && resume) {
   system_set_exec_state_flag(EXEC_CYCLE_START);
  }
  resumeCmd_ = resume;
  return 0;
}

int ecmcGrbl::setReset(int reset) {
  if(!resetCmd_ && reset) {
    mc_reset();
  }
  grblInitDone_ = 0;
  resetCmd_ = reset;
  return 0;
}

int ecmcGrbl::getBusy() {
  return getEcmcEpicsIOCState()!=16 || writerBusy_ || stepperInterruptEnable || !grblInitDone_;
}

int ecmcGrbl::getParserBusy() {
  return getEcmcEpicsIOCState()!=16 || writerBusy_ || !grblInitDone_;
}

int ecmcGrbl::getCodeRowNum() {
  return grblCommandBufferIndex_;
}

// Avoid issues with std:to_string()
std::string ecmcGrbl::to_string(int value) {
  std::ostringstream os;
  os << value;
  return os.str();
}

int ecmcGrbl::getError() {
  return errorCode_;
}

void ecmcGrbl::resetError() {
  errorCode_ = 0;
  errorCodeOld_ = 0;
}

void ecmcGrbl::addCommand(std::string command) {
  if(cfgDbgMode_){
    printf("%s:%s:%d:command %s\n",__FILE__,__FUNCTION__,__LINE__,command.c_str());
  }
  epicsMutexLock(grblCommandBufferMutex_);  
  grblCommandBuffer_.push_back(command.c_str());
  epicsMutexUnlock(grblCommandBufferMutex_);
  if(cfgDbgMode_){
    printf("%s:%s:%d: GRBL: INFO: Buffer size %d\n",
           __FILE__,__FUNCTION__,__LINE__,grblCommandBuffer_.size());
  }
}

void ecmcGrbl::loadGCodeFile(std::string fileName, int append) {
  if(cfgDbgMode_){
    printf("%s:%s:%d: file %s, append %d\n",__FILE__,__FUNCTION__,__LINE__,fileName.c_str(),append);
  }

  std::ifstream file;
  file.open(fileName);
  if (!file.good()) {
    if(cfgDbgMode_){
      printf("%s:%s:%d: GRBL: ERROR: File not found: %s (0x%x)\n",
             __FILE__,__FUNCTION__,__LINE__,fileName.c_str(),ECMC_PLUGIN_LOAD_FILE_ERROR_CODE);
    }
    errorCode_ = ECMC_PLUGIN_LOAD_FILE_ERROR_CODE;
    throw std::runtime_error("Error: File not found.");
    return;
  }
  
  // Clear buffer (since not append)
  if(!append) {
    setExecute(0);
    epicsMutexLock(grblCommandBufferMutex_);  
    grblCommandBuffer_.clear();
    epicsMutexUnlock(grblCommandBufferMutex_);
  }

  std::string line, lineNoComments;

  while (std::getline(file, line)) {
    if(line.length()>0) {
      addCommand(line);
    }
  }
}

void  ecmcGrbl::addConfig(std::string command) {
  
  if(cfgDbgMode_){
    printf("%s:%s:%d:command %s\n",__FILE__,__FUNCTION__,__LINE__,command.c_str());
  }
    
  if (getEcmcEpicsIOCState() == 16) {
    printf("%s:%s:%d: GRBL: ERROR: Configuratoin can only be applied during startup:(0x%x)\n",
        __FILE__,__FUNCTION__,__LINE__,ECMC_PLUGIN_CONFIG_ERROR_CODE);
    return;
  }

  // ignore comments
  std::string commandStrip = command.substr(0, command.find(ECMC_CONFIG_FILE_COMMENT_CHAR));
  if (commandStrip.length()==0) {
    return;
  }

  std::size_t found = commandStrip.find(ECMC_CONFIG_GRBL_CONFIG_CHAR);
  if (found==std::string::npos) {
    printf("%s:%s:%d: GRBL: ERROR: Configuration command not valid (0x%x)\n",
        __FILE__,__FUNCTION__,__LINE__,ECMC_PLUGIN_CONFIG_ERROR_CODE);
    return;
  }

  epicsMutexLock(grblConfigBufferMutex_);
  grblConfigBuffer_.push_back(commandStrip.c_str());
  epicsMutexUnlock(grblConfigBufferMutex_);
  if(cfgDbgMode_){
    printf("%s:%s:%d: GRBL: INFO: Buffer size %d\n",
           __FILE__,__FUNCTION__,__LINE__,grblConfigBuffer_.size());
  }
}

void ecmcGrbl::loadConfigFile(std::string fileName, int append) {

  if(cfgDbgMode_){
    printf("%s:%s:%d: file %s, append %d\n",__FILE__,__FUNCTION__,__LINE__,fileName.c_str(),append);
  }

  std::ifstream file;
  file.open(fileName);
  if (!file.good()) {
    if(cfgDbgMode_){
      printf("%s:%s:%d: GRBL: ERROR: File not found: %s (0x%x)\n",
             __FILE__,__FUNCTION__,__LINE__,fileName.c_str(),ECMC_PLUGIN_LOAD_FILE_ERROR_CODE);
    }
    errorCode_ = ECMC_PLUGIN_LOAD_FILE_ERROR_CODE;
    throw std::runtime_error("Error: File not found.");
    return;
  }
  
  // Clear buffer (since not append)
  if(!append) {
    setExecute(0);
    epicsMutexLock(grblConfigBufferMutex_);  
    grblConfigBuffer_.clear();
    epicsMutexUnlock(grblConfigBufferMutex_);
  }

  std::string line, lineNoComments;

  while (std::getline(file, line)) {
    if(line.length()>0) {
      addConfig(line);
    }
  }
}
