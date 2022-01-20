/*************************************************************************\
* Copyright (c) 2019 European Spallation Source ERIC
* ecmc is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
*
*  ecmcPluginExample.cpp
*
*  Created on: Mar 21, 2020
*      Author: anderssandstrom
*      Credits to  https://github.com/sgreg/dynamic-loading 
*
\*************************************************************************/

// Needed to get headers in ecmc right...
#define ECMC_IS_PLUGIN
#define ECMC_EXAMPLE_PLUGIN_VERSION 2

#ifdef __cplusplus
extern "C" {
#endif  // ifdef __cplusplus

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>


#include "ecmcPluginDefs.h"
#include "ecmcPluginClient.h"
#include "ecmcGrblDefs.h"
#include "ecmcGrblWrap.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static int    lastEcmcError   = 0;
static char*  lastConfStr     = NULL;
static int    alreadyLoaded   = 0;
int initDone = 0;
pthread_t tid;


void *ecmc_grbl_client_simulation_thread(void *ptr) {
//  printf("%s:%s:%d\n",__FILE__,__FUNCTION__,__LINE__);
//  for(;;) {
//    while(serial_get_tx_buffer_count()==0) {
//      delay_ms(1);      
//    }
//    printf("%c",ecmc_get_char_from_grbl_tx_buffer());
//  }
}

// copied for grbl main.c
void *ecmc_grbl_main_thread(void *ptr) {
  //printf("%s:%s:%d\n",__FILE__,__FUNCTION__,__LINE__);
//
  //// Initialize system upon power-up.
  //serial_init();   // Setup serial baud rate and interrupts
  //ecmc_init_file();  // create and clear file (simulated eeprom)
  //settings_restore(0b1111);  // restore all to defaults
  //settings_init(); // Load Grbl settings from EEPROM
  //stepper_init();  // Configure stepper pins and interrupt timers
  //system_init();   // Configure pinout pins and pin-change interrupt
  //memset(sys_position,0,sizeof(sys_position)); // Clear machine position.
  ////sei(); // Enable interrupts
  //// Initialize system state.
  //#ifdef FORCE_INITIALIZATION_ALARM
  //  // Force Grbl into an ALARM state upon a power-cycle or hard reset.
  //  sys.state = STATE_ALARM;
  //#else
  //  sys.state = STATE_IDLE;
  //#endif
  //
  //// Check for power-up and set system alarm if homing is enabled to force homing cycle
  //// by setting Grbl's alarm state. Alarm locks out all g-code commands, including the
  //// startup scripts, but allows access to settings and internal commands. Only a homing
  //// cycle '$H' or kill alarm locks '$X' will disable the alarm.
  //// NOTE: The startup script will run after successful completion of the homing cycle, but
  //// not after disabling the alarm locks. Prevents motion startup blocks from crashing into
  //// things uncontrollably. Very bad.
  //#ifdef HOMING_INIT_LOCK
  //  if (bit_istrue(settings.flags,BITFLAG_HOMING_ENABLE)) { sys.state = STATE_ALARM; }
  //#endif
  // // Grbl initialization loop upon power-up or a system abort. For the latter, all processes
  //// will return to this loop to be cleanly re-initialized.
  //for(;;) {    
  //  // Reset system variables.
  //  uint8_t prior_state = sys.state;
  //  memset(&sys, 0, sizeof(system_t)); // Clear system struct variable.
  //  sys.state = prior_state;
  //  sys.f_override = DEFAULT_FEED_OVERRIDE;  // Set to 100%
  //  sys.r_override = DEFAULT_RAPID_OVERRIDE; // Set to 100%
  //  sys.spindle_speed_ovr = DEFAULT_SPINDLE_SPEED_OVERRIDE; // Set to 100%
	//	memset(sys_probe_position,0,sizeof(sys_probe_position)); // Clear probe position.
  //  sys_probe_state = 0;
  //  sys_rt_exec_state = 0;
  //  sys_rt_exec_alarm = 0;
  //  sys_rt_exec_motion_override = 0;
  //  sys_rt_exec_accessory_override = 0;
//
  //  // Reset Grbl primary systems.
  //  serial_reset_read_buffer(); // Clear serial read buffer
  //  gc_init(); // Set g-code parser to default state
  //  spindle_init();
  //  coolant_init();
  //  limits_init();
  //  probe_init();
  //  plan_reset(); // Clear block buffer and planner variables
  //  st_reset(); // Clear stepper subsystem variables.
//
  //  // Sync cleared gcode and planner positions to current system position.
  //  plan_sync_position();
  //  gc_sync_position();
//
  //  // Print welcome message. Indicates an initialization has occured at power-up or with a reset.
  //  report_init_message();
//
  //  // ready for commands through serial interface
  //  initDone = 1;
  //  protocol_main_loop();
  //}
}

/** Optional. 
 *  Will be called once after successfull load into ecmc.
 *  Return value other than 0 will be considered error.
 *  configStr can be used for configuration parameters.
 **/
int grblConstruct(char *configStr)
{
  // only allow one loaded module
  if(alreadyLoaded) {    
    return 1;
  }
  alreadyLoaded = 1;

  // create grbl object and register data callback
  lastConfStr = strdup(configStr);

  return createGrbl(lastConfStr, getEcmcSampleTimeMS());
  // start grbl simulated client thread!
//  int err;
//
//  err = pthread_create(&(tid), NULL, *ecmc_grbl_client_simulation_thread, NULL);
//  if (err != 0) {
//    printf("\n Can't create thread :[%s]", strerror(err));
//    return 1;
//  }
//  else {
//    printf("\n grbl simulated client thread created successfully\n");
//  }
//  
//  // start grbl main thread and wait for init done!
//  printf("%s:%s:%d\n",__FILE__,__FUNCTION__,__LINE__);
//  
//  err = pthread_create(&(tid), NULL, *ecmc_grbl_main_thread, NULL);
//  if (err != 0) {
//    printf("\n Can't create thread :[%s]", strerror(err));
//    return 1;
//  }
//  else {
//    printf("\n grbl main thread created successfully\n");
//  }
  

  // whait for initDone!
//  printf("Waiting for grbl init..");
//  while(!initDone) {      
//    sleep(1);
//    printf(".");
//  }

//  printf("\n");
//  printf("\n grbl ready for commands!\n");
//  sleep(1);
//
//  // test some commands
//  printf("Test command:$\n");
//  ecmc_write_command_serial("$\n");
//  sleep(1);
//  printf("Test command:G0X10Y100\n");
//  ecmc_write_command_serial("G0X10Y100\n");
//  sleep(1);
//  printf("Test command:$G\n");
//  ecmc_write_command_serial("$G\n");
//  sleep(1);
//  printf("Test command:G4P4\n");
//  ecmc_write_command_serial("G4P4\n");
//  printf("Test command:G1X20Y200F20\n");
//  ecmc_write_command_serial("G1X20Y200F20\n");
//  printf("Test command:G4P4\n");
//  ecmc_write_command_serial("G4P4\n");
//  printf("Test command:G2X40Y220R20\n");
//  ecmc_write_command_serial("G2X40Y220R20\n");
//  printf("Test command:$\n");
//  ecmc_write_command_serial("$\n");
//  

  //printf("Test command:$J=X10.0Y-1.5\n");
  //ecmc_write_command_serial("$J=X10.0Y-1.5\0");
  //printf("Test command:#\n");
  //ecmc_write_command_serial("#\n");
  //printf("Test command:?\n");
  //ecmc_write_command_serial("?\0");
  //printf("Test command:G1X200Y100\n");
  //ecmc_write_command_serial("G1X200Y100\0");
  

//  printf("system_execute_line(G0 X11), %d \n ",system_execute_line("G0X11\0"));
//  printf("end\n");
//  printf("system_execute_line(G1X200Y100), %d \n",system_execute_line("G1X200Y100\0"));
//  printf("end\n");
//  printf("system_execute_line($$), %d \n",system_execute_line("$$\0"));
//  printf("end\n");
//  printf("system_execute_line($), %d \n",system_execute_line("$\0"));
//  printf("end\n");
//  printf("system_execute_line(#), %d \n",system_execute_line("#\0"));
//  printf("end\n");
//  printf("system_execute_line(?), %d \n",system_execute_line("?\0"));
//  printf("end\n");
//  printf("system_execute_line($J=X10.0Y-1.5), %d \n",system_execute_line("$J=X10.0Y-1.5\0"));
//  printf("end\n");
//  printf("gc_execute_line(G0 X100.25), %d \n",gc_execute_line("G0 X100.25\0"));
//  printf("end\n");
//  printf("gc_execute_line(G1X200Y100), %d \n",gc_execute_line("G1X200Y100\0"));
//  printf("end\n");
//  
//  return 0; //createSocketCAN(configStr,getEcmcSampleTimeMS());
}

/** Optional function.
 *  Will be called once at unload.
 **/
void grblDestruct(void)
{  
  if(lastConfStr){
    free(lastConfStr);
  }
  deleteGrbl();
}

/** Optional function.
 *  Will be called each realtime cycle if definded
 *  ecmcError: Error code of ecmc. Makes it posible for 
 *  this plugin to react on ecmc errors
 *  Return value other than 0 will be considered to be an error code in ecmc.
 **/
int grblRealtime(int ecmcError)
{  
  lastEcmcError = ecmcError;
  return 0; //execute();
}

/** Link to data source here since all sources should be availabe at this stage
 *  (for example ecmc PLC variables are defined only at enter of realtime)
 **/
int grblEnterRT(){
  return 0;
}

/** Optional function.
 *  Will be called once just before leaving realtime mode
 *  Return value other than 0 will be considered error.
 **/
int grblExitRT(void){
  return 0;
}
// Plc function for connect to can
double grbl_connect() {
  return 0;
}

// Register data for plugin so ecmc know what to use
struct ecmcPluginData pluginDataDef = {
  // Allways use ECMC_PLUG_VERSION_MAGIC
  .ifVersion = ECMC_PLUG_VERSION_MAGIC, 
  // Name 
  .name = "ecmcPluginGrbl",
  // Description
  .desc = "grbl plugin for use with ecmc.",
  // Option description
  .optionDesc = "\n    "ECMC_PLUGIN_DBG_PRINT_OPTION_CMD"<1/0>    : Enables/disables printouts from plugin, default = disabled (=0).\n",
  // Plugin version
  .version = ECMC_EXAMPLE_PLUGIN_VERSION,
  // Optional construct func, called once at load. NULL if not definded.
  .constructFnc = grblConstruct,
  // Optional destruct func, called once at unload. NULL if not definded.
  .destructFnc = grblDestruct,
  // Optional func that will be called each rt cycle. NULL if not definded.
  .realtimeFnc = grblRealtime,
  // Optional func that will be called once just before enter realtime mode
  .realtimeEnterFnc = grblEnterRT,
  // Optional func that will be called once just before exit realtime mode
  .realtimeExitFnc = grblExitRT,
  // PLC funcs
  .funcs[0] =
      { /*----can_connect----*/
        // Function name (this is the name you use in ecmc plc-code)
        .funcName = "grbl_connect",
        // Function description
        .funcDesc = "double grbl_connect() : Connect to grbl interface (from config str).",
        /**
        * 7 different prototypes allowed (only doubles since reg in plc).
        * Only funcArg${argCount} func shall be assigned the rest set to NULL.
        **/
        .funcArg0 = grbl_connect,
        .funcArg1 = NULL,
        .funcArg2 = NULL,
        .funcArg3 = NULL,
        .funcArg4 = NULL,
        .funcArg5 = NULL,
        .funcArg6 = NULL,
        .funcArg7 = NULL,
        .funcArg8 = NULL,
        .funcArg9 = NULL,
        .funcArg10 = NULL,
        .funcGenericObj = NULL,
      },
  .funcs[1] = {0},  // last element set all to zero..
  // PLC consts
  .consts[0] = {0}, // last element set all to zero..
};

ecmc_plugin_register(pluginDataDef);

# ifdef __cplusplus
}
# endif  // ifdef __cplusplus