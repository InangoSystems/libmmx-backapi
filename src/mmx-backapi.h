/* mmx-backapi.h
 *
 * Copyright (c) 2013-2021 Inango Systems LTD.
 *
 * Author: Inango Systems LTD. <support@inango-systems.com>
 * Creation Date: Sep 2013
 *
 * The author may be reached at support@inango-systems.com
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Subject to the terms and conditions of this license, each copyright holder
 * and contributor hereby grants to those receiving rights under this license
 * a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable
 * (except for failure to satisfy the conditions of this license) patent license
 * to make, have made, use, offer to sell, sell, import, and otherwise transfer
 * this software, where such license applies only to those patent claims, already
 * acquired or hereafter acquired, licensable by such copyright holder or contributor
 * that are necessarily infringed by:
 *
 * (a) their Contribution(s) (the licensed copyrights of copyright holders and
 * non-copyrightable additions of contributors, in source or binary form) alone;
 * or
 *
 * (b) combination of their Contribution(s) with the work of authorship to which
 * such Contribution(s) was added by such copyright holder or contributor, if,
 * at the time the Contribution is added, such addition causes such combination
 * to be necessarily infringed. The patent license shall not apply to any other
 * combinations which include the Contribution.
 *
 * Except as expressly stated above, no rights or licenses from any copyright
 * holder or contributor is granted under this license, whether expressly, by
 * implication, estoppel or otherwise.
 *
 * DISCLAIMER
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * NOTE
 *
 * This is part of a management middleware software package called MMX that was developed by Inango Systems Ltd.
 *
 * This version of MMX provides web and command-line management interfaces.
 *
 * Please contact us at Inango at support@inango-systems.com if you would like to hear more about
 * - other management packages, such as SNMP, TR-069 or Netconf
 * - how we can extend the data model to support all parts of your system
 * - professional sub-contract and customization services
 *
 */

/*
 * This library contains means of parsing and building messages from
 * MMX Entry point to backends and vice versa.
 */

#ifndef MMX_BACKAPI_H_
#define MMX_BACKAPI_H_

#include <syslog.h> /* log levels */

#include <microxml.h>

#include <ing_gen_utils.h>

#include "mmx-backapi-config.h"

/*
 * Socket configuration
 */
#define MMXBA_EP_ADDR       "127.0.0.1"

/* Error messages */
#define MMXBA_OK                  0
#define MMXBA_GENERAL_ERROR       1
#define MMXBA_INVALID_FORMAT      2
#define MMXBA_SYSTEM_ERROR        3
#define MMXBA_BAD_INPUT_PARAMS    4
#define MMXBA_NOT_ENOUGH_MEMORY   5
#define MMXBA_NOT_INITIALIZED     6


#define MMXBA_MAX_STR_OPNAME_LEN 16
#define MMXBA_MAX_NUMBER_OF_ANY_OP_PARAMS 16

typedef struct mmxba_packet_s {
    char flags[8];
    char msg[0];    /* xml message */
} mmxba_packet_t;

extern char mmxba_flags[8];


/* Message tags */
#define MMXBA_STR_REQUEST        "mmxReqRequest"
#define MMXBA_STR_RESPONSE       "mmxReqResponse"
#define MMXBA_STR_OPNAME         "opName"
#define MMXBA_STR_SEQNUM         "reqSeqNum"
#define MMXBA_STR_BEOBJNAME      "beObjName"
#define MMXBA_STR_MMXINSTANCE    "mmxInstance"
#define MMXBA_STR_BEKEYPARAMS    "beKeyParams"
#define MMXBA_STR_BEKEYNAMES     "beKeyNames"
#define MMXBA_STR_PARAMNAMES     "paramNames"
#define MMXBA_STR_PARAMVALUES    "paramValues"
#define MMXBA_STR_OPRESCODE      "opResCode"
#define MMXBA_STR_OPEXTCODE      "opExtErrCode"
#define MMXBA_STR_ERRMSG         "errMsg"
#define MMXBA_STR_ADDSTATUS      "addStatus"
#define MMX_STR_POSTOPSTATUS     "postOpStatus"

#define MMXBA_STR_OBJECTS        "objects"
#define MMXBA_STR_OBJKEYVALUES   "objKeyValues"

#define MMXBA_STR_ATTR_ARRAYSIZE  "arraySize"

/* #define MMXBA_STR_NAMEVALUEPAIR   "nameValuePair" */
#define MMXBA_STR_NAMEVALUEPAIR   "nvPair"
#define MMXBA_STR_NAME            "name"
#define MMXBA_STR_VALUE           "value"

#define MMXBA_STR_OPER_GET        "GET"
#define MMXBA_STR_OPER_SET        "SET"
#define MMXBA_STR_OPER_GETALL     "GETALL"
#define MMXBA_STR_OPER_ADDOBJ     "ADDOBJ"
#define MMXBA_STR_OPER_DELOBJ     "DELOBJ"

typedef enum mmxba_op_type_e {
    MMXBA_OP_TYPE_ERROR = -1,
    MMXBA_OP_TYPE_GET,
    MMXBA_OP_TYPE_SET,
    MMXBA_OP_TYPE_GETALL,
    MMXBA_OP_TYPE_ADDOBJ,
    MMXBA_OP_TYPE_DELOBJ
} mmxba_op_type_t;



typedef struct mmxba_req_mempool_s {
    int             initialized; 
    unsigned short  size_bytes;
    unsigned short  curr_offset;
    char            *pool;
} mmxba_req_mempool_t;


/* mmxba_request_t structure contains all parameters of management
   requests and response sent between the entry-point and backends.
   Some of fields are used in all messages, other ones - only in
   specific request or response types  */
typedef struct mmxba_request_s {
    mmxba_op_type_t op_type;            /* used in all messages */
    int opSeqNum;                       /* used in all messages */
    char beObjName[MMXBA_MAX_STR_LEN];  /* used in all messages */

    int opResCode;              /* result code, err code and err msg  */
    int opExtErrCode;           /* are used in all response messages  */
    char errMsg[MMXBA_MAX_STR_LEN];
    int postOpStatus;      /* post-operation status: 
                              used for SET/ADDOBJ/DELOBJ operations
                              0 - operation is fully completed,
                              1 - backend restart is needed;          */

    char mmxInstances[32];  /* exactly specifies object in MMX EP; used in */
                            /* req and resp for GET/SET/ADDOBJ/DELOBJ oper */
    
    uint32_t beKeyParamsNum;  /* exactly specifies object in backend - */
                              /* used for GET/SET/DELOBJ requests,     */
                              /* or partially specifies in ADDOBJ req  */
    nvpair_t beKeyParams[MMXBA_MAX_NUMBER_OF_KEY_PARAMS];

    union {
        /* paramNames - used in GET request message */
        struct {
            uint32_t arraySize;
            char paramNames[MMXBA_MAX_NUMBER_OF_GET_PARAMS][MMXBA_MAX_STR_LEN];
        } paramNames;

        /* paramValues - used in SET or ADDOBJ requests and in GET response */
        struct {
            uint32_t arraySize;
            nvpair_t paramValues[MMXBA_MAX_NUMBER_OF_SET_PARAMS];
        } paramValues;

        /* All parameters used in getall request and response */
        struct {
            uint32_t beKeyNamesNum;
            char beKeyNames[MMXBA_MAX_NUMBER_OF_KEY_PARAMS][MMXBA_MAX_STR_LEN];

            uint32_t objNum;
            char objects[MMXBA_MAX_NUMBER_OF_GETALL_PARAMS][MMXBA_MAX_STR_LEN];
        } getAll;
 
        /* ADDOBJ request parameters */
        struct {
            /*  Names of MMX parameters that are expected in the reponse */
            /*  (Usually it is the last BE key parameter)                */
            uint32_t beKeyNamesNum;
            char beKeyNames[MMXBA_MAX_NUMBER_OF_KEY_PARAMS][MMXBA_MAX_STR_LEN];
            
            /* Names-values pairs of parametes of the new object */
            uint32_t paramNum;
            nvpair_t paramValues[MMXBA_MAX_NUMBER_OF_SET_PARAMS];
        } addObj_req; 
        
        /* ADDOBJ response parameters */
        struct {
            /*  Names of MMX parameters that were requested */
            /*  (Usually it is the last BE key parameter)   */
            uint32_t beKeyNamesNum;
            char beKeyNames[MMXBA_MAX_NUMBER_OF_KEY_PARAMS][MMXBA_MAX_STR_LEN];

            /* Number of added objects (currently always 1) */
            /* Comma separated values of the requested beKey parameters */
            /* (group of values of each object are separated by ";")    */
            uint32_t objNum;
            char objects[MMXBA_MAX_NUMBER_OF_ADDED_INSTANCES][MMXBA_MAX_STR_LEN];

        } addObj_resp;
    };

    mmxba_req_mempool_t   mem_pool;
} mmxba_request_t;


/*
 * Parses management request/response header from xml_string  
 * into struct mmxba_request_t
 */
int mmx_backapi_message_hdr_parse(const char *xml_string, mmxba_request_t *req);

/*
 * Parses xml_string of management request or response into 
 * struct mmxba_request_t
 */
int mmx_backapi_message_parse(const char *xml_string, mmxba_request_t *req);

/*
 * Writes xml request from Entry point to Backend
 */
int mmx_backapi_request_build(mmxba_request_t *req, char *xml_string,
                              size_t xml_string_size);

/*
 * Writes xml response from Backend to Entry point
 */
int mmx_backapi_response_build(mmxba_request_t *req, char *xml_string, 
                               size_t xml_string_size);



/* --------------------------------------------------------------------
 *    Helper functions for work with message memory pool used 
 *  for keeping param values.
 *  Message memory pool is used for 
 *      GET responses, 
 *      SET and ADDOBJ requests
 * ----------------------------------------------------------------- */

/*
 *  Initialize backend message structure (of ep_message_t type)
 *  The caller must supply memory buffer that will be used for keeping
 *  parameters values
 */
int mmx_backapi_msgstruct_init (mmxba_request_t *req, char *mem_buff, 
                                 unsigned short mem_buff_size);

/*
 * Release front-api message structure (ep_message_t).
 * All fields of the message memory pool are initialized.
 * The functions does not perform memory deallocation of the pool!
 */
int mmx_backapi_msgstruct_release (mmxba_request_t *req);


#if 0
/*
 * Insert the value string to the specified name-value pair that is a part 
 * of the specified frontend-api message.
 * The function checks if there is enough space in the message memory pool.
 */
int mmx_backapi_msgstruct_insert_value (mmxba_request_t *req, 
                                         nvpair_t *nvPair, char *value);
#endif
                                          
/*
 * Insert the name and value strings to the specified name-value pair 
 * which is a part of the specified backend-api message.
 * The function checks if there is enough space in the message memory pool.
 */
int mmx_backapi_msgstruct_insert_nvpair(mmxba_request_t *req, nvpair_t *nvPair,
                                         char *name, char *value);


#endif /* MMX_BACKAPI_H_ */
