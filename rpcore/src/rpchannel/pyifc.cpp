
//author: Vinay Phegade

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include "pyifc.h"
#include "base64.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

int cbuf_to_xmlrpc(const char* func, const char* method, int size, const byte* data, int bufsize, byte* buf) {

	xmlDocPtr doc;
	xmlNodePtr root, params_node, param, param_value, value_type;
	doc = xmlNewDoc(BAD_CAST "1.0");
	root = xmlNewNode(NULL,BAD_CAST "methodCall");
	xmlNewChild(root, NULL, BAD_CAST "methodName", (xmlChar *)method);
	params_node = xmlNewChild(root, NULL, BAD_CAST "params", NULL);
	param = xmlNewChild(params_node, NULL, BAD_CAST "param", NULL);
	param_value = xmlNewChild(param, NULL, BAD_CAST "value", NULL);
	// TODO base64encode the input data make param_value child
	char* encoded_data;
	if (Base64Encode( (char *)data, &(encoded_data)) == 0 ) {
		value_type = xmlNewChild(param_value, NULL, BAD_CAST "string", BAD_CAST encoded_data);
		xmlDocSetRootElement(doc, root);
		xmlChar * xmlbuf;
		xmlDocDumpFormatMemory(doc, &xmlbuf, &bufsize, 0);
		memcpy(buf, xmlbuf, bufsize + 1);
		free(xmlbuf);
		free(encoded_data);
	}
	else {
		bufsize = -1;
	}

	xmlFreeDoc(doc);
	xmlCleanupParser();
	return bufsize;
}

int args_to_xmlrpc(const char* method, int nargs, char** args, int bufsize, byte* buf) {
	xmlDocPtr doc;
	xmlNodePtr root, params_node, param, param_value, value_type;
	doc = xmlNewDoc(BAD_CAST "1.0");
	root = xmlNewNode(NULL,BAD_CAST "methodCall");
	xmlNewChild(root, NULL, BAD_CAST "methodName",(xmlChar *) method);
	params_node = xmlNewChild(root, NULL, BAD_CAST "params", NULL);
	char* encoded_data;
	for( int i = 0 ; i < nargs ; i ++ ) {
		param = xmlNewChild(params_node, NULL, BAD_CAST "param", NULL);
		param_value = xmlNewChild(param, NULL, BAD_CAST "value", NULL);
		// TODO base64encode the input data make param_value child
		if (Base64Encode(args[i], &encoded_data) == 0) {
			xmlNewChild(param_value, NULL, BAD_CAST "string", BAD_CAST encoded_data);
		}
		else {
			xmlFreeDoc(doc);
			xmlCleanupParser();
			return -1;
		}
		//xmlNewChild(param_value, NULL, BAD_CAST "string", BAD_CAST args[i]);
	}
	xmlDocSetRootElement(doc, root);
	xmlChar * xmlbuf;
	xmlDocDumpFormatMemory(doc, &xmlbuf, &bufsize, 0);
	memcpy(buf,xmlbuf, bufsize + 1);

	xmlFreeDoc(doc);
	xmlCleanupParser();
	return bufsize;
}


int xmlrpc_to_cbuf(const char* func, int* psize, byte* data, const byte* buf) {
	xmlDocPtr doc;
	xmlNode * root = NULL, *cur_node = NULL, *param_node = NULL;
	char *method, *param;
	int xmldata_size = -1;

	doc = xmlParseMemory((char *)buf, strlen((char *)buf));
	root = xmlDocGetRootElement(doc);
	char *decoded_data;
	for(cur_node = root->children ; cur_node != NULL ; cur_node = cur_node->next) {
		if( cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (xmlChar *)"methodName")) {
			method = (char *)xmlNodeGetContent(cur_node);
		}
		else if(cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (xmlChar *)"params")) {
			//param_node = xmlNodeGetContent(cur_node);
			for(param_node = cur_node->children; param_node != NULL ; param_node = param_node->next ) {
				if(xmlNodeIsText(param_node))
					continue;
				param = (char *)xmlNodeGetContent(cur_node);

				if (Base64Decode(param, &decoded_data) ) {
					xmldata_size = *psize = -1;
					break;
				}
				*psize = strlen(decoded_data);
				memcpy(data, decoded_data, *psize+1);
				xmldata_size = *psize;
				free(decoded_data);
			}
		}
	}

	xmlFreeDoc(doc);
	xmlCleanupParser();
	return xmldata_size;
}

int xmlrpc_to_args(char** psz, int* pnargs, char**pargs, const byte* buf) {

	xmlDocPtr doc;
	xmlNode *root = NULL, *cur_node = NULL, *param_node = NULL;
	char *method, *param, *decoded_data;
	int i=0, arg_count = 0, status = -1;

	doc = xmlParseMemory((char*)buf, strlen((char*)buf));
	root = xmlDocGetRootElement(doc);

	for(cur_node = root->children; cur_node != NULL; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE  && 
				!xmlStrcmp(cur_node->name, (const xmlChar *) "methodName")) {
			method = (char*)xmlNodeGetContent(cur_node);
		}            
		else if (cur_node->type == XML_ELEMENT_NODE  && 
					!xmlStrcmp(cur_node->name, (const xmlChar *) "params")) {
			for (param_node = cur_node->children; param_node != NULL;  
					param_node = param_node->next) {
				if(xmlNodeIsText(param_node)) 
					continue;
				param = (char*) xmlNodeGetContent(param_node);
				/*if (param[strlen(param)-1] == '\n')
					param[strlen(param)-1] = '\0';
				if (param[0] == '\n')
					param ++;*/
				if(Base64Decode(param, &decoded_data) == 0 ) {
					pargs[arg_count] = strdup( decoded_data);
					free(decoded_data);
					arg_count++;
				}
				else {
					xmlFreeDoc (doc);
					xmlCleanupParser();
					return status;
				}
			}                
		}
	}
	xmlFreeDoc (doc);
	xmlCleanupParser();
	*psz = method;
	*pnargs = arg_count;
	status = *pnargs;

	return status;

}


#ifdef STANDALONE
int main(int argc, char **argv)
{
    xmlDocPtr doc = NULL;       /* document pointer */
    xmlNodePtr root_node = NULL, node = NULL, node1 = NULL;/* node pointers */
    xmlDtdPtr dtd = NULL;       /* DTD pointer */
    char buff[256];
    int i, j;

    LIBXML_TEST_VERSION;

    /*
     * Creates a new document, a node and set it as a root node
     */
    doc = xmlNewDoc(BAD_CAST "1.0");
    root_node = xmlNewNode(NULL, BAD_CAST "root");
    xmlDocSetRootElement(doc, root_node);

    /*
     * Creates a DTD declaration. Isn't mandatory.
     */
    dtd = xmlCreateIntSubset(doc, BAD_CAST "root", NULL, BAD_CAST "tree2.dtd");

    /*
     * xmlNewChild() creates a new node, which is "attached" as child node
     * of root_node node.
     */
    xmlNewChild(root_node, NULL, BAD_CAST "node1",
                BAD_CAST "content of node 1");
    /*
     * The same as above, but the new child node doesn't have a content
     */
    xmlNewChild(root_node, NULL, BAD_CAST "node2", NULL);

    /*
     * xmlNewProp() creates attributes, which is "attached" to an node.
     * It returns xmlAttrPtr, which isn't used here.
     */
    node =
        xmlNewChild(root_node, NULL, BAD_CAST "node3",
                    BAD_CAST "this node has attributes");
    xmlNewProp(node, BAD_CAST "attribute", BAD_CAST "yes");
    xmlNewProp(node, BAD_CAST "foo", BAD_CAST "bar");

    /*
     * Here goes another way to create nodes. xmlNewNode() and xmlNewText
     * creates a node and a text node separately. They are "attached"
     * by xmlAddChild()
     */
    node = xmlNewNode(NULL, BAD_CAST "node4");
    node1 = xmlNewText(BAD_CAST
                   "other way to create content (which is also a node)");
    xmlAddChild(node, node1);
    xmlAddChild(root_node, node);

    /*
     * A simple loop that "automates" nodes creation
     */
    for (i = 5; i < 7; i++) {
        sprintf(buff, "node%d", i);
        node = xmlNewChild(root_node, NULL, BAD_CAST buff, NULL);
        for (j = 1; j < 4; j++) {
            sprintf(buff, "node%d%d", i, j);
            node1 = xmlNewChild(node, NULL, BAD_CAST buff, NULL);
            xmlNewProp(node1, BAD_CAST "odd", BAD_CAST((j % 2) ? "no" : "yes"));
        }
    }
	xmlChar *temp_buffer = (xmlChar*)calloc(1,sizeof(xmlChar)*1024*10);
        int temp_buffer_size = 0;
        //xmlDocDumpFormatMemory(doc, &temp_buffer, &temp_buffer_size, 1);
        //printf("printing from buffer: length : %d  : \n %s\n",temp_buffer_size,temp_buffer);

    /*
     * Dumping document to stdio or file
     */
    xmlSaveFormatFileEnc(argc > 1 ? argv[1] : "-", doc, "UTF-8", 1);

    /*free the document */
    xmlFreeDoc(doc);

    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();

    /*
     * this is to debug memory for regression tests
     */
    xmlMemoryDump();
	char * data = "vijay";
	
	cbuf_to_xmlrpc("foo", "foo", 5, data, temp_buffer_size, temp_buffer);
	printf("cbuf to xmlrpc: length : %d :\n %s\n:",temp_buffer_size,temp_buffer);
	char * buff_arr[2];
	buff_arr[0] = &"vijay";
	buff_arr[1] = &"prakash";
	args_to_xmlrpc("foo", 2, buff_arr, temp_buffer_size, temp_buffer);

	printf("args_to_xmlrpc : length : %d : \n %s\n",temp_buffer_size,temp_buffer);
	return 0;
}
#endif
