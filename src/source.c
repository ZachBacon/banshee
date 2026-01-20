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

/* Callback for GtkTreeListModel to get children of an item */
static GListModel* get_source_children(gpointer item, gpointer user_data) {
    (void)user_data;
    ShriekSourceObject *source_obj = SHRIEK_SOURCE_OBJECT(item);
    GListModel *children = shriek_source_object_get_children(source_obj);
    
    if (g_list_model_get_n_items(children) == 0) {
        return NULL;  /* No children */
    }
    
    return g_object_ref(children);
}

SourceManager* source_manager_new(Database *db) {
    SourceManager *manager = g_new0(SourceManager, 1);
    manager->db = db;
    manager->sources = NULL;
    manager->source_store = g_list_store_new(SHRIEK_TYPE_SOURCE_OBJECT);
    manager->tree_list_model = NULL;
    manager->selection = NULL;
    return manager;
}

void source_manager_free(SourceManager *manager) {
    if (!manager) return;
    
    for (GList *l = manager->sources; l != NULL; l = l->next) {
        source_free((Source *)l->data);
    }
    g_list_free(manager->sources);
    
    g_clear_object(&manager->source_store);
    g_clear_object(&manager->tree_list_model);
    g_clear_object(&manager->selection);
    
    g_free(manager);
}

/* Recursively add source and its children to ShriekSourceObject */
static ShriekSourceObject* create_source_object_recursive(Source *source) {
    ShriekSourceObject *obj = shriek_source_object_new(
        source->name,
        source->icon_name,
        source
    );
    
    for (GList *l = source->children; l != NULL; l = l->next) {
        Source *child = (Source *)l->data;
        ShriekSourceObject *child_obj = create_source_object_recursive(child);
        shriek_source_object_add_child(obj, child_obj);
        g_object_unref(child_obj);
    }
    
    return obj;
}

void source_manager_populate(SourceManager *manager) {
    if (!manager) return;
    
    g_list_store_remove_all(manager->source_store);
    
    for (GList *l = manager->sources; l != NULL; l = l->next) {
        Source *source = (Source *)l->data;
        ShriekSourceObject *obj = create_source_object_recursive(source);
        g_list_store_append(manager->source_store, obj);
        g_object_unref(obj);
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

/* Factory setup callback for sidebar list items */
static void setup_sidebar_item(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    
    /* Expander for tree rows */
    GtkWidget *expander = gtk_tree_expander_new();
    gtk_tree_expander_set_indent_for_icon(GTK_TREE_EXPANDER(expander), TRUE);
    
    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *icon = gtk_image_new();
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    
    gtk_box_append(GTK_BOX(content_box), icon);
    gtk_box_append(GTK_BOX(content_box), label);
    
    gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), content_box);
    gtk_box_append(GTK_BOX(box), expander);
    
    gtk_list_item_set_child(list_item, box);
}

/* Factory bind callback for sidebar list items */
static void bind_sidebar_item(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    
    GtkWidget *box = gtk_list_item_get_child(list_item);
    GtkWidget *expander = gtk_widget_get_first_child(box);
    GtkWidget *content_box = gtk_tree_expander_get_child(GTK_TREE_EXPANDER(expander));
    GtkWidget *icon = gtk_widget_get_first_child(content_box);
    GtkWidget *label = gtk_widget_get_next_sibling(icon);
    
    GtkTreeListRow *row = gtk_list_item_get_item(list_item);
    gtk_tree_expander_set_list_row(GTK_TREE_EXPANDER(expander), row);
    
    ShriekSourceObject *source_obj = gtk_tree_list_row_get_item(row);
    if (source_obj) {
        gtk_image_set_from_icon_name(GTK_IMAGE(icon), 
            shriek_source_object_get_icon_name(source_obj));
        gtk_label_set_text(GTK_LABEL(label), 
            shriek_source_object_get_name(source_obj));
        g_object_unref(source_obj);
    }
}

GtkWidget* source_manager_create_sidebar(SourceManager *manager) {
    if (!manager) return NULL;
    
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
    
    /* Create tree list model for hierarchical display */
    manager->tree_list_model = gtk_tree_list_model_new(
        G_LIST_MODEL(g_object_ref(manager->source_store)),
        FALSE,  /* passthrough */
        TRUE,   /* autoexpand */
        get_source_children,
        NULL,
        NULL
    );
    
    /* Create selection model */
    manager->selection = gtk_single_selection_new(
        G_LIST_MODEL(g_object_ref(manager->tree_list_model)));
    gtk_single_selection_set_autoselect(manager->selection, FALSE);
    
    /* Create factory for list items */
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_sidebar_item), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_sidebar_item), NULL);
    
    /* Create list view */
    GtkWidget *list_view = gtk_list_view_new(
        GTK_SELECTION_MODEL(g_object_ref(manager->selection)), 
        factory);
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), list_view);
    
    return scrolled;
}

Source* source_get_by_selection(SourceManager *manager) {
    if (!manager || !manager->selection) return NULL;
    
    guint position = gtk_single_selection_get_selected(manager->selection);
    if (position == GTK_INVALID_LIST_POSITION) return NULL;
    
    GtkTreeListRow *row = g_list_model_get_item(
        G_LIST_MODEL(manager->tree_list_model), position);
    if (!row) return NULL;
    
    ShriekSourceObject *source_obj = gtk_tree_list_row_get_item(row);
    g_object_unref(row);
    
    if (!source_obj) return NULL;
    
    Source *source = shriek_source_object_get_source(source_obj);
    g_object_unref(source_obj);
    
    return source;
}

GListStore* source_manager_get_source_store(SourceManager *manager) {
    if (!manager) return NULL;
    return manager->source_store;
}

GtkSelectionModel* source_manager_get_selection(SourceManager *manager) {
    if (!manager) return NULL;
    return GTK_SELECTION_MODEL(manager->selection);
}

void source_update_count(Source *source, gint count) {
    if (!source) return;
    source->count = count;
}
