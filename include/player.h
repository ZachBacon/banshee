#ifndef PLAYER_H
#define PLAYER_H

#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <gtk/gtk.h>
#include <glib.h>

typedef enum {
    PLAYER_STATE_NULL,
    PLAYER_STATE_READY,
    PLAYER_STATE_PLAYING,
    PLAYER_STATE_PAUSED,
    PLAYER_STATE_STOPPED
} PlayerState;

typedef struct _PositionCallbackData PositionCallbackData;
typedef struct _EosCallbackData EosCallbackData;

typedef struct {
    GstElement *pipeline;
    GstElement *playbin;
    GstElement *video_sink;  /* Store the video sink for widget extraction */
    GstBus *bus;
    PlayerState state;
    gchar *current_uri;
    gdouble volume;
    gint64 duration;
    gint64 position;
    PositionCallbackData *position_cb_data;
    EosCallbackData *eos_cb_data;
    guint ui_position_timer_id;  /* Timer for UI position updates */
} MediaPlayer;

/* Player initialization and cleanup */
MediaPlayer* player_new(void);
void player_free(MediaPlayer *player);

/* Playback control */
gboolean player_play(MediaPlayer *player);
gboolean player_pause(MediaPlayer *player);
gboolean player_stop(MediaPlayer *player);
gboolean player_set_uri(MediaPlayer *player, const gchar *uri);

/* Volume and seeking */
void player_set_volume(MediaPlayer *player, gdouble volume);
gdouble player_get_volume(MediaPlayer *player);
gboolean player_seek(MediaPlayer *player, gint64 position);

/* Status queries */
gint64 player_get_duration(MediaPlayer *player);
gint64 player_get_position(MediaPlayer *player);
PlayerState player_get_state(MediaPlayer *player);

/* Event handling */
typedef void (*PlayerStateCallback)(MediaPlayer *player, PlayerState state, gpointer user_data);
typedef void (*PlayerPositionCallback)(MediaPlayer *player, gint64 position, gint64 duration, gpointer user_data);
typedef void (*PlayerEosCallback)(MediaPlayer *player, gpointer user_data);
void player_set_state_callback(MediaPlayer *player, PlayerStateCallback callback, gpointer user_data);
void player_set_position_callback(MediaPlayer *player, PlayerPositionCallback callback, gpointer user_data);
void player_set_eos_callback(MediaPlayer *player, PlayerEosCallback callback, gpointer user_data);

/* Video support */
void player_set_video_window(MediaPlayer *player, guintptr window_handle);
gboolean player_has_video(MediaPlayer *player);
GtkWidget* player_get_video_widget(MediaPlayer *player);  /* For gtksink */
void player_set_video_widget_ready_callback(MediaPlayer *player, GCallback callback, gpointer user_data);

/* Stream information and selection */
typedef struct {
    gint index;
    gchar *language;
    gchar *codec;
    gchar *title;
} StreamInfo;

gint player_get_audio_stream_count(MediaPlayer *player);
gint player_get_subtitle_stream_count(MediaPlayer *player);
gint player_get_current_audio_stream(MediaPlayer *player);
gint player_get_current_subtitle_stream(MediaPlayer *player);
void player_set_audio_stream(MediaPlayer *player, gint index);
void player_set_subtitle_stream(MediaPlayer *player, gint index);
void player_set_subtitles_enabled(MediaPlayer *player, gboolean enabled);
gboolean player_get_subtitles_enabled(MediaPlayer *player);
StreamInfo* player_get_audio_stream_info(MediaPlayer *player, gint index);
StreamInfo* player_get_subtitle_stream_info(MediaPlayer *player, gint index);
void player_free_stream_info(StreamInfo *info);

#endif /* PLAYER_H */
