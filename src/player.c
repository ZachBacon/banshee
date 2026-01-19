#include "player.h"
#include <gst/gst.h>
#include <string.h>

/* Callback data for gtksink widget notification */
typedef struct {
    MediaPlayer *player;
    GCallback widget_ready_callback;
    gpointer user_data;
} GtkSinkCallbackData;

/* Flag to track if we've already printed framerate info */
static gboolean framerate_detected = FALSE;

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

static void on_gtksink_widget_ready(GObject *sink, GParamSpec *pspec, gpointer user_data) {
    (void)pspec;
    GtkSinkCallbackData *data = (GtkSinkCallbackData *)user_data;
    
    GtkWidget *widget = NULL;
    g_object_get(sink, "widget", &widget, NULL);
    
    if (widget && data->widget_ready_callback) {
        g_print("Player: gtksink widget is ready\n");
        /* Call the user's callback */
        ((void (*)(GtkWidget *, gpointer))data->widget_ready_callback)(widget, data->user_data);
    }
}

typedef struct {
    PlayerStateCallback callback;
    gpointer user_data;
} CallbackData;

typedef struct {
    PlayerPositionCallback callback;
    gpointer user_data;
} PositionCallbackData;

static PositionCallbackData *position_callback_data = NULL;
static guint position_timer_id = 0;

/* Timer callback for position updates */
static gboolean position_timer_callback(gpointer user_data) {
    MediaPlayer *player = (MediaPlayer *)user_data;
    
    if (player->state == PLAYER_STATE_PLAYING && position_callback_data && position_callback_data->callback) {
        gint64 pos, dur;
        if (gst_element_query_position(player->playbin, GST_FORMAT_TIME, &pos) &&
            gst_element_query_duration(player->playbin, GST_FORMAT_TIME, &dur)) {
            position_callback_data->callback(player, pos, dur, position_callback_data->user_data);
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

#if 0
/* Original code commented out for testing */
static gboolean bus_callback_DISABLED(GstBus *bus, GstMessage *msg, gpointer data) {
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
            break;
        case GST_MESSAGE_BUFFERING: {
            gint percent = 0;
            gst_message_parse_buffering(msg, &percent);
            
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
        case GST_MESSAGE_ASYNC_DONE: {
            /* Pipeline is set up - emit position update if callback is set */
            if (position_callback_data && position_callback_data->callback) {
                gint64 pos, dur;
                if (gst_element_query_position(player->playbin, GST_FORMAT_TIME, &pos) &&
                    gst_element_query_duration(player->playbin, GST_FORMAT_TIME, &dur)) {
                    position_callback_data->callback(player, pos, dur, position_callback_data->user_data);
                }
            }
            
            /* Pipeline is set up and caps are negotiated - now we can detect framerate */
            if (!framerate_detected && GST_MESSAGE_SRC(msg) == GST_OBJECT(player->playbin)) {
                gint video_stream = -1;
                g_object_get(player->playbin, "current-video", &video_stream, NULL);
                if (video_stream >= 0) {
                    GstPad *video_pad = NULL;
                    g_signal_emit_by_name(player->playbin, "get-video-pad", video_stream, &video_pad);
                    if (video_pad) {
                        GstCaps *caps = gst_pad_get_current_caps(video_pad);
                        if (caps) {
                            GstStructure *structure = gst_caps_get_structure(caps, 0);
                            gint fps_num = 0, fps_denom = 1;
                            if (gst_structure_get_fraction(structure, "framerate", &fps_num, &fps_denom) && fps_denom > 0 && fps_num > 0) {
                                gdouble fps = (gdouble)fps_num / fps_denom;
                                g_print("Video: Native framerate detected: %.2f fps (%d/%d)\n", 
                                       fps, fps_num, fps_denom);
                                framerate_detected = TRUE;
                            }
                            gst_caps_unref(caps);
                        }
                        gst_object_unref(video_pad);
                    }
                }
            }
            break;
        }
        case GST_MESSAGE_ELEMENT: {
            /* Handle prepare-window-handle message for video overlay */
            if (gst_is_video_overlay_prepare_window_handle_message(msg)) {
                guintptr window_handle = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(player->playbin), "window-handle"));
                if (window_handle != 0) {
                    GstVideoOverlay *overlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(msg));
                    gst_video_overlay_set_window_handle(overlay, window_handle);
                    g_print("Set window handle via prepare-window-handle message: %lu\n", (unsigned long)window_handle);
                }
            }
            break;
        }
        case GST_MESSAGE_QOS: {
            /* Quality of Service message - disabled to avoid console I/O performance impact */
            /* Uncomment for debugging video performance issues */
            /*
            gboolean live;
            guint64 running_time, stream_time, timestamp, duration;
            gst_message_parse_qos(msg, &live, &running_time, &stream_time, &timestamp, &duration);
            
            GstFormat format;
            guint64 processed, dropped;
            gst_message_parse_qos_stats(msg, &format, &processed, &dropped);
            
            gint64 jitter;
            gdouble proportion;
            gint quality;
            gst_message_parse_qos_values(msg, &jitter, &proportion, &quality);
            
            if (dropped > 0) {
                g_print("QOS: Dropped %lu frames, processed %lu frames (%.1f%% dropped)\n",
                       (unsigned long)dropped, (unsigned long)processed,
                       (100.0 * dropped) / (processed + dropped));
            }
            
            if (jitter > 20000000) {
                g_print("QOS: High jitter detected: %.2f ms\n", jitter / 1000000.0);
            }
            
            if (proportion < 1.0) {
                g_print("QOS: System overload detected, proportion: %.2f (need to drop frames)\n", proportion);
            }
            */
            
            break;
        }
        default:
            break;
    }
    
    return TRUE;
}
#endif

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
        g_print("Configured dav1ddec with %d threads for smooth AV1 playback\n", n_threads);
    }
    /* Configure other video decoders */
    else if (g_str_has_prefix(name, "avdec_")) {
        g_object_set(element, "max-threads", g_get_num_processors(), NULL);
    }
    
    g_free(name);
}

/* Thread function to run GStreamer's bus watch in separate context */
static gpointer bus_thread_func(gpointer data) {
    MediaPlayer *player = (MediaPlayer *)data;
    g_main_context_push_thread_default(player->bus_context);
    g_main_loop_run(player->bus_loop);
    g_main_context_pop_thread_default(player->bus_context);
    return NULL;
}

/* Position update timer running in GStreamer context */
static gboolean gst_position_update(gpointer data) {
    MediaPlayer *player = (MediaPlayer *)data;
    
    if (player->state == PLAYER_STATE_PLAYING && position_callback_data && position_callback_data->callback) {
        gint64 pos, dur;
        if (gst_element_query_position(player->playbin, GST_FORMAT_TIME, &pos) &&
            gst_element_query_duration(player->playbin, GST_FORMAT_TIME, &dur)) {
            /* Emit to main thread via idle callback */
            g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                           (GSourceFunc)position_callback_data->callback,
                           player, NULL);
        }
    }
    
    return G_SOURCE_CONTINUE;
}

static GstBusSyncReply bus_sync_handler(GstBus *bus, GstMessage *msg, gpointer data) {
    MediaPlayer *player = (MediaPlayer *)data;
    (void)bus;
    
    /* Handle prepare-window-handle message synchronously */
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ELEMENT) {
        if (gst_is_video_overlay_prepare_window_handle_message(msg)) {
            guintptr window_handle = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(player->playbin), "window-handle"));
            if (window_handle != 0) {
                GstVideoOverlay *overlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(msg));
                gst_video_overlay_set_window_handle(overlay, window_handle);
                g_print("Set window handle via sync handler: %lu\n", (unsigned long)window_handle);
            }
            gst_message_unref(msg);
            return GST_BUS_DROP;
        }
    }
    
    return GST_BUS_PASS;
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
     * GST_PLAY_FLAG_VIDEO (1) | GST_PLAY_FLAG_AUDIO (2) | GST_PLAY_FLAG_TEXT (4) 
     * Also add SOFT_VOLUME (16) for volume control */
    gint flags;
    g_object_get(player->playbin, "flags", &flags, NULL);
    g_print("Player: Initial playbin flags: 0x%x\n", flags);
    flags |= (1 << 0) | (1 << 1) | (1 << 4);  /* VIDEO | AUDIO | SOFT_VOLUME */
    g_object_set(player->playbin, "flags", flags, NULL);
    g_print("Player: Playbin flags set to 0x%x (video + audio enabled)\n", flags);
    
    /* Explicitly set volume to ensure audio is not muted */
    g_object_set(player->playbin, "volume", 1.0, NULL);
    g_object_set(player->playbin, "mute", FALSE, NULL);
    
    /* Create gtksink for embedded video playback in GTK widget */
    player->video_sink = gst_element_factory_make("gtksink", "videosink");
    if (!player->video_sink) {
        /* Try gtkglsink as fallback for hardware acceleration */
        player->video_sink = gst_element_factory_make("gtkglsink", "videosink");
    }
    if (player->video_sink) {
        /* Set gtksink as the video sink for playbin - this embeds video in GTK */
        g_object_set(player->playbin, "video-sink", player->video_sink, NULL);
        g_print("Player: Using gtksink for embedded video playback\n");
    } else {
        g_print("Player: gtksink not available, video will use default sink\n");
    }
    
    /* Create and set an explicit audio sink to ensure audio works with video */
    GstElement *audio_sink = gst_element_factory_make("autoaudiosink", "audiosink");
    if (audio_sink) {
        g_object_set(player->playbin, "audio-sink", audio_sink, NULL);
        g_print("Player: Using autoaudiosink for audio playback\n");
    } else {
        g_print("Player: autoaudiosink not available, using default audio sink\n");
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
    
    /* Stop bus thread */
    if (player->bus_loop) {
        g_main_loop_quit(player->bus_loop);
    }
    if (player->bus_thread) {
        g_thread_join(player->bus_thread);
    }
    if (player->bus_loop) {
        g_main_loop_unref(player->bus_loop);
    }
    if (player->bus_context) {
        g_main_context_unref(player->bus_context);
    }
    
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
    g_print("Player: Setting URI: %s\n", full_uri);
    
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
    
    /* Debug: Print stream information */
    gint n_video = 0, n_audio = 0, n_text = 0;
    g_object_get(player->playbin, "n-video", &n_video, "n-audio", &n_audio, "n-text", &n_text, NULL);
    g_print("Player: Stream info - Video streams: %d, Audio streams: %d, Text streams: %d\n", 
            n_video, n_audio, n_text);
    
    gint current_audio = -1, current_video = -1;
    g_object_get(player->playbin, "current-audio", &current_audio, "current-video", &current_video, NULL);
    g_print("Player: Current streams - Video: %d, Audio: %d\n", current_video, current_audio);
    
    /* Debug: Check flags again */
    gint flags = 0;
    g_object_get(player->playbin, "flags", &flags, NULL);
    g_print("Player: Current flags: 0x%x (audio bit=%d, video bit=%d)\n", 
            flags, (flags >> 1) & 1, flags & 1);
    
    /* Debug: Check volume and mute state */
    gdouble vol = 0;
    gboolean mute = FALSE;
    g_object_get(player->playbin, "volume", &vol, "mute", &mute, NULL);
    g_print("Player: Volume: %.2f, Mute: %s\n", vol, mute ? "YES" : "NO");
    
    /* If there's audio but it's not selected, select it */
    if (n_audio > 0 && current_audio < 0) {
        g_print("Player: No audio stream selected, selecting stream 0\n");
        g_object_set(player->playbin, "current-audio", 0, NULL);
    }
    
    /* Start position timer for UI updates
     * Use 250ms interval to reduce UI update overhead during video playback.
     * This prevents the progress bar updates from causing choppy video. */
    if (position_callback_data && position_timer_id == 0) {
        gboolean is_video = is_current_file_video(player);
        guint interval = is_video ? 500 : 250;  /* Slower updates for video */
        position_timer_id = g_timeout_add(interval, position_timer_callback, player);
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
    if (position_timer_id != 0) {
        g_source_remove(position_timer_id);
        position_timer_id = 0;
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
    if (position_timer_id != 0) {
        g_source_remove(position_timer_id);
        position_timer_id = 0;
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
    (void)player;  /* Unused */
    
    if (position_callback_data) {
        g_free(position_callback_data);
    }
    
    if (callback) {
        position_callback_data = g_new0(PositionCallbackData, 1);
        position_callback_data->callback = callback;
        position_callback_data->user_data = user_data;
    } else {
        position_callback_data = NULL;
    }
}

/* Video support functions */
void player_set_video_window(MediaPlayer *player, guintptr window_handle) {
    if (!player || !player->playbin) return;
    
    g_print("Setting video window handle: %lu\n", (unsigned long)window_handle);
    
    /* Set the window handle on the playbin's video overlay */
    /* First try to set it directly on playbin */
    if (GST_IS_VIDEO_OVERLAY(player->playbin)) {
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(player->playbin), window_handle);
        g_print("Set window handle on playbin directly\n");
        return;
    }
    
    /* If that doesn't work, try to get the video sink and set it there */
    GstElement *video_sink = NULL;
    g_object_get(player->playbin, "video-sink", &video_sink, NULL);
    
    if (video_sink) {
        if (GST_IS_VIDEO_OVERLAY(video_sink)) {
            gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(video_sink), window_handle);
            g_print("Set window handle on video sink\n");
        }
        gst_object_unref(video_sink);
    } else {
        /* If no video sink is set yet, we need to handle it through a bus message */
        /* This will be caught when the pipeline is ready */
        g_print("No video sink yet, storing window handle for later\n");
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
    
    /* Get the widget from our stored gtksink reference */
    GtkWidget *widget = NULL;
    g_object_get(player->video_sink, "widget", &widget, NULL);
    
    return widget;
}

void player_set_video_widget_ready_callback(MediaPlayer *player, GCallback callback, gpointer user_data) {
    if (!player || !player->video_sink) return;
    
    /* Check if it's gtksink or gtkglsink */
    const gchar *factory_name = gst_plugin_feature_get_name(
        GST_PLUGIN_FEATURE(gst_element_get_factory(player->video_sink)));
    
    if (g_strcmp0(factory_name, "gtksink") == 0 || 
        g_strcmp0(factory_name, "gtkglsink") == 0) {
        
        /* Check if widget is already available */
        GtkWidget *widget = NULL;
        g_object_get(player->video_sink, "widget", &widget, NULL);
        
        if (widget) {
            /* Widget already exists, call callback immediately */
            g_print("Player: gtksink widget already available\n");
            ((void (*)(GtkWidget *, gpointer))callback)(widget, user_data);
        } else {
            /* Widget not ready yet, connect to notify signal */
            g_print("Player: Waiting for gtksink widget to be created...\n");
            
            GtkSinkCallbackData *cb_data = g_new0(GtkSinkCallbackData, 1);
            cb_data->player = player;
            cb_data->widget_ready_callback = callback;
            cb_data->user_data = user_data;
            
            g_signal_connect_data(player->video_sink, "notify::widget",
                                G_CALLBACK(on_gtksink_widget_ready),
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
    g_print("Player: Set audio stream to %d\n", index);
}

void player_set_subtitle_stream(MediaPlayer *player, gint index) {
    if (!player || !player->playbin) return;
    g_object_set(player->playbin, "current-text", index, NULL);
    
    /* Enable subtitles when selecting a stream */
    gint flags;
    g_object_get(player->playbin, "flags", &flags, NULL);
    flags |= (1 << 2);  /* GST_PLAY_FLAG_TEXT */
    g_object_set(player->playbin, "flags", flags, NULL);
    
    g_print("Player: Set subtitle stream to %d\n", index);
}

void player_set_subtitles_enabled(MediaPlayer *player, gboolean enabled) {
    if (!player || !player->playbin) return;
    
    gint flags;
    g_object_get(player->playbin, "flags", &flags, NULL);
    
    if (enabled) {
        flags |= (1 << 2);  /* GST_PLAY_FLAG_TEXT */
    } else {
        flags &= ~(1 << 2);
    }
    
    g_object_set(player->playbin, "flags", flags, NULL);
    g_print("Player: Subtitles %s\n", enabled ? "enabled" : "disabled");
}

gboolean player_get_subtitles_enabled(MediaPlayer *player) {
    if (!player || !player->playbin) return FALSE;
    
    gint flags;
    g_object_get(player->playbin, "flags", &flags, NULL);
    return (flags & (1 << 2)) != 0;
}

static void debug_print_tag(const GstTagList *list, const gchar *tag, gpointer user_data) {
    (void)user_data;
    gchar *str = NULL;
    if (gst_tag_list_get_string(list, tag, &str)) {
        g_print("  %s: %s\n", tag, str);
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
        g_print("Subtitle stream %d tags:\n", index);
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
