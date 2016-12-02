/******************************************************************************
 * @file: router-dbusif.c
 *
 * The file contains the implementation of the interfaces to handle D-Bus for
 * the PulseAudio router module.
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

#include <pulsecore/pulsecore-config.h>
#include <stdint.h>
#include <pulsecore/core-util.h>
#include <pulsecore/dbus-shared.h>
#include <pulsecore/protocol-dbus.h>
#include <pulsecore/dbus-util.h>
#include <pulsecore/llist.h>
#include "router-userdata.h"
#include "router-dbusif.h"
#define GENIVI_DBUS_PLUGIN  1

typedef void (*pending_cb_t)(struct userdata *, DBusMessage *, void *);

typedef struct pending {
    PA_LLIST_FIELDS(struct pending);
    struct userdata *u;
    DBusPendingCall *call;
    pending_cb_t cb;
    void *data;
} pending_dbus_calls_t;

struct router_dbusif {
    pa_dbus_connection *conn;
    char *pulse_router_dbus_return_interface_name;
    char *pulse_router_dbus_name;
    char *pulse_router_dbus_interface_name;
    char *pulse_router_dbus_path;
    char *am_command_dbus_name;
    char *am_command_dbus_interface_name;
    char *am_command_dbus_path;
    char *am_routing_dbus_name;
    char *am_routing_dbus_interface_name;
    char *am_routing_dbus_path;
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
    cb_routing_peek_source_reply_t cb_routing_peek_source_reply;
    cb_routing_peek_sink_reply_t cb_routing_peek_sink_reply;
    cb_routing_async_connect_t cb_routing_async_connect;
    cb_routing_async_disconnect_t cb_routing_async_disconnect;
    cb_routing_async_set_volume_t cb_routing_async_set_sink_volume;
    cb_routing_async_set_volume_t cb_routing_async_set_source_volume;
    cb_routing_async_set_source_state_t cb_routing_async_set_source_state;
    cb_routing_get_domain_of_source_reply_t cb_routing_get_domain_of_source_reply;
    cb_routing_get_domain_of_sink_reply_t cb_routing_get_domain_of_sink_reply;
    PA_LLIST_HEAD(pending_dbus_calls_t, pending_call_list);
};

static void free_routerif(struct userdata * u);

static bool send_message_with_reply(struct userdata *, DBusMessage *, pending_cb_t, void *);
/*
 * callbacks for the synchronous messages
 */
static void router_dbusif_connect_reply_cb(struct userdata *, DBusMessage *, void *);
static void router_dbusif_disconnect_reply_cb(struct userdata *, DBusMessage *, void *);
static void router_dbusif_register_domain_reply_cb(struct userdata *, DBusMessage *, void *);
static void router_dbusif_deregister_domain_reply_cb(struct userdata *, DBusMessage *, void *);
static void router_dbusif_register_sink_reply_cb(struct userdata *, DBusMessage *, void *);
static void router_dbusif_deregister_sink_reply_cb(struct userdata *, DBusMessage *, void *);
static void router_dbusif_register_source_reply_cb(struct userdata *, DBusMessage *, void *);
static void router_dbusif_deregister_source_reply_cb(struct userdata *, DBusMessage *, void *);
static void router_dbusif_peek_source_reply_cb(struct userdata *, DBusMessage *, void *);
static void router_dbusif_peek_sink_reply_cb(struct userdata *, DBusMessage *, void *);
static void router_dbusif_get_domain_of_source_reply_cb(struct userdata *, DBusMessage *, void *);
static void router_dbusif_get_domain_of_sink_reply_cb(struct userdata *, DBusMessage *, void *);

/* dbus message handlers */
static DBusHandlerResult router_dbusif_routing_async_connect_handler(DBusConnection *conn, DBusMessage *msg, void *arg);
static DBusHandlerResult router_dbusif_routing_async_disconnect_handler(DBusConnection *conn, DBusMessage *msg,
        void *arg);
static DBusHandlerResult router_dbusif_routing_async_set_sink_volume_handler(DBusConnection *conn, DBusMessage *msg,
        void *arg);
static DBusHandlerResult router_dbusif_routing_async_set_source_volume_handler(DBusConnection *conn, DBusMessage *msg,
        void *arg);
static DBusHandlerResult router_dbusif_routing_async_set_source_state_handler(DBusConnection *conn, DBusMessage *msg,
        void *arg);
static DBusHandlerResult router_dbusif_command_cb_new_connection_handler(DBusConnection *conn, DBusMessage *msg,
        void *arg);
static DBusHandlerResult router_dbusif_command_cb_removed_connection_handler(DBusConnection *conn, DBusMessage *msg,
        void *arg);
static DBusHandlerResult router_dbusif_command_cb_connection_state_changed_handler(DBusConnection *conn,
        DBusMessage *msg, void *arg);

typedef DBusHandlerResult (*method_t)(DBusConnection *, DBusMessage *, void *);

/**
 * @brief The callback function when any request is received on dbus.
 * @param conn: The dbus connection pointer.
 *        msg: The dbus message.
 *        args: The pointer which was registered while registering this function, The userdata.
 * @return DBusHandlerResult
 */
static DBusHandlerResult router_dbusif_method_handler(DBusConnection *conn, DBusMessage *msg, void *arg) {
    struct dispatch {
        const char *name;
        method_t method;
    };

    static struct dispatch dispatch_tbl[] = {
            { "asyncConnect", router_dbusif_routing_async_connect_handler },
            { "asyncDisconnect", router_dbusif_routing_async_disconnect_handler },
            { "asyncSetSinkVolume", router_dbusif_routing_async_set_sink_volume_handler },
            { "asyncSetSourceVolume", router_dbusif_routing_async_set_source_volume_handler },
            { "asyncSetSourceState", router_dbusif_routing_async_set_source_state_handler },
            { "NewMainConnection", router_dbusif_command_cb_new_connection_handler },
            { "RemovedMainConnection", router_dbusif_command_cb_removed_connection_handler },
            { "MainConnectionStateChanged", router_dbusif_command_cb_connection_state_changed_handler },
            { NULL, NULL } };

    struct userdata *u = (struct userdata *) arg;
    struct dispatch *d;
    const char *name;
    dbus_int16_t errcod;
    method_t method;
    DBusMessage *reply;
    dbus_bool_t success;
    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    ROUTER_FUNCTION_ENTRY;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(u);

    if ( dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_CALL
            || dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_SIGNAL ) {
        name = dbus_message_get_member(msg);

        pa_assert(name);
        pa_log_debug("Message with name=%s", name);
        for ( method = NULL, d = dispatch_tbl; d->name ; d++ ) {
            if ( !strcmp(name, d->name) ) {
                method = d->method;
                break;
            }
        }

        if ( method ) {
            result = method(conn, msg, u);
        }
    }
    ROUTER_FUNCTION_EXIT;
    return result;
}

/**
 * @brief The async connection request handler.
 * @param conn: The dbus connection pointer.
 *        msg: The dbus message.
 *        args: The pointer which was registered while registering this function, The userdata.
 * @return DBusHandlerResult
 */
static DBusHandlerResult router_dbusif_routing_async_connect_handler(DBusConnection *conn, DBusMessage *msg, void *arg) {
    DBusError error;
    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    am_connect_t cd;
    dbus_bool_t success = FALSE;
    dbus_int16_t status = E_NOT_POSSIBLE;
    DBusMessage *reply = NULL;

    struct userdata *u = (struct userdata *) arg;
    const char *name = dbus_message_get_member(msg);
    ROUTER_FUNCTION_ENTRY;
    pa_assert(u);
    pa_assert(msg);
    pa_assert(name);

    memset(&cd, 0, sizeof(cd));
    dbus_error_init(&error);
#ifdef GENIVI_DBUS_PLUGIN
    success = dbus_message_get_args(msg, &error, DBUS_TYPE_UINT16, &cd.handle, DBUS_TYPE_UINT16, &cd.connection_id,
            DBUS_TYPE_UINT16, &cd.source_id, DBUS_TYPE_UINT16, &cd.sink_id, DBUS_TYPE_INT32, &cd.connection_format,
            DBUS_TYPE_INVALID);
#else
    success = dbus_message_get_args(msg, &error,
            DBUS_TYPE_UINT16, &cd.handle,
            DBUS_TYPE_UINT16, &cd.connection_id,
            DBUS_TYPE_UINT16, &cd.source_id,
            DBUS_TYPE_UINT16, &cd.sink_id,
            DBUS_TYPE_INT16, &cd.connection_format,
            DBUS_TYPE_INVALID);
#endif
    if ( success == TRUE ) {
        reply = dbus_message_new_method_return(msg);
        if ( reply ) {
            success = dbus_message_append_args(reply, DBUS_TYPE_UINT16, &status, DBUS_TYPE_INVALID);
            if ( success == TRUE ) {
                success = dbus_connection_send(conn, reply, NULL);
                if ( success == TRUE ) {
                    result = DBUS_HANDLER_RESULT_HANDLED;
                    pa_log_debug("%s: handled message '%s'", __FILE__, name);
                }
            }
            dbus_message_unref(reply);
        }
        if ( u->dbusif->cb_routing_async_connect ) {
            status = u->dbusif->cb_routing_async_connect(u, cd.handle, cd.connection_id, cd.source_id, cd.sink_id,
                    cd.connection_format);
        }
    } else {
        if ( dbus_error_is_set(&error) == TRUE ) {
            pa_log_error("%s: error while parsing the message '%s', %s: %s", __FILE__, name, error.name, error.message);
        }
    }

    dbus_error_free(&error);
    ROUTER_FUNCTION_EXIT;
    return result;
}

/**
 * @brief The async disconnection request handler.
 * @param conn: The dbus connection pointer.
 *        msg: The dbus message.
 *        args: The pointer which was registered while registering this function, The userdata.
 * @return DBusHandlerResult
 */
static DBusHandlerResult router_dbusif_routing_async_disconnect_handler(DBusConnection *conn, DBusMessage *msg,
        void *arg) {
    DBusError error;
    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    uint16_t handle;
    uint16_t connection;
    dbus_bool_t success = FALSE;
    dbus_int16_t status = E_NOT_POSSIBLE;
    DBusMessage *reply = NULL;

    struct userdata *u = (struct userdata *) arg;
    const char *name = dbus_message_get_member(msg);
    ROUTER_FUNCTION_ENTRY;
    pa_assert(u);
    pa_assert(msg);
    pa_assert(name);

    dbus_error_init(&error);
    success = dbus_message_get_args(msg, &error, DBUS_TYPE_UINT16, &handle, DBUS_TYPE_UINT16, &connection,
            DBUS_TYPE_INVALID);

    if ( success == TRUE ) {
        if ( u->dbusif->cb_routing_async_disconnect ) {
            status = u->dbusif->cb_routing_async_disconnect(u, handle, connection);
        }
        reply = dbus_message_new_method_return(msg);
        if ( reply ) {
            success = dbus_message_append_args(reply, DBUS_TYPE_UINT16, &status, DBUS_TYPE_INVALID);
            if ( success == TRUE ) {
                success = dbus_connection_send(conn, reply, NULL);
                if ( success == TRUE ) {
                    result = DBUS_HANDLER_RESULT_HANDLED;
                    pa_log_debug("%s: handled message '%s'", __FILE__, name);
                }
            }
            dbus_message_unref(reply);
        }
    } else {
        if ( dbus_error_is_set(&error) == TRUE ) {
            pa_log_error("%s: error while parsing the message '%s', %s: %s", __FILE__, name, error.name, error.message);
        }
    }

    dbus_error_free(&error);
    ROUTER_FUNCTION_EXIT;
    return result;
}

/**
 * @brief The async set sink volume handler.
 * @param conn: The dbus connection pointer.
 *        msg: The dbus message.
 *        args: The pointer which was registered while registering this function, The userdata.
 * @return DBusHandlerResult
 */
static DBusHandlerResult router_dbusif_routing_async_set_sink_volume_handler(DBusConnection *conn, DBusMessage *msg,
        void *arg) {
    DBusError error;
    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    dbus_bool_t success = FALSE;
    dbus_int16_t status = E_NOT_POSSIBLE;
    DBusMessage *reply = NULL;

    uint16_t handle = 0;
    uint16_t sink_id = 0;
    int16_t volume = 0;
    int16_t ramp_type = 0;
    uint16_t ramp_time = 0;

    struct userdata *u = (struct userdata *) arg;
    const char *name = dbus_message_get_member(msg);
    ROUTER_FUNCTION_ENTRY;
    pa_assert(u);
    pa_assert(msg);
    pa_assert(name);

    dbus_error_init(&error);
    success = dbus_message_get_args(msg, &error, DBUS_TYPE_UINT16, &handle, DBUS_TYPE_UINT16, &sink_id, DBUS_TYPE_INT16,
            &volume, DBUS_TYPE_INT16, &ramp_type, DBUS_TYPE_UINT16, &ramp_time, DBUS_TYPE_INVALID);

    if ( success == TRUE ) {
        reply = dbus_message_new_method_return(msg);
        if ( reply ) {
            success = dbus_message_append_args(reply, DBUS_TYPE_UINT16, &status, DBUS_TYPE_INVALID);
            if ( success == TRUE ) {
                success = dbus_connection_send(conn, reply, NULL);
                if ( success == TRUE ) {
                    result = DBUS_HANDLER_RESULT_HANDLED;
                    pa_log_debug("%s: handled message '%s'", __FILE__, name);
                }
            }
            dbus_message_unref(reply);
        }
        if ( u->dbusif->cb_routing_async_set_sink_volume ) {
            status = u->dbusif->cb_routing_async_set_sink_volume(u, handle, sink_id, (int32_t) volume, ramp_type,
                    ramp_time);
        }
    } else {
        if ( dbus_error_is_set(&error) == TRUE ) {
            pa_log_error("%s: error while parsing the message '%s', %s: %s", __FILE__, name, error.name, error.message);
        }
    }

    dbus_error_free(&error);
    ROUTER_FUNCTION_EXIT;
    return result;
}

/**
 * @brief The async set source volume handler.
 * @param conn: The dbus connection pointer.
 *        msg: The dbus message.
 *        args: The pointer which was registered while registering this function, The userdata.
 * @return DBusHandlerResult
 */
static DBusHandlerResult router_dbusif_routing_async_set_source_volume_handler(DBusConnection *conn, DBusMessage *msg,
        void *arg) {
    DBusError error;
    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    dbus_bool_t success = FALSE;
    dbus_int16_t status = E_NOT_POSSIBLE;
    DBusMessage *reply = NULL;

    uint16_t handle = 0;
    uint16_t source_id = 0;
    int16_t volume = 0;
    int32_t ramp_type = 0;
    uint16_t ramp_time = 0;

    struct userdata *u = (struct userdata *) arg;
    const char *name = dbus_message_get_member(msg);
    ROUTER_FUNCTION_ENTRY;
    pa_assert(u);
    pa_assert(msg);
    pa_assert(name);

    dbus_error_init(&error);
    success = dbus_message_get_args(msg, &error, DBUS_TYPE_UINT16, &handle, DBUS_TYPE_UINT16, &source_id,
            DBUS_TYPE_INT16, &volume, DBUS_TYPE_INT16, &ramp_type, DBUS_TYPE_UINT16, &ramp_time, DBUS_TYPE_INVALID);

    if ( success == TRUE ) {
        reply = dbus_message_new_method_return(msg);
        if ( reply ) {
            success = dbus_message_append_args(reply, DBUS_TYPE_UINT16, &status, DBUS_TYPE_INVALID);
            if ( success == TRUE ) {
                success = dbus_connection_send(conn, reply, NULL);
                if ( success == TRUE ) {
                    result = DBUS_HANDLER_RESULT_HANDLED;
                    pa_log_debug("%s: handled message '%s'", __FILE__, name);
                }
            }
            dbus_message_unref(reply);
        }
        if ( u->dbusif->cb_routing_async_set_source_volume ) {
            status = u->dbusif->cb_routing_async_set_source_volume(u, handle, source_id, volume, ramp_type, ramp_time);
        }
    } else {
        if ( dbus_error_is_set(&error) == TRUE ) {
            pa_log_error("%s: error while parsing the message '%s', %s: %s", __FILE__, name, error.name, error.message);
        }
    }

    dbus_error_free(&error);
    ROUTER_FUNCTION_EXIT;
    return result;
}

/**
 * @brief The async set source handler
 * @param conn: The dbus connection pointer.
 *        msg: The dbus message.
 *        args: The pointer which was registered while registering this function, The userdata.
 * @return DBusHandlerResult
 */
static DBusHandlerResult router_dbusif_routing_async_set_source_state_handler(DBusConnection *conn, DBusMessage *msg,
        void *arg) {
    DBusError error;
    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    dbus_bool_t success = FALSE;
    dbus_int16_t status = E_NOT_POSSIBLE;
    DBusMessage *reply = NULL;

    uint16_t handle = 0;
    uint16_t source_id = 0;
    int32_t state = 0;

    struct userdata *u = (struct userdata *) arg;
    const char *name = dbus_message_get_member(msg);
    ROUTER_FUNCTION_ENTRY;
    pa_assert(u);
    pa_assert(msg);
    pa_assert(name);

    dbus_error_init(&error);
#ifdef GENIVI_DBUS_PLUGIN
    success = dbus_message_get_args(msg, &error, DBUS_TYPE_UINT16, &handle, DBUS_TYPE_UINT16, &source_id,
            DBUS_TYPE_INT32, &state, DBUS_TYPE_INVALID);
#else
    success = dbus_message_get_args(msg, &error,
            DBUS_TYPE_UINT16, &handle,
            DBUS_TYPE_UINT16, &source_id,
            DBUS_TYPE_INT16, &state,
            DBUS_TYPE_INVALID);
#endif
    if ( success == TRUE ) {
        reply = dbus_message_new_method_return(msg);
        if ( reply ) {
            success = dbus_message_append_args(reply, DBUS_TYPE_UINT16, &status, DBUS_TYPE_INVALID);
            if ( success == TRUE ) {
                success = dbus_connection_send(conn, reply, NULL);
                if ( success == TRUE ) {
                    result = DBUS_HANDLER_RESULT_HANDLED;
                    pa_log_debug("%s: handled message '%s'", __FILE__, name);
                }
            }
            dbus_message_unref(reply);
        }
        if ( u->dbusif->cb_routing_async_set_source_state ) {
            pa_log_error("source id = %d,source state = %d", source_id, state);
            status = u->dbusif->cb_routing_async_set_source_state(u, handle, source_id, state);
        }

    } else {
        if ( dbus_error_is_set(&error) == TRUE ) {
            pa_log_error("%s: error while parsing the message '%s', %s: %s", __FILE__, name, error.name, error.message);
        }
    }

    dbus_error_free(&error);
    ROUTER_FUNCTION_EXIT;
    return result;
}

/**
 * @brief The command side new connection notification handler
 * @param conn: The dbus connection pointer.
 *        msg: The dbus message.
 *        args: The pointer which was registered while registering this function, The userdata.
 * @return DBusHandlerResult
 */
static DBusHandlerResult router_dbusif_command_cb_new_connection_handler(DBusConnection *conn, DBusMessage *msg,
        void *arg) {
    DBusError error;
    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    am_main_connection_t mcd;
    dbus_bool_t success = FALSE;
    DBusMessage *reply;

    struct userdata *u = (struct userdata *) arg;
    const char *name = dbus_message_get_member(msg);
    ROUTER_FUNCTION_ENTRY;

    pa_assert(u);
    pa_assert(msg);
    pa_assert(name);
#if 0
    memset(&mcd, 0, sizeof(mcd));
    dbus_error_init(&error);
    success = dbus_message_get_args(msg, &error,
            DBUS_TYPE_UINT16, &mcd.connection_id,
            DBUS_TYPE_UINT16, &mcd.source_id,
            DBUS_TYPE_UINT16, &mcd.sink_id,
            DBUS_TYPE_INT32, &mcd.delay,
            DBUS_TYPE_INT32, &mcd.state,
            DBUS_TYPE_INVALID);

    if (success == TRUE)
    {
        if (u->dbusif->cb_new_main_connection)
        {
            u->dbusif->cb_new_main_connection(u,
                    &mcd);
        }
        reply = dbus_message_new_method_return(msg);
        if (reply) {
            success = dbus_connection_send(conn, reply, NULL);
            if (success == TRUE) {
                result = DBUS_HANDLER_RESULT_HANDLED;
                pa_log_debug("%s: handled message '%s'", __FILE__, name);
            }
            dbus_message_unref(reply);
        }
    }
    else {
        if (dbus_error_is_set(&error) == TRUE) {
            pa_log_error("%s: error while parsing the message '%s', %s: %s",
                    __FILE__, name, error.name, error.message);
        }
    }

    dbus_error_free(&error);
#endif
    result = DBUS_HANDLER_RESULT_HANDLED;
    ROUTER_FUNCTION_EXIT;
    return result;
}

/**
 * @brief The command side removed connection notification handler
 * @param conn: The dbus connection pointer.
 *        msg: The dbus message.
 *        args: The pointer which was registered while registering this function, The userdata.
 * @return DBusHandlerResult
 */
static DBusHandlerResult router_dbusif_command_cb_removed_connection_handler(DBusConnection *conn, DBusMessage *msg,
        void *arg) {
    DBusError error;
    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    dbus_bool_t success = FALSE;
    uint16_t main_connection = 0;
    DBusMessage *reply = NULL;

    struct userdata *u = (struct userdata *) arg;
    const char *name = dbus_message_get_member(msg);
    ROUTER_FUNCTION_ENTRY;
    pa_assert(u);
    pa_assert(msg);
    pa_assert(name);

    dbus_error_init(&error);
    success = dbus_message_get_args(msg, &error, DBUS_TYPE_UINT16, &main_connection, DBUS_TYPE_INVALID);

    if ( success == TRUE ) {
        if ( u->dbusif->cb_removed_main_connection ) {
            u->dbusif->cb_removed_main_connection(u, main_connection);
        }
        reply = dbus_message_new_method_return(msg);
        if ( reply ) {
            success = dbus_connection_send(conn, reply, NULL);
            if ( success == TRUE ) {
                result = DBUS_HANDLER_RESULT_HANDLED;
                pa_log_debug("%s: handled message '%s'", __FILE__, name);
            }
            dbus_message_unref(reply);
        }
    } else {
        if ( dbus_error_is_set(&error) == TRUE ) {
            pa_log_error("%s: error while parsing the message '%s', %s: %s", __FILE__, name, error.name, error.message);
        }
    }

    dbus_error_free(&error);
    ROUTER_FUNCTION_EXIT;
    return result;
}

/**
 * @brief The command side connection state change notification handler
 * @param conn: The dbus connection pointer.
 *        msg: The dbus message.
 *        args: The pointer which was registered while registering this function, The userdata.
 * @return DBusHandlerResult
 */
static DBusHandlerResult router_dbusif_command_cb_connection_state_changed_handler(DBusConnection *conn,
        DBusMessage *msg, void *arg) {
    DBusError error;
    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    dbus_bool_t success = FALSE;
    uint16_t main_connection = 0;
    int16_t state = 0;
    DBusMessage *reply = NULL;

    struct userdata *u = (struct userdata *) arg;
    const char *name = dbus_message_get_member(msg);
    ROUTER_FUNCTION_ENTRY;
    pa_assert(u);
    pa_assert(msg);
    pa_assert(name);

    dbus_error_init(&error);
    success = dbus_message_get_args(msg, &error, DBUS_TYPE_UINT16, &main_connection, DBUS_TYPE_INT16, &state,
            DBUS_TYPE_INVALID);

    if ( success == TRUE ) {
        if ( u->dbusif->cb_main_connection_state_changed ) {
            u->dbusif->cb_main_connection_state_changed(u, main_connection, (int32_t) state);
        }
        reply = dbus_message_new_method_return(msg);
        if ( reply ) {
            success = dbus_connection_send(conn, reply, NULL);
            if ( success == TRUE ) {
                result = DBUS_HANDLER_RESULT_HANDLED;
                pa_log_debug("%s: handled message '%s'", __FILE__, name);
            }
            dbus_message_unref(reply);
        }
    } else {
        if ( dbus_error_is_set(&error) == TRUE ) {
            pa_log_error("%s: error while parsing the message '%s', %s: %s", __FILE__, name, error.name, error.message);
        }
    }

    dbus_error_free(&error);
    ROUTER_FUNCTION_EXIT;
    return result;
}

/**
 * @brief This function performs the initialization of dbus interface.
 * @param u: The user data of the module.
 *        init_data: The data structure filled by the client which fills the dbus callbacks and other params.
 * @return router_dbusif
 */
router_dbusif *router_dbusif_init(struct userdata *u, router_init_data_t* init_data) {
    static const DBusObjectPathVTable vtable = { .message_function = router_dbusif_method_handler, };

    DBusError error;
    DBusConnection *dbusconn;
    int result;
    ROUTER_FUNCTION_ENTRY;
    router_dbusif* routerif = pa_xnew0(router_dbusif, 1);
    if ( routerif == NULL ) {
        return routerif;
    }

    PA_LLIST_HEAD_INIT(pending_dbus_calls_t, routerif->pending_call_list);

    routerif->pulse_router_dbus_return_interface_name = pa_xstrdup(init_data->pulse_router_dbus_return_interface_name);
    routerif->pulse_router_dbus_name = pa_xstrdup(init_data->pulse_router_dbus_name);
    routerif->pulse_router_dbus_interface_name = pa_xstrdup(init_data->pulse_router_dbus_interface_name);
    routerif->pulse_router_dbus_path = pa_xstrdup(init_data->pulse_router_dbus_path);
    routerif->am_command_dbus_name = pa_xstrdup(init_data->am_command_dbus_name);
    routerif->am_command_dbus_interface_name = pa_xstrdup(init_data->am_command_dbus_interface_name);
    routerif->am_command_dbus_path = pa_xstrdup(init_data->am_command_dbus_path);
    routerif->am_routing_dbus_name = pa_xstrdup(init_data->am_routing_dbus_name);
    routerif->am_routing_dbus_interface_name = pa_xstrdup(init_data->am_routing_dbus_interface_name);
    routerif->am_routing_dbus_path = pa_xstrdup(init_data->am_routing_dbus_path);
    routerif->am_watch_rule = pa_xstrdup(init_data->am_watch_rule);

    /* store callbacks in userdata */
    routerif->cb_new_main_connection = init_data->cb_new_main_connection;
    routerif->cb_removed_main_connection = init_data->cb_removed_main_connection;
    routerif->cb_main_connection_state_changed = init_data->cb_main_connection_state_changed;
    routerif->cb_command_connect_reply = init_data->cb_command_connect_reply;
    routerif->cb_command_disconnect_reply = init_data->cb_command_disconnect_reply;
    routerif->cb_routing_register_domain_reply = init_data->cb_routing_register_domain_reply;
    routerif->cb_routing_deregister_domain_reply = init_data->cb_routing_deregister_domain_reply;
    routerif->cb_routing_register_sink_reply = init_data->cb_routing_register_sink_reply;
    routerif->cb_routing_deregister_sink_reply = init_data->cb_routing_deregister_sink_reply;
    routerif->cb_routing_register_source_reply = init_data->cb_routing_register_source_reply;
    routerif->cb_routing_deregister_source_reply = init_data->cb_routing_deregister_source_reply;
    routerif->cb_routing_async_connect = init_data->cb_routing_async_connect;
    routerif->cb_routing_async_disconnect = init_data->cb_routing_async_disconnect;
    routerif->cb_routing_async_set_sink_volume = init_data->cb_routing_async_set_sink_volume;
    routerif->cb_routing_async_set_source_volume = init_data->cb_routing_async_set_source_volume;
    routerif->cb_routing_async_set_source_state = init_data->cb_routing_async_set_source_state;
    routerif->cb_routing_peek_source_reply = init_data->cb_routing_peek_source_reply;
    routerif->cb_routing_peek_sink_reply = init_data->cb_routing_peek_sink_reply;
    routerif->cb_routing_get_domain_of_source_reply = init_data->cb_routing_get_domain_of_source_reply;
    routerif->cb_routing_get_domain_of_sink_reply = init_data->cb_routing_get_domain_of_sink_reply;

    dbus_error_init(&error);
    routerif->conn = pa_dbus_bus_get(u->core, DBUS_BUS_SYSTEM, &error);

    if ( (routerif->conn == NULL) || (dbus_error_is_set(&error) == TRUE) ) {
        pa_log_error("%s: failed to get system Bus: %s: %s", __FILE__, error.name, error.message);
        goto fail;
    }

    dbusconn = pa_dbus_connection_get(routerif->conn);
    result = dbus_bus_request_name(dbusconn, routerif->pulse_router_dbus_name,
            DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);

    if ( result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER && result != DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER ) {
        pa_log_error("%s: D-Bus name request failed: %s: %s", __FILE__, error.name, error.message);
        goto fail;
    }

    pa_log_debug("%s: now owner of '%s' D-Bus name on system bus", __FILE__, routerif->pulse_router_dbus_name);

    dbus_connection_register_object_path(dbusconn, routerif->pulse_router_dbus_path, &vtable, u);

    dbus_bus_add_match(dbusconn, routerif->am_watch_rule, &error);
    dbus_connection_add_filter(dbusconn, router_dbusif_method_handler, u, NULL);
    ROUTER_FUNCTION_EXIT;
    return routerif;

    fail:
    MODULE_ROUTER_FREE(routerif->pulse_router_dbus_return_interface_name);
    MODULE_ROUTER_FREE(routerif->pulse_router_dbus_name);
    MODULE_ROUTER_FREE(routerif->pulse_router_dbus_interface_name);
    MODULE_ROUTER_FREE(routerif->pulse_router_dbus_path);
    MODULE_ROUTER_FREE(routerif->am_command_dbus_name);
    MODULE_ROUTER_FREE(routerif->am_command_dbus_interface_name);
    MODULE_ROUTER_FREE(routerif->am_command_dbus_path);
    MODULE_ROUTER_FREE(routerif->am_routing_dbus_name);
    MODULE_ROUTER_FREE(routerif->am_routing_dbus_interface_name);
    MODULE_ROUTER_FREE(routerif->am_routing_dbus_path);
    MODULE_ROUTER_FREE(routerif->am_watch_rule);
    MODULE_ROUTER_FREE(routerif);
    dbus_error_free(&error);
    return NULL;
}

/**
 * @brief This function uninitialises this module
 * @param routerif: The router interface structure.
 * @return void
 */

void router_dbusif_done(struct router_dbusif *routerif) {
    ROUTER_FUNCTION_ENTRY;
    MODULE_ROUTER_FREE(routerif->pulse_router_dbus_return_interface_name);
    MODULE_ROUTER_FREE(routerif->pulse_router_dbus_name);
    MODULE_ROUTER_FREE(routerif->pulse_router_dbus_interface_name);
    MODULE_ROUTER_FREE(routerif->pulse_router_dbus_path);
    MODULE_ROUTER_FREE(routerif->am_command_dbus_name);
    MODULE_ROUTER_FREE(routerif->am_command_dbus_interface_name);
    MODULE_ROUTER_FREE(routerif->am_command_dbus_path);
    MODULE_ROUTER_FREE(routerif->am_routing_dbus_name);
    MODULE_ROUTER_FREE(routerif->am_routing_dbus_interface_name);
    MODULE_ROUTER_FREE(routerif->am_routing_dbus_path);
    MODULE_ROUTER_FREE(routerif->am_watch_rule);
    MODULE_ROUTER_FREE(routerif);
    ROUTER_FUNCTION_EXIT;

}

/**
 * @brief This function sends the command side connect request.
 * @param u: The user data of the module.
 *        connect: The data structure for connect request.
 * @return int
 */
int router_dbusif_command_connect(struct userdata *u, am_main_connection_t* connect) {
    int result = -1;
    router_dbusif* dbusif = u->dbusif;
    dbus_bool_t success = FALSE;
    DBusMessage* dbus_request = NULL;
    am_main_connection_t* mainconnect_data;
    ROUTER_FUNCTION_ENTRY;
    if ( dbusif == NULL || dbusif->am_command_dbus_interface_name == NULL || dbusif->am_command_dbus_name == NULL
            || dbusif->am_command_dbus_path == NULL ) {
        return result;
    }
    do {
        dbus_request = dbus_message_new_method_call(dbusif->am_command_dbus_name, dbusif->am_command_dbus_path,
                dbusif->am_command_dbus_interface_name, "Connect");
        if ( dbus_request == NULL ) {
            pa_log_error("DBUS message allocation failed");
            break;
        }
        success = dbus_message_append_args(dbus_request, DBUS_TYPE_UINT16, &connect->source_id, DBUS_TYPE_UINT16,
                &connect->sink_id, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            pa_log_error("DBUS argument append failed");
            break;
        }

        mainconnect_data = pa_xmemdup(connect, sizeof(am_main_connection_t));
        if ( mainconnect_data == NULL ) {
            break;
        }
        success = send_message_with_reply(u, dbus_request, router_dbusif_connect_reply_cb, mainconnect_data);
        if ( success == FALSE ) {
            pa_log_error("dbus_connection_send failed");
            pa_xfree(mainconnect_data);
            break;
        }
    } while ( 0 );
    if ( dbus_request != NULL ) {
        dbus_message_unref(dbus_request);
    }
    ROUTER_FUNCTION_EXIT;
    return result;
}

/**
 * @brief This function sends the command side disconnect request.
 * @param u: The user data of the module.
 *        data: The data structure for disconnect request.
 * @return int
 */
int router_dbusif_command_disconnect(struct userdata *u, am_disconnect_t* data) {

    int result = -1;
    router_dbusif* dbusif = u->dbusif;
    dbus_bool_t success = FALSE;
    DBusMessage* dbus_request = NULL;
    DBusMessage* dbus_reply = NULL;

    uint16_t *disconnection_data = NULL;
    ROUTER_FUNCTION_ENTRY;
    if ( dbusif == NULL || dbusif->am_command_dbus_interface_name == NULL || dbusif->am_command_dbus_name == NULL
            || dbusif->am_command_dbus_path == NULL ) {
        return result;
    }
    do {
        dbus_request = dbus_message_new_method_call(dbusif->am_command_dbus_name, dbusif->am_command_dbus_path,
                dbusif->am_command_dbus_interface_name, "Disconnect");
        if ( dbus_request == NULL ) {
            pa_log_error("DBUS message allocation failed");
            break;
        }
        success = dbus_message_append_args(dbus_request, DBUS_TYPE_UINT16, &data->connection_id, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            pa_log_error("DBUS argument append failed");
            break;
        }

        disconnection_data = pa_xmemdup(data, sizeof(am_disconnect_t));
        success = send_message_with_reply(u, dbus_request, router_dbusif_disconnect_reply_cb, disconnection_data);

        if ( success == FALSE ) {
            pa_log_error("send_message_with_reply failed");
            pa_xfree(disconnection_data);
            break;
        }
    } while ( 0 );

    if ( dbus_request != NULL ) {
        dbus_message_unref(dbus_request);
    }
    ROUTER_FUNCTION_EXIT;
    return result;

}

/**
 * @brief This function sends the routing side domain register request
 * @param u: The user data of the module.
 *        domain: The data structure for domain registration.
 * @return int
 */
int router_dbusif_routing_register_domain(struct userdata *u, am_domain_register_t *domain) {
    int result = -1;
    router_dbusif* dbusif = u->dbusif;
    dbus_bool_t success = FALSE;
    DBusMessage* dbus_request = NULL;
    DBusMessage* dbus_reply = NULL;
    DBusMessageIter iter;
    dbus_bool_t sucess = FALSE;
    dbus_bool_t early;
    dbus_bool_t complete;
    ROUTER_FUNCTION_ENTRY;
    if ( dbusif == NULL || dbusif->am_routing_dbus_name == NULL || dbusif->am_routing_dbus_path == NULL
            || dbusif->am_routing_dbus_interface_name == NULL ) {
        return result;
    }
    do {
        dbus_request = dbus_message_new_method_call(dbusif->am_routing_dbus_name, dbusif->am_routing_dbus_path,
                dbusif->am_routing_dbus_interface_name, "registerDomain");
        if ( dbus_request == NULL ) {
            pa_log_error("DBUS message allocation failed");
            break;
        }

        pa_log_error("bus_name: %s object_path: %s interface_name: %s", dbusif->am_routing_dbus_name,
                dbusif->am_routing_dbus_path, dbusif->am_routing_dbus_interface_name);
        early = (domain->early == true) ? TRUE : FALSE;
        complete = (domain->complete == true) ? TRUE : FALSE;
        char* domainname = domain->name;
        char* busname = domain->busname;
        char* nodename = domain->nodename;

        dbus_message_iter_init_append(dbus_request, &iter);
#ifdef  GENIVI_DBUS_PLUGIN
        DBusMessageIter domainStructIter;
        success = dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, NULL, &domainStructIter);
        success = success && dbus_message_iter_append_basic(&domainStructIter, DBUS_TYPE_UINT16, &(domain->domain_id));
        success = success && dbus_message_iter_append_basic(&domainStructIter, DBUS_TYPE_STRING, &domainname);
        success = success && dbus_message_iter_append_basic(&domainStructIter, DBUS_TYPE_STRING, &busname);
        success = success && dbus_message_iter_append_basic(&domainStructIter, DBUS_TYPE_STRING, &nodename);
        success = success && dbus_message_iter_append_basic(&domainStructIter, DBUS_TYPE_BOOLEAN, &early);
        success = success && dbus_message_iter_append_basic(&domainStructIter, DBUS_TYPE_BOOLEAN, &complete);
        int16_t state = domain->state;
        success = success && dbus_message_iter_append_basic(&domainStructIter, DBUS_TYPE_INT16, &state);
        success = success && dbus_message_iter_close_container(&iter, &domainStructIter);
        char* dbus_strings = dbusif->pulse_router_dbus_name;
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &dbus_strings);
        dbus_strings = dbusif->pulse_router_dbus_path;
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &dbus_strings);
        dbus_strings = dbusif->pulse_router_dbus_return_interface_name;
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &dbus_strings);

#else
        success = dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT16, &domain->domain_id);
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &domainname);
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &busname);
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &nodename);
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_BOOLEAN, &early);
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_BOOLEAN, &complete);
        success = success
        && dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT16, &domain->state);
        char* return_if_name = dbusif->pulse_router_dbus_return_interface_name;
        success = success
        && dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &return_if_name);
#endif
        if ( success == FALSE ) {
            pa_log_error("DBUS argument append failed 8");
            break;
        }

        am_domain_register_t *domain_register_data = pa_xmemdup(domain, sizeof(am_domain_register_t));
        success = send_message_with_reply(u, dbus_request, router_dbusif_register_domain_reply_cb,
                domain_register_data);
        if ( success == FALSE ) {
            pa_log_error("send_message_with_reply failed");
            break;
        }
    } while ( 0 );
    if ( dbus_reply != NULL ) {
        dbus_message_unref(dbus_reply);
    }
    if ( dbus_request != NULL ) {
        dbus_message_unref(dbus_request);
    }
    ROUTER_FUNCTION_EXIT;
    return result;

}

/**
 * @brief This function sends the routing side domain deregister request
 * @param u: The user data of the module.
 *        data: The data structure for domain deregistration.
 * @return int
 */
int router_dbusif_routing_deregister_domain(struct userdata *u, am_domain_unregister_t* data) {

    int result = -1;
    router_dbusif* dbusif = u->dbusif;
    dbus_bool_t success = FALSE;
    DBusMessage* dbus_request = NULL;
    DBusMessage* dbus_reply = NULL;
    ROUTER_FUNCTION_ENTRY;
    if ( dbusif == NULL || dbusif->am_routing_dbus_name == NULL || dbusif->am_routing_dbus_path == NULL
            || dbusif->am_routing_dbus_interface_name == NULL ) {
        return result;
    }
    do {
        dbus_request = dbus_message_new_method_call(dbusif->am_routing_dbus_name, dbusif->am_routing_dbus_path,
                dbusif->am_routing_dbus_interface_name, "deregisterDomain");
        if ( dbus_request == NULL ) {
            pa_log_error("DBUS message allocation failed");
            break;
        }
        success = dbus_message_append_args(dbus_request, DBUS_TYPE_UINT16, &data->domain_id, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            pa_log_error("DBUS argument appent failed");
            break;
        }
        am_domain_unregister_t *domain_unregister_data = pa_xmemdup(data, sizeof(am_domain_unregister_t));
        success = send_message_with_reply(u, dbus_request, router_dbusif_deregister_domain_reply_cb,
                domain_unregister_data);
        if ( success == FALSE ) {
            pa_log_error("error in send_message_with_reply for de-register domain");
            break;
        }
    } while ( 0 );
    if ( dbus_reply != NULL ) {
        dbus_message_unref(dbus_reply);
    }
    if ( dbus_request != NULL ) {
        dbus_message_unref(dbus_request);
    }
    ROUTER_FUNCTION_EXIT;
    return result;

}

/**
 * @brief This internal function to append the sound property list.
 * @param iter: The dbus iterator.
 * @return dbus_bool_t
 */
static dbus_bool_t router_dbusif_append_list_sound_Property(DBusMessageIter* iter) {
    DBusMessageIter arrayIter;
    DBusMessageIter structIter;
    DBusMessageIter outerStructIter;
    int size = 0;
    dbus_bool_t success = true;
#ifdef GENIVI_DBUS_PLUGIN
    int32_t property = 1;
    int16_t value = 1;
    success = success && dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "(in)", &arrayIter);
    success = success && dbus_message_iter_open_container(&arrayIter, DBUS_TYPE_STRUCT, NULL, &outerStructIter);
    success = success && dbus_message_iter_append_basic(&outerStructIter, DBUS_TYPE_INT32, &property);
    success = success && dbus_message_iter_append_basic(&outerStructIter, DBUS_TYPE_INT16, &value);
    success = success && dbus_message_iter_close_container(&arrayIter, &outerStructIter);
    success = success && dbus_message_iter_close_container(iter, &arrayIter);
#else
    success = success && dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL, &outerStructIter);
    success = success && dbus_message_iter_append_basic(&outerStructIter, DBUS_TYPE_INT16, &size);
    success = success && dbus_message_iter_open_container(&outerStructIter, DBUS_TYPE_ARRAY, "(nn)", &arrayIter);
    success = success && dbus_message_iter_close_container(&outerStructIter, &arrayIter);
    success = success && dbus_message_iter_close_container(iter, &outerStructIter);
#endif
    return success;
}

/**
 * @brief This internal function to append the main sound property list.
 * @param iter: The dbus iterator.
 * @return dbus_bool_t
 */
static dbus_bool_t router_dbusif_append_list_main_sound_Property(DBusMessageIter* iter) {
    DBusMessageIter arrayIter;
    DBusMessageIter structIter;
    DBusMessageIter outerStructIter;
    int size = 0;
    dbus_bool_t success = true;
#ifdef GENIVI_DBUS_PLUGIN
    int32_t property = 1;
    int16_t value = 1;
    success = success && dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "(in)", &arrayIter);
    success = success && dbus_message_iter_open_container(&arrayIter, DBUS_TYPE_STRUCT, NULL, &outerStructIter);
    success = success && dbus_message_iter_append_basic(&outerStructIter, DBUS_TYPE_INT32, &property);
    success = success && dbus_message_iter_append_basic(&outerStructIter, DBUS_TYPE_INT16, &value);
    success = success && dbus_message_iter_close_container(&arrayIter, &outerStructIter);
    success = success && dbus_message_iter_close_container(iter, &arrayIter);
#else
    success = success && dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL, &outerStructIter);
    success = success && dbus_message_iter_append_basic(&outerStructIter, DBUS_TYPE_INT16, &size);
    success = success && dbus_message_iter_open_container(&outerStructIter, DBUS_TYPE_ARRAY, "(nn)", &arrayIter);
    success = success && dbus_message_iter_close_container(&outerStructIter, &arrayIter);
    success = success && dbus_message_iter_close_container(iter, &outerStructIter);
#endif
    return success;
}

/**
 * @brief This internal function to append the connection format list
 * @param iter: The dbus iterator.
 * @return dbus_bool_t
 */
static dbus_bool_t router_dbusif_append_list_conenction_format(DBusMessageIter* iter) {
    DBusMessageIter arrayIter;
    DBusMessageIter structIter;
    DBusMessageIter outerStructIter;
    int size = 3;
    int32_t connectionformat = 3;
    dbus_bool_t success = true;
#ifdef GENIVI_DBUS_PLUGIN
    success = success && dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "i", &arrayIter);
    for ( ; size > 0 ; size-- ) {
        success = success && dbus_message_iter_append_basic(&arrayIter, DBUS_TYPE_INT32, &connectionformat);
        connectionformat--;
    }
    success = success && dbus_message_iter_close_container(iter, &arrayIter);

#else
    success = success && dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL, &outerStructIter);
    success = success && dbus_message_iter_append_basic(&outerStructIter, DBUS_TYPE_INT16, &size);
    success = success && dbus_message_iter_open_container(&outerStructIter, DBUS_TYPE_ARRAY, "n", &arrayIter);
    for (;size>0;size--)
    {
        success = success && dbus_message_iter_append_basic(&arrayIter, DBUS_TYPE_INT16, &connectionformat);
        connectionformat--;
    }
    success = success && dbus_message_iter_close_container(&outerStructIter, &arrayIter);
    success = success && dbus_message_iter_close_container(iter, &outerStructIter);
#endif
    return success;
}

/**
 * @brief This internal function to append the notification configuration list
 * @param iter: The dbus iterator.
 * @return dbus_bool_t
 */
static dbus_bool_t router_dbusif_append_list_notification_configuration(DBusMessageIter* iter) {
    DBusMessageIter arrayIter;
    DBusMessageIter structIter;
    DBusMessageIter outerStructIter;
    int size = 0;
    dbus_bool_t success = true;
#ifdef GENIVI_DBUS_PLUGIN
    int32_t type = 1;
    int32_t param = 1;
    int16_t status = 1;
    success = success && dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "(iin)", &arrayIter);
    success = success && dbus_message_iter_open_container(&arrayIter, DBUS_TYPE_STRUCT, NULL, &outerStructIter);
    success = success && dbus_message_iter_append_basic(&outerStructIter, DBUS_TYPE_INT32, &type);
    success = success && dbus_message_iter_append_basic(&outerStructIter, DBUS_TYPE_INT32, &param);
    success = success && dbus_message_iter_append_basic(&outerStructIter, DBUS_TYPE_INT16, &status);
    success = success && dbus_message_iter_close_container(&arrayIter, &outerStructIter);
    success = success && dbus_message_iter_close_container(iter, &arrayIter);
#else
    success = success && dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL, &outerStructIter);
    success = success && dbus_message_iter_append_basic(&outerStructIter, DBUS_TYPE_INT16, &size);
    success = success && dbus_message_iter_open_container(&outerStructIter, DBUS_TYPE_ARRAY, "(nnn)", &arrayIter);
    success = success && dbus_message_iter_close_container(&outerStructIter, &arrayIter);
    success = success && dbus_message_iter_close_container(iter, &outerStructIter);
#endif
    return success;
}

/**
 * @brief This function sends the routing side register sink request.
 * @param u: The user data of the module.
 *        data: The data structure for sink registration.
 * @return int
 */
int router_dbusif_routing_register_sink(struct userdata *u, am_sink_register_t* data) {
    int result = -1;
    router_dbusif* dbusif = u->dbusif;
    dbus_bool_t success = TRUE;
    DBusMessage* dbus_request = NULL;
    DBusMessage* dbus_reply = NULL;
    DBusMessageIter iter;

    ROUTER_FUNCTION_ENTRY;
    if ( dbusif == NULL || dbusif->am_routing_dbus_name == NULL || dbusif->am_routing_dbus_path == NULL
            || dbusif->am_routing_dbus_interface_name == NULL ) {
        return result;
    }
    do {
        dbus_request = dbus_message_new_method_call(dbusif->am_routing_dbus_name, dbusif->am_routing_dbus_path,
                dbusif->am_routing_dbus_interface_name, "registerSink");
        if ( dbus_request == NULL ) {
            pa_log_error("DBUS message allocation failed");
            break;
        }

        dbus_message_iter_init_append(dbus_request, &iter);
        /*
         * TODO construct the dbus message
         */
        dbus_bool_t sinkVisible = FALSE;
        char* sinkname = data->name;
        if ( data->visible ) {
            sinkVisible = TRUE;
        }
#ifdef GENIVI_DBUS_PLUGIN
        DBusMessageIter SinkStructIter;
        success = success && dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, NULL, &SinkStructIter);
        success = success && dbus_message_iter_append_basic(&SinkStructIter, DBUS_TYPE_UINT16, &(data->sink_id));
        success = success && dbus_message_iter_append_basic(&SinkStructIter, DBUS_TYPE_STRING, &(sinkname));
        success = success && dbus_message_iter_append_basic(&SinkStructIter, DBUS_TYPE_UINT16, &(data->domain_id));
        int32_t sink_class_id = data->sink_class_id;
        success = success && dbus_message_iter_append_basic(&SinkStructIter, DBUS_TYPE_INT32, &(sink_class_id));
        success = success && dbus_message_iter_append_basic(&SinkStructIter, DBUS_TYPE_INT16, &(data->volume));
        success = success && dbus_message_iter_append_basic(&SinkStructIter, DBUS_TYPE_BOOLEAN, &sinkVisible);
        // Availability
        DBusMessageIter structAvailIter;
        success = success
                && dbus_message_iter_open_container(&SinkStructIter, DBUS_TYPE_STRUCT, NULL, &structAvailIter);
        int32_t available = data->available;
        success = success && dbus_message_iter_append_basic(&structAvailIter, DBUS_TYPE_INT32, &(available));
        int32_t availability_reason = data->availability_reason;
        success = success && dbus_message_iter_append_basic(&structAvailIter, DBUS_TYPE_INT32, &(availability_reason));
        success = success && dbus_message_iter_close_container(&SinkStructIter, &structAvailIter);

        // mute state
        success = success && dbus_message_iter_append_basic(&SinkStructIter, DBUS_TYPE_INT16, &(data->mute_state));
        success = success && dbus_message_iter_append_basic(&SinkStructIter, DBUS_TYPE_INT16, &(data->main_volume));
        //sound property
        success = success && router_dbusif_append_list_sound_Property(&SinkStructIter);
        // connection formats
        success = success && router_dbusif_append_list_conenction_format(&SinkStructIter);
        //listMainSoundProperties
        success = success && router_dbusif_append_list_main_sound_Property(&SinkStructIter);
        //listMainNotificationConfigurations
        success = success && router_dbusif_append_list_notification_configuration(&SinkStructIter);
        //listNotificationConfigurations
        success = success && router_dbusif_append_list_notification_configuration(&SinkStructIter);

        success = success && dbus_message_iter_close_container(&iter, &SinkStructIter);

#else
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT16, &(data->sink_id));
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &(sinkname));
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT16, &(data->domain_id));
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT16, &(data->sink_class_id));
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT16, &(data->volume));
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_BOOLEAN, &sinkVisible);
        // Availability
        DBusMessageIter structAvailIter;
        success = success && dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, NULL, &structAvailIter);
        success = success && dbus_message_iter_append_basic(&structAvailIter, DBUS_TYPE_INT16, &(data->available));
        success = success && dbus_message_iter_append_basic(&structAvailIter, DBUS_TYPE_INT16, &(data->availability_reason));
        success = success && dbus_message_iter_close_container(&iter, &structAvailIter);

        // mute state
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT16, &(data->mute_state));
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT16, &(data->main_volume));
        //sound property
        success = success && router_dbusif_append_list_sound_Property(&iter);
        // connection formats
        success = success && router_dbusif_append_list_conenction_format(&iter);
        //listMainSoundProperties
        success = success && router_dbusif_append_list_main_sound_Property(&iter);
        //listMainNotificationConfigurations
        success = success && router_dbusif_append_list_notification_configuration(&iter);
        //listNotificationConfigurations
        success = success && router_dbusif_append_list_notification_configuration(&iter);
#endif
        am_sink_register_t *sink_register_data = pa_xmemdup(data, sizeof(am_sink_register_t));
        pa_log_error("sink Name=%s", sink_register_data->name);
        dbus_reply = dbus_connection_send_with_reply_and_block(pa_dbus_connection_get(u->dbusif->conn), dbus_request,
                DBUS_TIMEOUT_USE_DEFAULT, NULL);

        if ( dbus_reply == NULL ) {
            pa_log_error("send_message_with_reply failed");
            break;
        }

        router_dbusif_register_sink_reply_cb(u, dbus_reply, sink_register_data);

    } while ( 0 );
    if ( dbus_reply != NULL ) {
        dbus_message_unref(dbus_reply);
    }
    if ( dbus_request != NULL ) {
        dbus_message_unref(dbus_request);
    }
    ROUTER_FUNCTION_EXIT;
    return result;
}

/**
 * @brief This function sends the routing side deregister sink request.
 * @param u: The user data of the module.
 *        data: The data structure for sink deregistration.
 * @return int
 */
int router_dbusif_routing_deregister_sink(struct userdata *u, am_sink_unregister_t* data) {
    int result = -1;
    router_dbusif* dbusif = u->dbusif;
    dbus_bool_t success = FALSE;
    DBusMessage* dbus_request = NULL;
    DBusMessage* dbus_reply = NULL;
    ROUTER_FUNCTION_ENTRY;
    if ( dbusif == NULL || dbusif->am_routing_dbus_name == NULL || dbusif->am_routing_dbus_path == NULL
            || dbusif->am_routing_dbus_interface_name == NULL ) {
        return result;
    }
    do {
        dbus_request = dbus_message_new_method_call(dbusif->am_routing_dbus_name, dbusif->am_routing_dbus_path,
                dbusif->am_routing_dbus_interface_name, "deregisterSink");
        if ( dbus_request == NULL ) {
            pa_log_error("DBUS message allocation failed");
            break;
        }
        success = dbus_message_append_args(dbus_request, DBUS_TYPE_UINT16, &data->sink_id, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            pa_log_error("DBUS argument append failed");
            break;
        }
        am_sink_unregister_t *sink_unregister_data = pa_xmemdup(data, sizeof(am_sink_unregister_t));
        success = send_message_with_reply(u, dbus_request, router_dbusif_deregister_sink_reply_cb,
                sink_unregister_data);
        if ( success == FALSE ) {
            pa_log_error("error in send_message_with_reply for de-register sink");
            break;
        }
    } while ( 0 );
    if ( dbus_reply != NULL ) {
        dbus_message_unref(dbus_reply);
    }
    if ( dbus_request != NULL ) {
        dbus_message_unref(dbus_request);
    }
    ROUTER_FUNCTION_EXIT;
    return result;
}

/**
 * @brief This function sends the routing side register source request.
 * @param u: The user data of the module.
 *        data: The data structure for source registration.
 * @return int
 */
int router_dbusif_routing_register_source(struct userdata *u, am_source_register_t* data) {
    int result = -1;
    router_dbusif* dbusif = u->dbusif;
    dbus_bool_t success = TRUE;
    DBusMessage* dbus_request = NULL;
    DBusMessage* dbus_reply = NULL;
    DBusMessageIter iter;

    ROUTER_FUNCTION_ENTRY;
    if ( dbusif == NULL || dbusif->am_routing_dbus_name == NULL || dbusif->am_routing_dbus_path == NULL
            || dbusif->am_routing_dbus_interface_name == NULL ) {
        return result;
    }
    do {
        dbus_request = dbus_message_new_method_call(dbusif->am_routing_dbus_name, dbusif->am_routing_dbus_path,
                dbusif->am_routing_dbus_interface_name, "registerSource");
        if ( dbus_request == NULL ) {
            pa_log_error("DBUS message allocation failed");
            break;
        }

        dbus_message_iter_init_append(dbus_request, &iter);
        /*
         * TODO construct the dbus message
         */
        DBusMessageIter outerStruct;
        success = dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, NULL, &outerStruct);
        char* sourcename = data->name;
        success = success && dbus_message_iter_append_basic(&outerStruct, DBUS_TYPE_UINT16, &(data->source_id));
        success = success && dbus_message_iter_append_basic(&outerStruct, DBUS_TYPE_UINT16, &(data->domain_id));
        success = success && dbus_message_iter_append_basic(&outerStruct, DBUS_TYPE_STRING, &(sourcename));
        success = success && dbus_message_iter_append_basic(&outerStruct, DBUS_TYPE_UINT16, &(data->source_class_id));
        int32_t sourcestate = data->source_state;
        success = success && dbus_message_iter_append_basic(&outerStruct, DBUS_TYPE_INT32, &sourcestate);
        success = success && dbus_message_iter_append_basic(&outerStruct, DBUS_TYPE_INT16, &(data->volume));
        dbus_bool_t sourceVisible = data->visible == true ? TRUE : FALSE;
        success = success && dbus_message_iter_append_basic(&outerStruct, DBUS_TYPE_BOOLEAN, &sourceVisible);
        // Availability
        DBusMessageIter structAvailIter;
        success = success && dbus_message_iter_open_container(&outerStruct, DBUS_TYPE_STRUCT, NULL, &structAvailIter);
        int32_t available = data->available;
        int32_t availability_reason = data->availability_reason;
        success = success && dbus_message_iter_append_basic(&structAvailIter, DBUS_TYPE_INT32, &(available));
        success = success && dbus_message_iter_append_basic(&structAvailIter, DBUS_TYPE_INT32, &(availability_reason));
        success = success && dbus_message_iter_close_container(&outerStruct, &structAvailIter);
        // InterruptState
        success = success && dbus_message_iter_append_basic(&outerStruct, DBUS_TYPE_UINT16, &(data->interrupt_state));

        success = success && router_dbusif_append_list_sound_Property(&outerStruct);
        // connection formats
        success = success && router_dbusif_append_list_conenction_format(&outerStruct);
        //listMainSoundProperties
        success = success && router_dbusif_append_list_main_sound_Property(&outerStruct);
        //listMainNotificationConfigurations
        success = success && router_dbusif_append_list_notification_configuration(&outerStruct);
        //listNotificationConfigurations
        success = success && router_dbusif_append_list_notification_configuration(&outerStruct);
        success = success && dbus_message_iter_close_container(&iter, &outerStruct);

        am_source_register_t *source_register_data = pa_xmemdup(data, sizeof(am_source_register_t));

        dbus_reply = dbus_connection_send_with_reply_and_block(pa_dbus_connection_get(u->dbusif->conn), dbus_request,
                DBUS_TIMEOUT_USE_DEFAULT, NULL);

        if ( dbus_reply == NULL ) {
            pa_log_error("send_message_with_reply failed");
            break;
        }
        router_dbusif_register_source_reply_cb(u, dbus_reply, source_register_data);
    } while ( 0 );
    if ( dbus_reply != NULL ) {
        dbus_message_unref(dbus_reply);
    }
    if ( dbus_request != NULL ) {
        dbus_message_unref(dbus_request);
    }
    ROUTER_FUNCTION_EXIT;
    return result;
}

/**
 * @brief This function sends the routing side peek sink request.
 * @param u: The user data of the module.
 *        sink_name: The name of the sink for peek
 * @return int
 */
int router_dbusif_routing_peek_sink(struct userdata *u, const char* sink_name) {
    int result = 0;
    router_dbusif* dbusif = u->dbusif;
    dbus_bool_t success = TRUE;
    DBusMessage* dbus_request = NULL;
    DBusMessage* dbus_reply = NULL;
    DBusMessageIter iter;

    ROUTER_FUNCTION_ENTRY;
    if ( dbusif == NULL || dbusif->am_routing_dbus_name == NULL || dbusif->am_routing_dbus_path == NULL
            || dbusif->am_routing_dbus_interface_name == NULL ) {
        return result;
    }
    do {
        dbus_request = dbus_message_new_method_call(dbusif->am_routing_dbus_name, dbusif->am_routing_dbus_path,
                dbusif->am_routing_dbus_interface_name, "peekSink");
        if ( dbus_request == NULL ) {
            pa_log_error("DBUS message allocation failed");
            break;
        }

        dbus_message_iter_init_append(dbus_request, &iter);
        /*
         * TODO construct the dbus message
         */
        char* sinkname = (char*) sink_name;
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &(sinkname));
        dbus_reply = dbus_connection_send_with_reply_and_block(pa_dbus_connection_get(u->dbusif->conn), dbus_request,
                DBUS_TIMEOUT_USE_DEFAULT, NULL);
        am_sink_register_t sink_data;
        strncpy(sink_data.name, sinkname, AM_MAX_NAME_LENGTH);
        if ( dbus_reply == NULL ) {
            pa_log_error("send_message_with_reply failed");
            break;
        }
        router_dbusif_peek_sink_reply_cb(u, dbus_reply, &sink_name);
    } while ( 0 );
    if ( dbus_reply != NULL ) {
        dbus_message_unref(dbus_reply);
    }
    if ( dbus_request != NULL ) {
        dbus_message_unref(dbus_request);
    }
    ROUTER_FUNCTION_EXIT;
    return result;

}

/**
 * @brief This function sends the routing side peek source equest.
 * @param u: The user data of the module.
 *        source_name: The source name.
 * @return int
 */
int router_dbusif_routing_peek_source(struct userdata *u, const char* source_name) {
    int result = -1;
    router_dbusif* dbusif = u->dbusif;
    dbus_bool_t success = TRUE;
    DBusMessage* dbus_request = NULL;
    DBusMessage* dbus_reply = NULL;
    DBusMessageIter iter;

    ROUTER_FUNCTION_ENTRY;
    if ( dbusif == NULL || dbusif->am_routing_dbus_name == NULL || dbusif->am_routing_dbus_path == NULL
            || dbusif->am_routing_dbus_interface_name == NULL ) {
        return result;
    }
    do {
        dbus_request = dbus_message_new_method_call(dbusif->am_routing_dbus_name, dbusif->am_routing_dbus_path,
                dbusif->am_routing_dbus_interface_name, "peekSource");
        if ( dbus_request == NULL ) {
            pa_log_error("DBUS message allocation failed");
            break;
        }

        dbus_message_iter_init_append(dbus_request, &iter);
        /*
         * TODO construct the dbus message
         */
        char* sourcename = (char*) source_name;
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &(sourcename));
        dbus_reply = dbus_connection_send_with_reply_and_block(pa_dbus_connection_get(u->dbusif->conn), dbus_request,
                DBUS_TIMEOUT_USE_DEFAULT, NULL);
        am_source_register_t source_register_data;
        strncpy(source_register_data.name, source_name, AM_MAX_NAME_LENGTH);
        if ( dbus_reply == NULL ) {
            pa_log_error("send_message_with_reply failed");
            break;
        }
        router_dbusif_peek_source_reply_cb(u, dbus_reply, &source_register_data);
    } while ( 0 );
    if ( dbus_reply != NULL ) {
        dbus_message_unref(dbus_reply);
    }
    if ( dbus_request != NULL ) {
        dbus_message_unref(dbus_request);
    }
    ROUTER_FUNCTION_EXIT;
    return result;

}

/**
 * @brief This function sends the routing side get domain of source.
 * @param u: The user data of the module.
 *        source_id: The source id.
 * @return int
 */
int router_dbusif_get_domain_of_source(struct userdata *u, const uint16_t source_id) {
    int result = -1;
    router_dbusif* dbusif = u->dbusif;
    dbus_bool_t success = TRUE;
    DBusMessage* dbus_request = NULL;
    DBusMessage* dbus_reply = NULL;
    DBusMessageIter iter;

    ROUTER_FUNCTION_ENTRY;
    if ( dbusif == NULL || dbusif->am_routing_dbus_name == NULL || dbusif->am_routing_dbus_path == NULL
            || dbusif->am_routing_dbus_interface_name == NULL ) {
        return result;
    }
    do {
        dbus_request = dbus_message_new_method_call(dbusif->am_routing_dbus_name, dbusif->am_routing_dbus_path,
                dbusif->am_routing_dbus_interface_name, "getDomainOfSource");
        if ( dbus_request == NULL ) {
            pa_log_error("DBUS message allocation failed");
            break;
        }

        dbus_message_iter_init_append(dbus_request, &iter);
        /*
         * TODO construct the dbus message
         */
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT16, &(source_id));
        dbus_reply = dbus_connection_send_with_reply_and_block(pa_dbus_connection_get(u->dbusif->conn), dbus_request,
                DBUS_TIMEOUT_USE_DEFAULT, NULL);
        am_domain_of_source_sink_t source_domain;
        source_domain.id = source_id;
        if ( dbus_reply == NULL ) {
            pa_log_error("send_message_with_reply failed");
            break;
        }
        router_dbusif_get_domain_of_source_reply_cb(u, dbus_reply, &source_domain);
    } while ( 0 );
    if ( dbus_reply != NULL ) {
        dbus_message_unref(dbus_reply);
    }
    if ( dbus_request != NULL ) {
        dbus_message_unref(dbus_request);
    }
    ROUTER_FUNCTION_EXIT;
    return result;

}

/**
 * @brief This function sends the routing side get domain of sink request.
 * @param u: The user data of the module.
 *        sink_id: The sink_id
 * @return int
 */
int router_dbusif_get_domain_of_sink(struct userdata *u, const uint16_t sink_id) {
    int result = -1;
    router_dbusif* dbusif = u->dbusif;
    dbus_bool_t success = TRUE;
    DBusMessage* dbus_request = NULL;
    DBusMessage* dbus_reply = NULL;
    DBusMessageIter iter;

    ROUTER_FUNCTION_ENTRY;
    if ( dbusif == NULL || dbusif->am_routing_dbus_name == NULL || dbusif->am_routing_dbus_path == NULL
            || dbusif->am_routing_dbus_interface_name == NULL ) {
        return result;
    }
    do {
        dbus_request = dbus_message_new_method_call(dbusif->am_routing_dbus_name, dbusif->am_routing_dbus_path,
                dbusif->am_routing_dbus_interface_name, "getDomainOfSink");
        if ( dbus_request == NULL ) {
            pa_log_error("DBUS message allocation failed");
            break;
        }

        dbus_message_iter_init_append(dbus_request, &iter);
        /*
         * TODO construct the dbus message
         */
        success = success && dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT16, &(sink_id));
        dbus_reply = dbus_connection_send_with_reply_and_block(pa_dbus_connection_get(u->dbusif->conn), dbus_request,
                DBUS_TIMEOUT_USE_DEFAULT, NULL);
        am_domain_of_source_sink_t sink_domain;
        sink_domain.id = sink_id;
        if ( dbus_reply == NULL ) {
            pa_log_error("send_message_with_reply failed");
            break;
        }
        router_dbusif_get_domain_of_sink_reply_cb(u, dbus_reply, &sink_domain);
    } while ( 0 );
    if ( dbus_reply != NULL ) {
        dbus_message_unref(dbus_reply);
    }
    if ( dbus_request != NULL ) {
        dbus_message_unref(dbus_request);
    }
    ROUTER_FUNCTION_EXIT;
    return result;

}

/**
 * @brief This function sends the routing side deregister source request.
 * @param u: The user data of the module.
 *        data: The data structure for sink deregistration.
 * @return int
 */
int router_dbusif_routing_deregister_source(struct userdata *u, am_source_unregister_t* data) {
    int result = -1;
    router_dbusif* dbusif = u->dbusif;
    dbus_bool_t success = FALSE;
    DBusMessage* dbus_request = NULL;
    DBusMessage* dbus_reply = NULL;
    ROUTER_FUNCTION_ENTRY;
    if ( dbusif == NULL || dbusif->am_routing_dbus_name == NULL || dbusif->am_routing_dbus_path == NULL
            || dbusif->am_routing_dbus_interface_name == NULL ) {
        return result;
    }
    do {
        dbus_request = dbus_message_new_method_call(dbusif->am_routing_dbus_name, dbusif->am_routing_dbus_path,
                dbusif->am_routing_dbus_interface_name, "deregisterSource");
        if ( dbus_request == NULL ) {
            pa_log_error("DBUS message allocation failed");
            break;
        }
        success = dbus_message_append_args(dbus_request, DBUS_TYPE_UINT16, &data->source_id, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            pa_log_error("DBUS argument append failed");
            break;
        }
        am_source_unregister_t *source_unregister_data = pa_xmemdup(data, sizeof(am_source_unregister_t));
        success = send_message_with_reply(u, dbus_request, router_dbusif_deregister_source_reply_cb,
                source_unregister_data);
        if ( success == FALSE ) {
            pa_log_error("error in send_message_with_reply for de-register source");
            break;
        }
    } while ( 0 );
    if ( dbus_reply != NULL ) {
        dbus_message_unref(dbus_reply);
    }
    if ( dbus_request != NULL ) {
        dbus_message_unref(dbus_request);
    }
    ROUTER_FUNCTION_EXIT;
    return result;
}

/**
 * @brief This function frees the router interface.
 * @param u: The user data of the module.
 * @return int
 */
static void free_routerif(struct userdata *u) {
    DBusConnection *dbusconn;
    pending_dbus_calls_t *p, *n;
    router_dbusif* routerif = u->dbusif;
    ROUTER_FUNCTION_ENTRY;
    if ( routerif ) {

        if ( routerif->conn ) {
            dbusconn = pa_dbus_connection_get(routerif->conn);
            PA_LLIST_FOREACH_SAFE(p, n, routerif->pending_call_list)
            {
                PA_LLIST_REMOVE(pending_dbus_calls_t, routerif->pending_call_list, p);
                dbus_pending_call_set_notify(p->call, NULL, NULL, NULL);
                dbus_pending_call_unref(p->call);
            }

            if ( u ) {
                dbus_connection_remove_filter(dbusconn, router_dbusif_method_handler, u);
            }

            dbus_bus_remove_match(dbusconn, routerif->am_watch_rule, NULL);

            pa_dbus_connection_unref(routerif->conn);
        }
        pa_xfree(routerif);
    }
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief This function allows to break the synchronous calls to asyn ones. Since this module runs in\
 * the context of the pulseaudio main loop better not to block for more time so even synchronous calls are
 * made async.
 * @param pend: The pending dbus calls.
 *        data:  The pointer of the data, depends on the dbus call.
 * @return int
 */
static void reply_cb(DBusPendingCall *pend, void *data) {
    pending_dbus_calls_t *pdata = (pending_dbus_calls_t*) data;
    struct userdata *u;
    router_dbusif *routerif;
    DBusMessage *reply;
    ROUTER_FUNCTION_ENTRY;
    pa_assert(pdata);
    pa_assert(pdata->call == pend);
    pa_assert_se((u = pdata->u));
    pa_assert_se((routerif = u->dbusif));

    PA_LLIST_REMOVE(pending_dbus_calls_t, routerif->pending_call_list, pdata);

    if ( (reply = dbus_pending_call_steal_reply(pend)) == NULL ) {
        pa_log("%s: pending call failed: invalid argument",
        __FILE__);
    } else {
        pdata->cb(u, reply, pdata->data);
        dbus_message_unref(reply);
    }
    pa_xfree((void *) pdata);
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief This function sends the syncronous request.
 * @param u: The user data of the module.
 *        msg: The dbus message.
 *        cb: The callback function to be called when reply is received
 *        data: The user data to be passed in callback.
 * @return int
 */
static bool send_message_with_reply(struct userdata *u, DBusMessage *msg, pending_cb_t cb, void *data) {
    router_dbusif *routerif;
    pending_dbus_calls_t* pdata = NULL;
    const char *method;
    DBusPendingCall *pend;
    DBusConnection *dbusconn;
    ROUTER_FUNCTION_ENTRY;

    pa_assert(u);
    pa_assert(msg);
    pa_assert(cb);
    pa_assert_se((routerif = u->dbusif));

    pdata = pa_xnew0(pending_dbus_calls_t, 1);
    pdata->u = u;
    pdata->cb = cb;
    pdata->data = data;

    dbusconn = pa_dbus_connection_get(routerif->conn);

    PA_LLIST_PREPEND(pending_dbus_calls_t, routerif->pending_call_list, pdata);
    if ( !dbus_connection_send_with_reply(dbusconn, msg, &pend, -1) ) {
        pa_log("%s: Failed to %s", __FILE__, method);
        goto failed;
    }

    pdata->call = pend;

    if ( !dbus_pending_call_set_notify(pend, reply_cb, pdata, NULL) ) {
        pa_log("%s: Can't set notification for %s", __FILE__, method);
        goto failed;
    }

    ROUTER_FUNCTION_EXIT;
    return true;

    failed: if ( pdata ) {
        PA_LLIST_REMOVE(pending_dbus_calls_t, routerif->pending_call_list, pdata);
        pa_xfree((void *) pdata);
    }
    return false;
}

/**
 * @brief The callback function for the command side connect reply.
 * @param u: The user data of the module.
 *        reply: The dbus reply message.
 *        data: The pointer to the data related to the request.
 * @return void
 */
static void router_dbusif_connect_reply_cb(struct userdata* u, DBusMessage * reply, void * data) {

    router_dbusif* dbusif;
    pa_assert(u);
    am_main_connection_t* mainconnect_data = (am_main_connection_t*) data;
    dbusif = u->dbusif;
    dbus_bool_t success = FALSE;
    uint16_t status;
    ROUTER_FUNCTION_ENTRY;
    // parse the message and get status and the connection id
    if ( dbus_message_get_type(reply) != DBUS_MESSAGE_TYPE_ERROR ) {
#ifdef GENIVI_DBUS_PLUGIN
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_INT16, &status, DBUS_TYPE_UINT16,
                &(mainconnect_data->connection_id), DBUS_TYPE_INVALID);
#else
        success = dbus_message_get_args(reply, NULL,
                DBUS_TYPE_UINT16, &(mainconnect_data->connection_id),
                DBUS_TYPE_UINT16, &status,
                DBUS_TYPE_INVALID);
#endif
        if ( !success ) {
            pa_log_error("got broken message from command AudioManager connect failed");
        } else {
            if ( dbusif->cb_command_connect_reply ) {
                dbusif->cb_command_connect_reply(u, (int) status, data);
            }

        }
    }
    MODULE_ROUTER_FREE(data);
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function for the command side disconnect reply.
 * @param u: The user data of the module.
 *        reply: The dbus reply message.
 *        data: The pointer to the data related to the request.
 * @return void
 */
static void router_dbusif_disconnect_reply_cb(struct userdata *u, DBusMessage *reply, void *data) {
    pa_assert(u);
    const char *error_descr;
    dbus_uint16_t status;
    dbus_bool_t success = FALSE;
    ROUTER_FUNCTION_ENTRY;
    if ( dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR ) {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &error_descr, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            error_descr = dbus_message_get_error_name(reply);
        }

        pa_log_error("%s: domain registration failed: '%s'", __FILE__, error_descr);
    } else {
#ifdef GENIVI_DBUS_PLUGIN
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_INT16, &status, DBUS_TYPE_INVALID);
#else
        success = dbus_message_get_args(reply, NULL,
                DBUS_TYPE_UINT16, &status,
                DBUS_TYPE_INVALID);
#endif
        if ( success == FALSE ) {
            pa_log_error("parsing of out parameters failed for command side disconnect reply");
        } else {
            pa_log_info("audiomanger replied to disconnect request: status %u", status);
            if ( u->dbusif->cb_command_disconnect_reply ) {
                u->dbusif->cb_command_disconnect_reply(u, status, data);
            }
        }
    }
    MODULE_ROUTER_FREE(data);
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function for the routing side register domain reply.
 * @param u: The user data of the module.
 *        reply: The dbus reply message.
 *        data: The pointer to the data related to the request.
 * @return void
 */
static void router_dbusif_register_domain_reply_cb(struct userdata *u, DBusMessage *reply, void *data) {
    const char *error_descr;
    dbus_uint16_t domain_id;
    dbus_uint16_t status;
    dbus_bool_t success = FALSE;
    ROUTER_FUNCTION_ENTRY;
    if ( dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR ) {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &error_descr, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            error_descr = dbus_message_get_error_name(reply);
        }

        pa_log_error("%s: domain registration failed: '%s'", __FILE__, error_descr);
    } else {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_UINT16, &domain_id, DBUS_TYPE_UINT16, &status,
                DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            pa_log_error("parsing of out parameters failed for domain registration");
        } else {
            pa_log_info("audiomanger replied to domain registration: domainID: %u, status %u", domain_id, status);
            ((am_domain_register_t*) data)->domain_id = domain_id;
            if ( u->dbusif->cb_routing_register_domain_reply ) {
                u->dbusif->cb_routing_register_domain_reply(u, status, data);
            }
        }
    }
    MODULE_ROUTER_FREE(data);
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function for the routing side deregister domain reply.
 * @param u: The user data of the module.
 *        reply: The dbus reply message.
 *        data: The pointer to the data related to the request.
 * @return void
 */
static void router_dbusif_deregister_domain_reply_cb(struct userdata *u, DBusMessage *reply, void *data) {
    const char *error_descr;
    dbus_uint16_t status;
    dbus_bool_t success = FALSE;
    ROUTER_FUNCTION_ENTRY;
    if ( dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR ) {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &error_descr, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            error_descr = dbus_message_get_error_name(reply);
        }

        pa_log_error("%s: domain registration failed: '%s'", __FILE__, error_descr);
    } else {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_UINT16, &status, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            pa_log_error("parsing of out parameters failed for domain registration");
        } else {
            pa_log_info("audiomanger replied to domain deregistration: domainID: %u, status %u",
                    ((am_domain_unregister_t*) data)->domain_id, status);
            if ( u->dbusif->cb_routing_deregister_domain_reply ) {
                u->dbusif->cb_routing_deregister_domain_reply(u, status, data);
            }
        }
    }
    MODULE_ROUTER_FREE(data);
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function for the routing side register sink reply.
 * @param u: The user data of the module.
 *        reply: The dbus reply message.
 *        data: The pointer to the data related to the request.
 * @return void
 */
static void router_dbusif_register_sink_reply_cb(struct userdata *u, DBusMessage *reply, void *data) {
    const char *error_descr;
    dbus_uint16_t sink_id;
    dbus_uint16_t status;
    dbus_bool_t success = FALSE;
    ROUTER_FUNCTION_ENTRY;
    if ( dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR ) {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &error_descr, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            error_descr = dbus_message_get_error_name(reply);
        }

        pa_log_error("%s: sink registration failed: '%s'", __FILE__, error_descr);
    } else {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_UINT16, &sink_id, DBUS_TYPE_UINT16, &status,
                DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            pa_log_error("parsing of out parameters failed for sink registration");
        } else {
            pa_log_info("audiomanger replied to sink registration: sinkID: %u, status %u", sink_id, status);
            ((am_sink_register_t*) data)->sink_id = sink_id;
            if ( u->dbusif->cb_routing_register_sink_reply ) {
                u->dbusif->cb_routing_register_sink_reply(u, status, data);
            }
        }
    }
    MODULE_ROUTER_FREE(data);
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function for the routing side unregister sink reply.
 * @param u: The user data of the module.
 *        reply: The dbus reply message.
 *        data: The pointer to the data related to the request.
 * @return void
 */
static void router_dbusif_deregister_sink_reply_cb(struct userdata *u, DBusMessage *reply, void *data) {
    const char *error_descr;
    dbus_uint16_t status;
    dbus_bool_t success = FALSE;
    ROUTER_FUNCTION_ENTRY;
    if ( dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR ) {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &error_descr, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            error_descr = dbus_message_get_error_name(reply);
        }

        pa_log_error("%s: sink deregistration failed: '%s'", __FILE__, error_descr);
    } else {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_UINT16, &status, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            pa_log_error("parsing of out parameters failed for sink registration");
        } else {
            pa_log_info("audiomanger replied to sink deregistration: sinkID: %u, status %u",
                    ((am_sink_unregister_t*) data)->sink_id, status);
            if ( u->dbusif->cb_routing_deregister_sink_reply ) {
                u->dbusif->cb_routing_deregister_sink_reply(u, status, data);
            }
        }
    }
    MODULE_ROUTER_FREE(data);
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function for the routing side register source reply.
 * @param u: The user data of the module.
 *        reply: The dbus reply message.
 *        data: The pointer to the data related to the request.
 * @return void
 */
static void router_dbusif_register_source_reply_cb(struct userdata *u, DBusMessage *reply, void *data) {
    const char *error_descr;
    dbus_uint16_t source_id;
    dbus_uint16_t status;
    dbus_bool_t success = FALSE;
    ROUTER_FUNCTION_ENTRY;
    if ( dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR ) {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &error_descr, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            error_descr = dbus_message_get_error_name(reply);
        }

        pa_log_error("%s: source registration failed: '%s'", __FILE__, error_descr);
    } else {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_UINT16, &source_id, DBUS_TYPE_UINT16, &status,
                DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            pa_log_error("parsing of out parameters failed for source registration");
        } else {
            pa_log_info("audiomanger replied to source registration: sourceID: %u, status %u", source_id, status);
            ((am_source_register_t*) data)->source_id = source_id;
            if ( u->dbusif->cb_routing_register_source_reply ) {
                u->dbusif->cb_routing_register_source_reply(u, status, data);
            }
        }
    }
    MODULE_ROUTER_FREE(data);
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function for the routing side source peek reply
 * @param u: The user data of the module.
 *        reply: The dbus reply message.
 *        data: The pointer to the data related to the request.
 * @return void
 */
static void router_dbusif_peek_source_reply_cb(struct userdata *u, DBusMessage *reply, void *data) {
    const char *error_descr;
    dbus_uint16_t source_id;
    dbus_uint16_t status;
    dbus_bool_t success = FALSE;
    ROUTER_FUNCTION_ENTRY;
    if ( dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR ) {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &error_descr, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            error_descr = dbus_message_get_error_name(reply);
        }

        pa_log_error("%s: peek source failed: '%s'", __FILE__, error_descr);
    } else {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_UINT16, &source_id, DBUS_TYPE_UINT16, &status,
                DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            pa_log_error("parsing of out parameters failed for source peek");
        } else {
            pa_log_info("audiomanger replied to source peek: sourceID: %u, status %u", source_id, status);
            ((am_source_register_t*) data)->source_id = source_id;
            if ( u->dbusif->cb_routing_peek_source_reply ) {
                u->dbusif->cb_routing_peek_source_reply(u, status, data);
            }
        }
    }
    //Not allocated dynamically
    //MODULE_ROUTER_FREE(data);
    ROUTER_FUNCTION_EXIT;

}

/**
 * @brief The callback function for the routing side peek sink reply.
 * @param u: The user data of the module.
 *        reply: The dbus reply message.
 *        data: The pointer to the data related to the request.
 * @return void
 */

static void router_dbusif_peek_sink_reply_cb(struct userdata *u, DBusMessage *reply, void *data) {
    const char *error_descr;
    dbus_uint16_t sink_id;
    dbus_uint16_t status;
    dbus_bool_t success = FALSE;
    ROUTER_FUNCTION_ENTRY;
    if ( dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR ) {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &error_descr, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            error_descr = dbus_message_get_error_name(reply);
        }

        pa_log_error("%s: peek sink failed: '%s'", __FILE__, error_descr);
    } else {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_UINT16, &sink_id, DBUS_TYPE_UINT16, &status,
                DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            pa_log_error("parsing of out parameters failed for sink registration");
        } else {
            pa_log_info("audiomanger replied to source registration: sinkID: %u, status %u", sink_id, status);
            ((am_sink_register_t*) data)->sink_id = sink_id;
            if ( u->dbusif->cb_routing_register_sink_reply ) {
                u->dbusif->cb_routing_register_sink_reply(u, status, data);
            }
        }
    }
    //Not allocated dynamically
    //MODULE_ROUTER_FREE(data);
    ROUTER_FUNCTION_EXIT;

}

/**
 * @brief The callback function for the routing side get domain of source reply.
 * @param u: The user data of the module.
 *        reply: The dbus reply message.
 *        data: The pointer to the data related to the request.
 * @return void
 */
static void router_dbusif_get_domain_of_source_reply_cb(struct userdata * u, DBusMessage *reply, void * data) {
    const char *error_descr;
    dbus_uint16_t domain_id;
    dbus_uint16_t status;
    dbus_bool_t success = FALSE;
    ROUTER_FUNCTION_ENTRY;
    if ( dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR ) {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &error_descr, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            error_descr = dbus_message_get_error_name(reply);
        }

        pa_log_error("%s: get domaion of source failed: '%s'", __FILE__, error_descr);
    } else {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_UINT16, &domain_id, DBUS_TYPE_UINT16, &status,
                DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            pa_log_error("parsing of out parameters failed for source peek");
        } else {
            pa_log_info("audiomanger replied to getDomainOfSource peek: domain_id: %u, status %u", domain_id, status);
            ((am_domain_of_source_sink_t*) data)->domain_id = domain_id;
            if ( u->dbusif->cb_routing_get_domain_of_source_reply ) {
                u->dbusif->cb_routing_get_domain_of_source_reply(u, status, data);
            }
        }
    }
    //Not allocated dynamically
    //MODULE_ROUTER_FREE(data);
    ROUTER_FUNCTION_EXIT;

}

/**
 * @brief The callback function for the routing side get domain of sink reply.
 * @param u: The user data of the module.
 *        reply: The dbus reply message.
 *        data: The pointer to the data related to the request.
 * @return void
 */
static void router_dbusif_get_domain_of_sink_reply_cb(struct userdata *u, DBusMessage *reply, void *data) {
    const char *error_descr;
    dbus_uint16_t domain_id;
    dbus_uint16_t status;
    dbus_bool_t success = FALSE;
    ROUTER_FUNCTION_ENTRY;
    if ( dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR ) {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &error_descr, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            error_descr = dbus_message_get_error_name(reply);
        }

        pa_log_error("%s: get domaion of source failed: '%s'", __FILE__, error_descr);
    } else {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_UINT16, &domain_id, DBUS_TYPE_UINT16, &status,
                DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            pa_log_error("parsing of out parameters failed for source peek");
        } else {
            pa_log_info("audiomanger replied to getDomainOfSink : domain_id: %u, status %u", domain_id, status);
            ((am_domain_of_source_sink_t*) data)->domain_id = domain_id;
            if ( u->dbusif->cb_routing_get_domain_of_sink_reply ) {
                u->dbusif->cb_routing_get_domain_of_sink_reply(u, status, data);
            }
        }
    }
    //Not allocated dynamically
    //MODULE_ROUTER_FREE(data);
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function for the routing side deregister source reply.
 * @param u: The user data of the module.
 *        reply: The dbus reply message.
 *        data: The pointer to the data related to the request.
 * @return void
 */
static void router_dbusif_deregister_source_reply_cb(struct userdata *u, DBusMessage *reply, void *data) {
    const char *error_descr;
    dbus_uint16_t status;
    dbus_bool_t success = FALSE;
    ROUTER_FUNCTION_ENTRY;
    if ( dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR ) {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &error_descr, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            error_descr = dbus_message_get_error_name(reply);
        }

        pa_log_error("%s: source deregistration failed: '%s'", __FILE__, error_descr);
    } else {
        success = dbus_message_get_args(reply, NULL, DBUS_TYPE_UINT16, &status, DBUS_TYPE_INVALID);
        if ( success == FALSE ) {
            pa_log_error("parsing of out parameters failed for source registration");
        } else {
            pa_log_info("audiomanger replied to source deregistration: sourceID: %u, status %u",
                    ((am_source_unregister_t*) data)->source_id, status);
            if ( u->dbusif->cb_routing_deregister_source_reply ) {
                u->dbusif->cb_routing_deregister_source_reply(u, status, data);
            }
        }
    }
    MODULE_ROUTER_FREE(data);
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The internal function to send the ack for async requests
 * @param u: The user data of the module.
 *        method_name: The name of the async request.
 *        handle: the request identifier.
 *        param1: The parameter for ack
 *        param2: The parameter for ack
 *        error: The error status for the ack
 * @return bool true on success.
 */
static bool send_ack(struct userdata *u, char *method_name, uint16_t handle, uint16_t *param1, int16_t *param2,
        uint16_t error) {
    DBusConnection *conn = NULL;
    DBusMessage *msg = NULL;
    bool status = true;
    dbus_bool_t success = FALSE;
    ROUTER_FUNCTION_ENTRY;
    pa_assert(u);
    pa_assert(method_name);
    pa_assert(u->dbusif);
    pa_assert(u->dbusif->am_routing_dbus_name);
    pa_assert(u->dbusif->am_routing_dbus_path);
    pa_assert(u->dbusif->am_routing_dbus_interface_name);
    conn = pa_dbus_connection_get(u->dbusif->conn);
    pa_assert(conn);

    msg = dbus_message_new_method_call(u->dbusif->am_routing_dbus_name, u->dbusif->am_routing_dbus_path,
            u->dbusif->am_routing_dbus_interface_name, method_name);
    do {
        if ( !msg ) {
            pa_log_error("%s: failed to create the D-Bus message for '%s'", __FILE__, method_name);
            break;
        }

        if ( param1 ) {
            success = dbus_message_append_args(msg, DBUS_TYPE_UINT16, &handle, DBUS_TYPE_UINT16, param1,
                    DBUS_TYPE_UINT16, &error, DBUS_TYPE_INVALID);
        } else if ( param2 ) {
            success = dbus_message_append_args(msg, DBUS_TYPE_UINT16, &handle, DBUS_TYPE_INT16, param2,
                    DBUS_TYPE_UINT16, &error, DBUS_TYPE_INVALID);
        } else {
            success = dbus_message_append_args(msg, DBUS_TYPE_UINT16, &handle, DBUS_TYPE_UINT16, &error,
                    DBUS_TYPE_INVALID);
        }

        if ( success == FALSE ) {
            pa_log_error("%s: failed to append args of DBus message '%s'", __FILE__, method_name);
            status = false;
            break;
        }

        success = dbus_connection_send(conn, msg, NULL);
        if ( success == FALSE ) {
            pa_log_error("%s: failed to send the D-Bus message '%s'", __FILE__, method_name);
            status = false;
            break;
        }
    } while ( 0 );

    if ( msg )
        dbus_message_unref(msg);
    ROUTER_FUNCTION_EXIT;

    return status;
}

/**
 * @brief The ack for async connect.
 * @param u: The user data of the module.
 *        handle: The identifier for the request.
 *        connection_id: The connection id
 *        error: The error status of the async request
 * @return void
 */
void router_dbusif_ack_connect(struct userdata *u, uint16_t handle, uint16_t connection_id, uint16_t error) {
    ROUTER_FUNCTION_ENTRY;
    send_ack(u, "ackConnect", handle, &connection_id, NULL, error);
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The ack for async disconnect.
 * @param u: The user data of the module.
 *        handle: The identifier for the request.
 *        connection_id: The connection id
 *        error: The error status of the async request
 * @return void
 */
void router_dbusif_ack_disconnect(struct userdata *u, uint16_t handle, uint16_t connection_id, uint16_t error) {
    ROUTER_FUNCTION_ENTRY;
    send_ack(u, "ackDisconnect", handle, &connection_id, NULL, error);
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The ack for async set sink volume.
 * @param u: The user data of the module.
 *        handle: The identifier for the request.
 *        volume: The new volume
 *        error: The error status of the async request
 * @return void
 */
void router_dbus_ack_set_sink_volume(struct userdata *u, uint16_t handle, uint16_t volume, uint16_t error) {
    ROUTER_FUNCTION_ENTRY;
#ifdef GENIVI_DBUS_PLUGIN
    send_ack(u, "ackSetSinkVolume", handle, NULL, &volume, error);
#else
    send_ack(u, "ackSetSinkVolumeChange", handle, NULL, &volume, error);
#endif
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The ack for async set source volume.
 * @param u: The user data of the module.
 *        handle: The identifier for the request.
 *        volume: The new volume value
 *        error: The error status of the async request
 * @return void
 */
void router_dbus_ack_set_source_volume(struct userdata *u, uint16_t handle, uint16_t volume, uint16_t error) {
    ROUTER_FUNCTION_ENTRY;
#ifdef GENIVI_DBUS_PLUGIN
    send_ack(u, "ackSetSourceVolume", handle, NULL, &volume, error);
#else
    send_ack(u, "ackSetSourceVolumeChange", handle, NULL, &volume, error);
#endif
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The ack for async set soruce state.
 * @param u: The user data of the module.
 *        handle: The identifier for the request.
 *        error: The error status of the async request
 * @return void
 */
void router_dbus_ack_set_source_state(struct userdata *u, uint16_t handle, uint16_t error) {
    ROUTER_FUNCTION_ENTRY;
    send_ack(u, "ackSetSourceState", handle, NULL, NULL, error);
    ROUTER_FUNCTION_EXIT;
}
