#include "trust-model.h"
#include "trust-models.h"
#include "float-helpers.h"
#include "applications.h"
#include "stereotypes.h"
#include "keystore.h"
#include <stdio.h>
#include "os/sys/log.h"
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

    tm->last_ping_response = 0;
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
    gaussian_dist_init_empty(&tm->throughput_in);
    gaussian_dist_init_empty(&tm->throughput_out);
}
/*-------------------------------------------------------------------------------------------------------------------*/
void edge_capability_tm_print(const edge_capability_tm_t* tm)
{
    printf("EdgeCapTM(");
    printf("ResQual=");
    dist_print(&tm->result_quality);
    printf(",ThroughputIn=");
    dist_print(&tm->throughput_in);
    printf(",ThroughputOut=");
    dist_print(&tm->throughput_out);
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
static float goodness_of_throughput(const edge_capability_t* capability)
{
    const gaussian_dist_t* in = &capability->tm.throughput_in;
    const gaussian_dist_t* out = &capability->tm.throughput_out;

    const trust_throughput_threshold_t* info = trust_throughput_thresholds_find(capability->name);
    ASSERT(info != NULL);

    if (in->count == 0 && out->count == 0)
    {
        // No values, so just return the best
        return 1;
    }
    else if (in->count == 0 && out->count > 0)
    {
        if (out->mean < info->out_threshold)
        {
            return 0;
        }
        else
        {
            return 1;
        }
        
    }
    else
    {
        // How good is both incoming and outgoing throughput
        if (in->mean >= info->in_threshold && out->mean >= info->out_threshold)
        {
            return 1;
        }
        else if (in->mean < info->in_threshold && out->mean > ((info->out_threshold/info->in_threshold)*in->mean) )
        {
            return (in->mean)/info->in_threshold;
        }
        else
        {
            return (out->mean)/info->out_threshold;
        }
        
    }
}
/*-------------------------------------------------------------------------------------------------------------------*/
float calculate_trust_value(edge_resource_t* edge, edge_capability_t* capability)
{
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

    w = find_trust_weight(capability->name, TRUST_METRIC_THROUGHPUT);
    e = goodness_of_throughput(capability);
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
void tm_update_task_throughput(edge_resource_t* edge, edge_capability_t* cap, const tm_throughput_info_t* info)
{
    if (info->direction == TM_THROUGHPUT_IN)
    {
        LOG_INFO("Updating Edge %s capability %s TM throughput in (%" PRIu32 " bytes/tick): ",
        edge_info_name(edge), cap->name, info->throughput);
        gaussian_dist_print(&cap->tm.throughput_in);
        LOG_INFO_(" -> ");

        gaussian_dist_update(&cap->tm.throughput_in, info->throughput);

        gaussian_dist_print(&cap->tm.throughput_in);
        LOG_INFO_("\n");
    }
    else if (info->direction == TM_THROUGHPUT_OUT)
    {
        LOG_INFO("Updating Edge %s capability %s TM throughput out (%" PRIu32 " bytes/tick): ",
        edge_info_name(edge), cap->name, info->throughput);
        gaussian_dist_print(&cap->tm.throughput_out);
        LOG_INFO_(" -> ");

        gaussian_dist_update(&cap->tm.throughput_out, info->throughput);

        gaussian_dist_print(&cap->tm.throughput_out);
        LOG_INFO_("\n");
    }
    else
    {
        LOG_ERR("Unknown throughput direction\n");
    }
}
/*-------------------------------------------------------------------------------------------------------------------*/
void tm_update_ping(edge_resource_t* edge, const tm_edge_ping_t* info)
{
    if (info->action == TM_PING_SENT)
    {
        // Do nothing
    }
    else if (info->action == TM_PING_RECEIVED)
    {
        LOG_INFO("Updating Edge %s TM last ping: %" PRIu32,
        edge_info_name(edge), edge->tm.last_ping_response);
        LOG_INFO_(" -> ");

        edge->tm.last_ping_response = clock_time();

        LOG_INFO_("%" PRIu32 "\n", edge->tm.last_ping_response);
    }
    else
    {
        LOG_ERR("Unknown ping action\n");
    }
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
    NANOCBOR_CHECK(nanocbor_fmt_array(enc, 3));
    NANOCBOR_CHECK(dist_serialise(enc, &cap->result_quality));
    NANOCBOR_CHECK(dist_serialise(enc, &cap->throughput_in));
    NANOCBOR_CHECK(dist_serialise(enc, &cap->throughput_out));

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
    NANOCBOR_CHECK(dist_deserialise(&arr, &cap->throughput_in));
    NANOCBOR_CHECK(dist_deserialise(&arr, &cap->throughput_out));

    if (!nanocbor_at_end(&arr))
    {
        return NANOCBOR_ERR_END;
    }

    nanocbor_leave_container(dec, &arr);

    return NANOCBOR_OK;
}
/*-------------------------------------------------------------------------------------------------------------------*/
