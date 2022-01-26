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

#include "ecmcGrblDefs.h"

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
int createGrbl(char *configStr, int exeSampleTimeMs);

/** \brief prepare for RT\n
  */
int enterRT();

/** \brief execute from rt loop\n
  */
int realtime(int ecmcError);

// Delete object
void deleteGrbl();

# ifdef __cplusplus
}
# endif  // ifdef __cplusplus

#endif  /* ECMC_GRBL_WRAP_H_ */
