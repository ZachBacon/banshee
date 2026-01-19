#include "source.h"
#include "radio.h"
#include "smartplaylist.h"
#include <string.h>

Source* source_new(const gchar *name, SourceType type) {
    Source *source = g_new0(Source, 1);
    source->name = g_strdup(name);
    source->type = type;
    source->media_types = MEDIA_TYPE_AUDIO;
    source->children = NULL;
    source->parent = NULL;
    source->icon_name = NULL;
    source->count = 0;
    return source;
}

void source_free(Source *source) {
    if (!source) return;
    
    g_free(source->name);
    g_free(source->icon_name);
    
    for (GList *l = source->children; l != NULL; l = l->next) {
        source_free((Source *)l->data);
    }
    g_list_free(source->children);
    
    g_free(source);
}

void source_add_child(Source *parent, Source *child) {
    if (!parent || !child) return;
    
    child->parent = parent;
    parent->children = g_list_append(parent->children, child);
}

SourceManager* source_manager_new(Database *db) {
    SourceManager *manager = g_new0(SourceManager, 1);
    manager->db = db;
    manager->sources = NULL;
    manager->tree_store = gtk_tree_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_POINTER);
    return manager;
}

void source_manager_free(SourceManager *manager) {
    if (!manager) return;
    
    for (GList *l = manager->sources; l != NULL; l = l->next) {
        source_free((Source *)l->data);
    }
    g_list_free(manager->sources);
    
    if (manager->tree_store) {
        g_object_unref(manager->tree_store);
    }
    
    g_free(manager);
}

static void add_source_to_store(GtkTreeStore *store, GtkTreeIter *parent_iter, Source *source) {
    GtkTreeIter iter;
    gtk_tree_store_append(store, &iter, parent_iter);
    gtk_tree_store_set(store, &iter,
                      0, source->name,
                      1, source->icon_name,
                      2, source->count,
                      3, source,
                      -1);
    
    for (GList *l = source->children; l != NULL; l = l->next) {
        add_source_to_store(store, &iter, (Source *)l->data);
    }
}

void source_manager_populate(SourceManager *manager) {
    if (!manager) return;
    
    gtk_tree_store_clear(manager->tree_store);
    
    for (GList *l = manager->sources; l != NULL; l = l->next) {
        add_source_to_store(manager->tree_store, NULL, (Source *)l->data);
    }
}

Source* source_create_music_library(Database *db) {
    Source *music = source_new("Music", SOURCE_TYPE_LIBRARY);
    music->icon_name = g_strdup("audio-x-generic");
    music->media_types = MEDIA_TYPE_AUDIO;
    
    if (db) {
        GList *tracks = database_get_all_tracks(db);
        music->count = g_list_length(tracks);
        g_list_free_full(tracks, (GDestroyNotify)database_free_track);
    }
    
    return music;
}

Source* source_create_video_library(Database *db) {
    Source *video = source_new("Videos", SOURCE_TYPE_LIBRARY);
    video->icon_name = g_strdup("video-x-generic");
    video->media_types = MEDIA_TYPE_VIDEO;
    video->count = 0; /* Would need video support in database */
    
    return video;
}

Source* source_create_radio(Database *db) {
    Source *radio = source_new("Radio", SOURCE_TYPE_RADIO);
    radio->icon_name = g_strdup("radio");
    radio->media_types = MEDIA_TYPE_AUDIO;
    
    if (db) {
        GList *stations = radio_station_get_all(db);
        radio->count = g_list_length(stations);
        g_list_free_full(stations, (GDestroyNotify)radio_station_free);
    }
    
    return radio;
}

Source* source_create_podcast(Database *db) {
    Source *podcast = source_new("Podcasts", SOURCE_TYPE_PODCAST);
    podcast->icon_name = g_strdup("podcast");
    podcast->media_types = MEDIA_TYPE_AUDIO | MEDIA_TYPE_VIDEO;
    podcast->count = 0; /* Would need podcast support */
    
    return podcast;
}

Source* source_create_audiobook(Database *db) {
    Source *audiobook = source_new("Audiobooks", SOURCE_TYPE_AUDIOBOOK);
    audiobook->icon_name = g_strdup("book");
    audiobook->media_types = MEDIA_TYPE_AUDIO;
    audiobook->count = 0;
    
    return audiobook;
}

void source_manager_add_source(SourceManager *manager, Source *source) {
    if (!manager || !source) return;
    
    manager->sources = g_list_append(manager->sources, source);
    source_manager_populate(manager);
}

void source_manager_remove_source(SourceManager *manager, Source *source) {
    if (!manager || !source) return;
    
    manager->sources = g_list_remove(manager->sources, source);
    source_free(source);
    source_manager_populate(manager);
}

Source* source_manager_get_active(SourceManager *manager) {
    if (!manager) return NULL;
    return manager->active_source;
}

void source_manager_set_active(SourceManager *manager, Source *source) {
    if (!manager) return;
    manager->active_source = source;
}

GList* source_manager_get_all(SourceManager *manager) {
    if (!manager) return NULL;
    return manager->sources;
}

void source_manager_add_default_sources(SourceManager *manager) {
    if (!manager) return;
    
    /* Music Library */
    Source *music = source_create_music_library(manager->db);
    source_manager_add_source(manager, music);
    
    /* Smart Playlists */
    Source *smart_playlists = source_new("Smart Playlists", SOURCE_TYPE_SMART_PLAYLIST);
    smart_playlists->icon_name = g_strdup("playlist");
    
    GList *smart_lists = smartplaylist_get_all_from_db(manager->db);
    for (GList *l = smart_lists; l != NULL; l = l->next) {
        SmartPlaylist *sp = (SmartPlaylist *)l->data;
        Source *sp_source = source_new(sp->name, SOURCE_TYPE_SMART_PLAYLIST);
        sp_source->icon_name = g_strdup("playlist");
        sp_source->user_data = sp;
        source_add_child(smart_playlists, sp_source);
    }
    
    source_manager_add_source(manager, smart_playlists);
    
    /* Regular Playlists */
    GList *playlists = database_get_all_playlists(manager->db);
    for (GList *l = playlists; l != NULL; l = l->next) {
        Playlist *pl = (Playlist *)l->data;
        Source *pl_source = source_new(pl->name, SOURCE_TYPE_PLAYLIST);
        pl_source->icon_name = g_strdup("playlist");
        pl_source->playlist_id = pl->id;
        source_manager_add_source(manager, pl_source);
        database_free_playlist(pl);
    }
    g_list_free(playlists);
    
    /* Radio */
    Source *radio = source_create_radio(manager->db);
    source_manager_add_source(manager, radio);
    
    /* Podcasts */
    Source *podcasts = source_create_podcast(manager->db);
    source_manager_add_source(manager, podcasts);
    
    /* Audiobooks */
    Source *audiobooks = source_create_audiobook(manager->db);
    source_manager_add_source(manager, audiobooks);
    
    /* Videos */
    Source *videos = source_create_video_library(manager->db);
    source_manager_add_source(manager, videos);
    
    manager->active_source = music;
}

GtkWidget* source_manager_create_sidebar(SourceManager *manager) {
    if (!manager) return NULL;
    
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
    
    GtkWidget *tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(manager->tree_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), FALSE);
    
    GtkCellRenderer *icon_renderer = gtk_cell_renderer_pixbuf_new();
    GtkCellRenderer *text_renderer = gtk_cell_renderer_text_new();
    
    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start(column, icon_renderer, FALSE);
    gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
    
    gtk_tree_view_column_add_attribute(column, icon_renderer, "icon-name", 1);
    gtk_tree_view_column_add_attribute(column, text_renderer, "text", 0);
    
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), tree_view);
    
    return scrolled;
}

Source* source_get_by_iter(SourceManager *manager, GtkTreeIter *iter) {
    if (!manager || !iter) return NULL;
    
    Source *source;
    gtk_tree_model_get(GTK_TREE_MODEL(manager->tree_store), iter, 3, &source, -1);
    return source;
}

void source_update_count(Source *source, gint count) {
    if (!source) return;
    source->count = count;
}
