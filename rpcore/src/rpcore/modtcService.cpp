//
//  File: tcService.cpp
//  Description: tcService implementation
//
//  Copyright (c) 2012, John Manferdelli.  All rights reserved.
//     Some contributions Copyright (c) 2012, Intel Corporation. 
//
// Use, duplication and disclosure of this file and derived works of
// this file are subject to and licensed under the Apache License dated
// January, 2004, (the "License").  This License is contained in the
// top level directory originally provided with the CloudProxy Project.
// Your right to use or distribute this file, or derived works thereof,
// is subject to your being bound by those terms and your use indicates
// consent to those terms.
//
// If you distribute this file (or portions derived therefrom), you must
// include License in or with the file and, in the event you do not include
// the entire License in the file, the file must contain a reference
// to the location of the License.


#include "jlmTypes.h"
#include "logging.h"
#include "tcIO.h"
#include "jlmcrypto.h"
#include "modtcService.h"
#include "keys.h"
#include "sha256.h"
#include "channelcoding.h"
#include "fileHash.h"
#include "jlmUtility.h"
#include "cryptoHelper.h"
#include "tao.h"
#include "TPMHostsupport.h"
#include "linuxHostsupport.h"

//#include "vault.h"
#include <stdlib.h>
#include <sys/wait.h> /* for wait */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef LINUX
#include <linux/un.h>
#else
#include <sys/un.h>
#endif
#include <errno.h>

#include "dombuilder.h"
#include "tcconfig.h"
#include "tcpchan.h"
#include "rp_api_code.h"

tcServiceInterface      g_myService;
int                     g_servicepid= 0;
extern bool				g_fterminateLoop;
u32                     g_fservicehashValid= false;
u32                     g_servicehashType= 0;
int                     g_servicehashSize= 0;
byte                    g_servicehash[32]= {
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
                        };

#define NUMPROCENTS 200
#define LAUNCH_ALLOWED		"launch allowed"	 
#define LAUNCH_NOT_ALLOWED	"launch not allowed"
#define KEYWORD_UNTRUSTED	"untrusted"





// ---------------------------------------------------------------------------


bool uidfrompid(int pid, int* puid)
{
    char        szfileName[256];
    struct stat fattr;

    sprintf(szfileName, "/proc/%d/stat", pid);
    if((lstat(szfileName, &fattr))!=0) {
        printf("uidfrompid: stat failed\n");
        return false;
    }
    *puid= fattr.st_uid;
    return true;
}


// ------------------------------------------------------------------------------


void serviceprocEnt::print()
{
    fprintf(g_logFile, "procid: %ld, ", (long int)m_procid);
    if(m_szexeFile!=NULL)
        fprintf(g_logFile, "file: %s, ", m_szexeFile);
    fprintf(g_logFile, "hash size: %d, ", m_sizeHash);
    PrintBytes("", m_rgHash, m_sizeHash);
}


serviceprocTable::serviceprocTable()
{
    m_numFree= 0;
    m_numFilled= 0;
    m_pFree= NULL;
    m_pMap= NULL;
    m_rgProcMap= NULL;
    m_rgProcEnts= NULL;
}


serviceprocTable::~serviceprocTable()
{
    // delete m_rgProcEnts;
    // delete m_rgProcMap;
    // m_numFree= 0;
    // m_numFilled= 0;
    // m_pFree= NULL;
    // m_pMap= NULL;
}


bool serviceprocTable::initprocTable(int size)
{
    int             i;
    serviceprocMap* p;

    m_rgProcEnts= new serviceprocEnt[size];
    m_rgProcMap= new serviceprocMap[size];
    p= &m_rgProcMap[0];
    m_pMap= NULL;
    m_pFree= p;
    for(i=0; i<(size-1); i++) {
        p= &m_rgProcMap[i];
        p->pElement= &m_rgProcEnts[i];
        p->pNext= &m_rgProcMap[i+1];
    }
    m_rgProcMap[size-1].pElement= &m_rgProcEnts[size-1];
    m_rgProcMap[size-1].pNext= NULL;
    m_numFree= size;
    m_numFilled= 0;
    return true;
}


bool serviceprocTable::addprocEntry(int procid, const char* file, int an, char** av,
                                    int sizeHash, byte* hash)
{
   //printf("Entering in addprocEntry");
    if(m_pFree==NULL)
        return false;
   //printf("m_pfree pass");
    if(sizeHash>32)
        return false;
   //printf("size hash pass: %d",procid);

	
    serviceprocMap* pMap= m_pFree;
    m_pFree= pMap->pNext;
    serviceprocEnt* pEnt= pMap->pElement;
    m_numFilled++;
    m_numFree--;
    pEnt->m_procid= procid;
    pEnt->m_sizeHash= sizeHash;
    memcpy(pEnt->m_rgHash, hash, sizeHash);
    pEnt->m_szexeFile= strdup(file);
    pMap->pNext= m_pMap;
    m_pMap= pMap;
   //printf("\n pEnt->m_procid : %d",  pEnt->m_procid);
    return true;
}


serviceprocEnt*  serviceprocTable::getEntfromprocId(int procid)
{
    serviceprocMap* pMap= m_pMap;
    serviceprocEnt* pEnt;
   //printf("\n inside getEntfromprocId");
    while(pMap!=NULL) {
        pEnt= pMap->pElement;
	printf("\n in while loop");
        if(pEnt->m_procid==procid) {
	   //printf("\n procid match");
            return pEnt;
        }
        pMap= pMap->pNext;
    }
   //printf("\n NOT FOUND");
    return NULL;
}


//Step through linked list m_pMap and delete node matching procid
void   serviceprocTable::removeprocEntry(int procid)
{
    serviceprocMap* pMap;
    serviceprocMap* pDelete;
    serviceprocEnt* pEnt;

    if(m_pMap==NULL)
        return;

    pEnt= m_pMap->pElement;
    if(pEnt->m_procid==procid) {
        pMap= m_pMap;
        m_pMap= pMap->pNext;
        pMap->pNext= m_pFree;
        m_pFree= pMap;
        m_numFree++;
        m_numFilled--;
        return;
    }
     
    pMap= m_pMap;   
    while(pMap->pNext!=NULL) {
        pDelete= pMap->pNext;
        pEnt= pDelete->pElement;
        if(pEnt->m_procid==procid) {
            pMap->pNext= pDelete->pNext;
            pDelete->pNext= m_pFree;
            m_pFree= pDelete;
            pEnt->m_procid= -1;
            m_numFree++;
            m_numFilled--;
            return;
        }
        pMap= pDelete;
    }
    return;
}
/*
serviceprocEnt*  serviceprocTable::getEntfromuuid(byte* uuid)
{
    serviceprocMap* pMap= m_pMap;
    serviceprocEnt* pEnt;
    while(pMap!=NULL) {
        pEnt= pMap->pElement;
        if( strcasecmp(pEnt->m_uuid, (char*) uuid) == 0 ) {
            return pEnt;
        }
        pMap= pMap->pNext;
    }
    return NULL;
}
*/
bool  serviceprocTable::getEntfromuuid(char* uuid)
{
    serviceprocMap* pMap= m_pMap;
    serviceprocEnt* pEnt;
    while(pMap!=NULL) {
        pEnt= pMap->pElement;
        if( strcasecmp(pEnt->m_uuid, (char*) uuid) == 0 ) {
            return true;
        }
        pMap= pMap->pNext;
    }
    return false;
}


//Step through linked list m_pMap and delete node matching uuid
void   serviceprocTable::removeprocEntry(char* uuid)
{
    serviceprocMap* pMap;
    serviceprocMap* pDelete;
    serviceprocEnt* pEnt;

    if(m_pMap==NULL)
        return;

    pEnt= m_pMap->pElement;
    if(strcasecmp(pEnt->m_uuid, uuid) == 0 ) {
        pMap= m_pMap;
        m_pMap= pMap->pNext;
        pMap->pNext= m_pFree;
        m_pFree= pMap;
        m_numFree++;
        m_numFilled--;
        return;
    }
     
    pMap= m_pMap;   
    while(pMap->pNext!=NULL) {
        pDelete= pMap->pNext;
        pEnt= pDelete->pElement;
        if(strcasecmp(pEnt->m_uuid, uuid) == 0 ) {
            pMap->pNext= pDelete->pNext;
            pDelete->pNext= m_pFree;
            m_pFree= pDelete;
            pEnt->m_procid= -1;
            m_numFree++;
            m_numFilled--;
            return;
        }
        pMap= pDelete;
    }
    return;
}

bool serviceprocTable::updateprocEntry(int procid, char* uuid, char *vdi_uuid)
{
   //printf("Inside updateprocEntry");
    serviceprocEnt* pEnt= getEntfromprocId(procid);
   //printf("after get call");
   //printf("\n pEnt->m_procid : %d",  pEnt->m_procid); 
    if(pEnt==NULL){
	printf("pEnt is Null");
        return false;
    }
   //printf("\n vm_uuid:%s vdi_uuid:%s",uuid, vdi_uuid);
    memset(pEnt->m_uuid, 0, g_max_uuid);
    memset(pEnt->m_vdi_uuid, 0, g_max_uuid);
    memcpy(pEnt->m_uuid, uuid, g_sz_uuid);
    memcpy(pEnt->m_vdi_uuid, vdi_uuid, g_sz_uuid);
   //printf("\n %s", pEnt->m_uuid);
   //printf("\n %s", pEnt->m_vdi_uuid);
    return true;
}

bool serviceprocTable::updateprocEntry(int procid, char* vm_image_id, char* vm_customer_id, char* vm_manifest_hash, char* vm_manifest_signature) {
    //geting the procentry related to this procid
    serviceprocEnt* pEnt= getEntfromprocId(procid);
    if(pEnt == NULL) {
        printf("pEnt is NULL");
        return false;
    }
    
    //    updating the proctable entry for current procid
    /*pEnt->m_vm_image_id = malloc(sizeof(char) * (strlen(vm_image_id) + 1));
    if(pEnt->m_vm_image_id == NULL) {
        printf("\nError in allocating memory for proctable attribute");
        return false;
    }*/
    strcpy(pEnt->m_vm_image_id,vm_image_id);
    pEnt->m_size_vm_image_id = strlen(pEnt->m_vm_image_id);
    /*pEnt->m_vm_customer_id = malloc(sizeof(char) * (strlen(vm_customer_id) + 1));
    if(pEnt->m_vm_customer_id == NULL) {
        printf("\n Error in allocating memory for proctable attribure");
        return false;
    }*/
    strcpy(pEnt->m_vm_customer_id,vm_customer_id);
    pEnt->m_size_vm_customer_id = strlen(pEnt->m_vm_customer_id);

    /*pEnt->m_vm_manifest_hash = malloc(sizeof(char) * (strlen(vm_manifest_hash) + 1));
    if(pEnt->m_vm_manifest_hash == NULL) {
        printf("\nError in allocating memory for proctable attribute");
        return false;
    }*/
    strcpy(pEnt->m_vm_manifest_hash,vm_manifest_hash);
    pEnt->m_size_vm_manifest_hash = strlen(pEnt->m_vm_manifest_hash);
    /*pEnt->m_vm_manifest_signature = malloc(sizeof(char) * (strlen(vm_manifest_signature) + 1));
    if(pEnt->m_vm_manifest_signature == NULL) {
        printf("\nError in allcoating memory for proctatble attribute");
        return false;
    }*/
    strcpy(pEnt->m_vm_manifest_signature,vm_manifest_signature);
    pEnt->m_size_vm_manifest_signature = strlen(pEnt->m_vm_manifest_signature);
    return true;
}


bool serviceprocTable::checkprocEntry(char* uuid, char *vdi_uuid)
{
   //printf("Inside checkprocEntry");
    serviceprocMap* pMap= m_pMap;
    serviceprocEnt* pEnt;
    while(pMap!=NULL) {
        pEnt= pMap->pElement;
	if(strcasecmp(pEnt->m_vdi_uuid, (char*) vdi_uuid) == 0){
		if(strcasecmp(pEnt->m_uuid, (char*) uuid) == 0){
			printf("Same");
			return true;
			
		}
		printf("diffenret");
		return false;
	}
        pMap= pMap->pNext;
    }
    return true;
}


bool serviceprocTable::gethashfromprocId(int procid, int* psize, byte* hash)
{
    serviceprocEnt* pEnt= getEntfromprocId(procid);

    if(pEnt==NULL)
        return false;

    *psize= pEnt->m_sizeHash;
    memcpy(hash, pEnt->m_rgHash, *psize);
    return true;
}


void serviceprocTable::print()
{
    serviceprocMap* pMap= m_pMap;
    serviceprocEnt* pEnt;

    while(pMap!=NULL) {
        pEnt= pMap->pElement;
        pEnt->print();
        pMap= pMap->pNext;
    }

    fprintf(g_logFile, "proc table %d entries, %d free\n\n", 
                m_numFilled, m_numFree);
    return;
}


// -------------------------------------------------------------------


tcServiceInterface::tcServiceInterface()
{
}


tcServiceInterface::~tcServiceInterface()
{
}


TCSERVICE_RESULT tcServiceInterface::initService(const char* execfile, int an, char** av)
{
#if 0
    u32     hashType= 0;
    int     sizehash= SHA256DIGESTBYTESIZE;
    byte    rgHash[SHA256DIGESTBYTESIZE];

    if(!getfileHash(execfile, &hashType, &sizehash, rgHash)) {
        fprintf(g_logFile, "initService: getfileHash failed %s\n", execfile);
        return TCSERVICE_RESULT_FAILED;
    }
#ifdef TEST
    fprintf(g_logFile, "initService size hash %d\n", sizehash);
    PrintBytes("getfile hash: ", rgHash, sizehash);
#endif

    if(sizehash>SHA256DIGESTBYTESIZE)
        return TCSERVICE_RESULT_FAILED;
    g_servicehashType= hashType;
    g_servicehashSize= sizehash;
    memcpy(g_servicehash, rgHash, sizehash);
    g_fservicehashValid= true;
#endif

    return TCSERVICE_RESULT_SUCCESS;
}

// *****************return the procId (rpid) for given uuid***************

TCSERVICE_RESULT tcServiceInterface::GetRpId(char *vm_uuid, byte * rpidbuf, int * rpidsize)
{
    fprintf(g_logFile,"In GetRpId function\n");
	serviceprocMap* pMap = m_procTable.m_pMap;
	serviceprocEnt *pEnt;
	while(pMap != NULL)
	{
		pEnt = pMap->pElement;
        fprintf(g_logFile,"uuids :: %s vs : %s\n",pEnt->m_uuid,vm_uuid);
		if(strcmp(pEnt->m_uuid,vm_uuid) == 0)
		{
            fprintf(g_logFile,"match found for Given UUID\n");
			//itoa(pEnt->m_procid, (char *)rpidbuf, 10);
            sprintf((char *)rpidbuf,"%d",pEnt->m_procid);
			*rpidsize = strlen((char *)rpidbuf);
			return TCSERVICE_RESULT_SUCCESS;
		}
		pMap = pMap->pNext;
	}
    fprintf(g_logFile,"Found NO Match of UUID\n");
	return TCSERVICE_RESULT_FAILED;
}

//*************************return vmeta for given procid(rpid)*************/

TCSERVICE_RESULT tcServiceInterface::GetVmMeta(int procId, byte *vm_imageId, int * vm_imageIdsize,
    						byte * vm_customerId, int * vm_customerIdsize, byte * vm_manifestHash, int * vm_manifestHashsize,
    						byte * vm_manifestSignature, int * vm_manifestSignaturesize)
{
    fprintf(g_logFile,"In function GetVmMeta\n");
	serviceprocMap* pMap = m_procTable.m_pMap;
	serviceprocEnt *pEnt;
	while(pMap != NULL)
	{
		pEnt = pMap->pElement;
		if(procId == pEnt->m_procid)
		{
            fprintf(g_logFile,"Match found for given RPid\n");
			fprintf(g_logFile,"vmimage id : %s\n",pEnt->m_vm_image_id);
			memcpy(vm_imageId,pEnt->m_vm_image_id,pEnt->m_size_vm_image_id + 1);
			fprintf(g_logFile,"vmimage id copied : %s\n",vm_imageId);
			*vm_imageIdsize = pEnt->m_size_vm_image_id ;
			memcpy(vm_customerId,pEnt->m_vm_customer_id,pEnt->m_size_vm_customer_id + 1);
			*vm_customerIdsize = pEnt->m_size_vm_customer_id ;
			memcpy(vm_manifestHash,pEnt->m_vm_manifest_hash, pEnt->m_size_vm_manifest_hash + 1);
			*vm_manifestHashsize = pEnt->m_size_vm_manifest_hash ;
			memcpy(vm_manifestSignature,pEnt->m_vm_manifest_signature,pEnt->m_size_vm_manifest_signature + 1);
			*vm_manifestSignaturesize = pEnt->m_size_vm_manifest_signature ;
			return TCSERVICE_RESULT_SUCCESS;
		}
		pMap = pMap->pNext;
	}
    fprintf(g_logFile,"Given RPID is not registered to RPCORE\n");
	return TCSERVICE_RESULT_FAILED;
}


TCSERVICE_RESULT tcServiceInterface::GetOsPolicyKey(u32* pType, 
                                            int* psize, byte* rgBuf)
{
	/*
    if(!m_trustedHome.m_policyKeyValid)
        return TCSERVICE_RESULT_DATANOTVALID ;
    if(*psize<m_trustedHome.m_sizepolicyKey)
        return TCSERVICE_RESULT_BUFFERTOOSMALL;
    memcpy(rgBuf, m_trustedHome.m_policyKey, m_trustedHome.m_sizepolicyKey);
    *pType= m_trustedHome.m_policyKeyType;
    *psize= m_trustedHome.m_sizepolicyKey;
*/
    return TCSERVICE_RESULT_FAILED;
}


TCSERVICE_RESULT tcServiceInterface::tcServiceInterface::GetOsCert(u32* pType,
                        int* psizeOut, byte* rgOut)
{
    if(!m_trustedHome.m_myCertificateValid)
        return TCSERVICE_RESULT_DATANOTVALID ;
        
    if(*psizeOut<m_trustedHome.m_myCertificateSize)
        return TCSERVICE_RESULT_BUFFERTOOSMALL;
    
    memcpy(rgOut, m_trustedHome.m_myCertificate, 
           m_trustedHome.m_myCertificateSize);
    
    *psizeOut= m_trustedHome.m_myCertificateSize;
    
    if(m_trustedHome.m_myCertificateType==KEYTYPERSA1024INTERNALSTRUCT)
        *pType= KEYTYPERSA1024SERIALIZED;
    else if(m_trustedHome.m_myCertificateType==KEYTYPERSA2048INTERNALSTRUCT)
        *pType= KEYTYPERSA2048SERIALIZED;
    else
        *pType= m_trustedHome.m_myCertificateType;

    return TCSERVICE_RESULT_SUCCESS;
}

/*
MH start of GetAIKCert.
This function returns dummy value as cert
TODO: relace codeto get actual aik cert
*/
TCSERVICE_RESULT tcServiceInterface::GetAIKCert(u32* pType,
                        int* psizeOut, byte* rgOut)
{
    char *dummy_aik_cert="This is dummy aik cert";
    memcpy(rgOut, dummy_aik_cert,
           strlen(dummy_aik_cert));

    *psizeOut= strlen(dummy_aik_cert);
    //*pType= "AIK_CERT";
    return TCSERVICE_RESULT_SUCCESS;
}
/*
MH start of GetTPMQuote.
This function returns dummy value as tpm quote
TODO: relace codeto get actual tpm quote
*/

TCSERVICE_RESULT tcServiceInterface::GetTPMQuote(char *nonceStr, byte* rgOut, int* psizeOut)
{
    char *dummy_tpm_quote="This is dummy tpm quote";
    memcpy(rgOut, dummy_tpm_quote,
           strlen(dummy_tpm_quote));

    *psizeOut= strlen(dummy_tpm_quote);
    //*pType= "AIK_CERT";
    return true;
}


TCSERVICE_RESULT tcServiceInterface::GetOsEvidence(int* psizeOut, byte* rgOut)
{
    if(!m_trustedHome.m_ancestorEvidenceValid)
        return TCSERVICE_RESULT_DATANOTVALID ;
    if(*psizeOut<m_trustedHome.m_ancestorEvidenceSize)
        return TCSERVICE_RESULT_BUFFERTOOSMALL;
    *psizeOut= m_trustedHome.m_ancestorEvidenceSize;
    memcpy(rgOut, m_trustedHome.m_ancestorEvidence, *psizeOut);

    return TCSERVICE_RESULT_SUCCESS;
}


TCSERVICE_RESULT tcServiceInterface::GetHostedMeasurement(int pid, u32* phashType, int* psize, byte* rgBuf)
{
    if(!m_procTable.gethashfromprocId(pid, psize, rgBuf)) {
        return TCSERVICE_RESULT_FAILED;
    }
    *phashType= HASHTYPEHOSTEDPROGRAM;
    return TCSERVICE_RESULT_SUCCESS;
}


TCSERVICE_RESULT tcServiceInterface::GetOsHash(u32* phashType, int* psize, byte* rgOut)
{
    if(!m_trustedHome.m_myMeasurementValid)
        return TCSERVICE_RESULT_DATANOTVALID ;
        
    if(*psize<m_trustedHome.m_myMeasurementSize)
        return TCSERVICE_RESULT_BUFFERTOOSMALL;
        
    *psize= m_trustedHome.m_myMeasurementSize;
    memcpy(rgOut, m_trustedHome.m_myMeasurement, *psize);
    *phashType= m_trustedHome.m_myMeasurementType;

    return TCSERVICE_RESULT_SUCCESS;
}


TCSERVICE_RESULT tcServiceInterface::GetServiceHash(u32* phashType, 
                    int* psize, byte* rgOut)
{
    if(!g_fservicehashValid)
        return TCSERVICE_RESULT_FAILED;
    *phashType= g_servicehashType;
    if(*psize<g_servicehashSize)
        return TCSERVICE_RESULT_FAILED;
    *psize= g_servicehashSize;
    memcpy(rgOut, g_servicehash, *psize);

    return TCSERVICE_RESULT_SUCCESS;
}


TCSERVICE_RESULT tcServiceInterface::TerminateApp(int sizeIn, byte* rgIn, int* psizeOut, byte* out)
{
	//remove entry from table.
	char uuid[48] = {0};
	
	if ((rgIn == NULL) || (psizeOut == NULL) || (out == NULL))
		return TCSERVICE_RESULT_FAILED;
		
	memset(uuid, 0, g_max_uuid);
    memcpy(uuid, rgIn, g_sz_uuid);
    
    g_myService.m_procTable.removeprocEntry(uuid);
    
    *psizeOut = sizeof(int);
    *((int*)out) = (int)1;
    
	return TCSERVICE_RESULT_SUCCESS;
}

TCSERVICE_RESULT tcServiceInterface::CheckAppID(char* in_uuid, char *vdi_uuid, int* psizeOut, byte* out)
{
        //remove entry from table.
        char uuid[48] = {0};
        char vuuid[48] = {0};
	bool match;
        if ((in_uuid == NULL) || (out == NULL))
                return TCSERVICE_RESULT_FAILED;

        memset(uuid, 0, g_max_uuid);
        memcpy(uuid, in_uuid, g_sz_uuid);

        memset(vuuid, 0, g_max_uuid);
        memcpy(vuuid, vdi_uuid, g_sz_uuid);
        match = g_myService.m_procTable.checkprocEntry(uuid, vuuid);

        *psizeOut = sizeof(int);
	if(match==true){
        	*((int*)out) = (int)0;
	}
	else{
		*((int*)out) = (int)1;
	}

        return TCSERVICE_RESULT_SUCCESS;
}

TCSERVICE_RESULT tcServiceInterface::CheckIS_MEASURED(char* in_uuid,  int* psizeOut, byte* out)
{
        //remove entry from table.
        char uuid[48] = {0};
        if ((in_uuid == NULL) || (out == NULL))
                return TCSERVICE_RESULT_FAILED;


        memset(uuid, 0, g_max_uuid);
        memcpy(uuid, in_uuid, g_sz_uuid);

	bool match;
        match = g_myService.m_procTable.getEntfromuuid(uuid);

	*psizeOut = sizeof(int);
	if(match==true){
                *((int*)out) = (int)0;
        }
        else{
                *((int*)out) = (int)1;
        }

        return TCSERVICE_RESULT_SUCCESS;
}

TCSERVICE_RESULT tcServiceInterface::UpdateAppID(char* str_rp_id, char* in_uuid, char *vdi_uuid, int* psizeOut, byte* out)
{
	//remove entry from table.
	char uuid[48] = {0};
	char vuuid[48] = {0};
	int  rp_id = -1; 
	if ((str_rp_id == NULL) || (in_uuid == NULL) || (out == NULL))
		return TCSERVICE_RESULT_FAILED;
	
	rp_id = atoi(str_rp_id);
	
	memset(uuid, 0, g_max_uuid);
        memcpy(uuid, in_uuid, g_sz_uuid);
    
	memset(vuuid, 0, g_max_uuid);	
	memcpy(vuuid, vdi_uuid, g_sz_uuid);
        g_myService.m_procTable.updateprocEntry(rp_id, uuid, vuuid);
    
        *psizeOut = sizeof(int);
        *((int*)out) = (int)1;
    
	return TCSERVICE_RESULT_SUCCESS;
}

TCSERVICE_RESULT tcServiceInterface::StartApp(tcChannel& chan,
                                int procid, const char* file, int an, char** av,
                                int* poutsize, byte* out)
{
    u32     uType= 0;
    int     size= SHA256DIGESTBYTESIZE;
    byte    rgHash1[SHA256DIGESTBYTESIZE];
    byte    rgHash2[SHA256DIGESTBYTESIZE];
    byte    rgHash[SHA256DIGESTBYTESIZE];
    char    kernelHashHex[2*SHA256DIGESTBYTESIZE] = {0};
    int     child= 0;
    int     i;
    int     uid= -1;

    char    kernel_file[1024] = {0};
    char    ramdisk_file[1024] = {0};
    char    disk_file[1024] = {0};
    char    manifest_file[1024] = {0};
    char    kernel[1024] = {0};
    char    initrd[1024] = {0};
    char*   config_file = NULL;
    char*   mtw_pubkey_file = "./pubkey.pem";
    bool    is_launch_allowed = false;
    char *  vm_image_id;
    char*   vm_customer_id;
    char*   vm_manifest_hash;
    char*   vm_manifest_signature;

    //char    command[512];
  if(an>30) {
        return TCSERVICE_RESULT_FAILED;
    }


    for ( i = 1; i < an; i++) {

        if( av[i] && strcmp(av[i], "-kernel") == 0 ){
            strcpy(kernel_file, av[++i]);
        }

        if( av[i] && strcmp(av[i], "-ramdisk") == 0 ){
            strcpy(ramdisk_file, av[++i]);
        }

        if( av[i] && strcmp(av[i], "-config") == 0 ){
            config_file = av[++i];
        }

        if( av[i] && strcmp(av[i], "-disk") == 0 ){
                        strcpy(disk_file, av[++i]);
                }
        if( av[i] && strcmp(av[i], "-manifest") == 0 ){
                        strcpy(manifest_file, av[++i]);
                }
    }

    //v: this code will be replaced by IMVM call flow

    //tftp_get_file("/tmp/rpimg/", kernel_file);
    //tftp_get_file("/tmp/rpimg/", ramdisk_file);
    //tftp_get_file("/tmp/rpimg/", config_file);

    //kernel_file = "/tmp/rpimg/kernel";
    //ramdisk_file = "/tmp/rpimg/ramdisk";
    //sprintf(kernel, "%s/%s", g_staging_dir, kernel_file);
    //sprintf(initrd, "%s/%s", g_staging_dir, ramdisk_file);

    //since we are providing full path in the request

   /*
    strcpy(kernel, kernel_file);
    strcpy(initrd, ramdisk_file);

    fprintf(stdout, "kernel_path=%s, ramdisk_path=%s\n", kernel, initrd);

    struct stat statBlock;
    if(( stat(kernel, &statBlock) != 0) || ( stat(initrd, &statBlock) != 0)){
        fprintf(g_logFile, "No kernel file specified for domain %s %s\n", kernel, initrd);
        return TCSERVICE_RESULT_FAILED;
   }

    Sha256      oHash;

    if(!getfileHash(kernel, &uType, &size, rgHash1)) {
        fprintf(g_logFile, "StartApp : getfilehash failed %s\n", kernel_file);
        return TCSERVICE_RESULT_FAILED;
    }

    for (i=0; i<SHA256DIGESTBYTESIZE; i++) {
        sprintf(kernelHashHex, "%s%02x", kernelHashHex, rgHash1[i]);
    }
    fprintf(stdout, "kernelHashHex = %s\n", kernelHashHex);

    if(!getfileHash(initrd, &uType, &size, rgHash2)) {
        fprintf(g_logFile, "StartApp : getfilehash failed %s\n", ramdisk_file);
        return TCSERVICE_RESULT_FAILED;
    }

    oHash.Init();
    oHash.Update(rgHash1, SHA256DIGESTBYTESIZE);
    oHash.Update(rgHash2, SHA256DIGESTBYTESIZE);
                                                                                                                                                                                   592,13-16     41%
    oHash.Final();
    oHash.GetDigest(rgHash);



    fprintf(stdout, "DISK = %s\n", disk_file);
    fprintf(stdout, "MANIFEST = %s\n", manifest_file);
  */
//////////////////////////////////////////////////////////////////////////////////////////////////////

       //create domain process shall check the whitelist
        child = create_domain(an, av);

       /*
        Currently rpcore reads the IMVM return value from a log file(\tmp\imvm-result.out)
       */
        pid_t pid=fork();
        if (pid==0) { // child process
            remove("/tmp/imvm-result.out");
            char command[512];
            if ((strcmp(ramdisk_file,"") == 0) && (strcmp(kernel_file,"") == 0))
                sprintf(command,"./verifier %s %s IMVM %s %d > /tmp/imvm-result.out 2>&1", manifest_file, disk_file, mtw_pubkey_file, child);
            else
			                sprintf(command,"./verifier %s %s IMVM %s %d %s %s > /tmp/imvm-result.out 2>&1", manifest_file, disk_file, mtw_pubkey_file, child, kernel_file, ramdisk_file);

            system(command);
            exit(127); // only if execv fails
        }
        else { // pid!=0; parent process
            waitpid(pid,0,0); // wait for child to exit
        }

        FILE* fp = fopen("/tmp/imvm-result.out", "rb");
        if(!fp) return -1;

        char imageHash[65];
        int flag=0;
        char launchPolicy[10];

        if (fp != NULL) {
            char line[1000];
            while(fgets(line,sizeof(line),fp)!= NULL)  {
                line[strlen ( line ) - 1] = '\0';

                if (strstr(line, "Audit") != NULL) {
                    strcpy(launchPolicy, "Audit");
                }
                else if (strstr(line, "Enforce") != NULL) {
                    strcpy(launchPolicy, "Enforce");
                }

                if (strstr(line, "Error in mounting the image") != NULL) {
                    fprintf(stdout, "Error in mounting the vm image\n");
                    flag=0;
                }

                if (strstr(line, "pass") != NULL) {
                    //Sha256 oHash;
                    //oHash.Init();
                    //oHash.Update((unsigned char *)imageHash, strlen(imageHash));
                    //oHash.Final();
                    //oHash.GetDigest(rgHash);
                    fprintf(stdout, "IMVM Verification Successfull\n");
                    flag=1;
                }
                else if ((strstr(line, "Failed") != NULL) && (strcmp(launchPolicy, "Audit") == 0)) {
                    fprintf(stdout, "IMVM Verification Failed, but continuing with VM launch as AUDIT launch policy is used\n");
                    flag=1;
                }
               if (strstr(line, "SHA-256-IMAGE-HASH") != NULL) {
                    char *token;
                    token = strtok(line, ":");
                    token = strtok(NULL, ":");
                    strcpy(imageHash, token);
                }
                if(strstr(line, "VM_IMAGE_ID") != NULL) {
                    char *token;
                    token = strtok(line, "=");
                    token = strtok(NULL,"=");
                    vm_image_id = (char *)malloc(sizeof(char)*(strlen(token) + 1));
                    if(vm_image_id == NULL) {
                        fprintf(g_logFile,"\n  StartApp : Error in allocating memory for vm_image_id");
                        return TCSERVICE_RESULT_FAILED;
                    }
                    strcpy(vm_image_id,token);
                }
                if(strstr(line, "VM_CUSTOMER_ID") != NULL) {
                    char *token;
                    token = strtok(line,"=");
                    token = strtok(NULL,"=");
                    vm_customer_id = (char *)malloc(sizeof(char) *(strlen(token) + 1));
                    if(vm_customer_id == NULL) {
                        fprintf(g_logFile,"\n StartApp : Error in allocating memory for vm_customer_id");
                        return TCSERVICE_RESULT_FAILED;
                    }
                    strcpy(vm_customer_id,token);
                }
                if(strstr(line, "VM_MANIFEST_HASH") != NULL) {
                    char * token;
                    token = strtok(line, "=");
                    token = strtok(NULL, "=");
                    vm_manifest_hash = (char *)malloc(sizeof(char) * (strlen(token) +1));
                    if(vm_manifest_hash == NULL) {
                        fprintf(g_logFile,"\n  StartApp : Error in allocating memory for vm_manifest_hash");
                        return TCSERVICE_RESULT_FAILED;
                    }
                    strcpy(vm_manifest_hash,token);
                }
                if(strstr(line, "VM_MANIFEST_SIGNATURE") != NULL) {
                    char * token;
                    token = strtok(line,"=");
                    token = strtok(NULL,"=");
                    vm_manifest_signature = (char *)malloc(sizeof(char) * (strlen(token) + 1));
                    if(vm_manifest_hash == NULL) {
                        fprintf(g_logFile,"\n  StartApp : Error in allocating memory for vm_manifest_hash");
                        return TCSERVICE_RESULT_FAILED;
                    }
                    strcpy(vm_manifest_signature,token);
                }
            }
            fclose(fp);
        }

        if (flag == 0) {
            fprintf(stdout, "IMVM Verification Failed\n");
            return TCSERVICE_RESULT_FAILED;
        }

//////////////////////////////////////////////////////////////////////////////////////////////////////

    //The code below does the work of converting 64 byte hex (imageHash) to 32 byte binary (rgHash)
    //same as in rpchannel/channelcoding.cpp:ascii2bin(),
    int c = 0;
    int len = strlen(imageHash);
    int iSize = 0;
    for (c= 0; c < len; c = c+2) {
        sscanf(&imageHash[c], "%02x", &rgHash[c/2]);
        iSize++;
    }

    //create domain process shall check the whitelist
    //child = create_domain(an, av);

   //printf("child : %d:", child);
    if(!g_myService.m_procTable.addprocEntry(child, kernel_file, 0, (char**) NULL, size, rgHash)) {
        fprintf(g_logFile, "StartApp: cant add to proc table\n");
        return TCSERVICE_RESULT_FAILED;
    }

   if(!g_myService.m_procTable.updateprocEntry(child, vm_image_id, vm_customer_id, vm_manifest_hash, vm_manifest_signature)) {
        fprintf(g_logFile, "SartApp : can't update proc table entry\n");
        return TCSERVICE_RESULT_FAILED;
    }

    for ( i = 1; i < an; i++) {
        if( av[i] ) {
            free (av[i]);
            av[i] = NULL;
        }
    }
    // free all allocated variable
    free(vm_image_id);
    free(vm_customer_id);
    free(vm_manifest_hash);
    free(vm_manifest_signature);
    
 *poutsize = sizeof(int);
    *((int*)out) = (int)child;

    return TCSERVICE_RESULT_SUCCESS;
}






#ifdef SEALUNSEAL_DOM0_STORAGE
// Debug Begin
bool SaveSealedData(const char* szFile, byte* buf, int size)
{
    fprintf(g_logFile, "SaveSealedData() %s %d\n",szFile, size);
    if(szFile==NULL)
        return false;
    int iWrite= open(szFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(iWrite<0)
        return false;
    ssize_t result = write(iWrite, buf, size);
    close(iWrite);
    return true;
}

bool GetSavedSealedData(const char* szFile, byte* buf, int* psize)
{
    fprintf(g_logFile, "GetSavedSealedData() %s\n",szFile);

    if(szFile==NULL) {
        return false;
    }

    int iRead= open(szFile, O_RDONLY);
    if(iRead<0) {
        fprintf(g_logFile, "GetSavedSealedData: Open failed\n");
        return false;
    }

    int n= read(iRead, buf, *psize);
    if(n<0) {
        fprintf(g_logFile, "GetSavedSealedData: Read error\n");
        close(iRead);
        return false;
    }
    *psize= n;
    close(iRead);
    fprintf(g_logFile, "GetSavedSealedData: Success Read *psize %d\n",n);
    return true;
}
//Debug End
#endif

TCSERVICE_RESULT tcServiceInterface::SealFor(int procid, int sizeIn, byte* rgIn, 
                                             int* psizeOut, byte* rgOut)
//  Sealed value is hash-size hash size-in rgIn
{
    byte    rgHash[32];
    int     hashSize= 0;

    if(!m_procTable.gethashfromprocId(procid, &hashSize, rgHash)) {
        fprintf(g_logFile, "SealFor can't find hash in procTable %ld\n", (long int)procid);
        return TCSERVICE_RESULT_FAILED;
    }
#ifdef TCTEST
    fprintf(g_logFile, "SealFor: %ld(proc), %d(hashsize), %d (size seal)\n",
           (long int)procid, hashSize, sizeIn);
#endif
    if(!m_trustedHome.Seal(hashSize, rgHash, sizeIn, rgIn,
                       psizeOut, rgOut)) {
        fprintf(g_logFile, "SealFor: seal failed\n");
        return TCSERVICE_RESULT_FAILED;
    }
#ifdef SEALUNSEAL_DOM0_STORAGE
	//debug code
   PrintBytes("SealFor: In Bytes\n", rgIn, sizeIn );
   fprintf(g_logFile, "SealFor: sizeIn %d\n", sizeIn);
   SaveSealedData("/home/s1/rp/config/Sealed", rgOut, *psizeOut);
   fprintf(g_logFile, "SealFor: *psizeOut %d\n", *psizeOut);
   PrintBytes("SealFor: Sealed Bytes\n", rgOut, *psizeOut);
#endif

#ifdef TCTEST
    fprintf(g_logFile, "tcServiceInterface::SealFor\n");
#endif
    
    return TCSERVICE_RESULT_SUCCESS;
}


TCSERVICE_RESULT tcServiceInterface::UnsealFor(int procid, int sizeIn, byte* rgIn, 
                            int* psizeOut, byte* rgOut)
{
    byte    rgHash[32];
    int     hashSize= 0;

    if(!m_procTable.gethashfromprocId(procid, &hashSize, rgHash)) {
        fprintf(g_logFile, "UnsealFor can't find hash in procTable %ld\n", (long int)procid);
        return TCSERVICE_RESULT_FAILED;
    }
#ifdef TCTEST
    fprintf(g_logFile, "UnsealFor: %ld(proc), %d(hashsize), %d (size seal)\n",
           (long int)procid, hashSize, sizeIn);
#endif

#ifdef SEALUNSEAL_DOM0_STORAGE
   // BEGIN
   int fileSize= 500;
   fprintf(g_logFile, "UnsealFor: sizeIn %d\n", sizeIn);
   //GetSavedSealedData("/home/s1/rp/config/Sealed", rgIn, sizeIn);
   PrintBytes("UnsealFor: rgIn\n", rgIn, sizeIn);
   GetSavedSealedData("/home/s1/rp/config/Sealed", rgIn, &fileSize);
   PrintBytes("UnsealFor: read rgIn\n", rgIn, fileSize);
   fprintf(g_logFile, "UnsealFor: fileSize %d\n", fileSize);
   PrintBytes("UnsealFor: Sealed Bytes\n", rgIn, fileSize);
   sizeIn= fileSize;
  // END
#endif

    if(!m_trustedHome.Unseal(hashSize, rgHash, sizeIn, rgIn,
                       psizeOut, rgOut)) {
        fprintf(g_logFile, "UnsealFor: unseal failed\n");
        return TCSERVICE_RESULT_FAILED;
    }

#ifdef SEALUNSEAL_DOM0_STORAGE
    // BEGIN
    PrintBytes("UnsealFor: Unsealed Bytes\n", rgOut, *psizeOut);
    // END
#endif

    return TCSERVICE_RESULT_SUCCESS;
}


TCSERVICE_RESULT tcServiceInterface::AttestFor(int procid, int sizeIn, byte* rgIn, 
                            int* psizeOut, byte* rgOut)
{
    byte    rgHash[32];
    int     hashSize= 32;

    if(!m_procTable.gethashfromprocId(procid, &hashSize, rgHash)) {
#ifdef TCTEST
        fprintf(g_logFile, "tcServiceInterface::AttestFor lookup failed\n");
        m_procTable.print();
#endif
        return TCSERVICE_RESULT_FAILED;
    }
#ifdef TEST
    fprintf(g_logFile, "tcServiceInterface::AttestFor procid: %d\n", 
            procid);
#endif
    if(!m_trustedHome.Attest(hashSize, rgHash, sizeIn, rgIn,
                       psizeOut, rgOut)) {
        return TCSERVICE_RESULT_FAILED;
    }
#ifdef TEST
        fprintf(g_logFile, "tcServiceInterface::AttestFor new output buf size\n",
               *psizeOut);
#endif
    return TCSERVICE_RESULT_SUCCESS;
}


// ------------------------------------------------------------------------------


bool  serviceRequest(tcChannel& chan, bool* pfTerminate)
{
    int                 procid;
    int                 origprocid;
    u32                 uReq;
    u32                 uStatus;

    char*               szappexecfile= NULL;

    int                 sizehash= SHA256DIGESTBYTESIZE;
    byte                hash[SHA256DIGESTBYTESIZE] = {0};

    int                 inparamsize;
    byte                inparams[PARAMSIZE] = {0};

    int                 outparamsize;
    byte                outparams[PARAMSIZE] = {0};

    int                 size;
    byte                rgBuf[PARAMSIZE] ={0};

    int                 pid;
    u32                 uType= 0;
    int                 an = 0;
    char*               av[32];
	char*				str_rp_id = NULL;
		
#ifdef TEST
    fprintf(g_logFile, "Entering serviceRequest\n");
#endif

    // get request
    //inparamsize= PARAMSIZE;
    inparamsize = outparamsize = PARAMSIZE;
    if(!chan.gettcBuf(&procid, &uReq, &uStatus, &origprocid, &inparamsize, inparams)) {
        fprintf(g_logFile, "serviceRequest: gettcBuf failed\n");
        return false;
    }
    if(uStatus==TCIOFAILED) {
        chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
        return false;
    }

#ifdef TEST
    fprintf(g_logFile, "serviceRequest after get procid: %d, req, %d, origprocid %d\n", 
           procid, uReq, origprocid); 
#endif

    char response;
    //inparamsize = outparamsize = PARAMSIZE;
    switch(uReq) {

      case RP2VM_GETOSHASH:
        size= PARAMSIZE;
        if(g_myService.GetOsHash(&uType, &size, rgBuf)!=TCSERVICE_RESULT_SUCCESS) {
            fprintf(g_logFile, "serviceRequest: getosHash failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        
        
#ifdef TEST
       fprintf(g_logFile, "serviceRequest: RP2VM_GETOSHASH type %d, size %d\n",
        uType, size);
        PrintBytes("OsHash in tc service ", rgBuf, size);
#endif
		
		
        outparamsize= encodeRP2VM_GETOSHASH("SHA1", size, rgBuf, PARAMSIZE, outparams);
        if(outparamsize<0) {
            fprintf(g_logFile, "serviceRequest: encodeRP2VM_GETOSHASH buffer too small\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)) {
            fprintf(g_logFile, "serviceRequest: sendtcBuf (getosHash) failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        return true;

      case RP2VM_GETOSCREDS:
        size= PARAMSIZE;
        if(g_myService.GetOsEvidence(&size, rgBuf)!=TCSERVICE_RESULT_SUCCESS) {
            fprintf(g_logFile, "serviceRequest: getosCredsfailed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        outparamsize= encodeRP2VM_GETOSCREDS(uType, size, rgBuf, 
                                      PARAMSIZE, outparams);
        if(outparamsize<0) {
            fprintf(g_logFile, "serviceRequest: encodeRP2VM_GETOSCREDS buffer too small\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)){
            fprintf(g_logFile, "serviceRequest: sendtcBuf (getosCreds) failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        return true;
        
      case RP2VM_GETOSCERT:
        size= PARAMSIZE;
        if(g_myService.GetOsCert(&uType, &size, rgBuf)!=TCSERVICE_RESULT_SUCCESS) {
            fprintf(g_logFile, "serviceRequest: getosCredsfailed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
	fwrite(rgBuf,1, size, stdout);
        outparamsize= encodeRP2VM_GETOSCERT("XML", size, rgBuf, PARAMSIZE, outparams);
        if(outparamsize<0) {
            fprintf(g_logFile, "serviceRequest: encodeRP2VM_GETOSCERT buffer too small\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)){
            fprintf(g_logFile, "serviceRequest: sendtcBuf (getosCert) failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        return true;
	
	//MH start of GETAIKCERT code	
      case RP2VM_GETAIKCERT:
        size= PARAMSIZE;
        if(g_myService.GetAIKCert(&uType, &size, rgBuf)!=TCSERVICE_RESULT_SUCCESS) {
            fprintf(g_logFile, "serviceRequest: get_aik_cert failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        outparamsize= encodeRP2VM_GETAIKCERT("XML", size, rgBuf, PARAMSIZE, outparams);
        if(outparamsize<0) {
            fprintf(g_logFile, "serviceRequest: encodeRP2VM_GETAIKCERT buffer too small\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)){
            fprintf(g_logFile, "serviceRequest: sendtcBuf (get_aik_cert) failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        return true;
	//MH end of GETAIKCERT
	

      case RP2VM_SEALFOR:
        // Input buffer to decode:
        //  size of sealdata || sealedata
        //  decodeVM2RP_SEALFOR outputs
        //  size of sealdata, sealedata
        //  sealfor returns m= ENC(hashsize||hash||sealsize||sealdata)
        //  returned buffer is sizeof(m) || m
        outparamsize= PARAMSIZE;
#ifdef TCTEST
        fprintf(g_logFile, "RP2VM_SEALFOR********about to sealFor\n");
        PrintBytes("inparams: ", inparams, inparamsize);
#endif
        if(!decodeVM2RP_SEALFOR(&outparamsize, outparams, inparams)) {
            fprintf(g_logFile, "serviceRequest: RP2VM_SEALFOR buffer too small\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        size= PARAMSIZE;
#ifdef TCTEST1
        fprintf(g_logFile, "about to sealFor %d\n", outparamsize);
        PrintBytes("bytes to seal: ", outparams, outparamsize);
#endif
        if(g_myService.SealFor(origprocid, outparamsize, outparams, &size, rgBuf)
                !=TCSERVICE_RESULT_SUCCESS) {
            fprintf(g_logFile, "serviceRequest: sealFor failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        outparamsize= encodeRP2VM_SEALFOR(size, rgBuf, PARAMSIZE, outparams);
        if(outparamsize<0) {
            fprintf(g_logFile, "serviceRequest: encodeRP2VM_SEALFOR buf too small\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)) {
            fprintf(g_logFile, "serviceRequest: sendtcBuf (sealFor) failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        return true;

      case RP2VM_UNSEALFOR:
        // outparamsize is sizeof(m)m outparams is m from above
        // unsealfor returns sizof unsealed data || unsealeddata
#ifdef TCTEST1
        fprintf(g_logFile, "RP2VM_UNSEALFOR********about to UnsealFor\n");
#endif
        outparamsize= PARAMSIZE;
        if(!decodeVM2RP_UNSEALFOR(&outparamsize, outparams, inparams)) {
            fprintf(g_logFile, "serviceRequest: service loop RP2VM_UNSEALFOR failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        size= PARAMSIZE;
#ifdef TCTEST
        fprintf(g_logFile, "about to UnsealFor %d\n", outparamsize);
#endif
        if(g_myService.UnsealFor(origprocid, outparamsize, outparams, &size, rgBuf)
                !=TCSERVICE_RESULT_SUCCESS) {
            fprintf(g_logFile, "serviceRequest: UnsealFor failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
#ifdef TCTEST1
        PrintBytes("return from UnsealFor:\n", rgBuf, size);
#endif
        outparamsize= encodeRP2VM_UNSEALFOR(size, rgBuf, PARAMSIZE, outparams);
        if(outparamsize<0) {
            fprintf(g_logFile, "serviceRequest: encodeRP2VM_SEALFOR buf too small\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)) {
            fprintf(g_logFile, "serviceRequest: sendtcBuf (unsealFor) failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        return true;

      case RP2VM_GETPROGHASH:
      //result of this call is ignored
        if(!decodeVM2RP_GETPROGHASH(&pid, outparams, inparams)) {
            fprintf(g_logFile, "serviceRequest: RP2VM_GETPROGHASH failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }

        // Process
	pid = origprocid;
#ifdef TEST
        fprintf(g_logFile, "looking up hash for pid %d\n", pid);
        g_myService.m_procTable.print();
        fflush(g_logFile);
#endif
        sizehash= SHA256DIGESTBYTESIZE;
        uType= SHA256HASH;
        if(!g_myService.m_procTable.gethashfromprocId(pid, &sizehash, hash)) {
#ifdef TEST
            fprintf(g_logFile, "hash not found setting to 0\n");
#endif
            memset(hash, 0, sizehash);
        }
#ifdef TEST
        fprintf(g_logFile, "program hash for pid found\n");
        PrintBytes("Hash: ", hash, sizehash);
        fflush(g_logFile);
#endif
        outparamsize= encodeRP2VM_GETPROGHASH("SHA256", sizehash, hash, PARAMSIZE, outparams);
        if(outparamsize<0) {
            fprintf(g_logFile, "serviceRequest: encodeTCSERVICEGETPROGHASHFROMSERVICE buf too small\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)) {
            fprintf(g_logFile, "serviceRequest: sendtcBuf (getproghash) failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        return true;

      case RP2VM_ATTESTFOR:
        outparamsize= PARAMSIZE;
        if(!decodeVM2RP_ATTESTFOR(&outparamsize, outparams, inparams)) {
            fprintf(g_logFile, "serviceRequest: RP2VM_ATTESTFOR failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        size= PARAMSIZE;
        if(g_myService.AttestFor(origprocid, outparamsize, outparams, &size, rgBuf)
                !=TCSERVICE_RESULT_SUCCESS) {
            fprintf(g_logFile, "serviceRequest: AttestFor failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        outparamsize= encodeRP2VM_ATTESTFOR(size, rgBuf, PARAMSIZE, outparams);
        if(outparamsize<0) {
            fprintf(g_logFile, "serviceRequest: encodeRP2VM_ATTESTFOR buf too small\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)) {
            fprintf(g_logFile, "serviceRequest: sendtcBuf (AttestFor) failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        return true;

      case RP2VM_STARTAPP:
#ifdef TCTEST1
        fprintf(g_logFile, "serviceRequest, RP2VM_STARTAPP, decoding\n");
#endif
        an= 20;
        if(!decodeVM2RP_STARTAPP(&szappexecfile, &an, 
                    (char**) av, inparams)) {
            fprintf(g_logFile, "serviceRequest: decodeRP2VM_STARTAPP failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        outparamsize= PARAMSIZE;
#ifdef TEST
        fprintf(g_logFile, "serviceRequest, about to StartHostedProgram %s, for %d\n",
                szappexecfile, origprocid);
#endif
        if(g_myService.StartApp(chan, origprocid, szappexecfile, an, av,
                                    &outparamsize, outparams)
                !=TCSERVICE_RESULT_SUCCESS) {
            fprintf(g_logFile, "serviceRequest: StartHostedProgram failed %s\n", szappexecfile);
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
#ifdef TEST
        fprintf(g_logFile, "serviceRequest, StartHostedProgram succeeded, about to send\n");
#endif
        if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)) {
            fprintf(g_logFile, "serviceRequest: sendtcBuf (startapp) failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        //
        // todo: release memory for szappexecfile??
        //
        return true;

      case RP2VM_CHECK_VM_VDI:

	printf("Inside RP2VM_CHECK_VM_VDI");
#ifdef TCTEST1
        fprintf(g_logFile, "serviceRequest, RP2VM_CHECK_VM_VDI, decoding\n");
#endif
        an= 20;
        //if(!decodeVM2RP_STARTAPP(&str_rp_id, &an, (char**) av, inparams)) {
        if(!decodeVM2RP_CHECK_VM_VDI(&str_rp_id, &an, (char**) av, inparams)) {
            fprintf(g_logFile, "serviceRequest: decodeRP2VM_CHECK_VM_VDI failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        outparamsize= PARAMSIZE;

#ifdef TEST
        fprintf(g_logFile, "serviceRequest, about to check VM : VDI %s, for %s\n",
                av[1], av[2]);
#endif
       //printf("%s , %s",av[1], av[2]);
                if (av[0] ){
                        if(g_myService.CheckAppID(av[1], av[2], &outparamsize, outparams)
                                        !=TCSERVICE_RESULT_SUCCESS) {
                                fprintf(g_logFile, "serviceRequest: check_vm_vdi failed \n");
                                chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
                                return false;
                        }

                        //release av[0] and str_rp_id
                }
#ifdef TEST
        fprintf(g_logFile, "serviceRequest, check_vm_vdi succeeded, about to send\n");
#endif
        if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)) {
            fprintf(g_logFile, "serviceRequest: sendtcBuf (setuuid) failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        //
        // todo: release memory for szappexecfile
        //
        return true;


      case RP2VM_SETUUID:
 
#ifdef TCTEST1
        fprintf(g_logFile, "serviceRequest, RP2VM_SETUUID, decoding\n");
#endif
        an= 20;
        //if(!decodeVM2RP_STARTAPP(&str_rp_id, &an, (char**) av, inparams)) {
	if(!decodeVM2RP_SETUUID(&str_rp_id, &an, (char**) av, inparams)) {
            fprintf(g_logFile, "serviceRequest: decodeRP2VM_SETUUID failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        outparamsize= PARAMSIZE;
        
#ifdef TEST
        fprintf(g_logFile, "serviceRequest, about to setuuid %s, for %d\n",
                av[1], origprocid);
#endif
	printf("%s , %s, %s",av[1], av[2], av[3]);
		if (av[0] ){
			if(g_myService.UpdateAppID(av[1], av[2], av[3], &outparamsize, outparams)
					!=TCSERVICE_RESULT_SUCCESS) {
				fprintf(g_logFile, "serviceRequest: setuuid failed %s\n", str_rp_id);
				chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
				return false;
			}
			
			//release av[0] and str_rp_id
		}
#ifdef TEST
        fprintf(g_logFile, "serviceRequest, setuuid succeeded, about to send\n");
#endif
        if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)) {
            fprintf(g_logFile, "serviceRequest: sendtcBuf (setuuid) failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        //
        // todo: release memory for szappexecfile
        //
        return true;
        
      case RP2VM_TERMINATEAPP:
#ifdef TCTEST1
        fprintf(g_logFile, "serviceRequest, RP2VM_TERMINATEAPP, decoding\n");
#endif
        
        if(!decodeVM2RP_TERMINATEAPP(&outparamsize, outparams, inparams)) {
            fprintf(g_logFile, "serviceRequest: decodeRP2VM_TERMINATEAPP failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        
        memcpy(inparams, outparams, outparamsize);
        inparamsize = outparamsize;
        outparamsize= PARAMSIZE;
        memset(outparams, 0, outparamsize);
		
		
       if(g_myService.TerminateApp(inparamsize, inparams, &outparamsize, outparams)
                !=TCSERVICE_RESULT_SUCCESS) {
            fprintf(g_logFile, "serviceRequest: terminate app failed %s\n", szappexecfile);
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        
        //outparam will be success or failure
        
#ifdef TEST
        fprintf(g_logFile, "serviceRequest, terminate app succeeded, about to send\n");
#endif
        if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)) {
            fprintf(g_logFile, "serviceRequest: sendtcBuf (terminateapp) failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        return true;
      /*
      case RP2VM_IS_MEASURED:
#ifdef TCTEST1
        fprintf(g_logFile, "serviceRequest, RP2VM_TERMINATEAPP, decoding\n");
#endif
        printf("inside RP2VM_IS_MEASURED"); 
        if(!decodeVM2RP_IS_MEASURED(&outparamsize, outparams, inparams)) {
            fprintf(g_logFile, "serviceRequest: decodeRP2VM_IS_MEASURED failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        
        memcpy(inparams, outparams, outparamsize);
        inparamsize = outparamsize;
        inparams[inparamsize] = 0; //outparams[outparamsize] = 0;
		outparamsize = sizeof(const char);
		
        response = (g_myService.m_procTable.getEntfromuuid(inparams) != NULL);
        outparamsize = encodeRP2VM_IS_MEASURED(response, outparamsize, outparams);
        //outparam will be success or failure
        
#ifdef TEST
        fprintf(g_logFile, "serviceRequest, uuid is %smeasured VM, about to send\n", response?"a ":"NOT AT ALL A ");
#endif
        if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)) {
            fprintf(g_logFile, "serviceRequest: sendtcBuf (is_measured) failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        return true;*/
	case RP2VM_IS_MEASURED:
#ifdef TCTEST1
        fprintf(g_logFile, "serviceRequest, RP2VM_TERMINATEAPP, decoding\n");
#endif
        printf("inside RP2VM_IS_MEASURED");
        if(!decodeVM2RP_IS_MEASURED(&str_rp_id, &an, (char**) av, inparams)) {
            fprintf(g_logFile, "serviceRequest: decodeRP2VM_IS_MEASURED failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }

        memcpy(inparams, outparams, outparamsize);
        inparamsize = outparamsize;
        inparams[inparamsize] = 0; //outparams[outparamsize] = 0;
                outparamsize = sizeof(const char);

	 if (av[0] ){
			if(g_myService.CheckIS_MEASURED(av[0], &outparamsize, outparams)!=TCSERVICE_RESULT_SUCCESS) {
                                fprintf(g_logFile, "serviceRequest: is_measured failed");
                                chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
                                return false;
                        }

                        //release av[0] and str_rp_id
                }
        //outparam will be success or failure

#ifdef TEST
        fprintf(g_logFile, "serviceRequest, uuid is %smeasured VM, about to send\n", response?"a ":"NOT AT ALL A ");
#endif
        if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)) {
            fprintf(g_logFile, "serviceRequest: sendtcBuf (is_measured) failed\n");
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
            return false;
        }
        return true;

      case TCSERVICETERMINATE:  // no reply required
#ifdef TEST
        fprintf(g_logFile, "serviceRequest, TCSERVICETERMINATE\n");
#endif
        g_myService.m_procTable.removeprocEntry(origprocid);
#ifdef TEST
        fprintf(g_logFile, "serviceRequest, removeprocEntry %d\n", origprocid);
        g_myService.m_procTable.print();
#endif
        return true;
	//MH start of GETTPMQUOTE code
	
       case RP2VM_GETTPMQUOTE:
		outparamsize= PARAMSIZE;
		printf("%s", inparams);
		if(!decodeVM2RP_GETTPMQUOTE(&outparamsize, outparams, inparams)) {
				fprintf(g_logFile, "serviceRequest: decodeVM2RP_GETTPMQUOTE failed\n");
					g_reqChannel.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
					return false;
			}

			//HL: make sure outparams ends with null
			outparams[outparamsize]='\0';

			#ifdef TEST
			   fprintf(g_logFile, "decodeVM2RP_GETTPMQUOTE: outparams %s, \n outparams size: %d\n", (char*)outparams, outparamsize);
			#endif
							//clear the buf "inparams" and reuse it as buffer for the quote request message
			inparamsize= PARAMSIZE;
			memset(inparams, 0, inparamsize);
			char nonce[4096];
			memcpy(nonce,outparams,outparamsize);
			if(! g_myService.GetTPMQuote(nonce, (byte*)inparams, &inparamsize)){
				fprintf(g_logFile, "serviceRequest: get_tpm_quote failed\n");
					chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
					return false;
			}
				outparamsize= encodeRP2VM_GETTPMQUOTE(inparamsize,(byte*)inparams, PARAMSIZE, outparams);
				if(outparamsize<0) {
						fprintf(g_logFile, "RP2VM_GETTPMQUOTE: encodeRP2VM_GETTPMQUOTE buf too small\n");
						g_reqChannel.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
								return false;
				}
			if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)){
					fprintf(g_logFile, "serviceRequest: sendtcBuf (getosCert) failed\n");
					chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
					return false;
			}
			return true;
			//MH end of GETTPMQUOTE code

			/***********new API ******************/
        case RP2VM_GETRPID:
        {
        	outparamsize = PARAMSIZE;
        	//printf("%s",inparams);
        	if(!decodeRP2VM_GETRPID(&outparamsize,outparams,inparams))
        	{
        		fprintf(g_logFile, "serviceRequest: decodeRP2VM_GETRPID failed\n");
        		g_reqChannel.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
        		return false;
        	}
            fprintf(g_logFile,"Inparams after decoding : %s\n",inparams);
            fprintf(g_logFile,"outparams after decoding: %s\n",outparams);
        	inparamsize = PARAMSIZE;
        	memset(inparams,0,inparamsize);
        	char uuid[50];
        	memcpy(uuid,outparams,outparamsize+1);
        	//call getRPID(outparams,&inparams)
        	if(g_myService.GetRpId(uuid,inparams,&inparamsize))
        	{
        		fprintf(g_logFile, "RP2VM_GETRPID: encodeRP2VM_GETRPID uuid does not exist\n");
				g_reqChannel.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
				return false;
        	}
            fprintf(g_logFile,"rpid : %s\n",inparams);

        	//then encode the result
        	outparamsize = PARAMSIZE;
        	outparamsize = encodeRP2VM_GETRPID(inparamsize,inparams,outparamsize,outparams);
            fprintf(g_logFile,"after encode : %s",outparams);
        	if(outparamsize<0) {
				fprintf(g_logFile, "RP2VM_GETRPID: encodeRP2VM_GETRPID buf too small\n");
				g_reqChannel.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
				return false;
        	}
			if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)){
				fprintf(g_logFile, "serviceRequest: sendtcBuf (getRPID) failed\n");
				chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
				return false;
			}
            fprintf(g_logFile,"************succesfully send the response*************** ");
            return true;
        }
        case RP2VM_GETVMMETA:
        {
        	outparamsize = PARAMSIZE;
			if(!decodeRP2VM_GETVMMETA(&outparamsize,outparams,inparams))
			{
				fprintf(g_logFile, "serviceRequest: decodeRP2VM_GETRPID failed\n");
				g_reqChannel.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
				return false;
			}
			//inparamsize = PARAMSIZE;
			//memcpy(inparams,0,inparamsize);
			//call to getVMMETA
			byte *vm_rpcustomerId;
			byte *vm_rpimageId;
			byte *vm_rpmanifestHash;
			byte *vm_rpmanifestSignature;
			vm_rpcustomerId = (byte *)malloc(sizeof(byte)*256);
			if(vm_rpcustomerId == NULL) {
                                fprintf(g_logFile, "RP2VM_GETRPID: memory cann't be allocated for customerId \n");
                                g_reqChannel.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
                                return false;
                        }

			vm_rpimageId = (byte *) malloc(sizeof(byte)*256);
			if(vm_rpimageId == NULL) {
                                fprintf(g_logFile, "RP2VM_GETRPID: memory cann't be allocated for imageId \n");
                                g_reqChannel.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
                                return false;
                        }
			vm_rpmanifestSignature = (byte *) malloc(sizeof(byte) *512);
			if(vm_rpmanifestSignature == NULL) {
                                fprintf(g_logFile, "RP2VM_GETRPID: memory cann't be allocated for manifestSignature \n");
                                g_reqChannel.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
                                return false;
                        }
			vm_rpmanifestHash = (byte *) malloc(sizeof(byte) * 64);
			if( vm_rpmanifestHash== NULL) {
                                fprintf(g_logFile, "RP2VM_GETRPID: memory cann't be allocated for vm_rpmanifestHash \n");
                                g_reqChannel.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
                                return false;
                        }
			int vm_rpimageIdsize, vm_rpcustomerIdsize,vm_rpmanifestHashsize,vm_rpmanifestSignaturesize;
			int in_procid = atoi((char *)outparams);
			if(g_myService.GetVmMeta(in_procid,vm_rpimageId, &vm_rpimageIdsize,vm_rpcustomerId, &vm_rpcustomerIdsize,
					vm_rpmanifestHash, &vm_rpmanifestHashsize,vm_rpmanifestSignature, &vm_rpmanifestSignaturesize))
			{
				fprintf(g_logFile, "RP2VM_GETRPID: encodeRP2VM_GETVMMETA RPID does not exist\n");
				g_reqChannel.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
				return false;
			}
			//create a map of data vmmeta data
			fprintf(g_logFile,"vmimage id : %s\n",vm_rpimageId);
			fprintf(g_logFile,"vmcustomer id : %s\n",vm_rpcustomerId);
			fprintf(g_logFile,"vmmanifest hash : %s\n",vm_rpmanifestHash);
			fprintf(g_logFile,"vm manifest signature : %s\n",vm_rpmanifestSignature);
			byte * metaMap[4];

			/*char *key1 = "VM_IMAGE_ID";
			char *key2 = "VM_CUSTOMER_ID";
			char *key3 = "VM_MANIFEST_HASH";
			char *key4 = "VM_MSANIFEST_SIGNATURE";*/
			//metaMap[0][0] = key1;
			metaMap[0] = vm_rpimageId;
			//metaMap[1][0] = key2;
			metaMap[1] = vm_rpcustomerId;
			//metaMap[2][0] = key3;
			metaMap[2] = vm_rpmanifestHash;
			//metaMap[3][0] = key4;
			metaMap[3] = vm_rpmanifestSignature;
			int numOfMetaComp = 4;
			//encode the vmMeta data
			outparamsize = PARAMSIZE;
			outparamsize = encodeRP2VM_GETVMMETA(numOfMetaComp,metaMap,outparamsize,outparams);
			fprintf(g_logFile,"after encode : %s\n",outparams);
			if(outparamsize<0) {
				fprintf(g_logFile, "RP2VM_GETRPID: encodeRP2VM_GETVMMETA buf too small\n");
				g_reqChannel.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
				return false;
			}
			if(!chan.sendtcBuf(procid, uReq, TCIOSUCCESS, origprocid, outparamsize, outparams)){
				fprintf(g_logFile, "serviceRequest: sendtcBuf (getVMMETA) failed\n");
				chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
				return false;
			}
		fprintf(g_logFile,"************succesfully send the response*************** \n");
		free(vm_rpimageId);
		free(vm_rpcustomerId);
		free(vm_rpmanifestHash);
		free(vm_rpmanifestSignature);
            return true;
        }
        default:
            chan.sendtcBuf(procid, uReq, TCIOFAILED, origprocid, 0, NULL);
        return false;
    }
}



int modmain(int an, char** av)
{
    int                 iRet= 0;
    TCSERVICE_RESULT    ret;
    bool                fInitKeys= false;
    const char*         szexecFile = av[0];

    int                 i;
    bool                fTerminate= false;
    bool                fServiceStart;
    const char*         directory= NULL;
    const char*		configfile = "./tcconfig.xml";
    int 		instanceValid = 0;
	

  
    for(i=0; i<an; i++) {
        if(strcmp(av[i], "-help")==0) {
            fprintf(g_logFile, "\nUsage: tcService.exe [-initKeys] ");
            return 0;
        }
        if(strcmp(av[i], "-initKeys")==0) {
            fInitKeys= true;
        }
         /*if(strcmp(av[i], "-directory")==0) {
            directory= av[++i];
        }*/
		if(strcmp(av[i], "-cfgfile")==0) {
            configfile= av[++i];
        }        
    }

	//populate_whitelist_hashes();

    g_servicepid = 0;//v: getpid();
    
    const char** parameters = NULL;
    directory = &g_config_dir[0];
    int parameterCount = 0;
    
	parameters = (const char**)&directory;
	parameterCount = 1;

  
    if(!g_myService.m_host.HostInit(PLATFORMTYPEHW, parameterCount, parameters)) {
        fprintf(g_logFile, "tcService main: can't init host\n");
        iRet= 1;
        goto cleanup;
    }
 

    if(fInitKeys) {
        taoFiles  fileNames;

        if(!fileNames.initNames(directory, "TrustedOS")) {
            fprintf(g_logFile, "tcService::main: cant init names\n");
            iRet= 1;
            goto cleanup;
        }
        unlink(fileNames.m_szsymFile);
        unlink(fileNames.m_szprivateFile);
#ifdef CSR_REQ
	unlink(fileNames.m_pem_szprivateFile);
#endif
        unlink(fileNames.m_szcertFile);
        unlink(fileNames.m_szAncestorEvidence);
    }

    if(!g_myService.m_trustedHome.EnvInit(PLATFORMTYPELINUX, "TrustedOS",
                                g_domain, directory, 
                                &g_myService.m_host, 0, NULL)) {
        fprintf(g_logFile, "tcService main: can't init environment\n");
        iRet= 1;
        goto cleanup;
    }

    /*
    if(fInitKeys) {
        // EnvInit should have initialized keys
        iRet= 0;
        goto cleanup;
    }
*/
    if(!g_myService.m_procTable.initprocTable(NUMPROCENTS)) {
        fprintf(g_logFile, "tcService main: Cant init proctable\n");
        iRet= 1;
        goto cleanup;
    }

#if 0
    ret= g_myService.initService(szexecFile, 0, NULL);
    if(ret!=TCSERVICE_RESULT_SUCCESS) {
        fprintf(g_logFile, "tcService main: initService failed %s\n", szexecFile);
        iRet= 1;
        goto cleanup;
    }
#endif

    // add self proctable entry
    g_myService.m_procTable.addprocEntry(g_servicepid, strdup(szexecFile), 0, NULL,
                                      g_myService.m_trustedHome.m_myMeasurementSize,
                                      g_myService.m_trustedHome.m_myMeasurement);
   

    while(!g_fterminateLoop) {
        fServiceStart= serviceRequest(g_reqChannel, &fTerminate);

        UNUSEDVAR(fServiceStart);

    }

cleanup:

    g_myService.m_trustedHome.EnvClose();
    g_myService.m_host.HostClose();
    closeLog();
    return iRet;
}


// ------------------------------------------------------------------------------

