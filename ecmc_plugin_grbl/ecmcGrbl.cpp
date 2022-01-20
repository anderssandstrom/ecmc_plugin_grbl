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
extern "C" {
#include "grbl.h"
}

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
    printf("%s/%s:%d: Error: Worker read thread ecmcGrbl object NULL..\n",
            __FILE__, __FUNCTION__, __LINE__);
    return;
  }
  ecmcGrbl * grblObj = (ecmcGrbl*)obj;
  grblObj->doReadWorker();
}

// Start worker for socket connect()
void f_worker_main(void *obj) {
  if(!obj) {
    printf("%s/%s:%d: Error: Worker main thread ecmcGrbl object NULL..\n",
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
  cfgDbgMode_       = 0;
  destructs_        = 0;
  connected_        = 0;  
  errorCode_        = 0;
  exeSampleTimeMs_  = exeSampleTimeMs;
  cfgXAxisId_       = -1;
  cfgYAxisId_       = -1;
  cfgZAxisId_       = -1;
  cfgSpindleAxisId_ = -1;
  grblInitDone_     = 0;

  parseConfigStr(configStr); // Assigns all configs
  
  //Check atleast one valid axis
  if(cfgXAxisId_<0 && cfgXAxisId_<0 && cfgXAxisId_<0 && cfgSpindleAxisId_<0) {
    throw std::out_of_range("No valid axis choosen.");
  }

  // Create worker thread for reading socket
  std::string threadname = "ecmc." ECMC_PLUGIN_ASYN_PREFIX ".read";
  if(epicsThreadCreate(threadname.c_str(), 0, 32768, f_worker_read, this) == NULL) {
    throw std::runtime_error("Error: Failed create worker thread for read().");
  }

  // Create worker thread for connecting socket
  threadname = "ecmc." ECMC_PLUGIN_ASYN_PREFIX ".main";
  if(epicsThreadCreate(threadname.c_str(), 0, 32768, f_worker_main, this) == NULL) {
    throw std::runtime_error("Error: Failed create worker thread for main().");
  }

  // wait for grblInitDone_!
  printf("Waiting for grbl init..");
  while(!grblInitDone_) {      
    delay_ms(100);
    printf(".");
  }
  delay_ms(100);
  printf("\n");
  printf("\n grbl ready for commands!\n");
  sleep(1);
  testGrbl();
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

      pThisOption = pNextOption;
    }    
    free(pOptions);
  }
}

// Read socket worker
void ecmcGrbl::doReadWorker() {
  // simulate serial connection here (need mutex)
  printf("%s:%s:%d\n",__FILE__,__FUNCTION__,__LINE__);
  for(;;) {
    while(serial_get_tx_buffer_count()==0) {
      delay_ms(1);      
    }
    printf("%c",ecmc_get_char_from_grbl_tx_buffer());
  }
}

// Main grbl worker (copied from grbl main.c)
void ecmcGrbl::doMainWorker() {
  printf("%s:%s:%d\n",__FILE__,__FUNCTION__,__LINE__);
  
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
  }
}

// grb realtime thread!!!  
void  ecmcGrbl::grblRTexecute() {

}

// Avoid issues with std:to_string()
std::string ecmcGrbl::to_string(int value) {
  std::ostringstream os;
  os << value;
  return os.str();
}

void ecmcGrbl::testGrbl() {

  // test some commands
  printf("Test command:$\n");
  ecmc_write_command_serial("$\n");
  sleep(1);
  printf("Test command:G0X10Y100\n");
  ecmc_write_command_serial("G0X10Y100\n");
  sleep(1);
  printf("Test command:$G\n");
  ecmc_write_command_serial("$G\n");
  sleep(1);
  printf("Test command:G4P4\n");
  ecmc_write_command_serial("G4P4\n");
  printf("Test command:G1X20Y200F20\n");
  ecmc_write_command_serial("G1X20Y200F20\n");
  printf("Test command:G4P4\n");
  ecmc_write_command_serial("G4P4\n");
  printf("Test command:G2X40Y220R20\n");
  ecmc_write_command_serial("G2X40Y220R20\n");
  printf("Test command:$\n");
  ecmc_write_command_serial("$\n");

}