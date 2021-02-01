/* mmx-backapi.c
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
 */


/*
 * These file contains functions of MMX Entry-point backend API.
 * The functions are used by the Entry-point worker thread and by the 
 * MMX backends to build and parse XML-string of the management requests
 * and response.
 * MMX management requests/response in XML-format are used for so-called
 * "backend'style" methods.
 */

#include "mmx-backapi.h"


/* MMX backend flags */
char mmxba_flags[] = {'1', 0, 0, 0, 0, 0, 0, 0};


/* ------------------------------------------------------------------- */
/*  MMX Backend internal macros for working with XML string            */
/* ------------------------------------------------------------------- */

#define GOTO_RET_WITH_ERROR(err_num, msg, ...)     do { \
    ing_log(LOG_ERR, msg"\n", ##__VA_ARGS__); \
    status = err_num; \
    goto ret; \
} while (0)


#define XML_GET_INT(node, tree, name, to)     do { \
    mxml_node_t *n = mxmlFindElement(node, tree, name, NULL, NULL, MXML_DESCEND); \
    if (n == NULL) \
        GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, "Could not find tag `%s'", name); \
    const char *s = mxmlGetOpaque(n); \
    to = atoi(s ? s : "0"); \
} while (0)

#define XML_GET_TEXT(node, tree, name, to, size_to)    do { \
    mxml_node_t *n = mxmlFindElement(node, tree, name, NULL, NULL, MXML_DESCEND); \
    if (n == NULL) \
        GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, "Could not find tag `%s'", name); \
    const char *s = mxmlGetOpaque(n); \
    strncpy(to, s ? s : "", size_to); \
} while (0)

#define XML_GET_NODE(node, tree, name, to)    do { \
    to = mxmlFindElement(node, tree, name, NULL, NULL, MXML_DESCEND); \
    if (to == NULL) \
        GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, "Could not find tag `%s'", name); \
} while (0)

#define XML_WRITE_INT(node, tree, name, value)     do { \
    node = mxmlNewElement(tree, name); \
    if (node == NULL) \
        GOTO_RET_WITH_ERROR(MMXBA_SYSTEM_ERROR, "Could not create tag `%s'", name); \
    node = mxmlNewInteger(node, value); \
    if (!node) \
        GOTO_RET_WITH_ERROR(MMXBA_SYSTEM_ERROR, "Could not write `%s'", name); \
} while (0)

#define XML_WRITE_TEXT(node, tree, name, value)     do { \
    node = mxmlNewElement(tree, name); \
    if (node == NULL) \
        GOTO_RET_WITH_ERROR(MMXBA_SYSTEM_ERROR, "Could not create tag `%s'", name); \
    node = mxmlNewText(node, 0, value); \
    if (!node) \
        GOTO_RET_WITH_ERROR(MMXBA_SYSTEM_ERROR, "Could not write `%s'", name); \
} while (0)

#define XML_PARSE_GET_VALUES(req, node, tree, max_elem_num, elem_num, elem_array)  do { \
    const char *arraySizeStr = mxmlElementGetAttrValue(node, MMXBA_STR_ATTR_ARRAYSIZE); \
    if (arraySizeStr == NULL) \
        GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, "Attribute `%s' in not set", \
                                                      MMXBA_STR_ATTR_ARRAYSIZE); \
    long int arraySize = strtol(arraySizeStr, NULL, 10); \
    if (arraySize < 0 || arraySize > max_elem_num) \
        GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, \
                   "Incorrect value of attribute `%s'", MMXBA_STR_ATTR_ARRAYSIZE); \
    elem_num = arraySize; \
    int i = 0; \
    mxml_node_t *subnode; \
    const char *sname, *svalue; \
    for (mxml_node_t *n = mxmlFindElement(node, tree, MMXBA_STR_NAMEVALUEPAIR, NULL, NULL, MXML_DESCEND); \
            n != NULL && i < arraySize; \
            n = mxmlFindElement(n, tree, MMXBA_STR_NAMEVALUEPAIR, NULL, NULL, MXML_DESCEND), i++) \
    { \
        subnode = mxmlFindElement(n, tree, MMXBA_STR_NAME, NULL, NULL, MXML_DESCEND); \
        if (!subnode) \
            GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, "Incorrect syntax: pair name missing"); \
        \
        sname = mxmlGetOpaque(subnode); \
        subnode = mxmlFindElement(n, tree, MMXBA_STR_VALUE, NULL, NULL, MXML_DESCEND); \
        if (!subnode) \
            GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, "Incorrect syntax: pair value missing"); \
        svalue = mxmlGetOpaque(subnode); \
        if (mmx_backapi_msgstruct_insert_nvpair(req, &(elem_array[i]), \
                                               (char *)sname, (char *)svalue) != MMXBA_OK )\
            GOTO_RET_WITH_ERROR(MMXBA_NOT_ENOUGH_MEMORY, "Not enough memory in the pool for param\n"); \
    } \
    if (i != arraySize) \
        GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, "Number of parameters does not match arraySize attribute"); \
} while(0)

#define XML_PARSE_GET_NAMES(node, tree, name_tag, max_elem_num, elem_num, elem_array)  do { \
    const char *arraySizeStr = mxmlElementGetAttrValue(node, MMXBA_STR_ATTR_ARRAYSIZE); \
    if (arraySizeStr == NULL) \
        GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, "Attribute `%s' in not set", \
                                                      MMXBA_STR_ATTR_ARRAYSIZE); \
    long int arraySize = strtol(arraySizeStr, NULL, 10); \
    if (arraySize < 0 || arraySize > max_elem_num) \
        GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, \
                   "Incorrect value of attribute `%s'", MMXBA_STR_ATTR_ARRAYSIZE); \
    elem_num = arraySize; \
    int i = 0; \
    for (mxml_node_t *n = mxmlFindElement(node, tree, name_tag, NULL, NULL, MXML_DESCEND); \
            n != NULL && i < arraySize; \
            n = mxmlFindElement(n, tree, name_tag, NULL, NULL, MXML_DESCEND), i++) \
    { \
        const char *s = mxmlGetOpaque(n); \
        strncpy(elem_array[i], s ? s : "", MMXBA_MAX_STR_LEN-1); \
    } \
    if (i != arraySize) \
        GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, "Number of parameters does not match arraySize attribute"); \
} while(0)

/* ------------------------------------------------------------------- */
/*  -----------  MMX Backend internal functions       -----------------*/
/* ------------------------------------------------------------------- */
static mmxba_op_type_t optype2num(const char *str)
{
    if (!strcmp(str, MMXBA_STR_OPER_GET))           return MMXBA_OP_TYPE_GET;
    else if (!strcmp(str, MMXBA_STR_OPER_SET))      return MMXBA_OP_TYPE_SET;
    else if (!strcmp(str, MMXBA_STR_OPER_GETALL))   return MMXBA_OP_TYPE_GETALL;
    else if (!strcmp(str, MMXBA_STR_OPER_ADDOBJ))   return MMXBA_OP_TYPE_ADDOBJ;
    else if (!strcmp(str, MMXBA_STR_OPER_DELOBJ))   return MMXBA_OP_TYPE_DELOBJ;

    return MMXBA_OP_TYPE_ERROR;
}

static int verify_optype(int optype)
{
    if ((optype == MMXBA_OP_TYPE_GET) || (optype == MMXBA_OP_TYPE_SET) ||
        (optype == MMXBA_OP_TYPE_ADDOBJ) || (optype == MMXBA_OP_TYPE_DELOBJ) ||
        (optype == MMXBA_OP_TYPE_GETALL))
        return TRUE;
    else
        return FALSE;
}

static const char *optype2str(mmxba_op_type_t op_type)
{
    switch(op_type)
    {
    case MMXBA_OP_TYPE_GET: return MMXBA_STR_OPER_GET; break;
    case MMXBA_OP_TYPE_SET: return MMXBA_STR_OPER_SET; break;
    case MMXBA_OP_TYPE_GETALL: return MMXBA_STR_OPER_GETALL; break;
    case MMXBA_OP_TYPE_ADDOBJ: return MMXBA_STR_OPER_ADDOBJ; break;
    case MMXBA_OP_TYPE_DELOBJ: return MMXBA_STR_OPER_DELOBJ; break;
    default: return "UNKNOWN";
    }
}


/* ------------------------------------------------------------------- */
/*  ----------------    MMX Backend API functions  --------------------*/
/* ------------------------------------------------------------------- */
int mmx_backapi_message_hdr_parse( const char *xml_string, 
                                   mmxba_request_t *req)
{
    int status = MMXBA_OK;
    char buf[MMXBA_MAX_STR_OPNAME_LEN];
    char *s;
    char *rootName = NULL;
    int isRequest = FALSE; 

    mxml_node_t *node = NULL;
    mxml_node_t *tree = mxmlLoadString(NULL, xml_string, MXML_OPAQUE_CALLBACK);

    if (!tree || ((rootName = (char *)mxmlGetElement(tree)) == NULL))
        GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, "Cannot load XML string of the message\n");
    
    //ing_log(LOG_INFO,"%s: rootName = %s\n", __func__, rootName);
    
    if (strcmp(rootName, MMXBA_STR_REQUEST) && strcmp(rootName, MMXBA_STR_RESPONSE))
        GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, "Bad type of management message\n");
    
    if (!strcmp(rootName, MMXBA_STR_REQUEST))
        isRequest = TRUE;

    /* Parse the common fields used both in request and in response*/
    XML_GET_TEXT(tree, tree, MMXBA_STR_OPNAME, buf, sizeof(buf));
    req->op_type = optype2num(buf);
    if (!verify_optype(req->op_type))
        ing_log(LOG_DEBUG,"%s: Unknown operation type %d\n", __func__, req->op_type);

    XML_GET_INT(tree, tree, MMXBA_STR_SEQNUM, req->opSeqNum);

    XML_GET_TEXT(tree, tree, MMXBA_STR_BEOBJNAME, req->beObjName, sizeof(req->beObjName));
    
    /* Parse result code and error elements - used in responses only */
    if (!isRequest)
    {
        node = mxmlFindElement(tree, tree, MMXBA_STR_OPRESCODE, NULL, NULL, MXML_DESCEND);
        if (node)
        {
            s = (char *)mxmlGetOpaque(node);
            req->opResCode = atoi(s ? s : "0");
        }
        node = mxmlFindElement(tree, tree, MMXBA_STR_OPEXTCODE, NULL, NULL, MXML_DESCEND);
        if (node)
        {
            s = (char *)mxmlGetOpaque(node);
            req->opExtErrCode = atoi(s ? s : "0");
        }
        node = mxmlFindElement(tree, tree, MMXBA_STR_ERRMSG, NULL, NULL, MXML_DESCEND);
        if (node)
        {
            s = (char *)mxmlGetOpaque(node);
            if (s)
                strcpy_safe(req->errMsg, s, sizeof(req->errMsg));
        }

        node = mxmlFindElement(tree, tree, MMX_STR_POSTOPSTATUS, NULL, NULL, MXML_DESCEND);
        if (node)
        {
            s = (char *)mxmlGetOpaque(node);
            req->postOpStatus = atoi(s ? s : "0");
        }
    }
    
    /*ing_log(LOG_DEBUG, "%s: Finish: optype %d, seqnum %d, be obj name %s\n", 
                       __func__, req->op_type,req->opSeqNum, req->beObjName); */
    
ret:
    mxmlDelete(tree);
    return status;
}


int mmx_backapi_message_parse(const char *xml_string, mmxba_request_t *req)
{
    int status = MMXBA_OK;
    char buf[MMXBA_MAX_STR_OPNAME_LEN];
    char *s;
    char *rootName = NULL;
    int isRequest = FALSE; 

    mxml_node_t *node = NULL;
    mxml_node_t *tree = mxmlLoadString(NULL, xml_string, MXML_OPAQUE_CALLBACK);
    
    /*ing_log(LOG_DEBUG, "%s: mem pool info: status %d, size %d bytes, addr 0x%lx\n",
            __func__, req->mem_pool.initialized, req->mem_pool.size_bytes, &(req->mem_pool));*/


    if (!tree || ((rootName = (char *)mxmlGetElement(tree)) == NULL))
        GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, "Cannot load XML string of the message");
    
    /*ing_log(LOG_INFO,"%s: rootName = %s\n", __func__, rootName);*/
    
    if (strcmp(rootName, MMXBA_STR_REQUEST) && strcmp(rootName, MMXBA_STR_RESPONSE))
        GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, "Bad type of management message");
    
    if (!strcmp(rootName, MMXBA_STR_REQUEST))
        isRequest = TRUE;

    /* Parse the common fields used both in request and in response*/
    XML_GET_TEXT(tree, tree, MMXBA_STR_OPNAME, buf, sizeof(buf));
    req->op_type = optype2num(buf);
    if (!verify_optype(req->op_type))
        ing_log(LOG_DEBUG,"%s: Unknown operation type %d\n", __func__, req->op_type);

    XML_GET_INT(tree, tree, MMXBA_STR_SEQNUM, req->opSeqNum);

    XML_GET_TEXT(tree, tree, MMXBA_STR_BEOBJNAME, req->beObjName, sizeof(req->beObjName));
    
    /* Parse result code and error elements - used in responses only */
    if (!isRequest)
    {
        node = mxmlFindElement(tree, tree, MMXBA_STR_OPRESCODE, NULL, NULL, MXML_DESCEND);
        if (node)
        {
            s = (char *)mxmlGetOpaque(node);
            req->opResCode = atoi(s ? s : "0");
        }
        node = mxmlFindElement(tree, tree, MMXBA_STR_OPEXTCODE, NULL, NULL, MXML_DESCEND);
        if (node)
        {
            s = (char *)mxmlGetOpaque(node);
            req->opExtErrCode = atoi(s ? s : "0");
        }
        node = mxmlFindElement(tree, tree, MMXBA_STR_ERRMSG, NULL, NULL, MXML_DESCEND);
        if (node)
        {
            s = (char *)mxmlGetOpaque(node);
            if (s)
                strcpy_safe(req->errMsg, s, sizeof(req->errMsg));
        }

        node = mxmlFindElement(tree, tree, MMX_STR_POSTOPSTATUS, NULL, NULL, MXML_DESCEND);
        if (node)
        {
            s = (char *)mxmlGetOpaque(node);
            req->postOpStatus = atoi(s ? s : "0");
        }
    }
    
    /* Parse  MMX instance element */
    if (req->op_type == MMXBA_OP_TYPE_GET || req->op_type == MMXBA_OP_TYPE_SET ||
        req->op_type == MMXBA_OP_TYPE_ADDOBJ || req->op_type == MMXBA_OP_TYPE_DELOBJ)
    {
        XML_GET_TEXT(tree, tree, MMXBA_STR_MMXINSTANCE, req->mmxInstances,
                     sizeof(req->mmxInstances));
    }
    
    /* Parse backend key parameters - used for GET/SET/DELOBJ operations*/
    if (req->op_type == MMXBA_OP_TYPE_GET || req->op_type == MMXBA_OP_TYPE_SET ||
        req->op_type == MMXBA_OP_TYPE_ADDOBJ || req->op_type == MMXBA_OP_TYPE_DELOBJ)
    {
        XML_GET_NODE(tree, tree, MMXBA_STR_BEKEYPARAMS, node);
        XML_PARSE_GET_VALUES(req, node, tree, MMXBA_MAX_NUMBER_OF_KEY_PARAMS, 
                             req->beKeyParamsNum, req->beKeyParams);
    }

    /* Parse param names - used only in GET request */
    if (isRequest && req->op_type == MMXBA_OP_TYPE_GET)
    { 
        if ((node = mxmlFindElement(tree, tree, MMXBA_STR_PARAMNAMES, NULL, NULL, MXML_DESCEND)) != NULL)
        {
            XML_PARSE_GET_NAMES(node, tree, MMXBA_STR_NAME, MMXBA_MAX_NUMBER_OF_GET_PARAMS, 
                                req->paramNames.arraySize, req->paramNames.paramNames);
        }
        else  /* print for debugging only */
            ing_log(LOG_DEBUG,"%s:GET request does not contain param names\n", __func__);
    }
    
    /* Parse param-name value pairs: used in GET response and SET request*/
    if ((!isRequest && req->op_type == MMXBA_OP_TYPE_GET) || 
         (isRequest && req->op_type == MMXBA_OP_TYPE_SET))
    {
        if ((node = mxmlFindElement(tree, tree, MMXBA_STR_PARAMVALUES, 
                                    NULL, NULL, MXML_DESCEND)) != NULL)
        {
            XML_PARSE_GET_VALUES(req, node, tree, MMXBA_MAX_NUMBER_OF_SET_PARAMS, 
                                 req->paramValues.arraySize, req->paramValues.paramValues);
        }
    }
    
    /* Parse BE key param names in GETALL request and response*/
    if (req->op_type == MMXBA_OP_TYPE_GETALL)
    {
       if ((node = mxmlFindElement(tree, tree, MMXBA_STR_BEKEYNAMES, 
                                         NULL, NULL, MXML_DESCEND)) == NULL)
            GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, MMXBA_STR_BEKEYNAMES" tag not found");

        XML_PARSE_GET_NAMES(node, tree, MMXBA_STR_NAME, MMXBA_MAX_NUMBER_OF_KEY_PARAMS,
                            req->getAll.beKeyNamesNum, req->getAll.beKeyNames); 
        
    }
    
    /* Parse BE key param names in ADDOBJ request and response*/
    if (req->op_type == MMXBA_OP_TYPE_ADDOBJ)
    {
        if ((node = mxmlFindElement(tree, tree, MMXBA_STR_BEKEYNAMES, 
                                           NULL, NULL, MXML_DESCEND)) == NULL)
            GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, MMXBA_STR_BEKEYNAMES" tag not found");

        XML_PARSE_GET_NAMES(node, tree, MMXBA_STR_NAME, MMXBA_MAX_NUMBER_OF_KEY_PARAMS, 
                            req->addObj_resp.beKeyNamesNum, req->addObj_resp.beKeyNames);
    }
    
    /* Parse param-name value pairs used in ADDOBJ request*/
    if (isRequest && req->op_type == MMXBA_OP_TYPE_ADDOBJ)
    {
        if ((node = mxmlFindElement(tree, tree, MMXBA_STR_PARAMVALUES, 
                                    NULL, NULL, MXML_DESCEND)) != NULL)
        {
            XML_PARSE_GET_VALUES(req, node, tree, MMXBA_MAX_NUMBER_OF_SET_PARAMS, 
                    req->addObj_req.paramNum, req->addObj_req.paramValues);
        }
    }
    
    /* Parse BE key param names and key values ("BE object instance")
       in GETALL and ADDOBJ responses                                */
    if (!isRequest && req->op_type == MMXBA_OP_TYPE_GETALL)
    {
        if ((node = mxmlFindElement(tree, tree, MMXBA_STR_OBJECTS, 
                                         NULL, NULL, MXML_DESCEND)) != NULL)
        {
            XML_PARSE_GET_NAMES(node, tree, MMXBA_STR_OBJKEYVALUES, 
                MMXBA_MAX_NUMBER_OF_GETALL_PARAMS, req->getAll.objNum, req->getAll.objects);
        }
    }

    if (!isRequest && req->op_type == MMXBA_OP_TYPE_ADDOBJ)
    {
        /* Parse addStatus in ADOBG response */
        
        if ((node = mxmlFindElement(tree, tree, MMXBA_STR_OBJECTS, 
                                    NULL, NULL, MXML_DESCEND)) != NULL)
        {
            XML_PARSE_GET_NAMES(node, tree, MMXBA_STR_OBJKEYVALUES, 1,
                                req->addObj_resp.objNum, req->addObj_resp.objects);
        }
    }

ret:
    mxmlDelete(tree);
    return status;
}

int mmx_backapi_request_build(mmxba_request_t *req, char *xml_string, size_t xml_string_size)
{
    int status = MMXBA_OK;
    int i = 0;
    int arraySize = 0;
    char buf[MMXBA_MAX_NUMBER_OF_ANY_OP_PARAMS];
    nvpair_t  *pnv = NULL;  /* param name-value pairs*/
    char *beKeyName;

    mxml_node_t *tree = NULL, *node = NULL;
    mxml_node_t *subnode1 = NULL, *subnode2 = NULL;

    if (!verify_optype(req->op_type))
        GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, "%s: Unknown operation type %d", 
                            __func__, req->op_type);

    tree = mxmlNewElement(MXML_NO_PARENT, MMXBA_STR_REQUEST);
    if (!tree)
        GOTO_RET_WITH_ERROR(MMXBA_SYSTEM_ERROR, "Could not create root element");

    /* ---- Add common header nodes that used by all requests -----*/
    XML_WRITE_TEXT(node, tree, MMXBA_STR_OPNAME, optype2str(req->op_type));
    XML_WRITE_INT( node, tree, MMXBA_STR_SEQNUM, req->opSeqNum);
    XML_WRITE_TEXT(node, tree, MMXBA_STR_BEOBJNAME, req->beObjName);
    
    /* ---- Add different nodes for different request types ----- */
    
    /* MMX instance is known and so should be added to the request 
       for GET, SET, ADDOBJ, DELOBJ requests */
    if (req->op_type == MMXBA_OP_TYPE_GET || req->op_type == MMXBA_OP_TYPE_SET ||
        req->op_type == MMXBA_OP_TYPE_ADDOBJ || req->op_type == MMXBA_OP_TYPE_DELOBJ)
    {
        XML_WRITE_TEXT(node, tree, MMXBA_STR_MMXINSTANCE, req->mmxInstances);
    }

    /* Values of backend key parameters are used for GET, SET, ADD/DELOBJ */
    if (req->op_type == MMXBA_OP_TYPE_GET || req->op_type == MMXBA_OP_TYPE_SET ||
        req->op_type == MMXBA_OP_TYPE_DELOBJ || req->op_type == MMXBA_OP_TYPE_ADDOBJ)
    {
        node = mxmlNewElement(tree, MMXBA_STR_BEKEYPARAMS);
        sprintf(buf, "%d", req->beKeyParamsNum);
        mxmlElementSetAttr(node, MMXBA_STR_ATTR_ARRAYSIZE, buf);

        for (i = 0; i < req->beKeyParamsNum; i++)
        {
            subnode1 = mxmlNewElement(node, MMXBA_STR_NAMEVALUEPAIR);
            subnode2 = mxmlNewElement(subnode1, MMXBA_STR_NAME);
            mxmlNewText(subnode2, 0, req->beKeyParams[i].name);
            subnode2 = mxmlNewElement(subnode1, MMXBA_STR_VALUE);
            mxmlNewText(subnode2, 0, req->beKeyParams[i].pValue);
        }
    }
    
    /* Names of backend key params are used for GETALL and ADDOBJ requests */
    if (req->op_type == MMXBA_OP_TYPE_GETALL || req->op_type == MMXBA_OP_TYPE_ADDOBJ )
    {
        arraySize = (req->op_type == MMXBA_OP_TYPE_GETALL) ? 
                req->getAll.beKeyNamesNum : req->addObj_req.beKeyNamesNum;
        
        sprintf(buf, "%d", arraySize);
        node = mxmlNewElement(tree, MMXBA_STR_BEKEYNAMES);
        mxmlElementSetAttr(node, MMXBA_STR_ATTR_ARRAYSIZE, buf);

        for (i = 0; i < arraySize; i++)
        {
            beKeyName = (req->op_type == MMXBA_OP_TYPE_GETALL) ? 
                    (char*)req->getAll.beKeyNames[i] : (char*)req->addObj_req.beKeyNames[i];
            subnode1 = mxmlNewElement(node, MMXBA_STR_NAME);
            mxmlNewText(subnode1, 0, beKeyName);
        }
    }
    
    /* Param names array is used for GET request */
    if (req->op_type == MMXBA_OP_TYPE_GET)
    {
        node = mxmlNewElement(tree, MMXBA_STR_PARAMNAMES);
        sprintf(buf, "%d", req->paramNames.arraySize);
        mxmlElementSetAttr(node, MMXBA_STR_ATTR_ARRAYSIZE, buf);

        for (i = 0; i < req->paramNames.arraySize; i++)
        {
            subnode1 = mxmlNewElement(node, MMXBA_STR_NAME);
            mxmlNewText(subnode1, 0, req->paramNames.paramNames[i]);
        }
    }

    /* Param values array is used for SET and ADDOBJ request */
    if (req->op_type == MMXBA_OP_TYPE_SET || req->op_type == MMXBA_OP_TYPE_ADDOBJ)
    {
        if (req->op_type == MMXBA_OP_TYPE_SET)
        {
            arraySize = req->paramValues.arraySize;
            pnv = (nvpair_t *)&req->paramValues.paramValues;
        }
        else 
        {
            arraySize = req->addObj_req.paramNum;
            pnv = (nvpair_t *)&req->addObj_req.paramValues;
        }
        
        node = mxmlNewElement(tree, MMXBA_STR_PARAMVALUES);
        sprintf(buf, "%d", arraySize);
        mxmlElementSetAttr(node, MMXBA_STR_ATTR_ARRAYSIZE, buf);

        for (i = 0; i < arraySize; i++)
        {
            subnode1 = mxmlNewElement(node, MMXBA_STR_NAMEVALUEPAIR);
            subnode2 = mxmlNewElement(subnode1, MMXBA_STR_NAME);
            mxmlNewText(subnode2, 0, pnv[i].name);
            subnode2 = mxmlNewElement(subnode1, MMXBA_STR_VALUE);
            mxmlNewText(subnode2, 0, pnv[i].pValue);
        }
    }
    
    if (mxmlSaveString(tree, xml_string, xml_string_size, MXML_NO_CALLBACK) <= 0)
        GOTO_RET_WITH_ERROR(MMXBA_SYSTEM_ERROR, "Could not save request to string");

ret:
    if (tree)
        mxmlDelete(tree);

    return status;
}

int mmx_backapi_response_build(mmxba_request_t *req, char *xml_string, size_t xml_string_size)
{
    int status = MMXBA_OK;
    int i = 0;
    char buf[MMXBA_MAX_NUMBER_OF_ANY_OP_PARAMS];
    int arraySize = 0;
    char * tempStr;

    mxml_node_t *tree = NULL, *node = NULL;
    mxml_node_t *subnode1 = NULL, *subnode2 = NULL;

    if (!verify_optype(req->op_type))
        GOTO_RET_WITH_ERROR(MMXBA_INVALID_FORMAT, "%s: Unknown operation type %d", 
                            __func__, req->op_type);
    

    tree = mxmlNewElement(MXML_NO_PARENT, MMXBA_STR_RESPONSE);
    if (!tree)
        GOTO_RET_WITH_ERROR(MMXBA_SYSTEM_ERROR, "Could not create root element");

   /* ---- Add common header nodes that used by all responses -----*/
    XML_WRITE_TEXT(node, tree, MMXBA_STR_OPNAME, optype2str(req->op_type));
    XML_WRITE_INT(node, tree, MMXBA_STR_SEQNUM, req->opSeqNum);
    XML_WRITE_INT(node, tree, MMXBA_STR_OPRESCODE, req->opResCode);
    XML_WRITE_INT(node, tree, MMXBA_STR_OPEXTCODE, req->opExtErrCode);
    XML_WRITE_TEXT(node, tree, MMXBA_STR_ERRMSG, req->errMsg);
    XML_WRITE_INT(node, tree, MMX_STR_POSTOPSTATUS, req->postOpStatus);
    XML_WRITE_TEXT(node, tree, MMXBA_STR_BEOBJNAME, req->beObjName);

    /* MMX instance should be added to the response of 
       GET, SET, ADDOBJ, DELOBJ operations */
    if (req->op_type == MMXBA_OP_TYPE_GET || req->op_type == MMXBA_OP_TYPE_SET ||
         req->op_type == MMXBA_OP_TYPE_ADDOBJ || req->op_type == MMXBA_OP_TYPE_DELOBJ)
    {
        XML_WRITE_TEXT(node, tree, MMXBA_STR_MMXINSTANCE, req->mmxInstances);
    }

    /* Add BE key params for GET/SET/DELOBJ response: the same as in requests*/
    if (req->op_type == MMXBA_OP_TYPE_GET || req->op_type == MMXBA_OP_TYPE_SET ||
        req->op_type == MMXBA_OP_TYPE_ADDOBJ || req->op_type == MMXBA_OP_TYPE_DELOBJ )
    {
        node = mxmlNewElement(tree, MMXBA_STR_BEKEYPARAMS);
        sprintf(buf, "%d", req->beKeyParamsNum);
        mxmlElementSetAttr(node, MMXBA_STR_ATTR_ARRAYSIZE, buf);

        for (i = 0; i < req->beKeyParamsNum; i++)
        {
            subnode1 = mxmlNewElement(node, MMXBA_STR_NAMEVALUEPAIR);
            subnode2 = mxmlNewElement(subnode1, MMXBA_STR_NAME);
            mxmlNewText(subnode2, 0, req->beKeyParams[i].name);
            subnode2 = mxmlNewElement(subnode1, MMXBA_STR_VALUE);
            mxmlNewText(subnode2, 0, req->beKeyParams[i].pValue);
        }
    }

    /* Add resulting name-value parameters for GET response */
    if (req->op_type == MMXBA_OP_TYPE_GET) 
    {
        node = mxmlNewElement(tree, MMXBA_STR_PARAMVALUES);
        sprintf(buf, "%d", req->paramValues.arraySize);
        mxmlElementSetAttr(node, MMXBA_STR_ATTR_ARRAYSIZE, buf);

        for (i = 0; i < req->paramValues.arraySize; i++)
        {
            subnode1 = mxmlNewElement(node, MMXBA_STR_NAMEVALUEPAIR);
            subnode2 = mxmlNewElement(subnode1, MMXBA_STR_NAME);
            mxmlNewText(subnode2, 0, req->paramValues.paramValues[i].name);
            subnode2 = mxmlNewElement(subnode1, MMXBA_STR_VALUE);
            mxmlNewText(subnode2, 0, req->paramValues.paramValues[i].pValue);
        }
    }

    /* Add BE key param names and BE instance for GETALL and ADDOBJ responses*/
    if (req->op_type == MMXBA_OP_TYPE_GETALL || 
        req->op_type == MMXBA_OP_TYPE_ADDOBJ)
    {
        /* Process BE key names array */
        arraySize = (req->op_type == MMXBA_OP_TYPE_GETALL) ? 
                     req->getAll.beKeyNamesNum : req->addObj_resp.beKeyNamesNum;
        node = mxmlNewElement(tree, MMXBA_STR_BEKEYNAMES);
        sprintf(buf, "%d", arraySize);
        mxmlElementSetAttr(node, MMXBA_STR_ATTR_ARRAYSIZE, buf);

        for (i = 0; i < arraySize; i++)
        {
            tempStr = (req->op_type == MMXBA_OP_TYPE_GETALL) ? 
                    (char*)req->getAll.beKeyNames[i] : 
                    (char*)req->addObj_resp.beKeyNames[i];
            subnode1 = mxmlNewElement(node, MMXBA_STR_NAME);
            mxmlNewText(subnode1, 0, tempStr);
        }

        /* Process BE objects array */
        arraySize = (req->op_type == MMXBA_OP_TYPE_GETALL) ? 
                          req->getAll.objNum : req->addObj_resp.objNum;
        node = mxmlNewElement(tree, MMXBA_STR_OBJECTS);
        sprintf(buf, "%d", arraySize);
        mxmlElementSetAttr(node, MMXBA_STR_ATTR_ARRAYSIZE, buf);

        for (i = 0; i < arraySize; i++)
        {
            tempStr = (req->op_type == MMXBA_OP_TYPE_GETALL) ? 
                             (char*)req->getAll.objects[i] :
                             (char*)req->addObj_resp.objects[i];
            subnode1 = mxmlNewElement(node, MMXBA_STR_OBJKEYVALUES);
            mxmlNewText(subnode1, 0, tempStr);
        }
    }

    if (mxmlSaveString(tree, xml_string, xml_string_size, MXML_NO_CALLBACK) <= 0)
        GOTO_RET_WITH_ERROR(MMXBA_SYSTEM_ERROR, "Could not save request to string");

ret:
    if (tree)
        mxmlDelete(tree);
    
    return status;
}


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
                                 unsigned short mem_buff_size)
{
    int status = 0;

    //ing_log(LOG_DEBUG,"%s - Start\n", __func__); 
 
    if (req == NULL || mem_buff == NULL || mem_buff_size <= 16)
        GOTO_RET_WITH_ERROR(MMXBA_BAD_INPUT_PARAMS, "%s: Bad input parameters\n", __func__);

    memset(mem_buff, 0, mem_buff_size);

    req->mem_pool.pool = mem_buff;
    req->mem_pool.size_bytes = mem_buff_size;

    req->mem_pool.curr_offset = 0;
    req->mem_pool.initialized = 1;

ret:
    return status;
}
 
 /*
  * Release front-api message structure (ep_message_t).
  * All fields of the message memory pool are initialized.
  * The functions does not perform memory deallocation of the pool!
  */
int mmx_backapi_msgstruct_release (mmxba_request_t *req)
{
    req->mem_pool.pool = 0;
    req->mem_pool.size_bytes = 0;
    req->mem_pool.curr_offset = 0;

    req->mem_pool.initialized = 0;

    return 0;
}


/*
 * Insert the value string to the specified name-value pair that is a part 
 * of the specified backend-api message.
 * The function checks if there is enough space in the message memory pool.
 */
 #if 0
int mmx_backapi_msgstruct_insert_value (mmxba_request_t *req, 
                                        nvpair_t *nvPair, char *value)
{
    int    status = 0;
    int    val_len = 0;
    size_t perm_len = 0;

    /*if (req == NULL || !req->mem_pool.initialized || !nvPair)
        GOTO_RET_WITH_ERROR(MMXBA_BAD_INPUT_PARAMS, 
            "Bad input params (init flag = %d)", req->mem_pool.initialized);*/
   if (req == NULL)
        GOTO_RET_WITH_ERROR(MMXBA_BAD_INPUT_PARAMS, "Bad input params 1 (req = NULL)");
            
   if (!req->mem_pool.initialized)
        GOTO_RET_WITH_ERROR(MMXBA_NOT_INITIALIZED, 
            "Bad input params 2 (init flag = %d)", req->mem_pool.initialized);
      
   if (!nvPair)
        GOTO_RET_WITH_ERROR(MMXBA_BAD_INPUT_PARAMS, "Bad input params 3 (nvpair = NULL)");

    perm_len = (size_t)(req->mem_pool.size_bytes - req->mem_pool.curr_offset);

    if (value)
        val_len = strlen(value);
    
    if(perm_len < val_len + 1)
        GOTO_RET_WITH_ERROR(MMXBA_NOT_ENOUGH_MEMORY, 
        "No space in back-api msg (val len %d, permitted len %d", val_len, perm_len);

    nvPair->pValue = req->mem_pool.pool + req->mem_pool.curr_offset;
    strcpy(nvPair->pValue, value ? value : "");

    req->mem_pool.curr_offset += (val_len + 1);
    /*ing_log(LOG_DEBUG,"Backend api msg pool: curr offset %d, value %s\n",
               req->mem_pool.curr_offset,nvPair->pValue); */

ret:
    return status;
}
#endif


/*
 * Insert the name and value strings to the specified name-value pair 
 * which is a part of the specified backend-api message.
 * The function checks if there is enough space in the message memory pool.
 */
int mmx_backapi_msgstruct_insert_nvpair(mmxba_request_t *req, nvpair_t *nvPair,
                                         char *name, char *value)
{
    int    status = 0;
    int    val_len = 0;
    size_t perm_len = 0;

   /* Verify input parameters */
   if ((req == NULL) || (name == NULL))
        GOTO_RET_WITH_ERROR(MMXBA_BAD_INPUT_PARAMS, "Bad input params (req or name = NULL)");

   if (!nvPair)
        GOTO_RET_WITH_ERROR(MMXBA_BAD_INPUT_PARAMS, "Bad input param (nvpair = NULL)");

   if (!req->mem_pool.initialized)
        GOTO_RET_WITH_ERROR(MMXBA_NOT_INITIALIZED, 
            "Bad input param (init flag = %d)", req->mem_pool.initialized);

    /* Check if we have enough space in the memory pool */
    perm_len = (size_t)(req->mem_pool.size_bytes - req->mem_pool.curr_offset);

    if (value)
        val_len = strlen(value);
    
    if(perm_len < val_len + 1)
        GOTO_RET_WITH_ERROR(MMXBA_NOT_ENOUGH_MEMORY, 
        "No space in back-api req pool (val len %d, permitted len %d", val_len, perm_len);

    /* Copy the value string to the memory pool and update the pool offset */
    nvPair->pValue = req->mem_pool.pool + req->mem_pool.curr_offset;
    strcpy(nvPair->pValue, value ? value : "");

    req->mem_pool.curr_offset += (val_len + 1);
    
    /* Copy the name string to the nvpair struct */
    if (sizeof(nvPair->name) < strlen(name))
        ing_log(LOG_DEBUG, "BE API: param name %s is truncated (permitted len is %d bytes)\n",
                            name, sizeof(nvPair->name));

    strcpy_safe(nvPair->name, name, sizeof(nvPair->name));
    
    /*ing_log(LOG_DEBUG,"Backend api msg pool: curr offset %d, value %s\n",
               req->mem_pool.curr_offset,nvPair->pValue); */

ret:
    return status;
}
