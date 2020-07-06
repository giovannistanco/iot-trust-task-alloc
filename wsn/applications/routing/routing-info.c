#include "routing.h"
#include "trust-models.h"

static const trust_weight_t weights[] = {
    { TRUST_METRIC_TASK_SUBMISSION, 0.33333f },
    { TRUST_METRIC_TASK_RESULT, 0.33333f },
    { TRUST_METRIC_RESULT_QUALITY, 0.33333f },
};

static trust_weights_t weights_info = {
    .application_name = ROUTING_APPLICATION_NAME,
    .weights = weights,
    .num = sizeof(weights)/sizeof(*weights)
};

void init_trust_weights_routing(void)
{
    trust_weights_add(&weights_info);
}
