#ifndef UI_H
#define UI_H

#include <gtk/gtk.h>
#include "player.h"
#include "database.h"
#include "source.h"
#include "browser.h"
#include "coverart.h"
#include "albumview.h"
#include "podcast.h"
#include "podcastview.h"
#include "chapterview.h"
#include "transcriptview.h"
#include "videoview.h"
#include "models.h"

/* Repeat mode enumeration */
typedef enum {
    REPEAT_MODE_OFF,
    REPEAT_MODE_SINGLE,
    REPEAT_MODE_PLAYLIST
} RepeatMode;

typedef struct {
    GtkApplication *app;
    GtkWidget *window;
    GtkWidget *main_box;
    
    /* Menu and toolbar */
    GtkWidget *menubar;
    GtkWidget *toolbar;
    
    /* Layout panes */
    GtkWidget *main_paned;      /* Sidebar | Content */
    GtkWidget *content_paned;   /* Browsers | Track list */
    
    /* Sidebar with sources - GTK4 GListStore/GtkListView */
    GtkWidget *sidebar;
    GtkWidget *sidebar_listview;
    SourceManager *source_manager;
    gulong source_selection_handler_id;
    
    /* Browser panels */
    GtkWidget *browser_container;
    BrowserView *artist_browser;
    BrowserView *album_browser;
    BrowserView *genre_browser;
    BrowserModel *artist_model;
    BrowserModel *album_model;
    BrowserModel *genre_model;
    
    /* Album grid view */
    AlbumView *album_view;
    GtkWidget *album_container;
    
    /* Podcast view */
    PodcastView *podcast_view;
    PodcastManager *podcast_manager;
    
    /* Video view */
    VideoView *video_view;
    GtkWidget *video_overlay_container;  /* Overlay container for video playback */
    
    /* Main content area - GTK4 GListStore/GtkColumnView */
    GtkWidget *content_area;
    GtkWidget *track_listview;           /* GtkColumnView widget */
    GListStore *track_store;             /* GListStore of ShriekTrackObject */
    GtkSingleSelection *track_selection; /* Selection model for track list */
    
    /* Cover art */
    CoverArtManager *coverart_manager;
    
    /* Playback controls */
    GtkWidget *control_box;
    GtkWidget *play_button;
    GtkWidget *pause_button;
    GtkWidget *stop_button;
    GtkWidget *prev_button;
    GtkWidget *next_button;
    GtkWidget *volume_scale;
    GtkWidget *seek_scale;
    
    /* Status bar */
    GtkWidget *statusbar;
    GtkWidget *now_playing_label;
    GtkWidget *time_label;
    GtkWidget *search_entry;
    
    /* Header bar cover art */
    GtkWidget *header_cover_art;
    
    /* Backend references */
    MediaPlayer *player;
    Database *database;
    
    /* Current playlist */
    GList *current_playlist;
    gint current_track_index;
    
    /* Shuffle and repeat */
    gboolean shuffle_enabled;
    RepeatMode repeat_mode;
    GtkWidget *shuffle_button;
    GtkWidget *repeat_button;
    
    /* Signal handlers */
    gulong track_selection_handler_id;
    gulong seek_handler_id;
} MediaPlayerUI;

/* UI initialization */
MediaPlayerUI* ui_new(MediaPlayer *player, Database *database, GtkApplication *app);
void ui_free(MediaPlayerUI *ui);
void ui_show(MediaPlayerUI *ui);

/* UI updates */
void ui_update_track_list(MediaPlayerUI *ui, GList *tracks);
void ui_update_now_playing(MediaPlayerUI *ui);
void ui_update_now_playing_podcast(MediaPlayerUI *ui, const gchar *podcast_title, const gchar *episode_title, const gchar *image_url);
void ui_update_now_playing_video(MediaPlayerUI *ui, const gchar *video_title);
void ui_update_position(MediaPlayerUI *ui, gint64 position, gint64 duration);

/* Callbacks */
void ui_on_play_clicked(GtkWidget *widget, gpointer user_data);
void ui_on_pause_clicked(GtkWidget *widget, gpointer user_data);
void ui_on_stop_clicked(GtkWidget *widget, gpointer user_data);
void ui_on_prev_clicked(GtkWidget *widget, gpointer user_data);
void ui_on_next_clicked(GtkWidget *widget, gpointer user_data);
void ui_on_track_selected(GtkSelectionModel *selection, guint position, guint n_items, gpointer user_data);

/* Preferences dialog */
void ui_show_preferences_dialog(MediaPlayerUI *ui);

/* Scan watched directories for new media on startup */
void ui_scan_watched_directories(MediaPlayerUI *ui);

#endif /* UI_H */
