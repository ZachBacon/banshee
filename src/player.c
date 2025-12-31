#include "player.h"
#include <gst/gst.h>
#include <string.h>

typedef struct {
    PlayerStateCallback callback;
    gpointer user_data;
} CallbackData;

static gboolean bus_callback(GstBus *bus, GstMessage *msg, gpointer data) {
    MediaPlayer *player = (MediaPlayer *)data;
    
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *err;
            gchar *debug_info;
            gst_message_parse_error(msg, &err, &debug_info);
            g_printerr("Error received from element %s: %s\n",
                       GST_OBJECT_NAME(msg->src), err->message);
            g_printerr("Debugging information: %s\n",
                       debug_info ? debug_info : "none");
            g_clear_error(&err);
            g_free(debug_info);
            player->state = PLAYER_STATE_NULL;
            break;
        }
        case GST_MESSAGE_EOS:
            g_print("End-Of-Stream reached.\n");
            player->state = PLAYER_STATE_STOPPED;
            break;
        case GST_MESSAGE_BUFFERING: {
            gint percent = 0;
            gst_message_parse_buffering(msg, &percent);
            g_print("Buffering (%3d%%)\r", percent);
            
            /* Wait until buffering is complete before continuing playback */
            if (percent < 100 && player->state == PLAYER_STATE_PLAYING) {
                gst_element_set_state(player->playbin, GST_STATE_PAUSED);
            } else if (percent == 100 && player->state == PLAYER_STATE_PLAYING) {
                gst_element_set_state(player->playbin, GST_STATE_PLAYING);
            }
            break;
        }
        case GST_MESSAGE_STATE_CHANGED: {
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(player->playbin)) {
                GstState old_state, new_state, pending_state;
                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                
                if (new_state == GST_STATE_PLAYING) {
                    player->state = PLAYER_STATE_PLAYING;
                } else if (new_state == GST_STATE_PAUSED) {
                    player->state = PLAYER_STATE_PAUSED;
                } else if (new_state == GST_STATE_NULL) {
                    player->state = PLAYER_STATE_NULL;
                } else if (new_state == GST_STATE_READY) {
                    player->state = PLAYER_STATE_READY;
                }
            }
            break;
        }
        default:
            break;
    }
    
    return TRUE;
}

MediaPlayer* player_new(void) {
    MediaPlayer *player = g_new0(MediaPlayer, 1);
    
    /* Initialize GStreamer if not already initialized */
    if (!gst_is_initialized()) {
        gst_init(NULL, NULL);
    }
    
    /* Create playbin element */
    player->playbin = gst_element_factory_make("playbin", "playbin");
    if (!player->playbin) {
        g_printerr("Failed to create playbin element\n");
        g_free(player);
        return NULL;
    }
    
#ifdef _WIN32
    /* On Windows, use directsoundsink or wasapisink for better performance */
    GstElement *audio_sink = gst_element_factory_make("wasapisink", "audio_sink");
    if (!audio_sink) {
        audio_sink = gst_element_factory_make("directsoundsink", "audio_sink");
    }
    if (audio_sink) {
        /* Balanced settings for smooth playback without jitter */
        g_object_set(audio_sink, 
                     "buffer-time", G_GINT64_CONSTANT(200000),    /* 200ms */
                     NULL);
        g_object_set(player->playbin, "audio-sink", audio_sink, NULL);
    }
#endif
    
    /* Enable buffering only for network streams (will be set per-URI) */
    g_object_set(player->playbin,
                 "buffer-size", -1,    /* Let GStreamer decide */
                 "buffer-duration", -1,  /* Let GStreamer decide */
                 NULL);
    
    player->pipeline = player->playbin;
    player->bus = gst_element_get_bus(player->playbin);
    gst_bus_add_watch(player->bus, bus_callback, player);
    
    player->state = PLAYER_STATE_NULL;
    player->current_uri = NULL;
    player->volume = 1.0;
    player->duration = 0;
    player->position = 0;
    
    return player;
}

void player_free(MediaPlayer *player) {
    if (!player) return;
    
    if (player->playbin) {
        gst_element_set_state(player->playbin, GST_STATE_NULL);
        gst_object_unref(player->playbin);
    }
    
    if (player->bus) {
        gst_object_unref(player->bus);
    }
    
    g_free(player->current_uri);
    g_free(player);
}

gboolean player_set_uri(MediaPlayer *player, const gchar *uri) {
    if (!player || !uri) return FALSE;
    
    /* Stop current playback */
    gst_element_set_state(player->playbin, GST_STATE_NULL);
    
    /* Set new URI */
    g_free(player->current_uri);
    player->current_uri = g_strdup(uri);
    
    /* Check if URI needs file:// prefix */
    gchar *full_uri;
    if (g_str_has_prefix(uri, "file://") || 
        g_str_has_prefix(uri, "http://") ||
        g_str_has_prefix(uri, "https://")) {
        full_uri = g_strdup(uri);
    } else {
        /* Convert file path to proper URI format (handles Windows paths correctly) */
        full_uri = g_filename_to_uri(uri, NULL, NULL);
        if (!full_uri) {
            /* Fallback if conversion fails */
            full_uri = g_strdup_printf("file://%s", uri);
        }
    }
    
    g_object_set(player->playbin, "uri", full_uri, NULL);
    
    /* Configure buffering based on URI type */
    if (g_str_has_prefix(uri, "http://") || g_str_has_prefix(uri, "https://")) {
        /* Enable buffering and progressive downloading for HTTP streams */
        g_object_set(player->playbin,
                     "buffer-size", 2 * 1024 * 1024,    /* 2MB buffer */
                     "buffer-duration", 5 * GST_SECOND,  /* 5 seconds */
                     NULL);
        
        gint flags;
        g_object_get(player->playbin, "flags", &flags, NULL);
        flags |= 0x00000080;  /* GST_PLAY_FLAG_DOWNLOAD */
        g_object_set(player->playbin, "flags", flags, NULL);
        g_print("Enabled progressive download for network stream\n");
    } else {
        /* For local files, disable buffering for immediate playback */
        g_object_set(player->playbin,
                     "buffer-size", -1,
                     "buffer-duration", -1,
                     NULL);
    }
    
    g_free(full_uri);
    
    /* Set to ready state to get duration */
    gst_element_set_state(player->playbin, GST_STATE_PAUSED);
    
    /* Wait for state change */
    GstState state;
    gst_element_get_state(player->playbin, &state, NULL, GST_CLOCK_TIME_NONE);
    
    /* Query duration */
    gst_element_query_duration(player->playbin, GST_FORMAT_TIME, &player->duration);
    
    return TRUE;
}

gboolean player_play(MediaPlayer *player) {
    if (!player || !player->playbin) return FALSE;
    
    GstStateChangeReturn ret = gst_element_set_state(player->playbin, GST_STATE_PLAYING);
    
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        return FALSE;
    }
    
    player->state = PLAYER_STATE_PLAYING;
    return TRUE;
}

gboolean player_pause(MediaPlayer *player) {
    if (!player || !player->playbin) return FALSE;
    
    GstStateChangeReturn ret = gst_element_set_state(player->playbin, GST_STATE_PAUSED);
    
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the paused state.\n");
        return FALSE;
    }
    
    player->state = PLAYER_STATE_PAUSED;
    return TRUE;
}

gboolean player_stop(MediaPlayer *player) {
    if (!player || !player->playbin) return FALSE;
    
    GstStateChangeReturn ret = gst_element_set_state(player->playbin, GST_STATE_NULL);
    
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the null state.\n");
        return FALSE;
    }
    
    player->state = PLAYER_STATE_STOPPED;
    player->position = 0;
    return TRUE;
}

void player_set_volume(MediaPlayer *player, gdouble volume) {
    if (!player || !player->playbin) return;
    
    /* Clamp volume between 0.0 and 1.0 */
    if (volume < 0.0) volume = 0.0;
    if (volume > 1.0) volume = 1.0;
    
    player->volume = volume;
    g_object_set(player->playbin, "volume", volume, NULL);
}

gdouble player_get_volume(MediaPlayer *player) {
    if (!player) return 0.0;
    return player->volume;
}

gboolean player_seek(MediaPlayer *player, gint64 position) {
    if (!player || !player->playbin) return FALSE;
    
    gboolean ret = gst_element_seek_simple(player->playbin, GST_FORMAT_TIME,
                                           GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
                                           position);
    
    if (ret) {
        player->position = position;
    }
    
    return ret;
}

gint64 player_get_duration(MediaPlayer *player) {
    if (!player || !player->playbin) return 0;
    
    gint64 duration;
    if (gst_element_query_duration(player->playbin, GST_FORMAT_TIME, &duration)) {
        player->duration = duration;
        return duration;
    }
    
    return player->duration;
}

gint64 player_get_position(MediaPlayer *player) {
    if (!player || !player->playbin) return 0;
    
    gint64 position;
    if (gst_element_query_position(player->playbin, GST_FORMAT_TIME, &position)) {
        player->position = position;
        return position;
    }
    
    return player->position;
}

PlayerState player_get_state(MediaPlayer *player) {
    if (!player) return PLAYER_STATE_NULL;
    return player->state;
}
