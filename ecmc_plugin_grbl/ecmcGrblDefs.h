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

// Options
#define ECMC_PLUGIN_DBG_PRINT_OPTION_CMD "DBG_PRINT="
#define ECMC_PLUGIN_X_AXIS_ID_OPTION_CMD "X_AXIS="
#define ECMC_PLUGIN_Y_AXIS_ID_OPTION_CMD "Y_AXIS="
#define ECMC_PLUGIN_Z_AXIS_ID_OPTION_CMD "Z_AXIS="
#define ECMC_PLUGIN_SPINDLE_AXIS_ID_OPTION_CMD "SPINDLE_AXIS="
#define ECMC_PLUGIN_AUTO_ENABLE_AT_START_OPTION_CMD "AUTO_ENABLE="
#define ECMC_PLUGIN_AUTO_START_OPTION_CMD "AUTO_START="

#define ECMC_PLUGIN_ASYN_PREFIX          "plugin.grbl"
#define ECMC_CONFIG_FILE_COMMENT_CHAR    "#"

#define ECMC_PLUGIN_GRBL_GENERAL_ERROR_CODE 0x100
#define ECMC_PLUGIN_GRBL_COMMAND_ERROR_CODE 0x101
#define ECMC_PLUGIN_AXIS_AT_LIMIT_ERROR_CODE 0x102
#define ECMC_PLUGIN_LOAD_FILE_ERROR_CODE 0x103
#define ECMC_PLUGIN_LIMIT_SWITCH_VIOLATION_ERROR_CODE 0x104

#define ECMC_PLUGIN_GRBL_GRBL_STARTUP_STRING "for help]"
#define ECMC_PLUGIN_GRBL_GRBL_OK_STRING "ok"
#define ECMC_PLUGIN_GRBL_GRBL_ERR_STRING "error"

#endif  /* ECMC_GRBL_DEFS_H_ */
