/******************************************************************************
 * @file: router-userdata.h
 *
 * The file contains the definition of user data structure
 *
 *
 *
 * @component: PulseAudio router module
 *
 * @author: Toshiaki Isogai <tisogai@jp.adit-jv.com>
 *          Kapildev Patel  <kpatel@jp.adit-jv.com>
 *
 * @copyright (c) 2016 Advanced Driver Information Technology.
 * This code is developed by Advanced Driver Information Technology.
 * Copyright of Advanced Driver Information Technology, Bosch, and DENSO.
 * All rights reserved.
 *
 *****************************************************************************/
#ifndef __USERDATA_ROUTER_H__
#define __USERDATA_ROUTER_H__

#include <pulsecore/protocol-dbus.h>
#include <pulsecore/log.h>

typedef struct router_dbusif router_dbusif;
typedef struct router_hooks router_hooks;

#define AM_MAX_SOURCE_SINK    100
#define AM_MAX_NAME_LENGTH    256

typedef struct name_id_map_t {
    uint16_t id;
    uint16_t domain_id;
    bool builtin;
    uint16_t source_state;
    float volume;
    bool volume_valid;
    void* data;
    char name[AM_MAX_NAME_LENGTH];
    char description[AM_MAX_NAME_LENGTH];
} name_id_map;

struct userdata {
    pa_core *core;
    router_hooks *h;
    router_dbusif *dbusif;
    pa_hashmap *main_connection_map;
    pa_hashmap *connection_map;
    void* domain;
    name_id_map sink_map[AM_MAX_SOURCE_SINK];
    name_id_map source_map[AM_MAX_SOURCE_SINK];

};

#define ROUTER_FUNCTION_ENTRY pa_log_info("%s entered",__func__)
#define ROUTER_FUNCTION_EXIT pa_log_info("%s exited",__func__)
#define MODULE_ROUTER_FREE(p) { if(p) pa_xfree(p); }
#endif //__USERDATA_ROUTER_H__
