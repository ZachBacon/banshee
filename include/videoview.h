#ifndef VIDEOVIEW_H
#define VIDEOVIEW_H

#include <gtk/gtk.h>
#include "database.h"
#include "player.h"

typedef struct {
    GtkWidget *main_container;      /* Main container for video view */
    GtkWidget *video_list_box;      /* Container for video list */
    GtkWidget *video_overlay_box;   /* Container for video overlay */
    GtkWidget *video_listview;      /* Tree view for video list */
    GtkListStore *video_store;      /* Store for video list */
    GtkWidget *video_widget;        /* Drawing area for video */
    GtkWidget *video_controls;      /* Video controls overlay */
    GtkWidget *overlay_container;   /* The UI overlay container to add video widget to */
    GtkWidget *content_stack;       /* Reference to content stack for switching views */
    Database *database;
    MediaPlayer *player;
    gboolean video_playing;         /* Whether video is currently playing */
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
gboolean video_view_is_showing_video(VideoView *view);

/* Selection callback */
typedef void (*VideoSelectedCallback)(gint video_id, const gchar *file_path, gpointer user_data);
void video_view_set_selection_callback(VideoView *view, VideoSelectedCallback callback, gpointer user_data);

#endif /* VIDEOVIEW_H */
