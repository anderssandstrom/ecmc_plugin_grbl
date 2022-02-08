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
  return realtime(ecmcError);
}

/** Link to data source here since all sources should be availabe at this stage
 *  (for example ecmc PLC variables are defined only at enter of realtime)
 **/
int grblEnterRT(){
  return enterRT();
}

/** Optional function.
 *  Will be called once just before leaving realtime mode
 *  Return value other than 0 will be considered error.
 **/
int grblExitRT(void){
  return 0;
}

// Plc function for execute grbl code
double grbl_set_execute(double exe) {
  return setExecute((int)exe);
}

// Plc function for halt grbl
double grbl_mc_halt(double halt) {
  return setHalt((int)halt);
}

// Plc function for resume grbl
double grbl_mc_resume(double halt) {
  return setResume((int)halt);
}

// Plc function for reset grbl
//double grbl_mc_reset(double halt) {
//  return setReset((int)halt);
//}

// Plc function for reset grbl
double grbl_get_busy() {
  return getBusy();
}

// Plc function for reset grbl
double grbl_get_parser_busy() {
  return getParserBusy();
}

// Plc function for reset grbl
double grbl_get_code_row_num() {
  return getCodeRowNum();
}

double grbl_reset_error() {
  return resetError();
}

double grbl_get_error() {
  return getError();
}

double grbl_get_all_enabled() {
  return getAllAxesEnabled();
}

double grbl_set_all_enable(double enable) {
  return setAllAxesEnable(enable);
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
  .optionDesc = "\n    "ECMC_PLUGIN_DBG_PRINT_OPTION_CMD"<1/0>    : Enables/disables printouts from plugin, default = disabled (=0).\n"
                "      "ECMC_PLUGIN_X_AXIS_ID_OPTION_CMD"<axis id>: Ecmc Axis id for use as grbl X axis, default = disabled (=-1).\n"
                "      "ECMC_PLUGIN_Y_AXIS_ID_OPTION_CMD"<axis id>: Ecmc Axis id for use as grbl Y axis, default = disabled (=-1).\n"
                "      "ECMC_PLUGIN_Z_AXIS_ID_OPTION_CMD"<axis id>: Ecmc Axis id for use as grbl Z axis, default = disabled (=-1).\n"
                "      "ECMC_PLUGIN_SPINDLE_AXIS_ID_OPTION_CMD"<axis id>: Ecmc Axis id for use as grbl spindle axis, default = disabled (=-1).\n"
                "      "ECMC_PLUGIN_AUTO_ENABLE_AT_START_OPTION_CMD"<1/0>: Auto enable the linked ecmc axes autmatically before start, default = disabled (=0).\n"
                "      "ECMC_PLUGIN_AUTO_START_OPTION_CMD"<1/0>: Auto start g-code at ecmc start, default = disabled (=0).\n"
  ,
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
      { /*----grbl_set_execute----*/
        // Function name (this is the name you use in ecmc plc-code)
        .funcName = "grbl_set_execute",
        // Function description
        .funcDesc = "double grbl_set_execute(<exe>) :  Trigg execution of loaded g-code at positive edge of <exe>",
        /**
        * 7 different prototypes allowed (only doubles since reg in plc).
        * Only funcArg${argCount} func shall be assigned the rest set to NULL.
        **/
        .funcArg0 = NULL,
        .funcArg1 = grbl_set_execute,
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
  .funcs[1] =
      { /*----grbl_mc_halt----*/
        // Function name (this is the name you use in ecmc plc-code)
        .funcName = "grbl_mc_halt",
        // Function description
        .funcDesc = "double grbl_mc_halt(<halt>) :  Halt grbl motion at positive edge of <halt>",
        /**
        * 7 different prototypes allowed (only doubles since reg in plc).
        * Only funcArg${argCount} func shall be assigned the rest set to NULL.
        **/
        .funcArg0 = NULL,
        .funcArg1 = grbl_mc_halt,
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
  .funcs[2] =
      { /*----grbl_mc_resume----*/
        // Function name (this is the name you use in ecmc plc-code)
        .funcName = "grbl_mc_resume",
        // Function description
        .funcDesc = "double grbl_mc_resume(<resume>) : Resume halted grbl motion at positive edge of <resume>",
        /**
        * 7 different prototypes allowed (only doubles since reg in plc).
        * Only funcArg${argCount} func shall be assigned the rest set to NULL.
        **/
        .funcArg0 = NULL,
        .funcArg1 = grbl_mc_resume,
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
  .funcs[3] =
      { /*----grbl_get_busy----*/
        // Function name (this is the name you use in ecmc plc-code)
        .funcName = "grbl_get_busy",
        // Function description
        .funcDesc = "double grbl_get_busy() :  Get grbl system busy (still executing motion code)",
        /**
        * 7 different prototypes allowed (only doubles since reg in plc).
        * Only funcArg${argCount} func shall be assigned the rest set to NULL.
        **/
        .funcArg0 = grbl_get_busy,
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
  .funcs[4] =
      { /*----grbl_get_parser_busy----*/
        // Function name (this is the name you use in ecmc plc-code)
        .funcName = "grbl_get_parser_busy",
        // Function description
        .funcDesc = "double grbl_get_parser_busy() :  Get g-code parser busy.",
        /**
        * 7 different prototypes allowed (only doubles since reg in plc).
        * Only funcArg${argCount} func shall be assigned the rest set to NULL.
        **/
        .funcArg0 = grbl_get_parser_busy,
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
  .funcs[5] =
      { /*----grbl_get_code_row_num----*/
        // Function name (this is the name you use in ecmc plc-code)
        .funcName = "grbl_get_code_row_num",
        // Function description
        .funcDesc = "double grbl_get_code_row_num() :  Get g-code row number currently preparing for exe.",
        /**
        * 7 different prototypes allowed (only doubles since reg in plc).
        * Only funcArg${argCount} func shall be assigned the rest set to NULL.
        **/
        .funcArg0 = grbl_get_code_row_num,
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
  .funcs[6] =
      { /*----grbl_get_error----*/
        // Function name (this is the name you use in ecmc plc-code)
        .funcName = "grbl_get_error",
        // Function description
        .funcDesc = "double grbl_get_error() :  Get error code.",
        /**
        * 7 different prototypes allowed (only doubles since reg in plc).
        * Only funcArg${argCount} func shall be assigned the rest set to NULL.
        **/
        .funcArg0 = grbl_get_error,
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
  .funcs[7] =
      { /*----grbl_reset_error----*/
        // Function name (this is the name you use in ecmc plc-code)
        .funcName = "grbl_reset_error",
        // Function description
        .funcDesc = "double grbl_reset_error() :  Reset error.",
        /**
        * 7 different prototypes allowed (only doubles since reg in plc).
        * Only funcArg${argCount} func shall be assigned the rest set to NULL.
        **/
        .funcArg0 = grbl_reset_error,
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
  .funcs[8] =
      { /*----grbl_get_all_enabled----*/
        // Function name (this is the name you use in ecmc plc-code)
        .funcName = "grbl_get_all_enabled",
        // Function description
        .funcDesc = "double grbl_get_all_enabled() :  Get all configured axes enabled.",
        /**
        * 7 different prototypes allowed (only doubles since reg in plc).
        * Only funcArg${argCount} func shall be assigned the rest set to NULL.
        **/
        .funcArg0 = grbl_get_all_enabled,
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
  .funcs[9] =
      { /*----grbl_set_all_enable----*/
        // Function name (this is the name you use in ecmc plc-code)
        .funcName = "grbl_set_all_enable",
        // Function description
        .funcDesc = "double grbl_set_all_enable(enable) : Set enable on all configured axes.",
        /**
        * 7 different prototypes allowed (only doubles since reg in plc).
        * Only funcArg${argCount} func shall be assigned the rest set to NULL.
        **/
        .funcArg0 = NULL,
        .funcArg1 = grbl_set_all_enable,
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

  .funcs[10] = {0},  // last element set all to zero..
  // PLC consts
  .consts[0] = {0}, // last element set all to zero..
};

ecmc_plugin_register(pluginDataDef);

# ifdef __cplusplus
}
# endif  // ifdef __cplusplus
