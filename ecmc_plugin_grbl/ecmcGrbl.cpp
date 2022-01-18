/*************************************************************************\
* Copyright (c) 2019 European Spallation Source ERIC
* ecmc is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
*
*  ecmcSocketCAN.cpp
*
*  Created on: Mar 22, 2020
*      Author: anderssandstrom
*      Credits to  https://github.com/sgreg/dynamic-loading 
*
\*************************************************************************/

// Needed to get headers in ecmc right...
#define ECMC_IS_PLUGIN

#include <sstream>
#include "ecmcSocketCAN.h"
#include "ecmcPluginClient.h"
#include "ecmcAsynPortDriver.h"
#include "ecmcAsynPortDriverUtils.h"
#include "epicsThread.h"

// Start worker for socket read()
void f_worker_read(void *obj) {
  if(!obj) {
    printf("%s/%s:%d: Error: Worker read thread ecmcSocketCAN object NULL..\n",
            __FILE__, __FUNCTION__, __LINE__);
    return;
  }
  ecmcSocketCAN * canObj = (ecmcSocketCAN*)obj;
  canObj->doReadWorker();
}

// Start worker for socket connect()
void f_worker_connect(void *obj) {
  if(!obj) {
    printf("%s/%s:%d: Error: Worker connect thread ecmcSocketCAN object NULL..\n",
            __FILE__, __FUNCTION__, __LINE__);
    return;
  }
  ecmcSocketCAN * canObj = (ecmcSocketCAN*)obj;
  canObj->doConnectWorker();
}

/** ecmc ecmcSocketCAN class
 * This object can throw: 
 *    - bad_alloc
 *    - invalid_argument
 *    - runtime_error
*/
ecmcSocketCAN::ecmcSocketCAN(char* configStr,
                             char* portName,
                             int exeSampleTimeMs) {
  // Init
  cfgCanIFStr_    = NULL;
  cfgDbgMode_     = 0;
  cfgAutoConnect_ = 1;
  destructs_      = 0;
  socketId_       = -1;
  connected_      = 0;
  writeBuffer_    = NULL;
  deviceCounter_  = 0;
  refreshNeeded_  = 0;
  errorCode_      = 0;
  masterDev_      = NULL;
  for(int i = 0; i<ECMC_CAN_MAX_DEVICES;i++) {
    devices_[i] = NULL;
  }

  exeSampleTimeMs_ = exeSampleTimeMs;
  
  memset(&ifr_,0,sizeof(struct ifreq));
  memset(&rxmsg_,0,sizeof(struct can_frame));
  memset(&addr_,0,sizeof(struct sockaddr_can));

  parseConfigStr(configStr); // Assigns all configs
  // Check valid nfft
  if(!cfgCanIFStr_ ) {
    throw std::out_of_range("CAN inteface must be defined (can0, vcan0...).");
  }

  // Create worker thread for reading socket
  std::string threadname = "ecmc." ECMC_PLUGIN_ASYN_PREFIX".read";
  if(epicsThreadCreate(threadname.c_str(), 0, 32768, f_worker_read, this) == NULL) {
    throw std::runtime_error("Error: Failed create worker thread for read().");
  }

  // Create worker thread for connecting socket
  threadname = "ecmc." ECMC_PLUGIN_ASYN_PREFIX".connect";
  if(epicsThreadCreate(threadname.c_str(), 0, 32768, f_worker_connect, this) == NULL) {
    throw std::runtime_error("Error: Failed create worker thread for connect().");
  }

  if(cfgAutoConnect_) {
    connectPrivate();
  }
  writeBuffer_ = new ecmcSocketCANWriteBuffer(socketId_, cfgDbgMode_);
  initAsyn();
}

ecmcSocketCAN::~ecmcSocketCAN() {
  // kill worker
  destructs_ = 1;  // maybe need todo in other way..
  doWriteEvent_.signal();
  doConnectEvent_.signal();

  for(int i = 0; i<ECMC_CAN_MAX_DEVICES;i++) {
    delete devices_[i];
  }

}

void ecmcSocketCAN::parseConfigStr(char *configStr) {

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
      
      // ECMC_PLUGIN_CONNECT_OPTION_CMD (1/0)
      if (!strncmp(pThisOption, ECMC_PLUGIN_CONNECT_OPTION_CMD, strlen(ECMC_PLUGIN_CONNECT_OPTION_CMD))) {
        pThisOption += strlen(ECMC_PLUGIN_DBG_PRINT_OPTION_CMD);
        cfgAutoConnect_ = atoi(pThisOption);
      }

      // ECMC_PLUGIN_IF_OPTION_CMD (Source string)
      else if (!strncmp(pThisOption, ECMC_PLUGIN_IF_OPTION_CMD, strlen(ECMC_PLUGIN_IF_OPTION_CMD))) {
        pThisOption += strlen(ECMC_PLUGIN_IF_OPTION_CMD);
        cfgCanIFStr_=strdup(pThisOption);
      }

      pThisOption = pNextOption;
    }    
    free(pOptions);
  }
  if(!cfgCanIFStr_) { 
    throw std::invalid_argument( "CAN interface not defined.");
  }
}

// For connect commands over asyn or plc. let worker connect
void ecmcSocketCAN::connectExternal() {
  if(!connected_) {
    doConnectEvent_.signal(); // let worker start
  }
}

void ecmcSocketCAN::connectPrivate() {

	if((socketId_ = socket(PF_CAN, SOCK_RAW, CAN_RAW)) == -1) {
    throw std::runtime_error( "Error while opening socket.");		
		return;
	}

	strcpy(ifr_.ifr_name, cfgCanIFStr_);
	ioctl(socketId_, SIOCGIFINDEX, &ifr_);
	
	addr_.can_family  = AF_CAN;
	addr_.can_ifindex = ifr_.ifr_ifindex;

	printf("%s at index %d\n", cfgCanIFStr_, ifr_.ifr_ifindex);

	if(bind(socketId_, (struct sockaddr *)&addr_, sizeof(addr_)) == -1) {		
    throw std::runtime_error( "Error in socket bind.");
    return;
	}
  connected_ = 1;
}

int ecmcSocketCAN::getConnected() {
  return connected_;
}

// Read socket worker
void ecmcSocketCAN::doReadWorker() {

  while(true) {
    
    if(destructs_) {
      break;
    }

    // Wait for new CAN frame 
    int bytes = read(socketId_, &rxmsg_, sizeof(rxmsg_));
    if(bytes == -1) {
      errorCode_ = errno;      
      printf("ecmcSocketCAN: read() fail with error %s.\n", strerror(errno));      
      refreshNeeded_ = 1;
      continue;
    }

    // forward all data to devices (including master)
    for(int i = 0; i < deviceCounter_; i++){
      devices_[i]->newRxFrame(&rxmsg_);
    }

    if(cfgDbgMode_) {
      // Simulate candump printout
      printf("r 0x%03X", rxmsg_.can_id);
      printf(" [%d]", rxmsg_.can_dlc);
      for(int i=0; i<rxmsg_.can_dlc; i++ ) {
        printf(" 0x%02X", rxmsg_.data[i]);
      }
      printf("\n");
    }
  }
}

// Connect socket worker
void ecmcSocketCAN::doConnectWorker() {

  while(true) {
    
    if(destructs_) {
      return;
    }
    doConnectEvent_.wait();
    if(destructs_) {
      return;
    }
    connectPrivate();
  }
}

int ecmcSocketCAN::getlastWritesError() {
  if(!writeBuffer_) { 
    return ECMC_CAN_ERROR_WRITE_BUFFER_NULL;
  }
  return writeBuffer_->getlastWritesErrorAndReset();
}

int ecmcSocketCAN::addWriteCAN(uint32_t canId,
                               uint8_t len,
                               uint8_t data0,
                               uint8_t data1,
                               uint8_t data2,
                               uint8_t data3,
                               uint8_t data4,
                               uint8_t data5,
                               uint8_t data6,
                               uint8_t data7) {

  if(!writeBuffer_) { 
    return ECMC_CAN_ERROR_WRITE_BUFFER_NULL;
  }

  writeBuffer_->addWriteCAN(canId,
                            len,
                            data0,
                            data1,
                            data2,
                            data3,
                            data4,
                            data5,
                            data6,
                            data7);
  return 0;
}
  
void  ecmcSocketCAN::execute() {

  for(int i = 0; i < deviceCounter_; i++){
    devices_[i]->execute();
  }

  int writeError=getlastWritesError();
  if (writeError) {
    errorCode_ = writeError;
    refreshNeeded_ = 1;
  }
  refreshAsynParams();
  return;
}

// Avoid issues with std:to_string()
std::string ecmcSocketCAN::to_string(int value) {
  std::ostringstream os;
  os << value;
  return os.str();
}

void ecmcSocketCAN::addMaster(uint32_t nodeId,
                              const char* name,
                              int lssSampleTimeMs,
                              int syncSampleTimeMs,
                              int heartSampleTimeMs) {

  if(masterDev_) {
   throw std::runtime_error("Master already added.");
  }
  if(deviceCounter_ >= ECMC_CAN_MAX_DEVICES) {
    throw std::out_of_range("Device array full.");
  }
  if(nodeId >= 128) {
    throw std::out_of_range("Node id out of range.");
  }

  if(lssSampleTimeMs <= 0) {
    throw std::out_of_range("LSS sample time ms out of range.");
  }

  if(syncSampleTimeMs <= 0) {
    throw std::out_of_range("Sync sample time ms out of range.");
  }

  if(heartSampleTimeMs <= 0) {
    throw std::out_of_range("Heart sample time ms out of range.");
  }

  masterDev_ = new ecmcCANOpenMaster(writeBuffer_,
                                     nodeId,
                                     exeSampleTimeMs_,
                                     lssSampleTimeMs,
                                     syncSampleTimeMs,
                                     heartSampleTimeMs,
                                     name,
                                     cfgDbgMode_);
  // add as a normal device also for execute and rxframe
  devices_[deviceCounter_] = masterDev_;
  deviceCounter_++;
}

void ecmcSocketCAN::addDevice(uint32_t nodeId,
                              const char* name,
                              int heartTimeoutMs){
  if(deviceCounter_ >= ECMC_CAN_MAX_DEVICES) {
    throw std::out_of_range("Device array full.");
  }
  if(nodeId >= 128) {
    throw std::out_of_range("Node id out of range.");
  }

  devices_[deviceCounter_] = new ecmcCANOpenDevice(writeBuffer_,nodeId,exeSampleTimeMs_,name,heartTimeoutMs,cfgDbgMode_);  
  deviceCounter_++;
}

int ecmcSocketCAN::findDeviceWithNodeId(uint32_t nodeId) {
  for(int i=0; i < deviceCounter_;i++) {
    if(devices_[i]) {
      if(devices_[i]->getNodeId() == nodeId) {
         return i;
      }
    }
  }
  return -1;
}

void ecmcSocketCAN::addPDO(uint32_t nodeId,
                           uint32_t cobId,
                           ecmc_can_direction rw,
                           uint32_t ODSize,
                           int readTimeoutMs,
                           int writeCycleMs,    //if <0 then write on demand.
                           const char* name) {
  int devId = findDeviceWithNodeId(nodeId);
  if(devId < 0) {
    throw std::out_of_range("Node id not found in any configured device.");
  }

  int errorCode = devices_[devId]->addPDO(cobId,
                                          rw,
                                          ODSize,
                                          readTimeoutMs,
                                          writeCycleMs,
                                          name);
  if(errorCode > 0) {
    throw std::runtime_error("AddPDO() failed.");
  }
}
              
void ecmcSocketCAN::addSDO(uint32_t nodeId,
                           uint32_t cobIdTx,    // 0x580 + CobId
                           uint32_t cobIdRx,    // 0x600 + Cobid
                           ecmc_can_direction rw,
                           uint16_t ODIndex,    // Object dictionary index
                           uint8_t ODSubIndex,  // Object dictionary subindex
                           uint32_t ODSize,
                           int readSampleTimeMs,
                           const char* name) {

  int devId = findDeviceWithNodeId(nodeId);
  if(devId < 0) {
    throw std::out_of_range("Node id not found in any configured device.");
  }

  int errorCode = devices_[devId]->addSDO(cobIdTx,
                                          cobIdRx,
                                          rw,
                                          ODIndex,
                                          ODSubIndex,
                                          ODSize,
                                          readSampleTimeMs,
                                          name);
  if(errorCode > 0) {
    throw std::runtime_error("AddSDO() failed.");
  }
}

void ecmcSocketCAN::initAsyn() {

  ecmcAsynPortDriver *ecmcAsynPort = (ecmcAsynPortDriver *)getEcmcAsynPortDriver();
  if(!ecmcAsynPort) {
    printf("ERROR: ecmcAsynPort NULL.");
    throw std::runtime_error( "ERROR: ecmcAsynPort NULL." );
  }
 
  // Add resultdata "plugin.can.read.error"
  std::string paramName = ECMC_PLUGIN_ASYN_PREFIX + std::string(".read.error");

  errorParam_ = ecmcAsynPort->addNewAvailParam(
                                          paramName.c_str(),     // name
                                          asynParamInt32,        // asyn type 
                                          (uint8_t*)&errorCode_, // pointer to data
                                          sizeof(errorCode_),    // size of data
                                          ECMC_EC_U32,           // ecmc data type
                                          0);                    // die if fail

  if(!errorParam_) {
    printf("ERROR: Failed create asyn param for data.");
    throw std::runtime_error( "ERROR: Failed create asyn param for: " + paramName);
  }
  errorParam_->setAllowWriteToEcmc(false);  // need to callback here
  errorParam_->refreshParam(1); // read once into asyn param lib
  ecmcAsynPort->callParamCallbacks(ECMC_ASYN_DEFAULT_LIST, ECMC_ASYN_DEFAULT_ADDR);

  // Add resultdata "plugin.can.read.connected"
  paramName = ECMC_PLUGIN_ASYN_PREFIX + std::string(".read.connected");

  connectedParam_ = ecmcAsynPort->addNewAvailParam(
                                          paramName.c_str(),     // name
                                          asynParamInt32,        // asyn type 
                                          (uint8_t*)&connected_, // pointer to data
                                          sizeof(connected_),    // size of data
                                          ECMC_EC_U32,           // ecmc data type
                                          0);                    // die if fail

  if(!connectedParam_) {
    printf("ERROR: Failed create asyn param for connected.");
    throw std::runtime_error( "ERROR: Failed create asyn param for: " + paramName);
  }
  connectedParam_->setAllowWriteToEcmc(false);  // need to callback here
  connectedParam_->refreshParam(1); // read once into asyn param lib
  ecmcAsynPort->callParamCallbacks(ECMC_ASYN_DEFAULT_LIST, ECMC_ASYN_DEFAULT_ADDR); 
}

// only refresh from "execute" thread
void ecmcSocketCAN::refreshAsynParams() {
  if(refreshNeeded_) {
    connectedParam_->refreshParamRT(1); // read once into asyn param lib
    errorParam_->refreshParamRT(1); // read once into asyn param lib
  }
  refreshNeeded_ = 0;
}