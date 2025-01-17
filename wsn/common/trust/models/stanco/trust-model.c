#include "trust-model.h"
#include "trust-models.h"
#include "float-helpers.h"
#include "applications.h"
#include "stereotypes.h"
#include "keystore.h"
#include <stdio.h>
#include "os/sys/log.h"
#include "rpl-icmp6.h"
#include "rpl-neighbor.h"
/*-------------------------------------------------------------------------------------------------------------------*/
#define LOG_MODULE "trust-comm"
#ifdef TRUST_MODEL_LOG_LEVEL
#define LOG_LEVEL TRUST_MODEL_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_NONE
#endif
/*-------------------------------------------------------------------------------------------------------------------*/
void edge_resource_tm_init(edge_resource_tm_t* tm)
{
    beta_dist_init(&tm->task_submission, 1, 1);
    beta_dist_init(&tm->task_result, 1, 1);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void edge_resource_tm_print(const edge_resource_tm_t* tm)
{
    printf("EdgeResourceTM(");
    printf("TaskSub=");
    dist_print(&tm->task_submission);
    printf(",TaskRes=");
    dist_print(&tm->task_result);
    printf(")");
}
/*-------------------------------------------------------------------------------------------------------------------*/
void edge_capability_tm_init(edge_capability_tm_t* tm)
{
    beta_dist_init(&tm->result_quality, 1, 1);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void edge_capability_tm_print(const edge_capability_tm_t* tm)
{
    printf("EdgeCapTM(");
    printf("ResQual=");
    dist_print(&tm->result_quality);
    printf(")");
}
/*-------------------------------------------------------------------------------------------------------------------*/
void peer_tm_init(peer_tm_t* tm)
{
}
/*-------------------------------------------------------------------------------------------------------------------*/
void peer_tm_print(const peer_tm_t* tm)
{
    printf("PeerTM(");
    printf(")");
}
/*-------------------------------------------------------------------------------------------------------------------*/
float calculate_trust_value(edge_resource_t* edge, edge_capability_t* capability)
{
    //uip_addr_t* ipaddr = &edge->ep.ipaddr;
    //const linkaddrt *lladdr2
    //link_stats_from_lladdr();
    rpl_neighbor_print_list("Let's print something");

    // Get the stereotype that may inform the trust value
    edge_stereotype_t* s = NULL;
    public_key_item_t* item = keystore_find_addr(&edge->ep.ipaddr);
    if (item != NULL)
    {
        s = edge_stereotype_find(&item->cert.tags);
    }

    float trust = 0;
    float w_total = 0;
    float w, e;

    beta_dist_t temp;

    w = find_trust_weight(capability->name, TRUST_METRIC_TASK_SUBMISSION);
    beta_dist_combine(&edge->tm.task_submission, s ? &s->edge_tm.task_submission : NULL, &temp);
    e = beta_dist_expected(&temp);
    trust += w * e;
    w_total += w;

    w = find_trust_weight(capability->name, TRUST_METRIC_TASK_RESULT);
    beta_dist_combine(&edge->tm.task_result, s ? &s->edge_tm.task_result : NULL, &temp);
    e = beta_dist_expected(&temp);
    trust += w * e;
    w_total += w;

    w = find_trust_weight(capability->name, TRUST_METRIC_RESULT_QUALITY);
    e = beta_dist_expected(&capability->tm.result_quality);
    trust += w * e;
    w_total += w;

#if defined(APPLICATION_CHALLENGE_RESPONSE) && defined(TRUST_MODEL_USE_CHALLENGE_RESPONSE)
    // This application is special, as its result quality applies to
    // other applications too (as long as they specify a weight for it).
    edge_capability_t* cr = edge_info_capability_find(edge, CHALLENGE_RESPONSE_APPLICATION_NAME);
    if (cr != NULL)
    {
        w = find_trust_weight(capability->name, TRUST_METRIC_CHALLENGE_RESP);
        e = beta_dist_expected(&cr->tm.result_quality);
        trust += w * e;
        w_total += w;
    }
#endif

    // The weights should add up to be 1, check this
    if (!isclose(w_total, 1.0f))
    {
        LOG_ERR("The trust weights should total up to be close to 1, they are %f\n", w_total);
    }

    return trust;
}
/*-------------------------------------------------------------------------------------------------------------------*/
void tm_update_task_submission(edge_resource_t* edge, edge_capability_t* cap, const tm_task_submission_info_t* info)
{
    bool should_update;
    const bool good = tm_task_submission_good(info, &should_update);

    if (!should_update)
    {
        return;
    }

    LOG_INFO("Updating Edge %s capability %s TM task_submission (req=%d, coap=%d): ",
        edge_info_name(edge), cap->name, info->coap_request_status, info->coap_status);
    beta_dist_print(&edge->tm.task_submission);
    LOG_INFO_(" -> ");

    if (good)
    {
        beta_dist_add_good(&edge->tm.task_submission);
    }
    else
    {
        beta_dist_add_bad(&edge->tm.task_submission);
    }

    beta_dist_print(&edge->tm.task_submission);
    LOG_INFO_("\n");
}
/*-------------------------------------------------------------------------------------------------------------------*/
void tm_update_task_result(edge_resource_t* edge, edge_capability_t* cap, const tm_task_result_info_t* info)
{
    LOG_INFO("Updating Edge %s capability %s TM task_result (result=%d): ", edge_info_name(edge), cap->name, info->result);
    beta_dist_print(&edge->tm.task_result);
    LOG_INFO_(" -> ");

    if (info->result == TM_TASK_RESULT_INFO_SUCCESS)
    {
        beta_dist_add_good(&edge->tm.task_result);
    }
    else
    {
        beta_dist_add_bad(&edge->tm.task_result);
    }

    beta_dist_print(&edge->tm.task_result);
    LOG_INFO_("\n");
}
/*-------------------------------------------------------------------------------------------------------------------*/
void tm_update_result_quality(edge_resource_t* edge, edge_capability_t* cap, const tm_result_quality_info_t* info)
{
    LOG_INFO("Updating Edge %s capability %s TM result_quality (good=%d): ", edge_info_name(edge), cap->name, info->good);
    beta_dist_print(&cap->tm.result_quality);
    LOG_INFO_(" -> ");

    if (info->good)
    {
        beta_dist_add_good(&cap->tm.result_quality);
    }
    else
    {
        beta_dist_add_bad(&cap->tm.result_quality);
    }

    beta_dist_print(&cap->tm.result_quality);
    LOG_INFO_("\n");
}
/*-------------------------------------------------------------------------------------------------------------------*/
#ifdef APPLICATION_CHALLENGE_RESPONSE
void tm_update_challenge_response(edge_resource_t* edge, const tm_challenge_response_info_t* info)
{
    bool should_update;
    const bool good = tm_challenge_response_good(info, &should_update);

    if (!should_update)
    {
        return;
    }

    edge_capability_t* cap = edge_info_capability_find(edge, CHALLENGE_RESPONSE_APPLICATION_NAME);
    if (cap == NULL)
    {
        LOG_ERR("Failed to find cr application\n");
        return;
    }

    const tm_result_quality_info_t info2 = { .good = good };
    tm_update_result_quality(edge, cap, &info2);
}
#endif
/*-------------------------------------------------------------------------------------------------------------------*/
int serialise_trust_edge_resource(nanocbor_encoder_t* enc, const edge_resource_tm_t* edge)
{
    NANOCBOR_CHECK(nanocbor_fmt_array(enc, 2));
    NANOCBOR_CHECK(dist_serialise(enc, &edge->task_submission));
    NANOCBOR_CHECK(dist_serialise(enc, &edge->task_result));

    return NANOCBOR_OK;
}
/*-------------------------------------------------------------------------------------------------------------------*/
int serialise_trust_edge_capability(nanocbor_encoder_t* enc, const edge_capability_tm_t* cap)
{
    NANOCBOR_CHECK(nanocbor_fmt_array(enc, 1));
    NANOCBOR_CHECK(dist_serialise(enc, &cap->result_quality));

    return NANOCBOR_OK;
}
/*-------------------------------------------------------------------------------------------------------------------*/
int deserialise_trust_edge_resource(nanocbor_value_t* dec, edge_resource_tm_t* edge)
{
    nanocbor_value_t arr;
    NANOCBOR_CHECK(nanocbor_enter_array(dec, &arr));
    NANOCBOR_CHECK(dist_deserialise(&arr, &edge->task_submission));
    NANOCBOR_CHECK(dist_deserialise(&arr, &edge->task_result));

    if (!nanocbor_at_end(&arr))
    {
        return NANOCBOR_ERR_END;
    }

    nanocbor_leave_container(dec, &arr);

    return NANOCBOR_OK;
}
/*-------------------------------------------------------------------------------------------------------------------*/
int deserialise_trust_edge_capability(nanocbor_value_t* dec, edge_capability_tm_t* cap)
{
    nanocbor_value_t arr;
    NANOCBOR_CHECK(nanocbor_enter_array(dec, &arr));
    NANOCBOR_CHECK(dist_deserialise(&arr, &cap->result_quality));

    if (!nanocbor_at_end(&arr))
    {
        return NANOCBOR_ERR_END;
    }

    nanocbor_leave_container(dec, &arr);

    return NANOCBOR_OK;
}
/*-------------------------------------------------------------------------------------------------------------------*/
void tm_process_dio(uip_ipaddr_t *from, rpl_dio_t *dio)
{
    LOG_INFO("Got DIO\n");
    LOG_INFO("from "); LOG_INFO_6ADDR(from); LOG_INFO_("\n");
    LOG_INFO("dag_id "); LOG_INFO_6ADDR(&dio->dag_id); LOG_INFO_("\n");
    LOG_INFO("ocp %" PRIu16 "\n", dio->ocp);
    LOG_INFO("rank %" PRIu16 "\n", dio->rank);
    LOG_INFO("grounded %" PRIu8 "\n", dio->grounded);
    LOG_INFO("mop %" PRIu8 "\n", dio->mop);
    LOG_INFO("preference %" PRIu8 "\n", dio->preference);
    LOG_INFO("version %" PRIu8 "\n", dio->version);
    LOG_INFO("instance_id %" PRIu8 "\n", dio->instance_id);
    LOG_INFO("dtsn %" PRIu8 "\n", dio->dtsn);
    LOG_INFO("dag_intdoubl %" PRIu8 "\n", dio->dag_intdoubl);
    LOG_INFO("dag_intmin %" PRIu8 "\n", dio->dag_intmin);
    LOG_INFO("dag_redund %" PRIu8 "\n", dio->dag_redund);
    LOG_INFO("default_lifetime %" PRIu8 "\n", dio->default_lifetime);
    LOG_INFO("lifetime_unit %" PRIu16 "\n", dio->lifetime_unit);
    LOG_INFO("dag_max_rankinc %" PRIu16 "\n", dio->dag_max_rankinc);
    LOG_INFO("dag_min_hoprankinc %" PRIu16 "\n", dio->dag_min_hoprankinc);
    LOG_INFO("destination_prefix.prefix "); LOG_INFO_6ADDR(&dio->destination_prefix.prefix); LOG_INFO_("\n");
    LOG_INFO("destination_prefix.lifetime %" PRIu32 "\n", dio->destination_prefix.lifetime);
    LOG_INFO("destination_prefix.length %" PRIu8 "\n", dio->destination_prefix.length);
    LOG_INFO("destination_prefix.flags %" PRIu8 "\n", dio->destination_prefix.flags);
    LOG_INFO("prefix_info.prefix "); LOG_INFO_6ADDR(&dio->prefix_info.prefix); LOG_INFO_("\n");
    LOG_INFO("prefix_info.lifetime %" PRIu32 "\n", dio->prefix_info.lifetime);
    LOG_INFO("prefix_info.length %" PRIu8 "\n", dio->prefix_info.length);
    LOG_INFO("prefix_info.flags %" PRIu8 "\n", dio->prefix_info.flags);
    LOG_INFO("dio.mc.type %" PRIu8 "\n", dio->mc.type);
    LOG_INFO("dio.mc.flags %" PRIu8 "\n", dio->mc.flags);
    LOG_INFO("dio.mc.aggr %" PRIu8 "\n", dio->mc.aggr);
    LOG_INFO("dio.mc.prec %" PRIu8 "\n", dio->mc.prec);
    LOG_INFO("dio.mc.length %" PRIu8 "\n", dio->mc.length);
    LOG_INFO("dio.mc.obj.etx %" PRIu16 "\n", dio->mc.obj.etx);
    LOG_INFO("dio.mc.obj.energy.flags %" PRIu8 "\n", dio->mc.obj.energy.flags);
    LOG_INFO("dio.mc.obj.energy.energy_est %" PRIu8 "\n", dio->mc.obj.energy.energy_est);
}
/*-------------------------------------------------------------------------------------------------------------------*/

void tm_process_dis(uip_ipaddr_t *from, int is_multicast)
{
    LOG_INFO("Got DIS\n");
    LOG_INFO("from "); LOG_INFO_6ADDR(from); LOG_INFO_("\n");
}
/*-------------------------------------------------------------------------------------------------------------------*/