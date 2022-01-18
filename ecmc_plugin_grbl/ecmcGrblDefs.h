/*************************************************************************\
* Copyright (c) 2019 European Spallation Source ERIC
* ecmc is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
*
*  ecmcGrblDefs.h
*
*  Created on: Jan 12, 2022
*      Author: anderssandstrom
*
\*************************************************************************/

#ifndef ECMC_GRBL_DEFS_H_
#define ECMC_GRBL_DEFS_H_

#include "grbl.h"

// Options
#define ECMC_PLUGIN_DBG_PRINT_OPTION_CMD   "DBG_PRINT="
#define ECMC_PLUGIN_ASYN_PREFIX            "plugin.grbl"


system_t sys;
int32_t sys_position[N_AXIS];      // Real-time machine (aka home) position vector in steps.
int32_t sys_probe_position[N_AXIS]; // Last probe position in machine coordinates and steps.
volatile uint8_t sys_probe_state;   // Probing state value.  Used to coordinate the probing cycle with stepper ISR.
volatile uint8_t sys_rt_exec_state;   // Global realtime executor bitflag variable for state management. See EXEC bitmasks.
volatile uint8_t sys_rt_exec_alarm;   // Global realtime executor bitflag variable for setting various alarms.
volatile uint8_t sys_rt_exec_motion_override; // Global realtime executor bitflag variable for motion-based overrides.
volatile uint8_t sys_rt_exec_accessory_override; // Global realtime executor bitflag variable for spindle/coolant overrides.
#ifdef DEBUG
  volatile uint8_t sys_rt_exec_debug;
#endif




#endif  /* ECMC_GRBL_DEFS_H_ */
