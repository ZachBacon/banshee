#ifndef VIDEOVIEW_H
#define VIDEOVIEW_H

#include <gtk/gtk.h>
#include "database.h"
#include "player.h"
#include "models.h"

typedef struct {
    GtkWidget *main_container;      /* Main container for video view */
    GtkWidget *video_list_box;      /* Container for video list */
    GtkWidget *video_overlay_box;   /* Container for video overlay */
    GtkWidget *video_columnview;    /* GtkColumnView for video list */
    GListStore *video_store;        /* GListStore for video list */
    GtkSingleSelection *video_selection; /* Selection model for video list */
    gulong video_selection_handler_id; /* Handler ID for selection changes */
    GtkWidget *video_widget;        /* Drawing area for video */
    GtkWidget *video_controls;      /* Video controls overlay */
    GtkWidget *controls_revealer;   /* Revealer for showing/hiding controls */
    GtkWidget *audio_menu_button;   /* Button for audio stream selection */
    GtkWidget *subtitle_menu_button; /* Button for subtitle selection */
    GtkWidget *back_button;         /* Button to go back to video list */
    GtkWidget *video_title_label;   /* Label showing current video title */
    GtkWidget *time_label;          /* Label showing current position / duration */
    GtkWidget *overlay_container;   /* The UI overlay container to add video widget to */
    GtkWidget *content_stack;       /* Reference to content stack for switching views */
    Database *database;
    MediaPlayer *player;
    gboolean video_playing;         /* Whether video is currently playing */
    gboolean controls_visible;      /* Whether controls are visible */
    guint controls_timeout_id;      /* Timeout for hiding controls */
    guint position_timeout_id;      /* Timeout for updating position label */
    GtkWidget *scrolled_window;     /* Scrolled window for list */
} VideoView;

/* Video list columns */
enum {
    VIDEO_COL_ID = 0,
    VIDEO_COL_TITLE,
    VIDEO_COL_ARTIST,
    VIDEO_COL_DURATION,
    VIDEO_COL_FILE_PATH,
    VIDEO_COL_COUNT
};

/* Video view functions */
VideoView* video_view_new(Database *database, MediaPlayer *player);
void video_view_free(VideoView *view);
GtkWidget* video_view_get_widget(VideoView *view);

/* Update video view */
void video_view_load_videos(VideoView *view);
void video_view_clear(VideoView *view);

/* Video playback */
void video_view_set_overlay_container(VideoView *view, GtkWidget *overlay_container);
void video_view_set_content_stack(VideoView *view, GtkWidget *content_stack);
void video_view_show_video(VideoView *view);
void video_view_hide_video(VideoView *view);
void video_view_hide_video_ui(VideoView *view);  /* Hide UI only, don't stop playback */
gboolean video_view_is_showing_video(VideoView *view);

/* Selection callback */
typedef void (*VideoSelectedCallback)(gint video_id, const gchar *file_path, gpointer user_data);
void video_view_set_selection_callback(VideoView *view, VideoSelectedCallback callback, gpointer user_data);

#endif /* VIDEOVIEW_H */
