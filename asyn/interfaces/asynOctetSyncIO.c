/*asynOctetSyncIO.c*/
/***********************************************************************
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory, and the Regents of the University of
* California, as Operator of Los Alamos National Laboratory, and
* Berliner Elektronenspeicherring-Gesellschaft m.b.H. (BESSY).
* asynDriver is distributed subject to a Software License Agreement
* found in file LICENSE that is included with this distribution.
***********************************************************************/
/*
 * This package provide a simple, synchronous interface to the "asyn" package. 
 * It will work with any device that asyn supports, e.g. serial, GPIB, and 
 * TCP/IP sockets.  It hides the details of asyn.  It is intended for use 
 * by applications which do simple synchronous reads, writes and write/reads.  
 * It does not support port-specific control, e.g. baud rates, 
 * GPIB Universal Commands, etc.
 *
 * Author:  Mark Rivers
 * Created: March 20, 2004
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cantProceed.h>

#define epicsExportSharedSymbols
#include <shareLib.h>
#include "asynDriver.h"
#include "asynOctet.h"
#include "asynDrvUser.h"
#include "drvAsynIPPort.h"
#include "asynOctetSyncIO.h"

typedef struct ioPvt {
   asynCommon   *pasynCommon;
   void         *pcommonPvt;
   asynOctet    *pasynOctet;
   void         *octetPvt;
   asynDrvUser  *pasynDrvUser;
   void         *drvUserPvt;
} ioPvt;

/*asynOctetSyncIO methods*/
static asynStatus connect(const char *port, int addr,
                               asynUser **ppasynUser, const char *drvInfo);
static asynStatus disconnect(asynUser *pasynUser);
static asynStatus openSocket(const char *server, int port,
                                         char **portName);
static asynStatus writeIt(asynUser *pasynUser,
    char const *buffer, size_t buffer_len, double timeout,size_t *nbytesTransfered);
static asynStatus writeRaw(asynUser *pasynUser,
    char const *buffer, size_t buffer_len, double timeout,size_t *nbytesTransfered);
static asynStatus readIt(asynUser *pasynUser, char *buffer, size_t buffer_len, 
                   double timeout, size_t *nbytesTransfered,int *eomReason);
static asynStatus readRaw(asynUser *pasynUser, char *buffer, size_t buffer_len, 
                   double timeout, size_t *nbytesTransfered,int *eomReason);
static asynStatus writeRead(asynUser *pasynUser, 
                        const char *write_buffer, size_t write_buffer_len,
                        char *read_buffer, size_t read_buffer_len,
                        double timeout,
                        size_t *nbytesOut, size_t *nbytesIn,int *eomReason);
static asynStatus flushIt(asynUser *pasynUser);
static asynStatus setInputEos(asynUser *pasynUser,
                     const char *eos,int eoslen);
static asynStatus getInputEos(asynUser *pasynUser,
                     char *eos, int eossize, int *eoslen);
static asynStatus setOutputEos(asynUser *pasynUser,
                     const char *eos,int eoslen);
static asynStatus getOutputEos(asynUser *pasynUser,
                     char *eos, int eossize, int *eoslen);
static asynStatus writeOnce(const char *port, int addr,
                    char const *buffer, size_t buffer_len, double timeout,
                    size_t *nbytesTransfered, const char *drvInfo);
static asynStatus writeRawOnce(const char *port, int addr,
                    char const *buffer, size_t buffer_len, double timeout,
                    size_t *nbytesTransfered, const char *drvInfo);
static asynStatus readOnce(const char *port, int addr,
                   char *buffer, size_t buffer_len, 
                   double timeout,
                   size_t *nbytesTransfered,int *eomReason, const char *drvInfo);
static asynStatus readRawOnce(const char *port, int addr,
                   char *buffer, size_t buffer_len, 
                   double timeout,
                   size_t *nbytesTransfered,int *eomReason, const char *drvInfo);
static asynStatus writeReadOnce(const char *port, int addr,
                        const char *write_buffer, size_t write_buffer_len,
                        char *read_buffer, size_t read_buffer_len,
                        double timeout,
                        size_t *nbytesOut, size_t *nbytesIn,int *eomReason,
                        const char *drvInfo);
static asynStatus flushOnce(const char *port, int addr,const char *drvInfo);
static asynStatus setInputEosOnce(const char *port, int addr,
                        const char *eos,int eoslen,const char *drvInfo);
static asynStatus getInputEosOnce(const char *port, int addr,
                        char *eos, int eossize, int *eoslen,const char *drvInfo);
static asynStatus setOutputEosOnce(const char *port, int addr,
                        const char *eos,int eoslen,const char *drvInfo);
static asynStatus getOutputEosOnce(const char *port, int addr,
                        char *eos, int eossize, int *eoslen,const char *drvInfo);

static asynOctetSyncIO asynOctetSyncIOManager = {
    connect,
    disconnect,
    openSocket,
    writeIt,
    writeRaw,
    readIt,
    readRaw,
    writeRead,
    flushIt,
    setInputEos,
    getInputEos,
    setOutputEos,
    getOutputEos,
    writeOnce,
    writeRawOnce,
    readOnce,
    readRawOnce,
    writeReadOnce,
    flushOnce,
    setInputEosOnce,
    getInputEosOnce,
    setOutputEosOnce,
    getOutputEosOnce
};
epicsShareDef asynOctetSyncIO *pasynOctetSyncIO = &asynOctetSyncIOManager;

static asynStatus connect(const char *port, int addr,
           asynUser **ppasynUser,const char *drvInfo)
{
    ioPvt         *pioPvt;
    asynUser      *pasynUser;
    asynStatus    status;
    asynInterface *pasynInterface;

    pioPvt = (ioPvt *)callocMustSucceed(1, sizeof(ioPvt),"asynOctetSyncIO");
    pasynUser = pasynManager->createAsynUser(0,0);
    pasynUser->userPvt = pioPvt;
    *ppasynUser = pasynUser;
    status = pasynManager->connectDevice(pasynUser, port, addr);
    if (status != asynSuccess) {
        return(status);
    }
    pasynInterface = pasynManager->findInterface(pasynUser, asynCommonType, 1);
    if (!pasynInterface) {
       epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
           "%s interface not supported\n", asynCommonType);
       return asynError;
    }
    pioPvt->pasynCommon = (asynCommon *)pasynInterface->pinterface;
    pioPvt->pcommonPvt = pasynInterface->drvPvt;
    pasynInterface = pasynManager->findInterface(pasynUser, asynOctetType, 1);
    if (!pasynInterface) {
       epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
           "%s interface not supported\n", asynOctetType);
       return asynError;
    }
    pioPvt->pasynOctet = (asynOctet *)pasynInterface->pinterface;
    pioPvt->octetPvt = pasynInterface->drvPvt;
    if(drvInfo) {
        /* Check for asynDrvUser interface */
        pasynInterface = pasynManager->findInterface(pasynUser,asynDrvUserType,1);
        if(pasynInterface) {
            asynDrvUser *pasynDrvUser;
            void       *drvPvt;
            pasynDrvUser = (asynDrvUser *)pasynInterface->pinterface;
            drvPvt = pasynInterface->drvPvt;
            status = pasynDrvUser->create(drvPvt,pasynUser,drvInfo,0,0);
            if(status==asynSuccess) {
                pioPvt->pasynDrvUser = pasynDrvUser;
                pioPvt->drvUserPvt = drvPvt;
            } else {
                asynPrint(pasynUser,ASYN_TRACE_ERROR,
                    "pasynDrvUser->create drvInfo %s error %s\n",
                         drvInfo, pasynUser->errorMessage);
            }
        }
    }
    return asynSuccess ;
}

static asynStatus disconnect(asynUser *pasynUser)
{
    ioPvt      *pioPvt = (ioPvt *)pasynUser->userPvt;
    asynStatus status;

    if(pioPvt->pasynDrvUser) {
        status = pioPvt->pasynDrvUser->destroy(pioPvt->drvUserPvt,pasynUser);
        if(status!=asynSuccess) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "asynOctetSyncIO pasynDrvUser->destroy failed %s\n",
                pasynUser->errorMessage);
            return status;
        }
    }
    status = pasynManager->freeAsynUser(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynOctetSyncIO freeAsynUser failed %s\n",
            pasynUser->errorMessage);
        return status;
    }
    free(pioPvt);
    return asynSuccess;
}

static asynStatus openSocket(const char *server, int port,
       char **portName)
{
    char portString[20];
    asynStatus status;

    sprintf(portString, "%d", port);
    *portName = calloc(1, strlen(server)+strlen(portString)+3);
    strcpy(*portName, server);
    strcat(*portName, ":");
    strcat(*portName, portString);
    status = drvAsynIPPortConfigure(*portName, *portName, 0, 0, 0);
    return(status);
}

static asynStatus writeIt(asynUser *pasynUser,
    char const *buffer, size_t buffer_len, double timeout,size_t *nbytesTransfered)
{
    asynStatus status;
    ioPvt      *pioPvt = (ioPvt *)pasynUser->userPvt;

    pasynUser->timeout = timeout;
    status = pasynManager->lockPort(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynOctetSyncIO lockPort failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = pioPvt->pasynOctet->write(
        pioPvt->octetPvt,pasynUser,buffer,buffer_len,nbytesTransfered);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO write failed %s\n",pasynUser->errorMessage);
    } else if(*nbytesTransfered!=buffer_len) {
         asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO write output_len %d but nbytesTransfered %d\n",
             buffer_len,*nbytesTransfered);
    } else {
         asynPrintIO(pasynUser, ASYN_TRACEIO_DEVICE,
             buffer,buffer_len,"asynOctetSyncIO wrote:\n");
    }
    if((pasynManager->unlockPort(pasynUser)) ) {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,
            "unlockPort error %s\n", pasynUser->errorMessage);
    }
    return status;
}

static asynStatus writeRaw(asynUser *pasynUser,
    char const *buffer, size_t buffer_len, double timeout,size_t *nbytesTransfered)
{
    asynStatus status;
    ioPvt      *pioPvt = (ioPvt *)pasynUser->userPvt;

    pasynUser->timeout = timeout;
    status = pasynManager->lockPort(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynOctetSyncIO lockPort failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = pioPvt->pasynOctet->writeRaw(
        pioPvt->octetPvt,pasynUser,buffer,buffer_len,nbytesTransfered);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO writeRaw failed %s\n",pasynUser->errorMessage);
    } else if(*nbytesTransfered!=buffer_len) {
         asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO write output_len %d but nbytesTransfered %d\n",
             buffer_len,*nbytesTransfered);
    } else {
         asynPrintIO(pasynUser, ASYN_TRACEIO_DEVICE,
             buffer,buffer_len,"asynOctetSyncIO wrote:\n");
    }
    if((pasynManager->unlockPort(pasynUser)) ) {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,
            "unlockPort error %s\n", pasynUser->errorMessage);
    }
    return status;
}

static asynStatus readIt(asynUser *pasynUser,
                   char *buffer, size_t buffer_len, 
                   double timeout,
                   size_t *nbytesTransfered,int *eomReason)
{
    asynStatus status;
    ioPvt      *pioPvt = (ioPvt *)pasynUser->userPvt;

    pasynUser->timeout = timeout;
    status = pasynManager->lockPort(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynOctetSyncIO lockPort failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = pioPvt->pasynOctet->read(
        pioPvt->octetPvt,pasynUser,buffer,buffer_len,nbytesTransfered,eomReason);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO read failed %s\n",pasynUser->errorMessage);
    } else {
         asynPrintIO(pasynUser, ASYN_TRACEIO_DEVICE,
             buffer,*nbytesTransfered,"asynOctetSyncIO read:\n");
    }
    if((pasynManager->unlockPort(pasynUser)) ) {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,
            "unlockPort error %s\n", pasynUser->errorMessage);
    }
    return status;
}

static asynStatus readRaw(asynUser *pasynUser,
                   char *buffer, size_t buffer_len, 
                   double timeout,
                   size_t *nbytesTransfered,int *eomReason)
{
    asynStatus status;
    ioPvt      *pioPvt = (ioPvt *)pasynUser->userPvt;

    pasynUser->timeout = timeout;
    status = pasynManager->lockPort(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynOctetSyncIO lockPort failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = pioPvt->pasynOctet->readRaw(
        pioPvt->octetPvt,pasynUser,buffer,buffer_len,nbytesTransfered,eomReason);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO readRaw failed %s\n",pasynUser->errorMessage);
    } else {
         asynPrintIO(pasynUser, ASYN_TRACEIO_DEVICE,
             buffer,*nbytesTransfered,"asynOctetSyncIO read:\n");
    }
    if((pasynManager->unlockPort(pasynUser)) ) {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,
            "unlockPort error %s\n", pasynUser->errorMessage);
    }
    return status;
}

static asynStatus writeRead(asynUser *pasynUser, 
                        const char *write_buffer, size_t write_buffer_len,
                        char *read_buffer, size_t read_buffer_len,
                        double timeout,
                        size_t *nbytesOut, size_t *nbytesIn,int *eomReason)
{
    asynStatus status;
    ioPvt      *pioPvt = (ioPvt *)pasynUser->userPvt;

    pasynUser->timeout = timeout;
    status = pasynManager->lockPort(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynOctetSyncIO lockPort failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = pioPvt->pasynOctet->flush(pioPvt->octetPvt,pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynOctetSyncIO flush failed %s\n",pasynUser->errorMessage);
    }
    status = pioPvt->pasynOctet->write(
        pioPvt->octetPvt,pasynUser,write_buffer,write_buffer_len,nbytesOut);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO write failed %s\n",pasynUser->errorMessage);
    } else if(*nbytesOut!=write_buffer_len) {
         asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO write output_len %d but nbytesTransfered %d\n",
             write_buffer_len,*nbytesOut);
    } else {
         asynPrintIO(pasynUser, ASYN_TRACEIO_DEVICE,
             write_buffer,*nbytesOut,"asynOctetSyncIO wrote:\n");
    }
    status = pioPvt->pasynOctet->read(
        pioPvt->octetPvt,pasynUser,read_buffer,read_buffer_len,nbytesIn,eomReason);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO read failed %s\n",pasynUser->errorMessage);
    } else {
         asynPrintIO(pasynUser, ASYN_TRACEIO_DEVICE,
             read_buffer,*nbytesIn,"asynOctetSyncIO read:\n");
    }
    if((pasynManager->unlockPort(pasynUser)) ) {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,
            "unlockPort error %s\n", pasynUser->errorMessage);
    }
    return status;
}

static asynStatus
    flushIt(asynUser *pasynUser)
{
    asynStatus status;
    ioPvt      *pioPvt = (ioPvt *)pasynUser->userPvt;

    pasynUser->timeout = 1.0;
    status = pasynManager->lockPort(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynOctetSyncIO lockPort failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = pioPvt->pasynOctet->flush(pioPvt->octetPvt,pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO readRaw failed %s\n",pasynUser->errorMessage);
    } else {
         asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,"asynOctetSyncIO flush\n");
    }
    if((pasynManager->unlockPort(pasynUser)) ) {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,
            "unlockPort error %s\n", pasynUser->errorMessage);
    }
    return status;
}

static asynStatus setInputEos(asynUser *pasynUser,
                     const char *eos,int eoslen)
{
    asynStatus status;
    ioPvt      *pioPvt = (ioPvt *)pasynUser->userPvt;

    pasynUser->timeout = 1.0;
    status = pasynManager->lockPort(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynOctetSyncIO lockPort failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = pioPvt->pasynOctet->setInputEos(
        pioPvt->octetPvt,pasynUser,eos,eoslen);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO readRaw failed %s\n",pasynUser->errorMessage);
    } else {
         asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
             "asynOctetSyncIO setInputEos eoslen %d\n",eoslen);
    }
    if((pasynManager->unlockPort(pasynUser)) ) {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,
            "unlockPort error %s\n", pasynUser->errorMessage);
    }
    return status;
}

static asynStatus getInputEos(asynUser *pasynUser,
                     char *eos, int eossize, int *eoslen)
{
    asynStatus status;
    ioPvt      *pioPvt = (ioPvt *)pasynUser->userPvt;

    pasynUser->timeout = 1.0;
    status = pasynManager->lockPort(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynOctetSyncIO lockPort failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = pioPvt->pasynOctet->getInputEos(
        pioPvt->octetPvt,pasynUser,eos,eossize,eoslen);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO readRaw failed %s\n",pasynUser->errorMessage);
    } else {
         asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
             "asynOctetSyncIO setInputEos eoslen %d\n",eoslen);
    }
    if((pasynManager->unlockPort(pasynUser)) ) {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,
            "unlockPort error %s\n", pasynUser->errorMessage);
    }
    return status;
}

static asynStatus setOutputEos(asynUser *pasynUser,
                     const char *eos,int eoslen)
{
    asynStatus status;
    ioPvt      *pioPvt = (ioPvt *)pasynUser->userPvt;

    pasynUser->timeout = 1.0;
    status = pasynManager->lockPort(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynOctetSyncIO lockPort failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = pioPvt->pasynOctet->setOutputEos(
        pioPvt->octetPvt,pasynUser,eos,eoslen);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO readRaw failed %s\n",pasynUser->errorMessage);
    } else {
         asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
             "asynOctetSyncIO setOutputEos eoslen %d\n",eoslen);
    }
    if((pasynManager->unlockPort(pasynUser)) ) {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,
            "unlockPort error %s\n", pasynUser->errorMessage);
    }
    return status;
}

static asynStatus getOutputEos(asynUser *pasynUser,
                     char *eos, int eossize, int *eoslen)
{
    asynStatus status;
    ioPvt      *pioPvt = (ioPvt *)pasynUser->userPvt;

    pasynUser->timeout = 1.0;
    status = pasynManager->lockPort(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynOctetSyncIO lockPort failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = pioPvt->pasynOctet->getOutputEos(
        pioPvt->octetPvt,pasynUser,eos,eossize,eoslen);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO readRaw failed %s\n",pasynUser->errorMessage);
    } else {
         asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
             "asynOctetSyncIO setOutputEos eoslen %d\n",eoslen);
    }
    if((pasynManager->unlockPort(pasynUser)) ) {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,
            "unlockPort error %s\n", pasynUser->errorMessage);
    }
    return status;
}

static asynStatus writeOnce(const char *port, int addr,
                    char const *buffer, size_t buffer_len, double timeout,
                    size_t *nbytesTransfered,const char *drvInfo)
{
    asynStatus status;
    asynUser   *pasynUser;

    status = connect(port,addr,&pasynUser,drvInfo);
    if(status!=asynSuccess) {
        printf("asynOctetSyncIO connect failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = writeIt(pasynUser,buffer,buffer_len,timeout,nbytesTransfered);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO write failed %s\n",pasynUser->errorMessage);
    }
    disconnect(pasynUser);
    return status;
}

static asynStatus writeRawOnce(const char *port, int addr,
                    char const *buffer, size_t buffer_len, double timeout,
                    size_t *nbytesTransfered,const char *drvInfo)
{
    asynStatus status;
    asynUser   *pasynUser;

    status = connect(port,addr,&pasynUser,drvInfo);
    if(status!=asynSuccess) {
        printf("asynOctetSyncIO connect failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = writeRaw(pasynUser,buffer,buffer_len,timeout,nbytesTransfered);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO writeRaw failed %s\n",pasynUser->errorMessage);
    }
    disconnect(pasynUser);
    return status;
}

static asynStatus readOnce(const char *port, int addr,
                   char *buffer, size_t buffer_len, 
                   double timeout,
                   size_t *nbytesTransfered,int *eomReason,const char *drvInfo)
{
    asynStatus status;
    asynUser   *pasynUser;

    status = connect(port,addr,&pasynUser,drvInfo);
    if(status!=asynSuccess) {
        printf("asynOctetSyncIO connect failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = readIt(pasynUser,buffer,buffer_len,
         timeout,nbytesTransfered,eomReason);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO read failed %s\n",pasynUser->errorMessage);
    }
    disconnect(pasynUser);
    return status;
}

static asynStatus readRawOnce(const char *port, int addr,
                   char *buffer, size_t buffer_len, 
                   double timeout,
                   size_t *nbytesTransfered,int *eomReason,const char *drvInfo)
{
    asynStatus status;
    asynUser   *pasynUser;

    status = connect(port,addr,&pasynUser,drvInfo);
    if(status!=asynSuccess) {
        printf("asynOctetSyncIO connect failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = readRaw(pasynUser,buffer,buffer_len,
         timeout,nbytesTransfered,eomReason);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO readRaw failed %s\n",pasynUser->errorMessage);
    }
    disconnect(pasynUser);
    return status;
}

static asynStatus writeReadOnce(const char *port, int addr,
                        const char *write_buffer, size_t write_buffer_len,
                        char *read_buffer, size_t read_buffer_len,
                        double timeout,
                        size_t *nbytesOut, size_t *nbytesIn, int *eomReason,
                        const char *drvInfo)
{
    asynStatus status;
    asynUser   *pasynUser;

    status = connect(port,addr,&pasynUser,drvInfo);
    if(status!=asynSuccess) {
        printf("asynOctetSyncIO connect failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = writeRead(pasynUser,write_buffer,write_buffer_len,
         read_buffer,read_buffer_len,
         timeout,nbytesOut,nbytesIn,eomReason);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO writeReadOnce failed %s\n",pasynUser->errorMessage);
    }
    disconnect(pasynUser);
    return status;
}
static asynStatus flushOnce(const char *port, int addr,const char *drvInfo)
{
    asynStatus status;
    asynUser   *pasynUser;

    status = connect(port,addr,&pasynUser,drvInfo);
    if(status!=asynSuccess) {
        printf("asynOctetSyncIO connect failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = flushIt(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO flush failed %s\n",pasynUser->errorMessage);
    }
    disconnect(pasynUser);
    return status;
}

static asynStatus setInputEosOnce(const char *port, int addr,
                        const char *eos,int eoslen,const char *drvInfo)
{
    asynStatus status;
    asynUser   *pasynUser;

    status = connect(port,addr,&pasynUser,drvInfo);
    if(status!=asynSuccess) {
        printf("asynOctetSyncIO connect failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = setInputEos(pasynUser,eos,eoslen);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO setInputEos failed %s\n",pasynUser->errorMessage);
    }
    disconnect(pasynUser);
    return status;
}

static asynStatus getInputEosOnce(const char *port, int addr,
                        char *eos, int eossize, int *eoslen,const char *drvInfo)
{
    asynStatus status;
    asynUser   *pasynUser;

    status = connect(port,addr,&pasynUser,drvInfo);
    if(status!=asynSuccess) {
        printf("asynOctetSyncIO connect failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = getInputEos(pasynUser,eos,eossize,eoslen);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO getInputEos failed %s\n",pasynUser->errorMessage);
    }
    disconnect(pasynUser);
    return status;
}

static asynStatus setOutputEosOnce(const char *port, int addr,
                        const char *eos,int eoslen,const char *drvInfo)
{
    asynStatus status;
    asynUser   *pasynUser;

    status = connect(port,addr,&pasynUser,drvInfo);
    if(status!=asynSuccess) {
        printf("asynOctetSyncIO connect failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = setOutputEos(pasynUser,eos,eoslen);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO setOutputEos failed %s\n",pasynUser->errorMessage);
    }
    disconnect(pasynUser);
    return status;
}

static asynStatus getOutputEosOnce(const char *port, int addr,
                        char *eos, int eossize, int *eoslen,const char *drvInfo)
{
    asynStatus status;
    asynUser   *pasynUser;

    status = connect(port,addr,&pasynUser,drvInfo);
    if(status!=asynSuccess) {
        printf("asynOctetSyncIO connect failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = getOutputEos(pasynUser,eos,eossize,eoslen);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
             "asynOctetSyncIO getOutputEos failed %s\n",pasynUser->errorMessage);
    }
    disconnect(pasynUser);
    return status;
}
