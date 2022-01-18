/*************************************************************************\
* Copyright (c) 2019 European Spallation Source ERIC
* ecmc is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
*
*  ecmcGrblWrap.h
*
*  Created on: Jan 12, 2022
*      Author: anderssandstrom
*
\*************************************************************************/
#ifndef ECMC_GRBL_WRAP_H_
#define ECMC_GRBL_WRAP_H_
#include "ecmcSocketCANDefs.h"

# ifdef __cplusplus
extern "C" {
# endif  // ifdef __cplusplus

/** \brief Create new SocketCAN object
 *
 *  The configuration string needs to define tha can interface by:\n
 *  "IF=<data source>;"\n
 *  Example:\n
 *  "IF=can0";\n
 *  \param[in] configStr Configuration string.\n
 *
 *  \return 0 if success or otherwise an error code.\n
 */
int createSocketCAN(char *configStr, int exeSampleTimeMs);

/** \brief Connect to SocketCAN interface\n
 */

int connectSocketCAN();

/** \brief Connected to can interface\n
 */
int getSocketCANConnectd();

/** \brief Get last error from writes\n
  */
int getlastWritesError();

/** \brief execute from rt loop\n
  */
int execute();

/** \brief add CAN frame to write buffer
 */
int  addWriteSocketCAN( double canId,
                        double len,
                        double data0,
                        double data1,
                        double data2,
                        double data3,
                        double data4,
                        double data5,
                        double data6,
                        double data7);

/** \brief Delete SocketCAN object\n
 *
 * Should be called when destructs.\n
 */
void deleteSocketCAN();

# ifdef __cplusplus
}
# endif  // ifdef __cplusplus

#endif  /* ECMC_GRBL_WRAP_H_ */
