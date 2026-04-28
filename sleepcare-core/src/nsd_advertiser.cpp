#include "nsd_advertiser.h"
#include "config.h"
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ScNsd {
    AvahiSimplePoll*   poll;
    AvahiClient*       client;
    AvahiEntryGroup*   group;
    int                port;
    char               device_id[64];
};

static void entry_group_cb(AvahiEntryGroup* g, AvahiEntryGroupState state, void* ud) {
    (void)ud;
    if (state == AVAHI_ENTRY_GROUP_ESTABLISHED)
        printf("[nsd] Service registered successfully.\n");
    else if (state == AVAHI_ENTRY_GROUP_COLLISION ||
             state == AVAHI_ENTRY_GROUP_FAILURE)
        fprintf(stderr, "[nsd] Entry group error: %d\n", (int)state);
}

static void register_service(ScNsd* nsd) {
    if (!nsd->group) {
        nsd->group = avahi_entry_group_new(nsd->client, entry_group_cb, nsd);
        if (!nsd->group) return;
    }

    char instance[128];
    snprintf(instance, sizeof(instance), "SleepCare-Pi-%s", nsd->device_id);

    AvahiStringList* txt =
        avahi_string_list_new("proto=v1", "tls=1", "ws=/ws", "cam=1", NULL);
    char dev_txt[128];
    snprintf(dev_txt, sizeof(dev_txt), "device_id=%s", nsd->device_id);
    txt = avahi_string_list_add(txt, dev_txt);

    int ret = avahi_entry_group_add_service_strlst(
        nsd->group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0,
        instance, "_sleepcare._tcp", NULL, NULL, (uint16_t)nsd->port, txt);

    avahi_string_list_free(txt);

    if (ret < 0) {
        fprintf(stderr, "[nsd] avahi_entry_group_add_service: %s\n",
                avahi_strerror(ret));
        return;
    }
    avahi_entry_group_commit(nsd->group);
}

static void client_cb(AvahiClient* c, AvahiClientState state, void* ud) {
    ScNsd* nsd = (ScNsd*)ud;
    nsd->client = c;
    if (state == AVAHI_CLIENT_S_RUNNING)
        register_service(nsd);
    else if (state == AVAHI_CLIENT_FAILURE)
        fprintf(stderr, "[nsd] Avahi client failure: %s\n",
                avahi_strerror(avahi_client_errno(c)));
}

ScNsd* sc_nsd_start(const char* device_id, int port) {
    ScNsd* nsd = (ScNsd*)calloc(1, sizeof(ScNsd));
    nsd->port  = port;
    snprintf(nsd->device_id, sizeof(nsd->device_id), "%s", device_id);

    nsd->poll = avahi_simple_poll_new();
    if (!nsd->poll) { free(nsd); return NULL; }

    int err = 0;
    nsd->client = avahi_client_new(avahi_simple_poll_get(nsd->poll),
                                   0, client_cb, nsd, &err);
    if (!nsd->client) {
        fprintf(stderr, "[nsd] avahi_client_new: %s\n", avahi_strerror(err));
        avahi_simple_poll_free(nsd->poll);
        free(nsd);
        return NULL;
    }
    return nsd;
}

void sc_nsd_poll(ScNsd* nsd, int timeout_ms) {
    if (nsd && nsd->poll)
        avahi_simple_poll_iterate(nsd->poll, timeout_ms);
}

void sc_nsd_stop(ScNsd* nsd) {
    if (!nsd) return;
    if (nsd->client) avahi_client_free(nsd->client);
    if (nsd->poll)   avahi_simple_poll_free(nsd->poll);
    free(nsd);
}
