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

// Start worker for socket read()
void f_worker_read(void *obj) {
  if(!obj) {
      printf("%s/%s:%d: GRBL: ERROR: Worker read thread ecmcGrbl object NULL..\n",
              __FILE__, __FUNCTION__, __LINE__);
    return;
  }
  ecmcGrbl * grblObj = (ecmcGrbl*)obj;
  grblObj->doReadWorker();
}

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
  ecmcError_            = 0;
  errorCodeOld_         = 0;
  exeSampleTimeMs_      = exeSampleTimeMs;
  cfgXAxisId_           = -1;
  cfgYAxisId_           = -1;
  cfgZAxisId_           = -1;
  cfgSpindleAxisId_     = -1;
  grblInitDone_         = 0;
  cfgAutoEnableAtStart_ = 0;
  autoEnableExecuted_   = 0;
  timeToNextExeMs_      = 0;
  writerBusy_           = 0;
  limitsSummary_        = 0;
  limitsSummaryOld_     = 0;
  grblCommandBufferIndex_ = 0;
  grblCommandBuffer_.clear();

  if(!(grblCommandBufferMutex_ = epicsMutexCreate())) {
    throw std::runtime_error("GRBL: ERROR: Failed create mutex thread for write().");
  }
  
  parseConfigStr(configStr); // Assigns all configs
  
  // simulate auto enable
  if(!cfgAutoEnableAtStart_) {
    autoEnableExecuted_ = 1;
  }

  // auto execute
  if(cfgAutoStart_) {
    setExecute(1);
  }

  // global varaible in grbl  
  enableDebugPrintouts = cfgDbgMode_;

  //Check atleast one valid axis
  if(cfgXAxisId_<0 && cfgXAxisId_<0 && cfgXAxisId_<0 && cfgSpindleAxisId_<0) {
    throw std::out_of_range("GRBL: ERROR: No valid axis choosen.");
  }

  //// Create worker thread for reading socket
  //std::string threadname = "ecmc.grbl.read";
  //if(epicsThreadCreate(threadname.c_str(), 0, 32768, f_worker_read, this) == NULL) {
  //  throw std::runtime_error("Error: Failed create worker thread for read().");
  //}

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
        cfgAutoEnableAtStart_ = atoi(pThisOption);
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

// Read socket worker
void ecmcGrbl::doReadWorker() {
//  // simulate serial connection here (need mutex)
//  if(cfgDbgMode_){
//    printf("%s:%s:%d\n",__FILE__,__FUNCTION__,__LINE__);
//  }
//  for(;;) {
//    //while(serial_get_tx_buffer_count()==0) {
//    //  delay_ms(1);      
//    //}
//    //printf("%c",ecmc_get_char_from_grbl_tx_buffer());
//    delay_ms(100);
//  }
}

// Write socket worker
void ecmcGrbl::doWriteWorker() {
  // simulate serial connection here (need mutex)
  std::string reply = "";
  if(cfgDbgMode_){
    printf("%s:%s:%d\n",__FILE__,__FUNCTION__,__LINE__);
  }
  
  // Wait for grbl startup string before send comamnds"['$' for help]" 
  // basically flush buffer
  for(;;) {
    while(serial_get_tx_buffer_count()==0) {
      delay_ms(2);
    }
    char c = ecmc_get_char_from_grbl_tx_buffer();
    reply += c;
    if(c == '\n' && 
      reply.find(ECMC_PLUGIN_GRBL_GRBL_STARTUP_STRING) != std::string::npos ) {
      if(cfgDbgMode_){
        printf("GRBL: INFO: Ready for commands: %s\n",reply.c_str());
      }
      break;
    }
  }

  // wait for epics state && auto enable at start
  while(getEcmcEpicsIOCState()!=16 || !autoEnableExecuted_) {
    delay_ms(2);
  }

  // GRBL ready, now we can send comamnds
  for(;;) {
    if( (grblCommandBuffer_.size() > grblCommandBufferIndex_) &&         
        executeCmd_) {              
            
      epicsMutexLock(grblCommandBufferMutex_);
      std::string commandRaw = grblCommandBuffer_[grblCommandBufferIndex_];
      epicsMutexUnlock(grblCommandBufferMutex_);
      std::string command = commandRaw.substr(0, commandRaw.find(ECMC_CONFIG_FILE_COMMENT_CHAR));
      
      if(command.length() == 0) {
        continue;
      }

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
      reply = "";

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
              printf("GRBL: INFO: Reply OK (command[%d] = %s)\n",
                     grblCommandBufferIndex_,
                     command.c_str());
            }
            break;

          } else if(reply.find(ECMC_PLUGIN_GRBL_GRBL_ERR_STRING) != std::string::npos) {
            if(cfgDbgMode_){
              printf("GRBL: ERROR: Reply ERROR (command[%d] = %s)\n",
                      grblCommandBufferIndex_,
                      command.c_str());
            }
            errorCode_ = ECMC_PLUGIN_GRBL_COMMAND_ERROR_CODE;
            // stop motion
            setExecute(0);            
            setReset(1);
            setReset(0);
            break; 

          } else if(reply.find(ECMC_PLUGIN_GRBL_GRBL_STARTUP_STRING) != std::string::npos ) {
            if(cfgDbgMode_){
              printf("GRBL: INFO: Ready for commands: %s\n",reply.c_str());
            }
            // system has reset
            setExecute(0);
            break;

          } else {
            // keep waiting (no break)            
            if(cfgDbgMode_){
              printf("GRBL: INFO: Reply non protocol related: %s\n",reply.c_str());

            }
          }
        }
      }
      // All rows written
      //if(grblCommandBufferIndex_ == grblCommandBuffer_.size()) {
      //  writerBusy_ = 0;
      //}
      grblCommandBufferIndex_++;
    }
    else {
      writerBusy_ = 0;
      // Wait for right condition to start
      delay_ms(5);
    }
  }
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

void  ecmcGrbl::autoEnableAxisAtStart(int ecmcAxisId) {
  
  if(!cfgAutoEnableAtStart_ || autoEnableExecuted_ || getEcmcEpicsIOCState()!=16) { 
    return;
  }
  
  // write to ecmc
  if(ecmcAxisId>=0) {
    setAxisEnable(ecmcAxisId,1);
  }    
}

bool  ecmcGrbl::getEcmcAxisEnabled(int ecmcAxisId) {
  int ena=0;
  getAxisEnabled(ecmcAxisId, &ena);
  return ena;
}

bool ecmcGrbl::getAllConfiguredAxisEnabled() {
  int ena = 1;
  if(cfgXAxisId_>=0 && ena) {
    ena = getEcmcAxisEnabled(cfgXAxisId_);
  }
  if(cfgYAxisId_>=0 && ena) {
    ena = getEcmcAxisEnabled(cfgYAxisId_);
  }
  if(cfgZAxisId_>=0 && ena) {
    ena = getEcmcAxisEnabled(cfgZAxisId_);
  }
  if(cfgSpindleAxisId_>=0 && ena) {
    ena = getEcmcAxisEnabled(cfgSpindleAxisId_);
  }
  return ena;
}

double  ecmcGrbl::getEcmcAxisActPos(int axis) {
  double pos=0;
  getAxisEncPosAct(axis,
                   &pos);
  return pos;
}

void ecmcGrbl::preExeAxes() {
  limitsSummary_ = 1;
  preExeAxis(cfgXAxisId_,X_AXIS);
  preExeAxis(cfgYAxisId_,Y_AXIS);
  preExeAxis(cfgZAxisId_,Z_AXIS);

  // Kill everything if limit switch violation
  giveControlToEcmcIfNeeded();

  //spindle
  autoEnableAxisAtStart(cfgSpindleAxisId_);
  if(getAllConfiguredAxisEnabled()) {
    autoEnableExecuted_ = 1;
  }
}

void ecmcGrbl::preExeAxis(int ecmcAxisId, int grblAxisId) {
  if(ecmcAxisId < 0) {
    return;
  }

  syncAxisPositionIfNotEnabled(ecmcAxisId, grblAxisId);
  autoEnableAxisAtStart(ecmcAxisId);
  checkLimits(ecmcAxisId);
}

void ecmcGrbl::checkLimits(int ecmcAxisId) {
  int lim = 0;
  getAxisLimitSwitchBwd(ecmcAxisId,&lim);
  limitsSummary_ = limitsSummary_ && lim;
  getAxisLimitSwitchFwd(ecmcAxisId,&lim);
  limitsSummary_ = limitsSummary_ && lim;
}

void ecmcGrbl::giveControlToEcmcIfNeeded() {

  // Give total control to ecmc at negative edge of any limit switch
  if(!limitsSummary_ && limitsSummaryOld_) {    
    int source = ECMC_DATA_SOURCE_INTERNAL;
    if(cfgXAxisId_>=0) {
      getAxisTrajSource(cfgXAxisId_,&source);
      if(source == ECMC_DATA_SOURCE_EXTERNAL) {
        setAxisTrajSource(cfgXAxisId_,ECMC_DATA_SOURCE_INTERNAL);
      }
    }

    if(cfgYAxisId_>=0) {
      getAxisTrajSource(cfgYAxisId_,&source);
      if(source == ECMC_DATA_SOURCE_EXTERNAL) {
        setAxisTrajSource(cfgYAxisId_,ECMC_DATA_SOURCE_INTERNAL);
      }
    }

    if(cfgZAxisId_>=0) {
      getAxisTrajSource(cfgZAxisId_,&source);
      if(source == ECMC_DATA_SOURCE_EXTERNAL) {
        setAxisTrajSource(cfgZAxisId_,ECMC_DATA_SOURCE_INTERNAL);
      }
    }

    // Stop spindle
    if(cfgSpindleAxisId_>=0) {
      setAxisTargetVel(cfgSpindleAxisId_, 0);
    }

    // Halt grbl and stop motion (even though should be handled by ecmc)
    setExecute(0);
    setHalt(0);
    setHalt(1);
    
    errorCode_ = ECMC_PLUGIN_LIMIT_SWITCH_VIOLATION_ERROR_CODE;

    // Also reset for safety (avoid autoenable)
    autoEnableExecuted_   = 1;
    cfgAutoStart_         = 0;
    cfgAutoEnableAtStart_ = 0;
  }

  limitsSummaryOld_ = limitsSummary_;
}

void ecmcGrbl::syncAxisPositionIfNotEnabled(int ecmcAxisId, int grblAxisId) {
  
  bool sync = 0;
  
  // sync positions when not enabled
  if(!getEcmcAxisEnabled(ecmcAxisId)) {    
    sync = 1;
  }

  // sync positions when ecmc is in internal mode
  if(!sync)  {
    int source = ECMC_DATA_SOURCE_INTERNAL;
    getAxisTrajSource(ecmcAxisId,&source);
    if(source == ECMC_DATA_SOURCE_INTERNAL) {
      sync = 1;
    }
  }

  if(sync) {
    sys_position[grblAxisId] = (int32_t)(double(settings.steps_per_mm[grblAxisId])*getEcmcAxisActPos(ecmcAxisId));
    plan_sync_position();
    gc_sync_position();
  }
}

// prepare for rt here  
int ecmcGrbl::enterRT() {
  return 0;
}

// grb realtime thread!!!  
int  ecmcGrbl::grblRTexecute(int ecmcError) {
  
  if(getEcmcEpicsIOCState()!=16) {
    return 0;
  }

  if((ecmcError_ == 0 && ecmcError>0) || (errorCode_>0 && errorCodeOld_ == 0)) {    
    setHalt(0);
    setHalt(1);
    if(ecmcError != errorCode_) {  // ecmc error the reset
      if(ecmcError>0 && ecmcError_ == 0) {
        setReset(0);
        setReset(1);
      }
    }
    setExecute(0);
    printf("GRBL: ERROR: ecmc 0x%x, plugin 0x%x\n",ecmcError,errorCode_);
    ecmcError_ = ecmcError;
    errorCodeOld_ = errorCode_;
  
    return errorCode_;
  }

  ecmcError_ = ecmcError;
  errorCodeOld_ = errorCode_;

  //auto enable, sync positions
  preExeAxes();
  
  double sampleRateMs = 0.0;
  if(grblInitDone_ && autoEnableExecuted_) {
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

void ecmcGrbl::postExeAxis(int ecmcAxisId, int grblAxisId) {
  if(ecmcAxisId>=0) {
    setAxisExtSetPos(ecmcAxisId,double(sys_position[grblAxisId])/double(settings.steps_per_mm[grblAxisId]));
  }
}

void ecmcGrbl::postExeAxes() {
  postExeAxis(cfgXAxisId_,X_AXIS);
  postExeAxis(cfgYAxisId_,Y_AXIS);
  postExeAxis(cfgZAxisId_,Z_AXIS);
  //  if(cfgSpindleAxisId_>=0) {
  //    setAxisTargetVel(xxx);
  //  }
}

// trigg start of g-code
int ecmcGrbl::setExecute(int exe) {
  if(!exe) {
    writerBusy_ = 0;
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
  resetCmd_ = reset;  
  return 0;
}

int ecmcGrbl::getBusy() {
  return getEcmcEpicsIOCState()!=16 || writerBusy_ || stepperInterruptEnable;
}

// Avoid issues with std:to_string()
std::string ecmcGrbl::to_string(int value) {
  std::ostringstream os;
  os << value;
  return os.str();
}

void ecmcGrbl::addCommand(std::string command) {
  if(cfgDbgMode_){
    printf("%s:%s:%d:\n",__FILE__,__FUNCTION__,__LINE__);
  }
  epicsMutexLock(grblCommandBufferMutex_);  
  grblCommandBuffer_.push_back(command.c_str());
  epicsMutexUnlock(grblCommandBufferMutex_);
  if(cfgDbgMode_){
    printf("%s:%s:%d:GRBL: INFO: Buffer size %d\n",
           __FILE__,__FUNCTION__,__LINE__,grblCommandBuffer_.size());
  }
}
void ecmcGrbl::loadFile(std::string fileName, int append) {
  if(cfgDbgMode_){
    printf("%s:%s:%d:\n",__FILE__,__FUNCTION__,__LINE__);
  }

  std::ifstream file;
  file.open(fileName);
  if (!file.good()) {
    if(cfgDbgMode_){
      printf("%s:%s:%d:GRBL: ERROR: File not found: %s (0x%x)\n",
             __FILE__,__FUNCTION__,__LINE__,fileName,ECMC_PLUGIN_LOAD_FILE_ERROR_CODE);
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
  int lineNumber = 1;
  int errorCode  = 0;

  while (std::getline(file, line)) {
    if(lineNoComments.length()>0) {
      addCommand(lineNoComments);
    }
  }
}
