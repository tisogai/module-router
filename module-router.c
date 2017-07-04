/******************************************************************************
 * @file: module-router.c
 *
 * The file contains the implementation of the hooks and the dbus-callbacks
 * for pulseaduio router module.
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
#include <pulsecore/core.h>
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/sink.h>
#include <pulsecore/sink-input.h>
#include <pulse/version.h>
#include <router-userdata.h>
#include <router-dbusif.h>

#define GENIVI_DBUS_PLUGIN       1
#define MODULE_ROUTER_EXTRA_LOGS 0

#if GENIVI_DBUS_PLUGIN
#define PULSE_ROUTER_DBUS_RETURN_INTERFACE_NAME "org.genivi.audiomanager.routinginterface"
#else
#define PULSE_ROUTER_DBUS_RETURN_INTERFACE_NAME "pulseaudio"
#endif
#define PULSE_ROUTER_DBUS_NAME "org.genivi.audiomanager.routing.pulseaudio"
#define PULSE_ROUTER_INTERFACE_NAME  "org.genivi.audiomanager.routing.pulseaudio"
#define PULSE_ROUTER_DBUS_PATH "/org/genivi/audiomanager/routing/pulseaudio"
#define AM_COMMAND_DBUS_NAME "org.genivi.audiomanager"
#define AM_COMMAND_DBUS_INTERFACE_NAME "org.genivi.audiomanager.commandinterface"
#define AM_COMMAND_DBUS_PATH "/org/genivi/audiomanager/commandinterface"
#define AM_ROUTING_DBUS_NAME "org.genivi.audiomanager"
#define AM_ROUTING_DBUS_INTERFACE_NAME "org.genivi.audiomanager.routinginterface"
#define AM_ROUTING_DBUS_PATH "/org/genivi/audiomanager/routinginterface"
#define AM_COMMAND_SIGNAL_WATCH_RULE "type='signal',interface='org.genivi.audiomanager.commandinterface'"

#define DS_UNKNOWN    0
#define DS_CONTROLLED 1

#define SS_ON  1
#define SS_OFF 2
#define SS_PAUSED 3

#define A_AVAILABLE   1
#define A_UNAVAILABLE 2

PA_MODULE_AUTHOR("Advanced Driver Information Technology");
PA_MODULE_DESCRIPTION("PulseAudio router plug-in");
PA_MODULE_VERSION( PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE( true);
PA_MODULE_USAGE("");

struct router_hooks {
    pa_hook_slot *hook_slot_sink_input_put;
    pa_hook_slot *hook_slot_sink_input_unlink;
    pa_hook_slot *hook_slot_source_output_put;
    pa_hook_slot *hook_slot_source_output_unlink;
    pa_hook_slot *hook_slot_sink_new;
    pa_hook_slot *hook_slot_sink_input_new;
    pa_hook_slot *hook_slot_source_new;
    pa_hook_slot *hook_slot_source_output_new;
};

/**
 * @brief This function is registered with the dbus interface module, it gets called when command side
 * Notification cbNewMainConnection is received.
 * @param u: The pointer to the userdata structure
 *        connection: The pointer to the main connection data structure.
 * @return void
 */
static void cb_new_main_connection(struct userdata *u, am_main_connection_t *connection) {
    ROUTER_FUNCTION_ENTRY;
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief This function is registered with the dbus interface module, it gets called when command side
 * Notification cbRemovedMainConnection is received.
 * @param u: The pointer to the userdata structure
 *        id: The id of the main connection which is removed.
 * @return void
 */
static void cb_removed_main_connection(struct userdata *u, uint16_t id) {
    ROUTER_FUNCTION_ENTRY;
    pa_log_debug("Main Connection removed ID = %d", id);
    pa_hashmap_remove(u->main_connection_map, (void*) (intptr_t) id);
    ROUTER_FUNCTION_EXIT;

}

/**
 * @brief This function is registered with the dbus interface module, it gets called when command side
 * Notification cbMainConnectionStateChanged is received.
 * @param u: The pointer to the userdata structure
 *        id: The main connection id
 *        state: The main connection state
 * @return void
 */
static void cb_main_connection_state_changed(struct userdata *u, uint16_t id, int32_t state) {
    ROUTER_FUNCTION_ENTRY;
    pa_log_debug("Main Connection state changed  ID = %d state = %d", id, state);
    am_main_connection_t* main_connection = pa_hashmap_get(u->main_connection_map, (void*) (intptr_t) id);
    if ( main_connection != NULL ) {
        main_connection->state = state;
    }
    ROUTER_FUNCTION_EXIT;

}

#if MODULE_ROUTER_EXTRA_LOGS
/**
 * @brief This function prints all the data present in the router module. It is used only for the debug purpose.
 * @param u: The pointer to the userdata structure, which has all the data related to the module.
 * @return void
 */
static void print_maps(struct userdata* u)
{
    int16_t index;
    pa_log_debug("print_maps: SinkMaps");
    for(index = 0;index < 10;index++)
    {
        pa_log_debug("sink ID=%d, builtin=%d, source_state=%d, ptr=%p, name=%s",
                u->sink_map[index].id,u->sink_map[index].builtin,u->sink_map[index].source_state,
                u->sink_map[index].data,u->sink_map[index].name);
    }
    pa_log_debug("print_maps: SourceMaps");
    for(index = 0;index < 10;index++)
    {
        pa_log_debug("Source ID=%d, builtin=%d, source_state=%d, ptr=%p, name=%s",
                u->source_map[index].id,u->source_map[index].builtin,u->source_map[index].source_state,
                u->source_map[index].data,u->source_map[index].name);
    }
    pa_log_debug("print_maps: ConenctionMaps");
    am_connect_t *c;
    void *s;
    PA_HASHMAP_FOREACH(c, u->connection_map, s)
    {
        pa_log_debug("Connection ID=%d, sourceID = %d, sinkID=%d",c->connection_id, c->source_id,c->sink_id);
    }
    pa_log_debug("print_maps: MainConenctionMaps");
    am_main_connection_t *mainConnection;
    PA_HASHMAP_FOREACH(mainConnection, u->main_connection_map, s)
    {
        pa_log_debug("Connection ID=%d, sourceID = %d, sinkID=%d state=%d",mainConnection->connection_id, mainConnection->source_id,mainConnection->sink_id,mainConnection->state);
    }

}
#endif

/**
 * @brief This function gets the free index in the map. The map can be source or sink map.
 * @param map: The pointer to the map
 * @return int16_t: The index of the free entry in the map.
 */
static int16_t get_free_map_index(name_id_map *map) {
    int16_t index = -1;
    int16_t i = 0;
    for ( int i = 0 ; i < AM_MAX_SOURCE_SINK ; i++ ) {
        if ( map[i].name[0] == '\0' ) {
            index = i;
            break;
        }
    }
    pa_log_debug("get_free_map_index index=%d", index);
    return index;
}

/**
 * @brief This function returns the index of the map entry from the id.
 * @param id: The id for which the map index is needed
 *        map: pointer to the map either source/sink map
 * @return int16_t: The index of the map.
 */
static int16_t get_map_index_from_id(const uint16_t id, const name_id_map *map) {
    int16_t index = -1;
    int16_t i = 0;
    for ( int i = 0 ; i < AM_MAX_SOURCE_SINK ; i++ ) {
        if ( map[i].id == id ) {
            index = i;
            break;
        }
    }
    pa_log_debug("get_map_index_from_id id=%d, index = %d", id, index);
    return index;
}

/**
 * @brief This function returns the map index which matches the name
 * @param name: The pointer to the name for which map index is needed.
 *        map: pointer to the map either source/sink map
 * @return int16_t: The index of the map.
 */
static int16_t get_map_index_from_name(const char* name, const name_id_map *map) {
    int16_t index = -1;
    int16_t i = 0;
    for ( int i = 0 ; i < AM_MAX_SOURCE_SINK ; i++ ) {
        if ( strcmp(map[i].name, name) == 0 ) {
            index = i;
            break;
        }
    }
    pa_log_debug("get_map_index_from_name name=%s, index = %d", name, index);
    return index;
}

/**
 * @brief This function returns the connection from the source id
 * @param u: The pointer to the userdata.
 *        source_id: The source id of which the connection is to be searched.
 * @return am_connect_t: The pointer to the am_connect structure.
 */
static am_connect_t* get_connection_from_source(struct userdata *u, uint16_t source_id) {
    am_connect_t *c = NULL;
    am_connect_t *ret_val = NULL;
    void *s;
    PA_HASHMAP_FOREACH(c, u->connection_map, s)
    {
        if ( c->source_id == source_id ) {
            ret_val = c;
            break;
        }
    }
    pa_log_debug("get_connection_from_source() sourceID = %d", source_id);
    return ret_val;
}

/**
 * @brief This function returns the connection from the source and sink.
 * @param u: The pointer to the userdata.
 *        source_id: The source id of which the connection is to be searched.
 *        sink_id: The sink id of which connection is to be searched
 * @return am_connect_t: The pointer to the am_connect structure.
 */
static am_connect_t* get_connection_from_src_sink(struct userdata *u, uint16_t source_id, uint16_t sink_id) {
    am_connect_t *c = NULL;
    am_connect_t *ret_val = NULL;
    void *s;
    PA_HASHMAP_FOREACH(c, u->connection_map, s)
    {
        if ( (c->source_id == source_id) && (c->sink_id == sink_id) ) {
            ret_val = c;
            break;
        }
    }
    pa_log_debug("get_connection_from_src_sink ()sourceID = %d sinkID=%d", source_id, sink_id);
    return ret_val;
}

/**
 * @brief This function returns the id from the sink_input pointer
 * @param sink_input: The pointer to the sink input.
 *        map: The pointer to the sink input map
 * @return uint16_t: The id of the source
 */
static uint16_t get_id_from_sink_input(pa_sink_input* sink_input, const name_id_map* map) {
    uint16_t id = 0;
    int16_t i = 0;
    for ( int i = 0 ; i < AM_MAX_SOURCE_SINK ; i++ ) {
        if ( map[i].data == sink_input ) {
            id = map[i].id;
            break;
        }
    }
    pa_log_debug("get_id_from_sink_input id=%d", id);
    return id;
}

/**
 * @brief This function removes the entry of source/sink from the respective map.
 * @param id: The id of source/sink which needs to be removed.
 *        map: The pointer to the source/sink map.
 * @return None
 */
static void remove_pa_pointer(uint16_t id, name_id_map* map) {
    int16_t i = 0;
    for ( int i = 0 ; i < AM_MAX_SOURCE_SINK ; i++ ) {
        if ( map[i].id == id ) {
            map[i].data = NULL;
            break;
        }
    }
    return;
}

/**
 * @brief This function returns the pulseaudio source/sink pointer from the audiomanager ID.
 * @param id: The id of source/sink.
 *        map: The pointer to the source/sink map.
 * @return void*: The pointer to the pulseaudio source/sinks.
 */
static void* get_pa_pointer_from_id(uint16_t id, const name_id_map* map) {
    int16_t i = 0;
    void* pa_ptr = NULL;
    for ( int i = 0 ; i < AM_MAX_SOURCE_SINK ; i++ ) {
        if ( map[i].id == id ) {
            pa_ptr = map[i].data;
            break;
        }
    }
    return pa_ptr;
}

/**
 * @brief This function returns the map index from the audio manager name.
 * @param: name: The audio manager name.
 *         map: The pointer to the source/sink map.
 * @return int16_t: The index of the map for which the name matches.
 */
static int16_t am_name_to_map_index(const char *name, const name_id_map *map) {
    int16_t index = -1;
    int16_t i = 0;
    for ( int i = 0 ; i < AM_MAX_SOURCE_SINK ; i++ ) {
        if ( strcmp(name, map[i].name) == 0 ) {
            index = i;
            break;
        }
    }
    return index;
}

/**
 * @brief This function returns the audiomanager ID from name.
 * @param: name: The audio manager name.
 *         map: The pointer to the source/sink map.
 * @return int16_t: The id for which the name matches in the map.
 */
static uint16_t am_name_to_id(const char *name, const name_id_map *map) {
    uint16_t id = 0;
    if ( name != NULL ) {
        for ( int i = 0 ; i < AM_MAX_SOURCE_SINK ; i++ ) {
            if ( strcmp(map[i].name, name) == 0 ) {
                id = map[i].id;
                break;
            }
        }
    }
    pa_log_debug("am_name_to_id name:%s id:%d", name, id);
    return id;
}

/**
 * @brief This function returns the audiomanager ID from name.
 * @param: name: The audio manager name.
 *         map: The pointer to the source/sink map.
 * @return int16_t: The id for which the name matches in the map.
 */
static char* id_to_am_name(const uint16_t id, name_id_map *map) {
    char *name = NULL;
    for ( int i = 0 ; i < AM_MAX_SOURCE_SINK ; i++ ) {
        if ( map[i].id == id ) {
            name = map[i].name;
            break;
        }
    }
    return name;
}

/**
 * @brief This function returns the pulseaudio property description from a given id.
 * @param: id: The audio manager id.
 *         map: The pointer to the source/sink map.
 * @return char*: The description of the source/sink.
 */
static char* id_to_pulse_description(const uint16_t id, const name_id_map *map) {
    char* description = NULL;
    for ( const name_id_map *m = map ; m->name[0] != '\0' ; m++ ) {
        if ( m->id == id ) {
            description = (char*) m->description;
            break;
        }
    }
    return description;
}

/**
 * @brief This function returns the audiomanager ID from pulseaudio description
 * @param: description: The pulseaudio description.
 *         map: The pointer to the source/sink map.
 * @return uint16_t: The id from the pulseaudio description.
 */
static uint16_t pulse_description_to_am_id(const char *description, const name_id_map *map) {
    uint16_t id = 0;
    if ( description ) {
        for ( const name_id_map *m = map ; m->name[0] != '\0' ; m++ ) {
            if ( (m->description) && (strcmp(m->description, description) == 0) ) {
                id = m->id;
                break;
            }
        }
    }
    return id;
}

/**
 * @brief This function checks if the source/sink is built-in i.e. not application source/sink.
 * @param: id: The id of the source/sink
 *         map: The pointer to the source/sink map.
 * @return bool: true if source/sink is built-in and vice versa.
 */
static bool is_source_sink_builtin(const uint16_t id, const name_id_map *map) {
    bool builtin = false;
    if ( id ) {
        for ( const name_id_map *m = map ; m->name[0] != '\0' ; m++ ) {
            if ( m->id == id ) {
                builtin = m->builtin;
                break;
            }
        }
    }
    return builtin;
}

/**
 * @brief This function returns the sink input pointer from the audiomanager id.
 * @param: u: The user data pointer.
 *         sink_id: The sink id.
 * @return pa_sink_input*: The pointer to the pulse audio sink input.
 */
static pa_sink_input* am_id_to_loopbacked_sink_input(struct userdata*u, uint16_t sink_id) {
    pa_sink_input* sink_input = NULL;
    pa_sink_input* return_sink_input = NULL;
    char* sink_name = id_to_am_name(sink_id, u->sink_map);
    uint32_t index;
    PA_IDXSET_FOREACH(sink_input, u->core->sink_inputs, index)
    {
        if ( !strcmp(sink_name, sink_input->sink->name) ) {
            return_sink_input = sink_input;
            break;
        }
    }
    return return_sink_input;
}

/**
 * @brief This function returns the sink input pointer from the audiomanager id.
 * @param: u: The user data pointer.
 *         sink_id: The sink id.
 * @return pa_sink_input*: The pointer to the pulse audio sink input.
 */
static pa_source* am_name_to_pa_source(struct userdata*u, char* source_name) {
    pa_source* source = NULL;
    pa_source* output_source = NULL;
    uint32_t index;
    PA_IDXSET_FOREACH(source, u->core->sources, index)
    {
        if ( !strcmp(source_name, source->name) ) {
            output_source = source;
            break;
        }
    }
    return output_source;
}

/**
 * @brief This function returns the source pointer from the pulseaudio description of the sink.
 * @param: u: The user data pointer.
 *         description: The pulseaudio description of the sink.
 * @return pa_source*: The pointer to the pulse audio source.
 */
static pa_source* pulse_description_to_loopbacked_source(const struct userdata* u, const char *description) {
    pa_source* pulse_source = NULL;
    pa_source* iterator;
    uint32_t index;
    if ( description ) {
        PA_IDXSET_FOREACH(iterator, u->core->sources, index)
        {
            if ( !strcmp(description, pa_proplist_gets(iterator->proplist, PA_PROP_DEVICE_DESCRIPTION)) ) {
                pulse_source = iterator;
                break;
            }
        }
    }
    return pulse_source;
}

/**
 * @brief This function returns the sink pointer from the pulseaudio description of the source.
 * @param: u: The user data pointer.
 *         description: The pulseaudio description of the source.
 * @return pa_sink*: The pointer to the pulse audio sink.
 */
static pa_sink* pulse_description_to_loopbacked_sink(const struct userdata* u, const char *description) {
    pa_sink* pulse_sink = NULL;
    pa_sink* iterator;
    uint32_t index;
    if ( description ) {
        PA_IDXSET_FOREACH(iterator, u->core->sinks, index)
        {
            if ( !strcmp(description, pa_proplist_gets(iterator->proplist, PA_PROP_DEVICE_DESCRIPTION)) ) {
                pulse_sink = iterator;
                break;
            }
        }
    }
    return pulse_sink;
}

/**
 * @brief This function returns the source output pointer from the pulseaudio description.
 * @param: u: The user data pointer.
 *         description: The pulseaudio description property.
 * @return pa_source_output*: The pointer to the pulse audio source output.
 */
static pa_source_output* pulse_description_to_loopbacked_source_output(const struct userdata* u,
        const char *description) {
    pa_source_output* pulse_source_output = NULL;
    pa_source_output* iterator;
    uint32_t index;
    char Loopback_description[1024];
    sprintf(Loopback_description, "Loopback to %s", description);
    if ( description ) {
        PA_IDXSET_FOREACH(iterator, u->core->source_outputs, index)
        {
            const char* proplist_description = pa_proplist_gets(iterator->proplist, PA_PROP_MEDIA_NAME);
            if ( (proplist_description != NULL) && !(strcmp(Loopback_description, proplist_description)) ) {
                pulse_source_output = iterator;
                break;
            }
        }
    }
    return pulse_source_output;
}

/**
 * @brief This function returns the sink output pointer from the pulseaudio description.
 * @param: u: The user data pointer.
 *         description: The pulseaudio description property.
 * @return pa_sink_input*: The pointer to the pulse audio sink input.
 */
static pa_sink_input* pulse_description_to_loopbacked_sink_input(const struct userdata* u, const char *description) {
    pa_sink_input* pulse_sink_input = NULL;
    pa_sink_input* iterator;
    uint32_t index;
    char Loopback_description[1024];
    sprintf(Loopback_description, "Loopback from %s", description);
    if ( description ) {
        PA_IDXSET_FOREACH(iterator, u->core->sink_inputs, index)
        {
            const char* proplist_description = pa_proplist_gets(iterator->proplist, PA_PROP_MEDIA_NAME);
            if ( (proplist_description != NULL) && !(strcmp(Loopback_description, proplist_description)) ) {
                pulse_sink_input = iterator;
                break;
            }
        }
    }
    return pulse_sink_input;
}

/**
 * @brief This function returns the pulseaudio module pointer from the source and sink id.
 * @param: u: The user data pointer.
 *         source_id: The source id.
 *         sink_id: The sink id.
 * @return pa_module*: The pointer to the pulse audio loopback module.
 */
static pa_module* get_loopback_module(struct userdata *u, uint16_t source_id, uint16_t sink_id) {
    pa_module* loopback_module = NULL;
    void* map_source_id;
    void* map_sink_id;
    void* state;
    char* description = id_to_pulse_description(source_id, u->source_map);
    pa_sink_input* sink_input = pulse_description_to_loopbacked_sink_input(u, description);
    description = id_to_pulse_description(sink_id, u->sink_map);
    pa_source_output* source_output = pulse_description_to_loopbacked_source_output(u, description);
    if ( sink_input && source_output ) {
        if ( sink_input->module == source_output->module ) {
            loopback_module = sink_input->module;
        }
    }
    return loopback_module;
}

/**
 * @brief This function loads the loopback module between the source and sink.
 * @param: u: The user data pointer.
 *         source_id: The source id.
 *         sink_id: The sink id.
 * @return pa_module*: The pointer to the pulse audio loopback module.
 */
static pa_module* load_loopback_module(struct userdata*u, uint16_t source_id, uint16_t sink_id) {
    pa_module* loopback_module = NULL;
    char* source_name = id_to_am_name(source_id, u->source_map);
    char* sink_name = id_to_am_name(sink_id, u->sink_map);
    char arguments[1024];

    if ( false == pa_module_exists("module-loopback") ) {
        /* No operation */
    } else {
        sprintf(arguments, "source=%s", source_name);
        loopback_module = pa_module_load(u->core, "module-loopback", arguments);
        if ( loopback_module != NULL ) {
            // get the sink input connected to the sink
            pa_sink_input* sink_input = am_id_to_loopbacked_sink_input(u, sink_id);
            if ( sink_input != NULL ) {
                pa_sink_input_cork(sink_input, true);
            }
        } else {
            pa_log_error("Failed to load the Loopback module");
        }
    }
    return loopback_module;
}

/**
 * @brief This function replaces the whitespaces with the hash '#' character. These is needed because of
 * limitation in the controller topology descrption which cant parse whitespaces at present.
 * @param: char*: The string to replace the whitespaces.
 * @return void.
 */
static void replace_whitespace_with_hash(char* string) {
    int counter = 0;
    pa_log_info("%s", string);
    while ( string[counter] != '\0' ) {
        if ( string[counter] == ' ' ) {
            string[counter] = '#';
        }
        counter++;
    }
    pa_log_info("%s", string);
}

/**
 * @brief This function validates the name, if returns false if pointer is
 * NULL or the name is blank.
 * @param name: pointer to the name
 * @ret bool: true if name is valid and vice versa.
 */
static bool is_valid_name(const char* name ) {
	ROUTER_FUNCTION_ENTRY;
    bool is_valid = true;
    if((name == NULL)) {
        return false;
    }
    if(name != NULL)
    {
    	if(name[0] == '\0')
    	{
    		return false;
    	}
    }
    ROUTER_FUNCTION_EXIT;
    return is_valid;
}

static bool is_stream_for_probe(pa_proplist* proplist)
{
    /*
     * qt framework uses gstreamer for audio playback. Gstreamer creates one stream with media.name
     * property 'pulseaudio probe', to get the capability, I guess. These streams are
     * never used for actual playback. For playback a stream with name playback stream is
     * created.
     * Since the application.name for probe and playback stream are same. The AM is not
     * able to distinguish both streams. As a workaround we filter out the probe streams now.
     */
    bool probestream =false;
    char* media_name = (char*) pa_proplist_gets(proplist,PA_PROP_MEDIA_NAME);

    if((media_name != NULL) && (0==strcmp(media_name,"pulsesink probe")))
    {
        pa_log_info("is_stream_for_probe probe found");
        probestream = true;
    }
    return probestream;
}

/**
 * @brief This function returns the AudioManager name from the pulseaudio
 * properties media.role.
 * @param: proplist: The pointer to the pulseaudio property list.
 *         am_name: The audiomanager name of the sink_input.
 */
static void get_am_name_from_media_role(pa_proplist* proplist, char* am_name) {
    char *name_copy = (char*) pa_proplist_gets( proplist,
                                                PA_PROP_MEDIA_ROLE);
    if(is_valid_name(name_copy)) {
        strncpy(am_name, name_copy, AM_MAX_NAME_LENGTH);
        replace_whitespace_with_hash(am_name);
    }
}

/**
 * @brief This function returns the AudioManager name from the pulseaudio
 * properties application.name.
 * @param: proplist: The pointer to the pulseaudio property list.
 *         am_name: The audiomanager name of the sink_input.
 */
static void get_am_name_from_application_name(pa_proplist* proplist, char* am_name) {
    char *name_copy = (char*) pa_proplist_gets( proplist,
                                                PA_PROP_APPLICATION_NAME);
    if(is_valid_name(name_copy)) {
        strncpy(am_name, name_copy, AM_MAX_NAME_LENGTH);
        replace_whitespace_with_hash(am_name);
    }
}

/**
 * @brief This function returns the AudioManager name from the pulseaudio
 * properties. It checks the media role first if not found it reads the application
 * name
 * @param: proplist: The pointer to the pulseaudio property list.
 *         am_name: The audiomanager name of the sink_input.
 */
static void get_am_name_for_sink_source_stream(pa_proplist* proplist, char* am_name) {
    char am_name_copy[AM_MAX_NAME_LENGTH];
    memset(am_name_copy,0,sizeof(am_name_copy));
    get_am_name_from_media_role(proplist,am_name_copy);
    if(am_name_copy[0] == '\0') {
        get_am_name_from_application_name(proplist,am_name_copy);
    }
    strncpy(am_name,am_name_copy,AM_MAX_NAME_LENGTH);
}


/**
 * @brief This function returns the AudioManager name for sink from the pulseaudio
 * properties device description.
 * @param: proplist: The pointer to the pulseaudio property list.
 *         am_name: The audiomanager name of the sink.
 */
static void get_am_name_from_device_description(pa_proplist* proplist, char* am_name) {
	ROUTER_FUNCTION_ENTRY;
	if(proplist != NULL)
	{
    char *sink_name_copy = (char*) pa_proplist_gets(proplist,
                                                    PA_PROP_DEVICE_DESCRIPTION);
    if(is_valid_name(sink_name_copy)) {
        strncpy(am_name, sink_name_copy, AM_MAX_NAME_LENGTH);
        replace_whitespace_with_hash(am_name);
    }
	}
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief This function sets the volume in a pulseaudio volume structure.
 * @param: destchannelVolume: The pointer to the pulseaudio volume structure.
 *         num_channels: The number of channels to be set.
 *         volume: volume value
 * @return void.
 */
static void set_pa_volume(pa_cvolume* destchannelVolume, int num_channels, uint32_t volume) {
    int i;
    destchannelVolume->channels = num_channels;
    for ( i = 0; i < num_channels ; i++ ) {
        destchannelVolume->values[i] = volume;
    }
}

/**
 * @brief The hook/callback function called from the pulseaudio main loop whenever any sink_input is connected
 * to a sink.
 * @param: c: The pointer to pulseaudio core.
 *         sink_input: The sink input pointer.
 *         u: The pointer to the user data.
 * @return pa_hook_result: The result of the hook function.
 */
static pa_hook_result_t hook_callback_sink_input_put(pa_core *c, pa_sink_input *sink_input, struct userdata *u) {
    pa_log_info("hook_callback_sink_input_put index=%d",sink_input->index);
    pa_assert(c);
    pa_assert(u);
    pa_assert(sink_input);
    bool corked = false;
    pa_sink_input_state_t state;

    if(true == is_stream_for_probe(sink_input->proplist))
    {
    	return PA_HOOK_OK;
    }

    pa_sink_input_set_mute(sink_input, true, false);
    pa_sink_input_cork(sink_input, true);

    char source_name[AM_MAX_NAME_LENGTH];
    memset(source_name,0,sizeof(source_name));
    get_am_name_for_sink_source_stream(sink_input->proplist,source_name);

    char sink_name[AM_MAX_NAME_LENGTH];
    memset(sink_name,0,sizeof(sink_name));
    get_am_name_from_device_description(sink_input->sink->proplist,sink_name);

    pa_log_debug("hook_callback_sink_input_put source Name=%s sink_name=%s", source_name, sink_name);

    uint16_t source_id = am_name_to_id(source_name, u->source_map);
    uint16_t sink_id = am_name_to_id(sink_name, u->sink_map);
    int source_index = get_map_index_from_id(source_id, u->source_map);
    if ( source_index != -1 ) {
        u->source_map[source_index].data = (void*) sink_input;
        /* set the sink input volume */
        if ( ((u->source_map[source_index].builtin == false)) && (u->source_map[source_index].volume_valid == true) ) {
            pa_cvolume channelVolume;
            set_pa_volume(&channelVolume, sink_input->volume.channels, (uint32_t) (u->source_map[source_index].volume));
            pa_sink_input_set_volume(sink_input, &channelVolume, false, false);
        }

    }

    if ( (source_id != 0) && (sink_id != 0) ) {
        am_connect_t* con = get_connection_from_src_sink(u, source_id, sink_id);
        if ( (con != NULL) && (source_index != -1) ) {
        /**
         * This was added for AM aware application support if application is AM aware
         * it would first send the AM connect request and then create the pulseaudio
         * stream. Router module has to send the dummy ackConnect to the audiomanager
         * and in sink_input_put we check if connection is already present then start the stream.
         * Now for AM aware application the steps would be
         * 1. create a stream in corked state
         * 2. send AM connect request
         * 3. start playing if connection state changed to CS_CONNECTED.
         */
#if 0
            if ( u->source_map[source_index].source_state == SS_ON ) {
                if ( sink_input->muted == true ) {
                    pa_sink_input_set_mute(sink_input, false, false);
                }
                pa_sink_input_cork(sink_input, false);
            }
#endif
            pa_log_info("connection already present ");
        }
    }
    else if ( source_id != 0 ) {
        //Check if the source is connected to any other sink then the requested one in put request.
        am_connect_t* con = get_connection_from_source(u, source_id);
        if ( con != NULL ) {
            int sink_index = get_map_index_from_id(con->sink_id, u->sink_map);
            if ( sink_index != -1 ) {
                pa_sink* sink = (pa_sink*) (u->sink_map[sink_index].data);
                if ( sink != NULL ) {
                    int return_code = pa_sink_input_move_to(sink_input, sink, false);
                    pa_log_debug("sink Input move return=%d", return_code);
                }
                if ( u->source_map[source_index].source_state == SS_ON ) {
                    pa_sink_input_set_mute(sink_input, false, false);
                    pa_sink_input_cork(sink_input, false);
                }
                pa_log_info("connection already present moving to new");
                ROUTER_FUNCTION_EXIT;
                return PA_HOOK_OK;
            }
        }
    }

    /* 
     * send connect request to audio manager
     */
    am_main_connection_t connection_data;
    connection_data.connection_id = 0;
    connection_data.source_id = source_id;
    connection_data.sink_id = sink_id;
    if ( connection_data.source_id == 0 ) {
        if ( (source_name != NULL) && (strstr(source_name, "Loopback from") != NULL) ) {
            char description[256];
            memset(description, 0, sizeof(description));
            sscanf(source_name, "Loopback#from#%s", description);
            pa_log_debug("Loopback from -> %s", description);
            connection_data.source_id = pulse_description_to_am_id(description, u->source_map);
            pa_log_debug("source id =  %d", connection_data.source_id);
            pa_source* source = am_name_to_pa_source(u, id_to_am_name(connection_data.source_id, u->source_map));
            if ( source != NULL ) {
                pa_source_suspend(source, true, PA_SUSPEND_INTERNAL);
            }
        }
    }
    connection_data.delay = 0;
    connection_data.state = 0;
    if ( (connection_data.source_id != 0) && (connection_data.sink_id != 0) ) {
    	pa_log_info("AudioManager connect request");
        router_dbusif_command_connect(u, &connection_data);
    }

#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
    return PA_HOOK_OK;
}

/**
 * @brief The hook/callback function called from the pulseaudio main loop whenever any source_output is connected
 * to a source.
 * @param: c: The pointer to pulseaudio core.
 *         source_output: The source output pointer.
 *         u: The pointer to the user data.
 * @return pa_hook_result: The result of the hook function.
 */

static pa_hook_result_t hook_callback_source_output_put(pa_core *c, pa_source_output *source_output, struct userdata *u) {
    ROUTER_FUNCTION_ENTRY;
    pa_assert(c);
    pa_assert(source_output);
    pa_assert(u);
    bool corked = false;
    pa_source_output_state_t state;

    char sink_name[AM_MAX_NAME_LENGTH];
    memset(sink_name,0,sizeof(sink_name));
    get_am_name_for_sink_source_stream(source_output->proplist, sink_name);
    // in case of implicit loopback module loading this event should be ignored.
    if ( strstr(sink_name, "Loopback#to") == NULL ) {

        pa_assert(sink_name);
        pa_log_debug("sink name = %s", sink_name);
        state = pa_source_output_get_state(source_output);
        corked = (state == PA_SOURCE_OUTPUT_CORKED);
        if ( (!corked) && (!source_output->muted) ) {
            pa_source_output_set_mute(source_output, true, false);
            pa_source_output_cork(source_output, true);
        }

        am_main_connection_t connection_data;
        connection_data.connection_id = 0;
        //TODO : why this is hardcoded????
        connection_data.source_id = am_name_to_id("Mic", u->source_map);
        connection_data.sink_id = am_name_to_id(sink_name, u->sink_map);
        connection_data.delay = 0;
        connection_data.state = 0;
        if ( (connection_data.source_id != 0) && (connection_data.sink_id != 0) ) {
            int index = get_map_index_from_id(connection_data.sink_id, u->sink_map);
            if ( index != -1 ) {
                u->sink_map[index].data = source_output;
            }
            router_dbusif_command_connect(u, &connection_data);
        }
    }
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
    return PA_HOOK_OK;
}

/**
 * @brief The hook/callback function called from the pulseaudio main loop whenever any sink_input is unconnected
 * to a sink.
 * @param: c: The pointer to pulseaudio core.
 *         sink_input: The sink input pointer.
 *         u: The pointer to the user data.
 * @return pa_hook_result: The result of the hook function.
 */
static pa_hook_result_t hook_callback_sink_input_unlink(pa_core *c, pa_sink_input *sink_input, struct userdata *u) {
    ROUTER_FUNCTION_ENTRY;
    pa_log_info("hook_callback_sink_input_unlink index=%d",sink_input->index);
    pa_assert(c);
    pa_assert(sink_input);
    pa_assert(u);

    if(true == is_stream_for_probe(sink_input->proplist))
    {
        return PA_HOOK_OK;
    }

    bool corked = false;
    char source_name[AM_MAX_NAME_LENGTH];
    memset(source_name,0,sizeof(source_name));
    get_am_name_for_sink_source_stream(sink_input->proplist,source_name);
    uint16_t source_id = get_id_from_sink_input(sink_input, u->source_map);
    pa_log_debug("source name = %s source id: %d", source_name, source_id);
    if ((source_name != NULL) && (source_id == 0) && strstr(source_name, "Loopback from") != NULL ) {
        char description[256];
        memset(description, 0, sizeof(description));
        sscanf(source_name, "Loopback from %s", description);
        source_id = pulse_description_to_am_id(description, u->source_map);
    }

    /*
     * send disconnect request to the Audiomanager.
     */
    am_main_connection_t* conn;
    void *s;
    bool found = false;
    PA_HASHMAP_FOREACH(conn, u->main_connection_map, s)
    {
        if ( conn->source_id == source_id ) {
            found = true;
            break;
        }
    }
    if ( found ) {
        am_disconnect_t disconnectData;
        disconnectData.connection_id = conn->connection_id;
        router_dbusif_command_disconnect(u, &disconnectData);
    }
    int source_index = get_map_index_from_id(source_id, u->source_map);
    if ( (source_index != -1) && (u->source_map[source_index].builtin == false) ) {
        remove_pa_pointer(source_id, u->source_map);
    }
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
    return PA_HOOK_OK;
}

/**
 * @brief The hook/callback function called from the pulseaudio main loop whenever any source_output is unconnected
 * from a source.
 * @param: c: The pointer to pulseaudio core.
 *         source_output: The source output pointer.
 *         u: The pointer to the user data.
 * @return pa_hook_result: The result of the hook function.
 */
static pa_hook_result_t hook_callback_source_output_unlink(pa_core *c, pa_source_output *source_output,
        struct userdata *u) {
    ROUTER_FUNCTION_ENTRY;
    pa_assert(c);
    pa_assert(source_output);
    pa_assert(u);
    bool corked = false;
    char sink_name[AM_MAX_NAME_LENGTH];
    memset(sink_name,0,sizeof(sink_name));
    get_am_name_for_sink_source_stream(source_output->proplist,sink_name);
    pa_log_debug("hook_callback_source_output_unlink sink name = %s", sink_name);

    if ( strstr(sink_name, "Loopback to") == NULL ) {
        uint16_t sink_id = am_name_to_id(sink_name, u->sink_map);
        am_main_connection_t* conn;
        void *s;
        bool found = false;
        PA_HASHMAP_FOREACH(conn, u->main_connection_map, s)
        {
            if ( conn->sink_id == sink_id ) {
                found = true;
                break;
            }
        }
        if ( found ) {
            am_disconnect_t disconnectData;
            disconnectData.connection_id = conn->connection_id;
            router_dbusif_command_disconnect(u, &disconnectData);
        }
        int sink_index = get_map_index_from_id(sink_id, u->sink_map);
        if ( (sink_index != -1) && (u->sink_map[sink_index].builtin == false) ) {
            remove_pa_pointer(sink_id, u->sink_map);
        }
    }
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
    return PA_HOOK_OK;
}

static pa_hook_result_t register_sink_new(pa_core *c, pa_proplist* proplist, pa_cvolume* sink_volume,pa_sink* sink, struct userdata *u) {
    ROUTER_FUNCTION_ENTRY;
    am_sink_register_t sink_register;
    pa_assert(c);
    pa_assert(u);

    memset(&sink_register, 0, sizeof(am_sink_register_t));
    get_am_name_from_device_description(proplist,sink_register.name);

    if ( 0 == am_name_to_id(sink_register.name, u->sink_map) ) {
        pa_log_debug("sink Name=%s", sink_register.name);
        sink_register.domain_id = ((am_domain_register_t*) u->domain)->domain_id;
        sink_register.available = A_AVAILABLE;
        sink_register.sink_class_id = 1;
        sink_register.mute_state = 2;
        sink_register.mute_state = SS_OFF;
        sink_register.sink_id = 0;
        sink_register.visible = true;
        int index = get_free_map_index(u->sink_map);
        if ( index != -1 ) {
            strncpy(u->sink_map[index].name, sink_register.name, AM_MAX_NAME_LENGTH);
            strncpy(u->sink_map[index].description, sink_register.name, AM_MAX_NAME_LENGTH);
            u->sink_map[index].id = 0;
            u->sink_map[index].builtin = true;
            u->sink_map[index].data = sink;
            int16_t audiomanagervolume;
            audiomanagervolume = ((0.04577706569008926527809567406729) * sink_volume->values[0] ) - 3000;
            sink_register.volume = audiomanagervolume;
            /*
             * Convert the range [0-100] -> [0-65535]
             */
            sink_register.main_volume = sink_volume->values[0] * 100 / 65535;
	        pa_log_info("sink volume=%d , main_volume=%d",sink_register.volume,sink_register.main_volume );
            router_dbusif_routing_register_sink(u, &sink_register);
        }
    }
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
    return PA_HOOK_OK;

}

/**
 * @brief The hook/callback function called from the pulseaudio main loop whenever a new sink appears in the system.
 * @param: c: The pointer to pulseaudio core.
 *         sink: The sink pointer.
 *         u: The pointer to the user data.
 * @return pa_hook_result: The result of the hook function.
 */

static pa_hook_result_t hook_callback_sink_new(pa_core *c, pa_sink_new_data *sink, struct userdata *u) {
    ROUTER_FUNCTION_ENTRY;
    pa_assert(c);
    pa_assert(sink);
    pa_assert(u);
    register_sink_new(c,sink->proplist,&(sink->volume),NULL,u);
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif

    ROUTER_FUNCTION_EXIT;
    return PA_HOOK_OK;
}

static pa_hook_result_t register_source_new(pa_core *c, pa_proplist* proplist, pa_cvolume* source_volume, pa_source *source, struct userdata *u) {
    ROUTER_FUNCTION_ENTRY;
    am_source_register_t source_register;
    pa_assert(c);
    pa_assert(u);

    memset(&source_register, 0, sizeof(am_source_register_t));
    get_am_name_from_device_description(proplist,source_register.name);
    if ( 0 != am_name_to_id(source_register.name, u->source_map) ) {
        pa_log_debug("source name=%s", source_register.name);
        source_register.domain_id = ((am_domain_register_t*) (u->domain))->domain_id;
        source_register.availability_reason = 0;
        source_register.available = A_AVAILABLE;
        source_register.interrupt_state = 0;
        source_register.source_class_id = 1;
        source_register.source_id = 0;
        source_register.source_state = SS_OFF;
        source_register.visible = true;
        int index = get_free_map_index(u->source_map);
        if ( index != -1 ) {
            strncpy(u->source_map[index].name, source_register.name, AM_MAX_NAME_LENGTH);
            strncpy(u->source_map[index].description, source_register.name, AM_MAX_NAME_LENGTH);
            u->source_map[index].id = 0;
            u->source_map[index].builtin = true;
            u->source_map[index].data = source;
            int16_t audiomanagervolume;
            audiomanagervolume = ((0.04577706569008926527809567406729) * source_volume->values[0] ) - 3000;
            source_register.volume = source_volume->values[0];
	    pa_log_info("source volume=%d",source_register.volume);
            router_dbusif_routing_register_source(u, &source_register);
        }
    }
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
    return PA_HOOK_OK;
}

/**
 * @brief The hook/callback function called from the pulseaudio main loop whenever any source appears in system.
 * @param: c: The pointer to pulseaudio core.
 *         source: The sink input pointer.
 *         u: The pointer to the user data.
 * @return pa_hook_result: The result of the hook function.
 */
static pa_hook_result_t hook_callback_source_new(pa_core *c, pa_source_new_data* source, struct userdata *u) {
    ROUTER_FUNCTION_ENTRY;
    am_source_register_t source_register;
    pa_assert(c);
    pa_assert(source);
    pa_assert(u);
    register_source_new(c,source->proplist,&(source->volume),NULL,u);
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
    return PA_HOOK_OK;
}

/**
 * @brief The hook/callback function called from the pulseaudio main loop whenever any sink input appears in the
 * system.
 * @param: c: The pointer to pulseaudio core.
 *         new_data: The pointer to the pa_sink_input_new_data. Please note this structure is not same
 *         as pa_source_output Don't know why!!!
 *         u: The pointer to the user data.
 * @return pa_hook_result: The result of the hook function.
 */

static pa_hook_result_t hook_callback_sink_input_new(pa_core *c, pa_sink_input_new_data *new_data, struct userdata *u) {
    ROUTER_FUNCTION_ENTRY;

    bool already_present = false;

    pa_assert(c);
    pa_assert(new_data);
    pa_assert(u);

    if(true == is_stream_for_probe(new_data->proplist))
    {
        return PA_HOOK_OK;
    }

    char source_name[AM_MAX_NAME_LENGTH];
    memset(source_name,0,sizeof(source_name));
    get_am_name_for_sink_source_stream(new_data->proplist,source_name);
    if ( (source_name == NULL) || (0 != am_name_to_id(source_name, u->source_map))
            || (NULL != strstr(source_name, "Loopback from")) ) {
#if MODULE_ROUTER_EXTRA_LOGS
        print_maps(u);
#endif
        ROUTER_FUNCTION_EXIT;
        return PA_HOOK_OK;
    }
    /* Peek and figure out if already registered */
    router_dbusif_routing_peek_source(u, source_name);
    if ( 0 != am_name_to_id(source_name, u->source_map) ) {
        router_dbusif_get_domain_of_source(u, am_name_to_id(source_name, u->source_map));
        if ( u->source_map[get_map_index_from_name(source_name, u->source_map)].domain_id != 0 ) {
#if MODULE_ROUTER_EXTRA_LOGS
            print_maps(u);
#endif
            ROUTER_FUNCTION_EXIT;
            return PA_HOOK_OK;
        }
    }

    int index = get_map_index_from_name(source_name, u->source_map);
    if ( index == -1 ) {
        index = get_free_map_index(u->source_map);
        u->source_map[index].id = 0;
    }
    if ( index != -1 ) {
        already_present = (u->source_map[index].domain_id == 0) ? false : true;
    }
    strncpy(u->source_map[index].name, source_name, AM_MAX_NAME_LENGTH);
    strncpy(u->source_map[index].description, source_name, AM_MAX_NAME_LENGTH);
    u->source_map[index].id = 0;
    u->source_map[index].builtin = false;
    u->source_map[index].data = NULL;
    if ( already_present == false ) {
        am_source_register_t source_register;
        memset(&source_register, 0, sizeof(am_source_register_t));
        strncpy(source_register.name, source_name, AM_MAX_NAME_LENGTH);
        source_register.domain_id = ((am_domain_register_t*) (u->domain))->domain_id;
        source_register.availability_reason = 0;
        source_register.available = A_AVAILABLE;
        source_register.interrupt_state = 0;
        source_register.source_class_id = 1;
        source_register.source_id = 0;
        source_register.source_state = SS_OFF;
        source_register.visible = true;
	/*
         * For some reson this volume comes as zero so it gets translated to -3000
         * presently hard code to 0
         */
        //int16_t audiomanagervolume;
        //audiomanagervolume = ((0.04577706569008926527809567406729) * new_data->volume.values[0] ) - 3000;
        source_register.volume = 100;
	pa_log_info("source volume=%d",source_register.volume);
        router_dbusif_routing_register_source(u, &source_register);
    }

#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
    return PA_HOOK_OK;

}

/**
 * @brief The hook/callback function called from the pulseaudio main loop whenever any source output is appears in the
 * system.
 * @param: c: The pointer to pulseaudio core.
 *         new_data: The pointer to the pa_source_output_new_data. Please note this structure is not same
 *         as pa_source_output Dont know why!!!
 *         u: The pointer to the user data.
 * @return pa_hook_result: The result of the hook function.
 */
static pa_hook_result_t hook_callback_source_output_new(pa_core *c, pa_source_output_new_data* new_data,
        struct userdata *u) {
    ROUTER_FUNCTION_ENTRY;
    bool already_present = false;

    pa_assert(c);
    pa_assert(new_data);
    pa_assert(u);
    char sink_name[AM_MAX_NAME_LENGTH];
    memset(sink_name,0,sizeof(sink_name));
    get_am_name_for_sink_source_stream(new_data->proplist,sink_name);
    if ( (sink_name == NULL) || (0 != am_name_to_id(sink_name, u->sink_map))
            || (NULL != strstr(sink_name, "Loopback from")) ) {
#if MODULE_ROUTER_EXTRA_LOGS
        print_maps(u);
#endif
        ROUTER_FUNCTION_EXIT;
        return PA_HOOK_OK;
    }
    /* Peek and figure out if already registered */
    router_dbusif_routing_peek_sink(u, sink_name);
    if ( 0 != am_name_to_id(sink_name, u->sink_map) ) {
        router_dbusif_get_domain_of_source(u, am_name_to_id(sink_name, u->sink_map));
        if ( u->source_map[get_map_index_from_name(sink_name, u->sink_map)].domain_id != 0 ) {
#if MODULE_ROUTER_EXTRA_LOGS
            print_maps(u);
#endif
            ROUTER_FUNCTION_EXIT;
            return PA_HOOK_OK;
        }
    }

    int index = get_free_map_index(u->sink_map);
    if ( index == -1 ) {
        index = get_free_map_index(u->sink_map);
        u->sink_map[index].id = 0;
    }
    if ( index != -1 ) {
        already_present = (u->sink_map[index].domain_id == 0) ? false : true;
    }
    strncpy(u->sink_map[index].name, sink_name, AM_MAX_NAME_LENGTH);
    strncpy(u->sink_map[index].description, sink_name, AM_MAX_NAME_LENGTH);
    u->sink_map[index].id = 0;
    u->sink_map[index].builtin = false;
    u->sink_map[index].data = NULL;
    if ( already_present == false ) {
        am_sink_register_t sink_register;
        memset(&sink_register, 0, sizeof(am_sink_register_t));
        strncpy(sink_register.name, sink_name, AM_MAX_NAME_LENGTH);
        sink_register.domain_id = ((am_domain_register_t*) (u->domain))->domain_id;
        sink_register.availability_reason = 0;
        sink_register.available = A_AVAILABLE;
        sink_register.main_volume = 100;
        sink_register.sink_class_id = 1;
        sink_register.mute_state = 2;
        sink_register.mute_state = SS_OFF;
        sink_register.sink_id = 0;
        sink_register.visible = true;
        strncpy(u->sink_map[index].name, sink_register.name, AM_MAX_NAME_LENGTH);
        strncpy(u->sink_map[index].description, sink_register.name, AM_MAX_NAME_LENGTH);
	/*
         * For some reson this volume comes as zero so it gets translated to -3000
         * presently hard code to 0
         */
        //int16_t audiomanagervolume;
        //audiomanagervolume = ((0.04577706569008926527809567406729) * new_data->volume.values[0] ) - 3000;
        sink_register.volume = 0;
       /*
        * Convert the range [0-100] -> [0-65535]
        */
        sink_register.main_volume = 100;//new_data->volume.values[0] * 100 / 65535;
        pa_log_info("sink volume=%d , main_volume=%d",sink_register.volume,sink_register.main_volume );
        router_dbusif_routing_register_sink(u, &sink_register);
    }
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
    return PA_HOOK_OK;
}

/**
 * @brief The function to register the source with audio manager.
 * @param: u: The pointer to the user data.
 * @return void
 */
static void router_discover_register_source(struct userdata* u) {
    uint32_t index;
    pa_source* source;
    ROUTER_FUNCTION_ENTRY;
    PA_IDXSET_FOREACH(source, u->core->sources, index)
    {
        pa_cvolume* source_volume = (pa_cvolume*) pa_source_get_volume(source, true);
        register_source_new(u->core, source->proplist,source_volume,source, u);
    }
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The function to register the sink with audio manager.
 * @param: u: The pointer to the user data.
 * @return void
 */
static void router_discover_register_sink(struct userdata* u) {
    ROUTER_FUNCTION_ENTRY;
    pa_sink* sink;
    uint32_t index;
    PA_IDXSET_FOREACH(sink, u->core->sinks, index)
    {
        pa_cvolume* sink_volume = (pa_cvolume*) pa_sink_get_volume(sink, true);
        register_sink_new(u->core, sink->proplist, sink_volume,sink, u);
    }
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function from the dbus interface, reply to the command side connect request.
 * @param: u: The pointer to the user data.
 *         status: The status of the connect request
 *         data: The pointer to the am_main_connection which was used for connect request.
 * @return void
 */
static void cb_command_connect_reply(struct userdata *u, int status, void* data) {
    am_main_connection_t* main_connect_data = (am_main_connection_t*) data;
    ROUTER_FUNCTION_ENTRY;
    pa_assert(u);
    pa_assert(main_connect_data);

    pa_log_debug("main connection id=%d", main_connect_data->connection_id);

    if ( (status == E_OK) && (main_connect_data->connection_id != 0) ) {
        am_main_connection_t* main_connect_data_map = pa_xmemdup(main_connect_data, sizeof(am_main_connection_t));
        pa_log_debug("adding connection with id = %d status=%d", main_connect_data->connection_id, status);
        pa_hashmap_put(u->main_connection_map, (void*) (intptr_t) main_connect_data->connection_id,
                main_connect_data_map);
    }
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;

}

/**
 * @brief The callback function from the dbus interface, reply to the command side disconnect request.
 * @param: u: The pointer to the user data.
 *         status: The status of the connect request
 *         data: The pointer to the data specific to this request.
 * @return void
 */

static void cb_command_disconnect_reply(struct userdata *u, int status, void* data) {
    ROUTER_FUNCTION_ENTRY;
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function from the dbus interface, reply to the domain register.
 * @param: u: The pointer to the user data.
 *         status: The status of the connect request
 *         data: The pointer to the am_domain_register_t which was used for register domain
 * @return void
 */
static void cb_routing_register_domain_reply(struct userdata *u, int status, void* data) {
    am_domain_register_t* domain_register = (am_domain_register_t*) data;
    ROUTER_FUNCTION_ENTRY;
    pa_assert(u);
    pa_assert(domain_register);

    ((am_domain_register_t*) (u->domain))->domain_id = domain_register->domain_id;
    pa_log_debug("domainid=%d", domain_register->domain_id);
    /*
     * Get the list of source and register each and every source
     */
    router_discover_register_source(u);

    /*
     * Get the list of sink and register each and every sink
     */
    router_discover_register_sink(u);
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function from the dbus interface, reply to the peek sink.
 * @param: u: The pointer to the user data.
 *         status: The status of the connect request
 *         data: The pointer to the am_sink_register_t which was used for peek sink. The only valid field in
 *         this structure would be name and sink id. Please note this function will get called before the peek
 *         sink returns.
 * @return void
 */
static void cb_routing_peek_sink_reply(struct userdata *u, int status, void* data) {
    am_sink_register_t* sink = (am_sink_register_t*) data;
    ROUTER_FUNCTION_ENTRY;
    pa_assert(u);
    pa_assert(sink);

    pa_log_info("sink name=%s", sink->name);
    int index = am_name_to_map_index(sink->name, u->sink_map);
    if ( status == E_OK ) {
        int index = get_map_index_from_id(sink->sink_id, u->sink_map);
        if ( (index != -1) ) {
            pa_log_error("updating sink:%s id=%d", sink->name, sink->sink_id);
            strncmp(u->sink_map[index].name, sink->name, AM_MAX_NAME_LENGTH);
        } else {
            index = get_map_index_from_name(sink->name, u->sink_map);
            if ( index != -1 ) {
                u->sink_map[index].id = sink->sink_id;
            } else {
                index = get_free_map_index(u->sink_map);
                u->sink_map[index].id = sink->sink_id;
                strncmp(u->sink_map[index].name, sink->name, AM_MAX_NAME_LENGTH);
            }
        }
    }
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function from the dbus interface, reply to the peek source.
 * @param: u: The pointer to the user data.
 *         status: The status of the connect request
 *         data: The pointer to the am_source_register_t which was used for peek sink. The only valid field in
 *         this structure would be name and source id. Please note this function will get called before the peek
 *         source returns.
 * @return void
 */
static void cb_routing_peek_source_reply(struct userdata *u, int status, void* data) {
    am_source_register_t* source = (am_source_register_t*) data;
    ROUTER_FUNCTION_ENTRY;
    pa_assert(u);
    pa_assert(source);

    pa_log_debug("source name=%s id=%d", source->name, source->source_id);
    if ( status == E_OK ) {
        int index = get_map_index_from_id(source->source_id, u->source_map);
        if ( (index != -1) ) {
            pa_log_debug("updating source:%s id=%d", source->name, source->source_id);
            strncpy(u->source_map[index].name, source->name, AM_MAX_NAME_LENGTH);
            u->source_map[index].id = source->source_id;
        } else {
            index = get_map_index_from_name(source->name, u->source_map);
            if ( index != -1 ) {
                u->source_map[index].id = source->source_id;
                u->source_map[index].source_state = SS_OFF;
            } else {
                index = get_free_map_index(u->source_map);
                u->source_map[index].id = source->source_id;
                strncpy(u->source_map[index].name, source->name, AM_MAX_NAME_LENGTH);
                u->source_map[index].source_state = SS_OFF;
            }
        }
    }
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function from the dbus interface, reply to the get domain of sink.
 * @param: u: The pointer to the user data.
 *         status: The status of the connect request
 *         data: The pointer to the am_domain_of_source_sink_t which was used for get domain of request.
 * @return void
 */
static void cb_routing_get_domain_of_sink(struct userdata *u, int status, void* data) {
    am_domain_of_source_sink_t* sink = (am_domain_of_source_sink_t*) data;
    ROUTER_FUNCTION_ENTRY;

    pa_assert(u);
    pa_assert(sink);

    if ( status == E_OK ) {
        int index = get_map_index_from_id(sink->id, u->sink_map);
        if ( (index != -1) ) {
            u->sink_map[index].domain_id = sink->domain_id;
        }
    }
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;

}

/**
 * @brief The callback function from the dbus interface, reply to the get domain of source.
 * @param: u: The pointer to the user data.
 *         status: The status of the connect request
 *         data: The pointer to the am_domain_of_source_sink_t which was used for get domain of request.
 * @return void
 */
static void cb_routing_get_domain_of_source(struct userdata *u, int status, void* data) {
    am_domain_of_source_sink_t* source = (am_domain_of_source_sink_t*) data;
    ROUTER_FUNCTION_ENTRY;

    pa_assert(u);
    pa_assert(source);

    if ( status == E_OK ) {
        int16_t index = get_map_index_from_id(source->id, u->source_map);
        if ( (index != -1) ) {
            u->source_map[index].domain_id = source->domain_id;
        }
    }
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;

}

/**
 * @brief The callback function from the dbus interface, reply to the domain unregister function.
 * @param: u: The pointer to the user data.
 *         status: The status of the connect request
 *         data: The pointer to the data specific to this request.
 * @return void
 */
static void cb_routing_deregister_domain_reply(struct userdata *u, int status, void* data) {
    ROUTER_FUNCTION_ENTRY;
    ROUTER_FUNCTION_EXIT;

}

/**
 * @brief The callback function from the dbus interface, reply to the sink unregister function.
 * @param: u: The pointer to the user data.
 *         status: The status of the connect request
 *         data: The pointer to the data specific to this request.
 * @return void
 */
static void cb_routing_deregister_sink_reply(struct userdata *u, int status, void* data) {
    ROUTER_FUNCTION_ENTRY;
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function from the dbus interface, reply to the source unregister function.
 * @param: u: The pointer to the user data.
 *         status: The status of the connect request
 *         data: The pointer to the data specific to this request.
 * @return void
 */
static void cb_routing_deregister_source_reply(struct userdata *u, int status, void* data) {
    ROUTER_FUNCTION_ENTRY;
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function from the dbus interface, reply to the source register function.
 * @param: u: The pointer to the user data.
 *         status: The status of the connect request
 *         data: The pointer to the am_source_register_t used during source register.
 * @return void
 */
static void cb_routing_register_source_reply(struct userdata *u, int status, void* data) {
    am_source_register_t* source = (am_source_register_t*) data;
    ROUTER_FUNCTION_ENTRY;

    pa_assert(u);
    pa_assert(source);

    pa_log_debug("cb_routing_register_source_reply source name=%s", source->name);
    int index = am_name_to_map_index(source->name, u->source_map);
    if ( (index != -1) && (status == E_OK) ) {
        u->source_map[index].id = source->source_id;
        pa_log_error("updating source:%s id=%d", source->name, source->source_id);
    }
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function from the dbus interface, reply to the sink register function.
 * @param: u: The pointer to the user data.
 *         status: The status of the connect request
 *         data: The pointer to the am_sink_register_t used during source register.
 * @return void
 */
static void cb_routing_register_sink_reply(struct userdata *u, int status, void* data) {
    am_sink_register_t* sink = (am_sink_register_t*) data;
    ROUTER_FUNCTION_ENTRY;

    pa_assert(u);
    pa_assert(sink);

    pa_log_debug("cb_routing_register_source_reply sink name=%s", sink->name);
    int index = am_name_to_map_index(sink->name, u->sink_map);
    if ( (index != -1) && (status == E_OK) ) {
        u->sink_map[index].id = sink->sink_id;
        pa_log_error("updating sink:%s id=%d", sink->name, sink->sink_id);
    }
    ROUTER_FUNCTION_EXIT;
}

/**
 * @brief The callback function from the dbus interface, when async connect is received.
 * @param: u: The pointer to the user data.
 *         handle: The indentifier for this request.
 *         connectionid: The identifier for the connection
 *         source_id: The source id.
 *         sink_id: sink id.
 *         format: The connection format.
 * @return uint16_t: The return for this request.
 */
static uint16_t cb_routing_async_connect(struct userdata *u, uint16_t handle, uint16_t connection_id,
        uint16_t source_id, uint16_t sink_id, int32_t format) {
    uint16_t ack_status = E_OK;
    am_connect_t *conn_data;
    pa_module* loopback_module = NULL;
    ROUTER_FUNCTION_ENTRY;

    pa_assert(u);
    pa_assert(connection_id != 0);

    int already_in_map = !!pa_hashmap_get(u->connection_map, (void*) (intptr_t) connection_id);
    if ( !already_in_map ) {
        if ( (true == is_source_sink_builtin(source_id, u->source_map))
                && (true == is_source_sink_builtin(sink_id, u->sink_map)) ) {
            loopback_module = get_loopback_module(u, source_id, sink_id);
            if ( !loopback_module ) {
                loopback_module = load_loopback_module(u, source_id, sink_id);
                if ( loopback_module == NULL ) {
                    ack_status = E_NOT_POSSIBLE;
                }
            }
        }
        if ( ack_status == E_OK ) {
            conn_data = pa_xnew0(am_connect_t, 1);
            conn_data->handle = handle;
            conn_data->source_id = source_id;
            conn_data->sink_id = sink_id;
            conn_data->connection_id = connection_id;
            conn_data->connection_format = format;
            pa_hashmap_put(u->connection_map, (void*) (intptr_t) connection_id, conn_data);
        }
    } else {
        ack_status = E_NOT_POSSIBLE;
    }

    /* send ack */
    router_dbusif_ack_connect(u, handle, connection_id, ack_status);
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;

    return E_OK;
}

/**
 * @brief The callback function from the dbus interface, when async disconnect is received.
 * @param: u: The pointer to the user data.
 *         handle: The indentifier for this request.
 *         connection_id: The identifier for the connection
 * @return uint16_t: The return for this request.
 */
static uint16_t cb_routing_async_disconnect(struct userdata *u, uint16_t handle, uint16_t connection_id) {
    ROUTER_FUNCTION_ENTRY;

    uint16_t ack_status = E_OK;
    am_connect_t *conn_data = NULL;
    pa_assert(u);
    pa_assert(connection_id != 0);

    conn_data = pa_hashmap_get(u->connection_map, (void*) (intptr_t) connection_id);
    if ( (connection_id != 0) && (conn_data != NULL) ) {

        pa_module* loopback_module;
        if ( true == is_source_sink_builtin(conn_data->source_id, u->source_map)
                && true == is_source_sink_builtin(conn_data->sink_id, u->sink_map) ) {
            loopback_module = get_loopback_module(u, conn_data->source_id, conn_data->sink_id);
            if ( loopback_module ) {
#if PA_CHECK_VERSION(7,99,1)
            	pa_module_unload(loopback_module, true);
#else
            	pa_module_unload(u->core, loopback_module, true);
#endif
            }
        }
        pa_hashmap_remove(u->connection_map, (void*) (intptr_t) connection_id);
        pa_xfree(conn_data);
    }

    router_dbusif_ack_disconnect(u, handle, connection_id, ack_status);
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
    return E_OK;
}

/**
 * @brief The callback function from the dbus interface, when async set sink volume is received.
 * @param: u: The pointer to the user data.
 *         handle: The indentifier for this request.
 *         sink_id: sink id.
 *         volume: The volume to be set for sink.
 *         ramp_type: The type of ramp from source to destination volume
 *         ramp_time: The time for the volume change from source to destination volume.
 * @return uint16_t: The return for this request.
 */
static uint16_t cb_routing_async_set_sink_volume(struct userdata *u, uint16_t handle, uint16_t sink_id, int16_t volume,
        int16_t ramp_type, uint16_t ramp_time) {

    ROUTER_FUNCTION_ENTRY;
    pa_cvolume channelVolume;

    pa_assert(u);

    float volume_norm = (-65535.0 / 3000) * (-3000 - (int16_t) volume);
    pa_log_debug("cb_routing_async_set_sink_volume RequestedVol = %d NormalizedVol = %f", volume, volume_norm);

    int index = get_map_index_from_id(sink_id, u->sink_map);
    if ( index != -1 ) {
        if ( u->sink_map[index].builtin == false ) {
            pa_source_output *source_output = (pa_source_output*) u->sink_map[index].data;
            if ( source_output != NULL ) {
                set_pa_volume(&channelVolume, source_output->volume.channels, (uint32_t) volume_norm);
                pa_source_output_set_volume(source_output, &channelVolume, false, false);
            }
        } else {
            pa_sink* sink = (pa_sink*) u->sink_map[index].data;
            if ( sink != NULL ) {
                set_pa_volume(&channelVolume, sink->soft_volume.channels, (uint32_t) volume_norm);
                pa_sink_set_volume(sink, &channelVolume, false, false);
            }
            else {
                /*
                *Try to find the sink from the user structure
                */
                 uint32_t index;
                 PA_IDXSET_FOREACH(sink, u->core->sinks, index) {
                     char sink_am_name[AM_MAX_NAME_LENGTH];
                     get_am_name_from_device_description(sink->proplist,sink_am_name);
                     if(sink_am_name[0]!='\0')
                     {
                         int sink_map_index = get_map_index_from_name(sink_am_name,u->sink_map);
                         if(sink_map_index != -1)
                         {
                             u->sink_map[sink_map_index].data = sink;
                             set_pa_volume(&channelVolume, sink->soft_volume.channels, (uint32_t) volume_norm);
                             pa_sink_set_volume(sink, &channelVolume, false, false);
                             break;
                         }
                     }
                 }
            }
        }
    } else {
        index = get_free_map_index(u->sink_map);
    }
    if ( index != -1 ) {
        u->sink_map[index].id = sink_id;
        u->sink_map[index].volume = volume_norm;
        u->sink_map[index].volume_valid = true;
    }

    router_dbus_ack_set_sink_volume(u, handle, volume, E_OK);
#if ROUTER_MODULE_EXTRA_LOGS
    print_maps();
#endif
    ROUTER_FUNCTION_EXIT;
    return E_OK;
}

/**
 * @brief The callback function from the dbus interface, when async set source volume is received.
 * @param: u: The pointer to the user data.
 *         handle: The indentifier for this request.
 *         source_id: source id.
 *         volume: The volume to be set for sink.
 *         ramp_type: The type of ramp from source to destination volume
 *         ramp_time: The time for the volume change from source to destination volume.
 * @return uint16_t: The return for this request.
 */

static uint16_t cb_routing_async_set_source_volume(struct userdata *u, uint16_t handle, uint16_t source_id,
        int16_t volume, int16_t ramp_type, uint16_t ramp_time) {
    ROUTER_FUNCTION_ENTRY;
    pa_cvolume channelVolume;

    pa_assert(u);

    float volume_norm = (-65535.0 / 3000) * (-3000 - volume);
    pa_log_debug("cb_routing_async_set_source_volume RequestedVol = %d NormalizedVol = %f", volume, volume_norm);

    // get the sink_input from the id
    int index = get_map_index_from_id(source_id, u->source_map);
    if ( index != -1 ) {
        if ( u->source_map[index].builtin == false ) {
            pa_sink_input *sink_input = (pa_sink_input*) u->source_map[index].data;
            if ( sink_input != NULL ) {
                set_pa_volume(&channelVolume, sink_input->volume.channels, (uint32_t) volume_norm);
                pa_sink_input_set_volume(sink_input, &channelVolume, false, false);
            }
        } else {
            pa_source* source = (pa_source*) u->source_map[index].data;
            if ( source != NULL ) {
                set_pa_volume(&channelVolume, source->real_volume.channels, (uint32_t) volume_norm);
                pa_source_set_volume(source, &channelVolume, false, false);
            }
            else {
                uint32_t index;
                ROUTER_FUNCTION_ENTRY;
                PA_IDXSET_FOREACH(source, u->core->sources, index)
                {
                    char source_am_name[AM_MAX_NAME_LENGTH];
                    get_am_name_from_device_description(source->proplist,source_am_name);
                    if(source_am_name[0]!='\0')
                    {
                        int source_map_index = get_map_index_from_name(source_am_name,u->sink_map);
                        if(source_map_index != -1)
                        {
                            u->sink_map[source_map_index].data = source;
                            set_pa_volume(&channelVolume, source->real_volume.channels, (uint32_t) volume_norm);
                            pa_source_set_volume(source, &channelVolume, false, false);
                            break;
                        }
                    }
                }

            }
        }
    } else {
        index = get_free_map_index(u->source_map);
    }
    if ( index != -1 ) {
        u->source_map[index].id = source_id;
        u->source_map[index].volume = volume_norm;
        u->source_map[index].volume_valid = true;
    }

    router_dbus_ack_set_source_volume(u, handle, volume, E_OK);
#if ROUTER_MODULE_EXTRA_LOGS
    print_maps();
#endif
    ROUTER_FUNCTION_EXIT;
    return E_OK;
}

/**
 * @brief The callback function from the dbus interface, when async set source state is received.
 * @param: u: The pointer to the user data.
 *         handle: The indentifier for this request.
 *         source_id: source id.
 *         state: The source state
 * @return uint16_t: The return for this request.
 */
static uint16_t cb_routing_async_set_source_state(struct userdata *u, uint16_t handle, uint16_t source_id,
        int32_t state) {

    ROUTER_FUNCTION_ENTRY;
    bool found = false;
    int source_index;
    pa_assert(u);

    pa_log_debug("cb_routing_async_set_source_state handle = %d, source_id =%d, state = %d", handle, source_id, state);
    source_index = get_map_index_from_id(source_id, u->source_map);
    if ( source_index != -1 ) {
        if ( u->source_map[source_index].builtin == false ) {
            pa_sink_input* sink_input = (pa_sink_input*) u->source_map[source_index].data;
            if ( sink_input != NULL ) {
                bool corked = (pa_sink_input_get_state(sink_input) == PA_SINK_INPUT_CORKED);
                pa_log_debug("sink input corked = %d", corked);
                if ( state == SS_ON ) {
                    // un-cork the stream if already corked
                    if ( sink_input->muted ) {
                        pa_sink_input_set_mute(sink_input, false, false);
                    }
                    if ( corked ) {
                        pa_sink_input_cork(sink_input, false);
                    }
                } else if ( (state == SS_OFF) || ((state == SS_PAUSED)) ) {
                    // cork the stream if already un-corked
                    if ( !sink_input->muted ) {
                        pa_sink_input_set_mute(sink_input, true, false);
                    }
                    if ( !corked ) {
                        pa_sink_input_cork(sink_input, true);
                    }
                }
            }
        } else {
            pa_source* source = (pa_source*) u->source_map[source_index].data;
            if ( (source != NULL) && (u->source_map[source_index].builtin == true) ) {
                if ( state == SS_ON ) {
                    pa_source_suspend(source, false, PA_SUSPEND_INTERNAL);
                } else {
                    pa_source_suspend(source, true, PA_SUSPEND_INTERNAL);
                }
            }
        }
    } else {
        source_index = get_free_map_index(u->source_map);
    }
    if ( source_index != -1 ) {
        u->source_map[source_index].id = source_id;
        u->source_map[source_index].source_state = state;
    }

    router_dbus_ack_set_source_state(u, handle, E_OK);
#if MODULE_ROUTER_EXTRA_LOGS
    print_maps(u);
#endif
    ROUTER_FUNCTION_EXIT;
    return E_OK;
}

/**
 * @brief Pulse audio calls this function after loading the module. Pulseaudio has a feature to statically link
 * a module with the pulseaudio server in order to speed up start time. In case we want to statically link this module
 * then change is needed in this function name. There is alreadya a method to do this.
 * @param: m: The pointer to the module.
 * @return int
 */
int pa__init(pa_module *m) {

    struct userdata *u = NULL;
    unsigned int i = 0;
    router_init_data_t init_data;
    am_domain_register_t *domain;
    ROUTER_FUNCTION_ENTRY;
    pa_assert(m);
    m->userdata = u = pa_xnew0(struct userdata, 1);
    pa_assert(u);
    u->core = m->core;
    u->h = pa_xnew0(struct router_hooks, 1);
    pa_assert(u->h);

    u->h->hook_slot_sink_input_put = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_PUT], PA_HOOK_LATE + 30,
            (pa_hook_cb_t) hook_callback_sink_input_put, u);

    u->h->hook_slot_sink_input_unlink = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_UNLINK],
            PA_HOOK_LATE + 30, (pa_hook_cb_t) hook_callback_sink_input_unlink, u);

    u->h->hook_slot_source_output_put = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_PUT],
            PA_HOOK_LATE + 30, (pa_hook_cb_t) hook_callback_source_output_put, u);
    u->h->hook_slot_source_output_unlink = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_UNLINK],
            PA_HOOK_LATE + 30, (pa_hook_cb_t) hook_callback_source_output_unlink, u);
    u->h->hook_slot_sink_new = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_FIXATE], PA_HOOK_LATE + 30,
            (pa_hook_cb_t) hook_callback_sink_new, u);
    u->h->hook_slot_sink_input_new = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_NEW], PA_HOOK_LATE + 30,
            (pa_hook_cb_t) hook_callback_sink_input_new, u);
    u->h->hook_slot_source_new = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_FIXATE], PA_HOOK_LATE + 30,
            (pa_hook_cb_t) hook_callback_source_new, u);
    u->h->hook_slot_source_output_new = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_NEW],
            PA_HOOK_LATE + 30, (pa_hook_cb_t) hook_callback_source_output_new, u);

    /*
     * initialize dbus interface
     */
    memset(&init_data, 0, sizeof(init_data));
    init_data.bus_type = DBUS_BUS_SYSTEM;
    init_data.pulse_router_dbus_name = pa_xstrdup(PULSE_ROUTER_DBUS_NAME);
    init_data.pulse_router_dbus_return_interface_name = pa_xstrdup(
    PULSE_ROUTER_DBUS_RETURN_INTERFACE_NAME);
    init_data.pulse_router_dbus_interface_name = pa_xstrdup(PULSE_ROUTER_INTERFACE_NAME);
    init_data.pulse_router_dbus_path = pa_xstrdup(PULSE_ROUTER_DBUS_PATH);
    init_data.am_command_dbus_name = pa_xstrdup(AM_COMMAND_DBUS_NAME);
    init_data.am_command_dbus_interface_name = pa_xstrdup(AM_COMMAND_DBUS_INTERFACE_NAME);
    init_data.am_command_dbus_path = pa_xstrdup(AM_COMMAND_DBUS_PATH);
    init_data.am_routing_dbus_name = pa_xstrdup(AM_ROUTING_DBUS_NAME);
    init_data.am_routing_dbus_interface_name = pa_xstrdup(AM_ROUTING_DBUS_INTERFACE_NAME);
    init_data.am_routing_dbus_path = pa_xstrdup(AM_ROUTING_DBUS_PATH);
    init_data.am_watch_rule = pa_xstrdup(AM_COMMAND_SIGNAL_WATCH_RULE);
    init_data.cb_new_main_connection = cb_new_main_connection;
    init_data.cb_removed_main_connection = cb_removed_main_connection;
    init_data.cb_main_connection_state_changed = cb_main_connection_state_changed;
    init_data.cb_command_connect_reply = cb_command_connect_reply;
    init_data.cb_command_disconnect_reply = cb_command_disconnect_reply;
    init_data.cb_routing_register_domain_reply = cb_routing_register_domain_reply;
    init_data.cb_routing_deregister_domain_reply = cb_routing_deregister_domain_reply;
    init_data.cb_routing_register_sink_reply = cb_routing_register_sink_reply;
    init_data.cb_routing_deregister_sink_reply = cb_routing_deregister_sink_reply;
    init_data.cb_routing_register_source_reply = cb_routing_register_source_reply;
    init_data.cb_routing_deregister_source_reply = cb_routing_deregister_source_reply;
    init_data.cb_routing_async_connect = cb_routing_async_connect;
    init_data.cb_routing_async_disconnect = cb_routing_async_disconnect;
    init_data.cb_routing_async_set_sink_volume = cb_routing_async_set_sink_volume;
    init_data.cb_routing_async_set_source_volume = cb_routing_async_set_source_volume;
    init_data.cb_routing_async_set_source_state = cb_routing_async_set_source_state;
    init_data.cb_routing_peek_sink_reply = cb_routing_peek_sink_reply;
    init_data.cb_routing_peek_source_reply = cb_routing_peek_source_reply;
    init_data.cb_routing_get_domain_of_source_reply = cb_routing_get_domain_of_source;
    init_data.cb_routing_get_domain_of_sink_reply = cb_routing_get_domain_of_sink;

    u->dbusif = router_dbusif_init(u, &init_data);
    pa_assert(u->dbusif);
    MODULE_ROUTER_FREE(init_data.pulse_router_dbus_name);
    MODULE_ROUTER_FREE(init_data.pulse_router_dbus_return_interface_name);
    MODULE_ROUTER_FREE(init_data.pulse_router_dbus_interface_name);
    MODULE_ROUTER_FREE(init_data.pulse_router_dbus_path);
    MODULE_ROUTER_FREE(init_data.am_command_dbus_name);
    MODULE_ROUTER_FREE(init_data.am_command_dbus_interface_name);
    MODULE_ROUTER_FREE(init_data.am_command_dbus_path);
    MODULE_ROUTER_FREE(init_data.am_routing_dbus_name);
    MODULE_ROUTER_FREE(init_data.am_routing_dbus_interface_name);
    MODULE_ROUTER_FREE(init_data.am_routing_dbus_path);
    MODULE_ROUTER_FREE(init_data.am_watch_rule);

    /*
     * register pulse domain
     */
    domain = pa_xnew0(am_domain_register_t, 1);
    domain->domain_id = 0;
    strcpy(domain->name, "PulseAudio");
    strcpy(domain->busname, PULSE_ROUTER_DBUS_NAME);
    strcpy(domain->nodename, "pulseaudio");
    domain->early = false;
    domain->complete = true;
    domain->state = DS_CONTROLLED;

    memset(u->source_map, 0, sizeof(u->source_map));
    memset(u->sink_map, 0, sizeof(u->sink_map));
    router_dbusif_routing_register_domain(u, domain);
    u->domain = domain;
    /*
     * create main connection hash map
     */
    u->main_connection_map = pa_hashmap_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);
    /*
     * create connection hash map
     */
    u->connection_map = pa_hashmap_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);

    ROUTER_FUNCTION_EXIT;
    return 0;
}

/**
 * @brief Pulse audio calls this function when it unloads the module.
 * @param: m: The pointer to the module.
 * @return void
 */
void pa__done(pa_module *m) {

    ROUTER_FUNCTION_ENTRY;
    if ( m ) {
        struct userdata *u = m->userdata;
        router_dbusif_done(u);
        if ( u ) {
            if ( u->h ) {
                if ( u->h->hook_slot_sink_input_put ) {
                    pa_hook_slot_free(u->h->hook_slot_sink_input_put);
                }
                if ( u->h->hook_slot_sink_input_unlink ) {
                    pa_hook_slot_free(u->h->hook_slot_sink_input_unlink);
                }
                pa_xfree(u->h);
            }
            pa_hashmap_free(u->main_connection_map);;
            pa_hashmap_free(u->connection_map);
            MODULE_ROUTER_FREE(u->domain);
            pa_xfree(u);
        }
    }
    ROUTER_FUNCTION_EXIT;
}

