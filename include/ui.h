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

typedef struct {
    GtkWidget *window;
    GtkWidget *main_box;
    
    /* Menu and toolbar */
    GtkWidget *menubar;
    GtkWidget *toolbar;
    
    /* Layout panes */
    GtkWidget *main_paned;      /* Sidebar | Content */
    GtkWidget *content_paned;   /* Browsers | Track list */
    
    /* Sidebar with sources */
    GtkWidget *sidebar;
    GtkWidget *sidebar_treeview;
    SourceManager *source_manager;
    
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
    
    /* Main content area */
    GtkWidget *content_area;
    GtkWidget *track_listview;
    GtkListStore *track_store;
    
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
    
    /* Signal handlers */
    gulong track_selection_handler_id;
    gulong seek_handler_id;
} MediaPlayerUI;

/* UI initialization */
MediaPlayerUI* ui_new(MediaPlayer *player, Database *database);
void ui_free(MediaPlayerUI *ui);
void ui_show(MediaPlayerUI *ui);

/* UI updates */
void ui_update_track_list(MediaPlayerUI *ui, GList *tracks);
void ui_update_now_playing(MediaPlayerUI *ui);
void ui_update_now_playing_podcast(MediaPlayerUI *ui, const gchar *podcast_title, const gchar *episode_title, const gchar *image_url);
void ui_update_position(MediaPlayerUI *ui, gint64 position, gint64 duration);

/* Callbacks */
void ui_on_play_clicked(GtkWidget *widget, gpointer user_data);
void ui_on_pause_clicked(GtkWidget *widget, gpointer user_data);
void ui_on_stop_clicked(GtkWidget *widget, gpointer user_data);
void ui_on_prev_clicked(GtkWidget *widget, gpointer user_data);
void ui_on_next_clicked(GtkWidget *widget, gpointer user_data);
void ui_on_track_selected(GtkTreeSelection *selection, gpointer user_data);

/* Preferences dialog */
void ui_show_preferences_dialog(MediaPlayerUI *ui);

#endif /* UI_H */
