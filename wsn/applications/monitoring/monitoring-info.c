#include "monitoring.h"
#include "trust-models.h"

static const trust_weight_t weights[] = {
    { TRUST_METRIC_TASK_SUBMISSION, 1.0f },
};

static trust_weights_t weights_info = {
    .application_name = MONITORING_APPLICATION_NAME,
    .weights = weights,
    .num = sizeof(weights)/sizeof(*weights)
};

void init_trust_weights_monitoring(void)
{
    trust_weights_add(&weights_info);
}
