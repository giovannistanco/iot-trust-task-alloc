#pragma once

#include "distributions.h"

#include "nanocbor-helper.h"

#define TRUST_MODEL_TAG 8
#define TRUST_MODEL_NO_PEER_PROVIDED
#define TRUST_MODEL_NO_PERIODIC_BROADCAST

#ifndef APPLICATIONS_MONITOR_THROUGHPUT
#error "Must define APPLICATIONS_MONITOR_THROUGHPUT"
#endif
#ifndef TRUST_MODEL_PERIODIC_EDGE_PING
#error "Must define TRUST_MODEL_PERIODIC_EDGE_PING"
#endif

struct edge_resource;
struct edge_capability;

/*-------------------------------------------------------------------------------------------------------------------*/
// Per-Edge interactions
typedef struct edge_resource_tm {
    // When submitting a task, did the Edge accept it correctly?
    beta_dist_t task_submission;

    // Was a task result received when it was expected
    beta_dist_t task_result;

    // Last time a ping was received
    clock_time_t last_ping_response;

} edge_resource_tm_t;
/*-------------------------------------------------------------------------------------------------------------------*/
void edge_resource_tm_init(edge_resource_tm_t* tm);
void edge_resource_tm_print(const edge_resource_tm_t* tm);
/*-------------------------------------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------------------------------------*/
// Per-Application of Edge interactions
typedef struct edge_capability_tm {
    // Was the result correct or not (nodes do not have the capability to evaluate response 'goodness')
    beta_dist_t result_quality;

    gaussian_dist_t throughput_in;
    gaussian_dist_t throughput_out;

} edge_capability_tm_t;
/*-------------------------------------------------------------------------------------------------------------------*/
void edge_capability_tm_init(edge_capability_tm_t* tm);
void edge_capability_tm_print(const edge_capability_tm_t* tm);
/*-------------------------------------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------------------------------------*/
typedef struct peer_tm {

} peer_tm_t;
/*-------------------------------------------------------------------------------------------------------------------*/
void peer_tm_init(peer_tm_t* tm);
void peer_tm_print(const peer_tm_t* tm);
/*-------------------------------------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------------------------------------*/
float calculate_trust_value(struct edge_resource* edge, struct edge_capability* capability);
/*-------------------------------------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------------------------------------*/
int serialise_trust_edge_resource(nanocbor_encoder_t* enc, const edge_resource_tm_t* edge);
int serialise_trust_edge_capability(nanocbor_encoder_t* enc, const edge_capability_tm_t* cap);
int deserialise_trust_edge_resource(nanocbor_value_t* dec, edge_resource_tm_t* edge);
int deserialise_trust_edge_capability(nanocbor_value_t* dec, edge_capability_tm_t* cap);
/*-------------------------------------------------------------------------------------------------------------------*/
