/******************************************************************************
 * @file: router-dbusif.h
 *
 * The file contains the declarations of the interfaces to handle D-Bus for the
 * PulseAudio router module.
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

#ifndef __ROUTER_DBUSIFACE_H__
#define __ROUTER_DBUSIFACE_H__

#define E_OK 0
#define E_NOT_POSSIBLE 7

typedef struct {
    uint16_t handle;
    uint16_t source_id;
    uint16_t sink_id;
    uint16_t connection_id;
    int32_t connection_format;
} am_connect_t;

typedef struct {
    uint16_t connection_id;
    uint16_t source_id;
    uint16_t sink_id;
    int32_t delay;
    int32_t state;
} am_main_connection_t;

typedef struct {
    uint16_t connection_id;
} am_disconnect_t;

typedef struct {

    uint16_t domain_id;
    char name[AM_MAX_NAME_LENGTH];
    char busname[AM_MAX_NAME_LENGTH];
    char nodename[AM_MAX_NAME_LENGTH];
    bool early;
    bool complete;
    int state;

} am_domain_register_t;

typedef struct {

    uint16_t sink_id;
    char name[AM_MAX_NAME_LENGTH];
    uint16_t domain_id;
    uint16_t sink_class_id;
    int16_t volume;
    bool visible;
    int available;
    uint16_t availability_reason;
    int mute_state;
    int16_t main_volume;
    //std::vector<am_SoundProperty_s> listSoundProperties;
    //std::vector<am_CustomConnectionFormat_t> listConnectionFormats;
    //std::vector<am_MainSoundProperty_s> listMainSoundProperties;
    //std::vector<am_NotificationConfiguration_s> listMainNotificationConfigurations;
    //std::vector<am_NotificationConfiguration_s> listNotificationConfigurations;
} am_sink_register_t;

typedef struct {

    uint16_t source_id;
    uint16_t domain_id;
    char name[AM_MAX_NAME_LENGTH];
    uint16_t source_class_id;
    uint16_t source_state;
    int16_t volume;
    bool visible;
    int16_t available;
    int16_t availability_reason;
    uint16_t interrupt_state;
    //std::vector<am_SoundProperty_s> listSoundProperties;
    //std::vector<am_CustomConnectionFormat_t> listConnectionFormats;
    //std::vector<am_MainSoundProperty_s> listMainSoundProperties;
    //std::vector<am_NotificationConfiguration_s> listMainNotificationConfigurations;
    //std::vector<am_NotificationConfiguration_s> listNotificationConfigurations;
} am_source_register_t;

typedef struct {
    uint16_t domain_id;
} am_domain_unregister_t;

typedef struct {
    uint16_t sink_id;
} am_sink_unregister_t;

typedef struct {
    uint16_t source_id;
} am_source_unregister_t;

typedef struct {
    uint16_t id;
    uint16_t domain_id;

} am_domain_of_source_sink_t;

typedef void (*cb_new_main_connection_t)(struct userdata*, am_main_connection_t*);
typedef void (*cb_removed_main_connection_t)(struct userdata*, uint16_t);
typedef void (*cb_main_connection_state_changed_t)(struct userdata*, uint16_t, int32_t);
typedef void (*cb_command_connect_reply_t)(struct userdata*, int32_t status, void*);
typedef void (*cb_command_disconnect_reply_t)(struct userdata*, int32_t status, void*);
typedef void (*cb_routing_register_domain_reply_t)(struct userdata*, int32_t status, void*);
typedef void (*cb_routing_deregister_domain_reply_t)(struct userdata*, int32_t status, void*);
typedef void (*cb_routing_register_sink_reply_t)(struct userdata*, int32_t status, void*);
typedef void (*cb_routing_deregister_sink_reply_t)(struct userdata*, int32_t status, void*);
typedef void (*cb_routing_register_source_reply_t)(struct userdata*, int32_t status, void*);
typedef void (*cb_routing_deregister_source_reply_t)(struct userdata*, int32_t status, void*);
typedef void (*cb_routing_peek_source_reply_t)(struct userdata*, int32_t status, void*);
typedef void (*cb_routing_peek_sink_reply_t)(struct userdata*, int32_t status, void*);
typedef void (*cb_routing_get_domain_of_source_reply_t)(struct userdata*, int32_t status, void*);
typedef void (*cb_routing_get_domain_of_sink_reply_t)(struct userdata*, int32_t status, void*);

typedef void (*cb_routing_deregister_source_reply_t)(struct userdata*, int32_t status, void*);
typedef uint16_t (*cb_routing_async_connect_t)(struct userdata*, uint16_t, uint16_t, uint16_t, uint16_t, int32_t);
typedef uint16_t (*cb_routing_async_disconnect_t)(struct userdata*, uint16_t, uint16_t);
typedef uint16_t (*cb_routing_async_set_volume_t)(struct userdata*, uint16_t, uint16_t, int16_t, int16_t, uint16_t);
typedef uint16_t (*cb_routing_async_set_source_state_t)(struct userdata*, uint16_t, uint16_t, int32_t);

typedef struct {

    DBusBusType bus_type;
    char* pulse_router_dbus_return_interface_name;
    char* pulse_router_dbus_name;
    char* pulse_router_dbus_interface_name;
    char* pulse_router_dbus_path;
    char* am_command_dbus_name;
    char* am_command_dbus_interface_name;
    char* am_command_dbus_path;
    char* am_routing_dbus_name;
    char* am_routing_dbus_interface_name;
    char* am_routing_dbus_path;
    char* am_watch_rule;
    cb_new_main_connection_t cb_new_main_connection;
    cb_removed_main_connection_t cb_removed_main_connection;
    cb_main_connection_state_changed_t cb_main_connection_state_changed;

    cb_command_connect_reply_t cb_command_connect_reply;
    cb_command_disconnect_reply_t cb_command_disconnect_reply;
    cb_routing_register_domain_reply_t cb_routing_register_domain_reply;
    cb_routing_deregister_domain_reply_t cb_routing_deregister_domain_reply;
    cb_routing_register_sink_reply_t cb_routing_register_sink_reply;
    cb_routing_deregister_sink_reply_t cb_routing_deregister_sink_reply;
    cb_routing_register_source_reply_t cb_routing_register_source_reply;
    cb_routing_deregister_source_reply_t cb_routing_deregister_source_reply;

    cb_routing_async_connect_t cb_routing_async_connect;
    cb_routing_async_disconnect_t cb_routing_async_disconnect;
    cb_routing_async_set_volume_t cb_routing_async_set_sink_volume;
    cb_routing_async_set_volume_t cb_routing_async_set_source_volume;
    cb_routing_async_set_source_state_t cb_routing_async_set_source_state;
    cb_routing_peek_source_reply_t cb_routing_peek_source_reply;
    cb_routing_peek_sink_reply_t cb_routing_peek_sink_reply;
    cb_routing_get_domain_of_source_reply_t cb_routing_get_domain_of_source_reply;
    cb_routing_get_domain_of_sink_reply_t cb_routing_get_domain_of_sink_reply;

} router_init_data_t;

router_dbusif *router_dbusif_init(struct userdata *u, router_init_data_t* init_data);
void router_dbusif_done(struct router_dbusif *routerif);

int router_dbusif_command_connect(struct userdata *u, am_main_connection_t* data);
int router_dbusif_command_disconnect(struct userdata *u, am_disconnect_t* data);
int router_dbusif_routing_register_domain(struct userdata *u, am_domain_register_t* data);
int router_dbusif_routing_deregister_domain(struct userdata *u, am_domain_unregister_t* data);

int router_dbusif_routing_register_sink(struct userdata *u, am_sink_register_t* data);
int router_dbusif_routing_deregister_sink(struct userdata *u, am_sink_unregister_t* data);

int router_dbusif_routing_peek_sink(struct userdata *u, const char* sink_name);
int router_dbusif_routing_peek_source(struct userdata *u, const char* source_name);
int router_dbusif_get_domain_of_source(struct userdata *u, const uint16_t source_id);
int router_dbusif_get_domain_of_sink(struct userdata *u, const uint16_t sink_id);

int router_dbusif_routing_register_source(struct userdata *u, am_source_register_t* data);
int router_dbusif_routing_deregister_source(struct userdata *u, am_source_unregister_t* data);

int router_dbusif_ack_async_connect(struct userdata *u, int handle, uint16_t connectionID, int error);
int router_dbusif_ack_async_disconnect(struct userdata *u, int handle, uint16_t connectionID, int error);

void router_dbusif_ack_connect(struct userdata *u, uint16_t handle, uint16_t connection_id, uint16_t error);

void router_dbusif_ack_disconnect(struct userdata *u, uint16_t handle, uint16_t connection_id, uint16_t error);

void router_dbus_ack_set_sink_volume(struct userdata *u, uint16_t handle, uint16_t volume, uint16_t error);

void router_dbus_ack_set_source_volume(struct userdata *u, uint16_t handle, uint16_t volume, uint16_t error);

void router_dbus_ack_set_source_state(struct userdata *u, uint16_t handle, uint16_t error);

#endif /* __ROUTER_DBUSIFACE_H__ */
