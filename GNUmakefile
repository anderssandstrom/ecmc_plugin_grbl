include /ioc/tools/driver.makefile

MODULE = ecmc_plugin_grbl

BUILDCLASSES = Linux
ARCH_FILTER = deb10%

# Run 7.0.6 for now
EXCLUDE_VERSIONS+=3 7.0.5 7.0.7

IGNORE_MODULES += asynMotor
IGNORE_MODULES += motorBase

USR_CXXFLAGS += -std=c++17
OPT_CXXFLAGS_YES = -O3

# dependencies
ECmasterECMC_VERSION = v1.1.0
motorECMC_VERSION = 7.0.7-ESS
ecmc_VERSION = v9.0.1_RC1

################################################################################
# THIS RELATES TO THE EtherCAT MASTER LIBRARY
# IT IS OF PARAMOUNT IMPORTANCE TO LOAD THE PROPER KERNEL MODULE
# ################################################################################
USR_LDFLAGS += -lethercat

EC_MASTER_LIB = ${EPICS_MODULES}/ECmasterECMC/${ECmasterECMC_VERSION}/R${EPICSVERSION}/lib/${T_A}
USR_LDFLAGS += -Wl,-rpath=${EC_MASTER_LIB}
USR_LDFLAGS +=         -L ${EC_MASTER_LIB}

BASE_DIR = ecmc_plugin_motion
SRC_DIR = $(BASE_DIR)/src
DB_DIR =  $(BASE_DIR)/Db

APPSRC_GRBL := grbl
APPSRC_ECMC := ecmc_plugin_grbl

SOURCES+=$(APPSRC_GRBL)/grbl_motion_control.c
SOURCES+=$(APPSRC_GRBL)/grbl_gcode.c
SOURCES+=$(APPSRC_GRBL)/grbl_spindle_control.c
SOURCES+=$(APPSRC_GRBL)/grbl_coolant_control.c
SOURCES+=$(APPSRC_GRBL)/grbl_serial.c 
SOURCES+=$(APPSRC_GRBL)/grbl_protocol.c
SOURCES+=$(APPSRC_GRBL)/grbl_stepper.c
SOURCES+=$(APPSRC_GRBL)/grbl_eeprom.c
SOURCES+=$(APPSRC_GRBL)/grbl_settings.c
SOURCES+=$(APPSRC_GRBL)/grbl_planner.c
SOURCES+=$(APPSRC_GRBL)/grbl_nuts_bolts.c
SOURCES+=$(APPSRC_GRBL)/grbl_limits.c
SOURCES+=$(APPSRC_GRBL)/grbl_jog.c
SOURCES+=$(APPSRC_GRBL)/grbl_print.c
SOURCES+=$(APPSRC_GRBL)/grbl_probe.c
SOURCES+=$(APPSRC_GRBL)/grbl_report.c
SOURCES+=$(APPSRC_GRBL)/grbl_system.c

SOURCES+=$(APPSRC_ECMC)/ecmcPluginGrbl.c
SOURCES+=$(APPSRC_ECMC)/ecmcGrbl.cpp
SOURCES+=$(APPSRC_ECMC)/ecmcGrblWrap.cpp

DBDS   += $(APPSRC_ECMC)/ecmcGrbl.dbd

#SOURCES += $(foreach d,${SRC_DIR}, $(wildcard $d/*.c) $(wildcard $d/*.cpp))
HEADERS += $(foreach d,${SRC_DIR}, $(wildcard $d/*.h))
DBDS += $(foreach d,${SRC_DIR}, $(wildcard $d/*.dbd))
#SCRIPTS += $(BASE_DIR)/startup.cmd
#SCRIPTS += $(BASE_DIR)/addMotionObj.cmd
TEMPLATES += $(wildcard $(DB_DIR)/*.template)

