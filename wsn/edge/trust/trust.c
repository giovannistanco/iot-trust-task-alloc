#include "trust.h"

#include "contiki.h"
#include "os/net/linkaddr.h"
#include "os/sys/log.h"
#include "uip.h"
#include "os/net/ipv6/uip-ds6.h"
#include "os/net/ipv6/uiplib.h"

#include <stdio.h>

#include "applications.h"
#include "trust-common.h"
#include "mqtt-over-coap.h"
#include "keys.h"

#include "monitoring.h"

/*-------------------------------------------------------------------------------------------------------------------*/
#define LOG_MODULE "trust"
#ifdef TRUST_MODEL_LOG_LEVEL
#define LOG_LEVEL TRUST_MODEL_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_NONE
#endif
/*-------------------------------------------------------------------------------------------------------------------*/
#define BASE_PUBLISH_TOPIC_LEN     (MQTT_EDGE_NAMESPACE_LEN + 1 + MQTT_IDENTITY_LEN + 1)
#define MAX_PUBLISH_TOPIC_LEN      (BASE_PUBLISH_TOPIC_LEN + 64)
/*-------------------------------------------------------------------------------------------------------------------*/
static char pub_topic[MAX_PUBLISH_TOPIC_LEN];
/*-------------------------------------------------------------------------------------------------------------------*/
#define PUBLISH_ANNOUNCE_PERIOD_SHORT (CLOCK_SECOND * 2 * 60)
#define PUBLISH_ANNOUNCE_PERIOD_LONG  (PUBLISH_ANNOUNCE_PERIOD_SHORT * 15)
#define PUBLISH_CAPABILITY_PERIOD     (CLOCK_SECOND * 5)
#define PUBLISH_ANNOUNCE_SHORT_TO_LONG 5
/*-------------------------------------------------------------------------------------------------------------------*/
static struct etimer publish_announce_timer;
static struct etimer publish_capability_timer;
/*-------------------------------------------------------------------------------------------------------------------*/
static uint8_t announce_short_count;
/*-------------------------------------------------------------------------------------------------------------------*/
static bool
get_global_address(char* buf, size_t buf_len)
{
  for (int i = 0; i < UIP_DS6_ADDR_NB; i++)
  {
    uint8_t state = uip_ds6_if.addr_list[i].state;

    if (uip_ds6_if.addr_list[i].isused && (state == ADDR_TENTATIVE || state == ADDR_PREFERRED))
    {
      uiplib_ipaddr_snprint(buf, buf_len, &uip_ds6_if.addr_list[i].ipaddr);
      return true;
    }
  }

  return false;
}
/*-------------------------------------------------------------------------------------------------------------------*/
bool
publish_announce(void)
{
    int ret;

    ret = snprintf(pub_topic + BASE_PUBLISH_TOPIC_LEN, MAX_PUBLISH_TOPIC_LEN - BASE_PUBLISH_TOPIC_LEN, MQTT_EDGE_ACTION_ANNOUNCE);
    if (ret <= 0 || ret >= MAX_PUBLISH_TOPIC_LEN - BASE_PUBLISH_TOPIC_LEN)
    {
        LOG_ERR("snprintf pub_topic failed %d\n", ret);
        return false;
    }

    char ip_addr_buf[UIPLIB_IPV6_MAX_STR_LEN];
    ret = get_global_address(ip_addr_buf, sizeof(ip_addr_buf));
    if (!ret)
    {
        LOG_ERR("Failed to obtain global IP address\n");
        return false;
    }

    char publish_buffer[2 + 9 + UIPLIB_IPV6_MAX_STR_LEN + 1 + DTLS_EC_KEY_SIZE*4];
    ret = snprintf(publish_buffer, sizeof(publish_buffer),
        "{"
            "\"addr\":\"%s\""
        "}",
        ip_addr_buf
    );
    if (ret <= 0 || ret >= sizeof(publish_buffer))
    {
        return false;
    }

    char* buf_ptr = publish_buffer + ret + 1;

    // Include our public key and certificate here
    memcpy(buf_ptr + DTLS_EC_KEY_SIZE*0, our_key.pub_key.x, DTLS_EC_KEY_SIZE);
    memcpy(buf_ptr + DTLS_EC_KEY_SIZE*1, our_key.pub_key.y, DTLS_EC_KEY_SIZE);
    memcpy(buf_ptr + DTLS_EC_KEY_SIZE*2, our_pubkey_sig.r,  DTLS_EC_KEY_SIZE);
    memcpy(buf_ptr + DTLS_EC_KEY_SIZE*3, our_pubkey_sig.s,  DTLS_EC_KEY_SIZE);

    LOG_DBG("Publishing announce [topic=%s, data=%s]\n", pub_topic, publish_buffer);

    return mqtt_over_coap_publish(pub_topic, publish_buffer, ret+1+DTLS_EC_KEY_SIZE*4);
}
/*-------------------------------------------------------------------------------------------------------------------*/
bool
publish_add_capability(const char* name)
{
    int ret;

    ret = snprintf(pub_topic + BASE_PUBLISH_TOPIC_LEN, MAX_PUBLISH_TOPIC_LEN - BASE_PUBLISH_TOPIC_LEN,
                   MQTT_EDGE_ACTION_CAPABILITY "/%s/" MQTT_EDGE_ACTION_CAPABILITY_ADD, name);
    if (ret <= 0 || ret >= MAX_PUBLISH_TOPIC_LEN - BASE_PUBLISH_TOPIC_LEN)
    {
        LOG_ERR("snprintf pub_topic failed %d\n", ret);
        return false;
    }

    char publish_buffer[2 + 1];
    ret = snprintf(publish_buffer, sizeof(publish_buffer),
        "{"
        "}"
    );
    if (ret <= 0 || ret >= sizeof(publish_buffer))
    {
        return false;
    }

    LOG_DBG("Publishing announce [topic=%s, data=%s]\n", pub_topic, publish_buffer);

    return mqtt_over_coap_publish(pub_topic, publish_buffer, ret+1);
}
/*-------------------------------------------------------------------------------------------------------------------*/
bool
publish_remove_capability(const char* name)
{
    int ret;

    ret = snprintf(pub_topic + BASE_PUBLISH_TOPIC_LEN, MAX_PUBLISH_TOPIC_LEN - BASE_PUBLISH_TOPIC_LEN,
                   MQTT_EDGE_ACTION_CAPABILITY "/%s/" MQTT_EDGE_ACTION_CAPABILITY_REMOVE, name);
    if (ret <= 0 || ret >= MAX_PUBLISH_TOPIC_LEN - BASE_PUBLISH_TOPIC_LEN)
    {
        LOG_ERR("snprintf pub_topic failed %d\n", ret);
        return false;
    }

    char publish_buffer[2 + 1];
    ret = snprintf(publish_buffer, sizeof(publish_buffer),
        "{"
        "}"
    );
    if (ret <= 0 || ret >= sizeof(publish_buffer))
    {
        return false;
    }

    LOG_DBG("Publishing announce [topic=%s, data=%s]\n", pub_topic, publish_buffer);

    return mqtt_over_coap_publish(pub_topic, publish_buffer, ret+1);
}
/*-------------------------------------------------------------------------------------------------------------------*/
static void
periodic_publish_announce(void)
{
    LOG_DBG("Attempting to publish announce...\n");
    bool ret = publish_announce();
    if (!ret)
    {
        LOG_ERR("Failed to publish announce\n");
    }
    else
    {
        LOG_DBG("Announce sent! Starting capability publish timer...\n");
        etimer_set(&publish_capability_timer, PUBLISH_CAPABILITY_PERIOD);
    }

    if (publish_announce_timer.timer.interval == PUBLISH_ANNOUNCE_PERIOD_SHORT)
    {
        announce_short_count += 1;

        if (announce_short_count >= PUBLISH_ANNOUNCE_SHORT_TO_LONG)
        {
            LOG_DBG("Moving to less frequent announce intervals\n");
            etimer_reset_with_new_interval(&publish_announce_timer, PUBLISH_ANNOUNCE_PERIOD_LONG);
        }
        else
        {
            etimer_reset(&publish_announce_timer);
        }
    }
    else
    {
        etimer_reset(&publish_announce_timer);
    }
}
/*-------------------------------------------------------------------------------------------------------------------*/
static void
periodic_publish_capability(void)
{
    LOG_DBG("Attempting to publish capabilities...\n");
    // TODO: This needs to be based off services running on the Edge observer node this sensor node is connected to.

    publish_add_capability(MONITORING_APPLICATION_NAME);
}
/*-------------------------------------------------------------------------------------------------------------------*/
static bool
init(void)
{
    int len = snprintf(pub_topic, BASE_PUBLISH_TOPIC_LEN+1, MQTT_EDGE_NAMESPACE "/%02x%02x%02x%02x%02x%02x%02x%02x/",
                       linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                       linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[3],
                       linkaddr_node_addr.u8[4], linkaddr_node_addr.u8[5],
                       linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);
    if (len != BASE_PUBLISH_TOPIC_LEN)
    {
        LOG_ERR("Failed to create pub_topic (%d != %u)\n", len, BASE_PUBLISH_TOPIC_LEN);
        return false;
    }

    LOG_DBG("Base MQTT topic is set to %s\n", pub_topic);

    trust_common_init();

    announce_short_count = 0;

    return true;
}
/*-------------------------------------------------------------------------------------------------------------------*/
PROCESS(trust_model, "Trust Model process");
/*-------------------------------------------------------------------------------------------------------------------*/
PROCESS_THREAD(trust_model, ev, data)
{
    PROCESS_BEGIN();

    bool ret = init();
    if (!ret)
    {
        PROCESS_EXIT();
    }

    /* Setup a periodic timer that expires after PERIOD seconds. */
    etimer_set(&publish_announce_timer, PUBLISH_ANNOUNCE_PERIOD_SHORT);

    while (1)
    {
        PROCESS_YIELD();

        if (ev == PROCESS_EVENT_TIMER && data == &publish_announce_timer) {
          periodic_publish_announce();
        }

        if (ev == PROCESS_EVENT_TIMER && data == &publish_capability_timer) {
          periodic_publish_capability();
        }
    }

    PROCESS_END();
}
/*-------------------------------------------------------------------------------------------------------------------*/