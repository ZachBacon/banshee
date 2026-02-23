#include "player.h"
#include <gst/gst.h>
#include <gtk/gtk.h>
#include <string.h>

/* GStreamer playbin flag constants */
#define GST_PLAY_FLAG_VIDEO    (1 << 0)
#define GST_PLAY_FLAG_AUDIO    (1 << 1)
#define GST_PLAY_FLAG_TEXT     (1 << 2)
#define GST_PLAY_FLAG_SOFT_VOLUME (1 << 4)
#define GST_PLAY_FLAG_DOWNLOAD (1 << 7)

/* Callback data for GTK4 video sink (paintable) notification */
typedef struct {
    MediaPlayer *player;
    GCallback widget_ready_callback;
    gpointer user_data;
} GtkSinkCallbackData;

/* Helper function to check if current file is a video file */
static gboolean is_current_file_video(MediaPlayer *player) {
    if (!player || !player->current_uri) return FALSE;
    
    gchar *lower_uri = g_ascii_strdown(player->current_uri, -1);
    gboolean is_video = g_str_has_suffix(lower_uri, ".mp4") ||
                       g_str_has_suffix(lower_uri, ".mkv") ||
                       g_str_has_suffix(lower_uri, ".avi") ||
                       g_str_has_suffix(lower_uri, ".mov") ||
                       g_str_has_suffix(lower_uri, ".wmv") ||
                       g_str_has_suffix(lower_uri, ".webm") ||
                       g_str_has_suffix(lower_uri, ".m4v") ||
                       g_str_has_suffix(lower_uri, ".3gp") ||
                       g_str_has_suffix(lower_uri, ".flv");
    g_free(lower_uri);
    return is_video;
}

static void on_gtk4_paintable_ready(GObject *sink, GParamSpec *pspec, gpointer user_data) {
    (void)pspec;
    GtkSinkCallbackData *data = (GtkSinkCallbackData *)user_data;
    
    GdkPaintable *paintable = NULL;
    g_object_get(sink, "paintable", &paintable, NULL);
    
    if (paintable && data->widget_ready_callback) {
        g_debug("Player: gtk4paintablesink paintable is ready");
        /* Create a GtkPicture widget to display the paintable */
        GtkWidget *picture = gtk_picture_new_for_paintable(paintable);
        gtk_widget_set_hexpand(picture, TRUE);
        gtk_widget_set_vexpand(picture, TRUE);
        /* Call the user's callback with the picture widget */
        ((void (*)(GtkWidget *, gpointer))data->widget_ready_callback)(picture, data->user_data);
        g_object_unref(paintable);
    }
}

typedef struct {
    PlayerStateCallback callback;
    gpointer user_data;
} CallbackData;

struct _PositionCallbackData {
    PlayerPositionCallback callback;
    gpointer user_data;
};

struct _EosCallbackData {
    PlayerEosCallback callback;
    gpointer user_data;
};

/* Timer callback for position updates */
static gboolean position_timer_callback(gpointer user_data) {
    MediaPlayer *player = (MediaPlayer *)user_data;
    
    if (player->state == PLAYER_STATE_PLAYING && player->position_cb_data && player->position_cb_data->callback) {
        gint64 pos, dur;
        if (gst_element_query_position(player->playbin, GST_FORMAT_TIME, &pos) &&
            gst_element_query_duration(player->playbin, GST_FORMAT_TIME, &dur)) {
            player->position_cb_data->callback(player, pos, dur, player->position_cb_data->user_data);
        }
    }
    
    return G_SOURCE_CONTINUE;  /* Keep the timer running */
}

static gboolean bus_callback(GstBus *bus, GstMessage *msg, gpointer data) {
    MediaPlayer *player = (MediaPlayer *)data;
    (void)bus;
    
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
            player->state = PLAYER_STATE_STOPPED;
            /* Notify UI to advance to next track */
            if (player->eos_cb_data && player->eos_cb_data->callback) {
                g_idle_add_once((GSourceOnceFunc)player->eos_cb_data->callback, player->eos_cb_data->user_data);
            }
            break;
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


/* Callback to optimize decoder settings for smooth playback */
static void on_element_added(GstBin *bin, GstElement *element, gpointer user_data) {
    (void)bin;
    (void)user_data;
    
    gchar *name = gst_element_get_name(element);
    
    /* Configure dav1d decoder for optimal multi-threaded AV1 decoding */
    if (g_str_has_prefix(name, "dav1ddec")) {
        gint n_threads = g_get_num_processors();
        if (n_threads > 8) n_threads = 8;  /* Cap at 8 threads */
        g_object_set(element, "n-threads", n_threads, NULL);
        g_debug("Configured dav1ddec with %d threads for smooth AV1 playback", n_threads);
    }
    /* Configure other video decoders */
    else if (g_str_has_prefix(name, "avdec_")) {
        g_object_set(element, "max-threads", g_get_num_processors(), NULL);
    }
    
    g_free(name);
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
    
    /* Ensure playbin flags include both audio and video playback BEFORE setting sinks.
     * Also add SOFT_VOLUME (bit 4) for volume control */
    gint flags;
    g_object_get(player->playbin, "flags", &flags, NULL);
    g_debug("Player: Initial playbin flags: 0x%x", flags);
    flags |= GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_SOFT_VOLUME;
    g_object_set(player->playbin, "flags", flags, NULL);
    g_debug("Player: Playbin flags set to 0x%x (video + audio enabled)", flags);
    
    /* Explicitly set volume to ensure audio is not muted */
    g_object_set(player->playbin, "volume", 1.0, NULL);
    g_object_set(player->playbin, "mute", FALSE, NULL);
    
    /* Create GTK4 video sink for embedded video playback */
    player->video_sink = gst_element_factory_make("gtk4paintablesink", "videosink");
    if (player->video_sink) {
        /* Set gtk4paintablesink as the video sink for playbin */
        g_object_set(player->playbin, "video-sink", player->video_sink, NULL);
        g_debug("Player: Using gtk4paintablesink for embedded video playback");
    } else {
        g_debug("Player: gtk4paintablesink not available, video will use default sink");
    }
    
    /* Create and set an explicit audio sink to ensure audio works with video */
    GstElement *audio_sink = gst_element_factory_make("autoaudiosink", "audiosink");
    if (audio_sink) {
        g_object_set(player->playbin, "audio-sink", audio_sink, NULL);
        g_debug("Player: Using autoaudiosink for audio playback");
    } else {
        g_debug("Player: autoaudiosink not available, using default audio sink");
    }
    
    /* Create pipeline and bus */
    player->pipeline = player->playbin;
    player->bus = gst_element_get_bus(player->playbin);
    gst_bus_add_watch(player->bus, bus_callback, player);
    
    /* Initialize state */
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
    
    g_free(player->position_cb_data);
    g_free(player->eos_cb_data);
    g_free(player->current_uri);
    g_free(player);
}

gboolean player_set_uri(MediaPlayer *player, const gchar *uri) {
    if (!player || !uri || uri[0] == '\0') return FALSE;
    
    /* Validate local file existence */
    if (!g_str_has_prefix(uri, "http://") && !g_str_has_prefix(uri, "https://")) {
        const gchar *file_path = uri;
        if (g_str_has_prefix(uri, "file://")) {
            gchar *tmp = g_filename_from_uri(uri, NULL, NULL);
            if (tmp) {
                gboolean exists = g_file_test(tmp, G_FILE_TEST_EXISTS);
                g_free(tmp);
                if (!exists) {
                    g_warning("player_set_uri: file does not exist: %s", uri);
                    return FALSE;
                }
            }
        } else {
            if (!g_file_test(file_path, G_FILE_TEST_EXISTS)) {
                g_warning("player_set_uri: file does not exist: %s", file_path);
                return FALSE;
            }
        }
    }
    
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
    g_debug("Player: Setting URI: %s", full_uri);
    
    /* Configure buffering based on URI type */
    if (g_str_has_prefix(uri, "http://") || g_str_has_prefix(uri, "https://")) {
        /* Enable buffering and progressive downloading for HTTP streams */
        g_object_set(player->playbin,
                     "buffer-size", 2 * 1024 * 1024,    /* 2MB buffer */
                     "buffer-duration", 5 * GST_SECOND,  /* 5 seconds */
                     NULL);
        
        gint flags;
        g_object_get(player->playbin, "flags", &flags, NULL);
        flags |= GST_PLAY_FLAG_DOWNLOAD;
        g_object_set(player->playbin, "flags", flags, NULL);
    } else {
        /* For local files, disable buffering for immediate playback */
        g_object_set(player->playbin,
                     "buffer-size", -1,
                     "buffer-duration", -1,
                     NULL);
    }
    
    g_free(full_uri);
    
    /* Set to paused state to preroll and get duration.
     * Don't block waiting for state change - let GStreamer handle it async.
     * Duration will be queried when playback starts. */
    gst_element_set_state(player->playbin, GST_STATE_PAUSED);
    
    /* For remote streams only, wait a bit for buffering.
     * Local files don't need to wait. */
    if (g_str_has_prefix(uri, "http://") || g_str_has_prefix(uri, "https://")) {
        GstState state;
        gst_element_get_state(player->playbin, &state, NULL, 2 * GST_SECOND);
    }
    
    /* Try to query duration (may not be available yet for async preroll) */
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
    
    /* Log stream information */
    gint n_video = 0, n_audio = 0, n_text = 0;
    g_object_get(player->playbin, "n-video", &n_video, "n-audio", &n_audio, "n-text", &n_text, NULL);
    g_debug("Player: Stream info - Video: %d, Audio: %d, Text: %d", n_video, n_audio, n_text);
    
    gint current_audio = -1, current_video = -1;
    g_object_get(player->playbin, "current-audio", &current_audio, "current-video", &current_video, NULL);
    g_debug("Player: Current streams - Video: %d, Audio: %d", current_video, current_audio);
    
    gint flags = 0;
    g_object_get(player->playbin, "flags", &flags, NULL);
    g_debug("Player: Flags: 0x%x (audio=%d, video=%d)", flags, (flags >> 1) & 1, flags & 1);
    
    gdouble vol = 0;
    gboolean mute = FALSE;
    g_object_get(player->playbin, "volume", &vol, "mute", &mute, NULL);
    g_debug("Player: Volume: %.2f, Mute: %s", vol, mute ? "YES" : "NO");
    
    /* If there's audio but it's not selected, select it */
    if (n_audio > 0 && current_audio < 0) {
        g_debug("Player: No audio stream selected, selecting stream 0");
        g_object_set(player->playbin, "current-audio", 0, NULL);
    }
    
    /* Start position timer for UI updates
     * Use 250ms interval to reduce UI update overhead during video playback.
     * This prevents the progress bar updates from causing choppy video. */
    if (player->position_cb_data && player->ui_position_timer_id == 0) {
        gboolean is_video = is_current_file_video(player);
        guint interval = is_video ? 500 : 250;  /* Slower updates for video */
        player->ui_position_timer_id = g_timeout_add(interval, position_timer_callback, player);
    }
    
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
    
    /* Stop position timer */
    if (player->ui_position_timer_id != 0) {
        g_source_remove(player->ui_position_timer_id);
        player->ui_position_timer_id = 0;
    }
    
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
    
    /* Stop position timer */
    if (player->ui_position_timer_id != 0) {
        g_source_remove(player->ui_position_timer_id);
        player->ui_position_timer_id = 0;
    }
    
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
    
    /* Use ACCURATE flag for more precise seeking, especially important for chapters.
     * FLUSH clears the pipeline buffers for immediate seek.
     * Remove KEY_UNIT to seek to exact position rather than nearest keyframe. */
    gboolean ret = gst_element_seek_simple(player->playbin, GST_FORMAT_TIME,
                                           GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
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

void player_set_position_callback(MediaPlayer *player, PlayerPositionCallback callback, gpointer user_data) {
    if (!player) return;
    
    g_free(player->position_cb_data);
    
    if (callback) {
        player->position_cb_data = g_new0(PositionCallbackData, 1);
        player->position_cb_data->callback = callback;
        player->position_cb_data->user_data = user_data;
    } else {
        player->position_cb_data = NULL;
    }
}

void player_set_eos_callback(MediaPlayer *player, PlayerEosCallback callback, gpointer user_data) {
    if (!player) return;
    
    g_free(player->eos_cb_data);
    
    if (callback) {
        player->eos_cb_data = g_new0(EosCallbackData, 1);
        player->eos_cb_data->callback = callback;
        player->eos_cb_data->user_data = user_data;
    } else {
        player->eos_cb_data = NULL;
    }
}

/* Video support functions */
void player_set_video_window(MediaPlayer *player, guintptr window_handle) {
    if (!player || !player->playbin) return;
    
    g_debug("Setting video window handle: %lu", (unsigned long)window_handle);
    
    /* Set the window handle on the playbin's video overlay */
    /* First try to set it directly on playbin */
    if (GST_IS_VIDEO_OVERLAY(player->playbin)) {
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(player->playbin), window_handle);
        g_debug("Set window handle on playbin directly");
        return;
    }
    
    /* If that doesn't work, try to get the video sink and set it there */
    GstElement *video_sink = NULL;
    g_object_get(player->playbin, "video-sink", &video_sink, NULL);
    
    if (video_sink) {
        if (GST_IS_VIDEO_OVERLAY(video_sink)) {
            gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(video_sink), window_handle);
            g_debug("Set window handle on video sink");
        }
        gst_object_unref(video_sink);
    } else {
        /* If no video sink is set yet, we need to handle it through a bus message */
        /* This will be caught when the pipeline is ready */
        g_debug("No video sink yet, storing window handle for later");
        g_object_set_data(G_OBJECT(player->playbin), "window-handle", GUINT_TO_POINTER(window_handle));
    }
}

gboolean player_has_video(MediaPlayer *player) {
    if (!player || !player->playbin) return FALSE;
    
    gint n_video = 0;
    g_object_get(player->playbin, "n-video", &n_video, NULL);
    return (n_video > 0);
}

GtkWidget* player_get_video_widget(MediaPlayer *player) {
    if (!player || !player->video_sink) return NULL;
    
    /* For GTK4, get the paintable and create a GtkPicture widget */
    GdkPaintable *paintable = NULL;
    g_object_get(player->video_sink, "paintable", &paintable, NULL);
    
    if (paintable) {
        GtkWidget *picture = gtk_picture_new_for_paintable(paintable);
        gtk_widget_set_hexpand(picture, TRUE);
        gtk_widget_set_vexpand(picture, TRUE);
        g_object_unref(paintable);
        return picture;
    }
    
    return NULL;
}

void player_set_video_widget_ready_callback(MediaPlayer *player, GCallback callback, gpointer user_data) {
    if (!player || !player->video_sink) return;
    
    /* Check if it's gtk4paintablesink */
    const gchar *factory_name = gst_plugin_feature_get_name(
        GST_PLUGIN_FEATURE(gst_element_get_factory(player->video_sink)));
    
    if (g_strcmp0(factory_name, "gtk4paintablesink") == 0) {
        
        /* Check if paintable is already available */
        GdkPaintable *paintable = NULL;
        g_object_get(player->video_sink, "paintable", &paintable, NULL);
        
        if (paintable) {
            /* Paintable already exists, create widget and call callback immediately */
            g_debug("Player: gtk4paintablesink paintable already available");
            GtkWidget *picture = gtk_picture_new_for_paintable(paintable);
            gtk_widget_set_hexpand(picture, TRUE);
            gtk_widget_set_vexpand(picture, TRUE);
            ((void (*)(GtkWidget *, gpointer))callback)(picture, user_data);
            g_object_unref(paintable);
        } else {
            /* Paintable not ready yet, connect to notify signal */
            g_debug("Player: Waiting for gtk4paintablesink paintable to be created...");
            
            GtkSinkCallbackData *cb_data = g_new0(GtkSinkCallbackData, 1);
            cb_data->player = player;
            cb_data->widget_ready_callback = callback;
            cb_data->user_data = user_data;
            
            g_signal_connect_data(player->video_sink, "notify::paintable",
                                G_CALLBACK(on_gtk4_paintable_ready),
                                cb_data,
                                (GClosureNotify)g_free,
                                0);
        }
    }
}

/* Stream information and selection functions */

gint player_get_audio_stream_count(MediaPlayer *player) {
    if (!player || !player->playbin) return 0;
    gint n_audio = 0;
    g_object_get(player->playbin, "n-audio", &n_audio, NULL);
    return n_audio;
}

gint player_get_subtitle_stream_count(MediaPlayer *player) {
    if (!player || !player->playbin) return 0;
    gint n_text = 0;
    g_object_get(player->playbin, "n-text", &n_text, NULL);
    return n_text;
}

gint player_get_current_audio_stream(MediaPlayer *player) {
    if (!player || !player->playbin) return -1;
    gint current = -1;
    g_object_get(player->playbin, "current-audio", &current, NULL);
    return current;
}

gint player_get_current_subtitle_stream(MediaPlayer *player) {
    if (!player || !player->playbin) return -1;
    gint current = -1;
    g_object_get(player->playbin, "current-text", &current, NULL);
    return current;
}

void player_set_audio_stream(MediaPlayer *player, gint index) {
    if (!player || !player->playbin) return;
    g_object_set(player->playbin, "current-audio", index, NULL);
    g_debug("Player: Set audio stream to %d", index);
}

void player_set_subtitle_stream(MediaPlayer *player, gint index) {
    if (!player || !player->playbin) return;
    g_object_set(player->playbin, "current-text", index, NULL);
    
    /* Enable subtitles when selecting a stream */
    gint flags;
    g_object_get(player->playbin, "flags", &flags, NULL);
    flags |= GST_PLAY_FLAG_TEXT;
    g_object_set(player->playbin, "flags", flags, NULL);
    
    g_debug("Player: Set subtitle stream to %d", index);
}

void player_set_subtitles_enabled(MediaPlayer *player, gboolean enabled) {
    if (!player || !player->playbin) return;
    
    gint flags;
    g_object_get(player->playbin, "flags", &flags, NULL);
    
    if (enabled) {
        flags |= GST_PLAY_FLAG_TEXT;
    } else {
        flags &= ~GST_PLAY_FLAG_TEXT;
    }
    
    g_object_set(player->playbin, "flags", flags, NULL);
    g_debug("Player: Subtitles %s", enabled ? "enabled" : "disabled");
}

gboolean player_get_subtitles_enabled(MediaPlayer *player) {
    if (!player || !player->playbin) return FALSE;
    
    gint flags;
    g_object_get(player->playbin, "flags", &flags, NULL);
    return (flags & GST_PLAY_FLAG_TEXT) != 0;
}

static void debug_print_tag(const GstTagList *list, const gchar *tag, gpointer user_data) {
    (void)user_data;
    gchar *str = NULL;
    if (gst_tag_list_get_string(list, tag, &str)) {
        g_debug("  %s: %s", tag, str);
        g_free(str);
    }
}

static gchar* get_tag_string(GstTagList *tags, const gchar *tag) {
    if (!tags) return NULL;
    gchar *value = NULL;
    gst_tag_list_get_string(tags, tag, &value);
    return value;
}

StreamInfo* player_get_audio_stream_info(MediaPlayer *player, gint index) {
    if (!player || !player->playbin) return NULL;
    
    gint n_audio = player_get_audio_stream_count(player);
    if (index < 0 || index >= n_audio) return NULL;
    
    StreamInfo *info = g_new0(StreamInfo, 1);
    info->index = index;
    
    /* Get tags for this audio stream */
    GstTagList *tags = NULL;
    g_signal_emit_by_name(player->playbin, "get-audio-tags", index, &tags);
    
    if (tags) {
        info->language = get_tag_string(tags, GST_TAG_LANGUAGE_CODE);
        info->codec = get_tag_string(tags, GST_TAG_AUDIO_CODEC);
        info->title = get_tag_string(tags, GST_TAG_TITLE);
        
        /* If no title, try language name */
        if (!info->title && info->language) {
            info->title = g_strdup(info->language);
        }
        
        gst_tag_list_unref(tags);
    }
    
    /* Default title if none found */
    if (!info->title) {
        info->title = g_strdup_printf("Audio Track %d", index + 1);
    }
    
    return info;
}

StreamInfo* player_get_subtitle_stream_info(MediaPlayer *player, gint index) {
    if (!player || !player->playbin) return NULL;
    
    gint n_text = player_get_subtitle_stream_count(player);
    if (index < 0 || index >= n_text) return NULL;
    
    StreamInfo *info = g_new0(StreamInfo, 1);
    info->index = index;
    
    /* Get tags for this subtitle stream */
    GstTagList *tags = NULL;
    g_signal_emit_by_name(player->playbin, "get-text-tags", index, &tags);
    
    if (tags) {
        info->language = get_tag_string(tags, GST_TAG_LANGUAGE_CODE);
        info->codec = get_tag_string(tags, GST_TAG_SUBTITLE_CODEC);
        info->title = get_tag_string(tags, GST_TAG_TITLE);
        
        /* Try LANGUAGE_NAME if available (more human-readable than code) */
        if (!info->title) {
            info->title = get_tag_string(tags, GST_TAG_LANGUAGE_NAME);
        }
        
        /* Debug: print all available tags for this stream */
        g_debug("Subtitle stream %d tags:", index);
        gst_tag_list_foreach(tags, (GstTagForeachFunc)debug_print_tag, NULL);
        
        gst_tag_list_unref(tags);
    }
    
    /* Default title if none found */
    if (!info->title) {
        info->title = g_strdup_printf("Subtitle Track %d", index + 1);
    }
    
    return info;
}

void player_free_stream_info(StreamInfo *info) {
    if (!info) return;
    g_free(info->language);
    g_free(info->codec);
    g_free(info->title);
    g_free(info);
}
