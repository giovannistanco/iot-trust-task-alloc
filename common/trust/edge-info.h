#pragma once

#include "contiki.h"
#include "net/ipv6/uip.h"
#include "lib/list.h"

#include <stdbool.h>

#include "trust-common.h"

#include "coap-endpoint.h"

/*-------------------------------------------------------------------------------------------------------------------*/
typedef struct edge_capability
{
	struct edge_capability *next;

	char name[EDGE_CAPABILITY_NAME_LEN + 1];
} edge_capability_t;
/*-------------------------------------------------------------------------------------------------------------------*/
typedef struct edge_resource
{
	struct edge_resource *next;

	uip_ipaddr_t addr;
	char name[MQTT_IDENTITY_LEN + 1];

	LIST_STRUCT(capabilities);
} edge_resource_t;
/*-------------------------------------------------------------------------------------------------------------------*/
void edge_info_init(void);
/*-------------------------------------------------------------------------------------------------------------------*/
edge_resource_t* edge_info_add(uip_ipaddr_t addr, const char* ident);
void edge_info_remove(edge_resource_t* edge);
/*-------------------------------------------------------------------------------------------------------------------*/
edge_resource_t* edge_info_find_addr(uip_ipaddr_t addr);
edge_resource_t* edge_info_find_ident(const char* ident);
/*-------------------------------------------------------------------------------------------------------------------*/
edge_resource_t* edge_info_iter(void);
edge_resource_t* edge_info_next(edge_resource_t* iter);
/*-------------------------------------------------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------------------------------------------------*/
edge_capability_t* edge_info_capability_add(edge_resource_t* edge, const char* name);
/*-------------------------------------------------------------------------------------------------------------------*/
edge_capability_t* edge_info_capability_find(edge_resource_t* edge, const char* name);
/*-------------------------------------------------------------------------------------------------------------------*/
void edge_info_get_server_endpoint(edge_resource_t* edge, coap_endpoint_t* ep, bool secure);
/*-------------------------------------------------------------------------------------------------------------------*/
extern process_event_t pe_edge_capability_add;
extern process_event_t pe_edge_capability_remove;
/*-------------------------------------------------------------------------------------------------------------------*/