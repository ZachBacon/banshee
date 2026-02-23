#ifndef PODCASTVIEW_H
#define PODCASTVIEW_H

#include <gtk/gtk.h>
#include "podcast.h"
#include "database.h"
#include "chapterview.h"
#include "transcriptview.h"
#include "models.h"

/* Callback for episode playback */
typedef void (*EpisodePlayCallback)(gpointer user_data, const gchar *uri, const gchar *title, GList *chapters, const gchar *transcript_url, const gchar *transcript_type, GList *funding);

/* Callback for seeking during playback */
typedef void (*SeekCallback)(gpointer user_data, gdouble time_seconds);

typedef struct {
    GtkWidget *container;
    GtkWidget *paned;
    
    /* Podcast list (left side) - GTK4 GListStore/GtkColumnView */
    GtkWidget *podcast_listview;
    GListStore *podcast_store;
    GtkSingleSelection *podcast_selection;
    gulong podcast_selection_handler_id;
    
    /* Episode list (right side) - GTK4 GListStore/GtkColumnView */
    GtkWidget *episode_listview;
    GListStore *episode_store;
    GtkSingleSelection *episode_selection;
    gulong episode_selection_handler_id;
    
    /* Toolbar buttons */
    GtkWidget *add_button;
    GtkWidget *remove_button;
    GtkWidget *refresh_button;
    GtkWidget *download_button;
    GtkWidget *cancel_button;
    
    /* Download progress tracking */
    GtkWidget *progress_bar;
    GtkWidget *progress_label;
    GtkWidget *progress_box;
    GHashTable *download_progress;  /* episode_id -> gdouble (progress) */
    gint current_download_id;
    
    /* Episode-specific buttons */
    GtkWidget *chapters_button;
    GtkWidget *transcript_button;
    GtkWidget *support_button;
    GtkWidget *value_button;  /* Value 4 Value (Lightning) button */
    
    /* Live indicator */
    GtkWidget *live_indicator;      /* Label showing "🔴 LIVE" when podcast is live */
    GtkWidget *live_button;         /* Button to view/play live stream */
    GList *current_live_items;      /* Current live items for selected podcast */
    
    /* Episode data */
    ChapterView *chapter_view;
    GtkWidget *chapter_popover;
    GList *current_chapters;
    
    gchar *current_transcript_url;
    gchar *current_transcript_type;
    GList *current_funding;
    GList *current_value;     /* Current episode/podcast Value 4 Value info */
    
    GtkWidget *transcript_popover;
    GtkWidget *funding_popover;
    GtkWidget *value_popover; /* Value 4 Value popover */
    
    /* Backend references */
    PodcastManager *podcast_manager;
    Database *database;
    
    /* Playback callback */
    EpisodePlayCallback play_callback;
    gpointer play_callback_data;
    
    /* Seek callback */
    SeekCallback seek_callback;
    gpointer seek_callback_data;
    
    gint selected_podcast_id;
    
    /* Flags for async safety */
    gboolean destroyed;  /* Set when view is being freed; checked by async callbacks */
} PodcastView;

void podcast_view_set_play_callback(PodcastView *view, EpisodePlayCallback callback, gpointer user_data);
void podcast_view_set_seek_callback(PodcastView *view, SeekCallback callback, gpointer user_data);

/* Podcast view lifecycle */
PodcastView* podcast_view_new(PodcastManager *manager, Database *database);
void podcast_view_free(PodcastView *view);
GtkWidget* podcast_view_get_widget(PodcastView *view);

/* Podcast operations */
void podcast_view_add_subscription(PodcastView *view);
void podcast_view_refresh_podcasts(PodcastView *view);
void podcast_view_play_episode(PodcastView *view, gint episode_id);
void podcast_view_refresh_episodes(PodcastView *view, gint podcast_id);
void podcast_view_download_episode(PodcastView *view, gint episode_id);
void podcast_view_filter(PodcastView *view, const gchar *search_text);

/* Podcast information */
Podcast* podcast_view_get_selected_podcast(PodcastView *view);

/* Episode-specific features */
void podcast_view_update_episode_features(PodcastView *view, GList *chapters, const gchar *transcript_url, const gchar *transcript_type, GList *funding);

#endif /* PODCASTVIEW_H */
