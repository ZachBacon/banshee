#ifndef SOURCE_H
#define SOURCE_H

#include <gtk/gtk.h>
#include "database.h"

/* Source types */
typedef enum {
    SOURCE_TYPE_LIBRARY,
    SOURCE_TYPE_PLAYLIST,
    SOURCE_TYPE_SMART_PLAYLIST,
    SOURCE_TYPE_RADIO,
    SOURCE_TYPE_PODCAST,
    SOURCE_TYPE_DEVICE,
    SOURCE_TYPE_AUDIOBOOK,
    SOURCE_TYPE_QUEUE
} SourceType;

/* Media types */
typedef enum {
    MEDIA_TYPE_AUDIO = 1 << 0,
    MEDIA_TYPE_VIDEO = 1 << 1,
    MEDIA_TYPE_PODCAST = 1 << 2,
    MEDIA_TYPE_AUDIOBOOK = 1 << 3,
    MEDIA_TYPE_RADIO = 1 << 4
} MediaType;

/* Source structure */
typedef struct _Source {
    gint id;
    gchar *name;
    gchar *icon_name;
    SourceType type;
    MediaType media_types;
    struct _Source *parent;
    GList *children;
    gint count;
    gint64 duration;
    gint playlist_id;
    gboolean expanded;
    gpointer user_data; /* Type-specific data */
} Source;

/* Source manager */
typedef struct {
    GList *sources;
    Source *active_source;
    GtkTreeStore *tree_store;
    Database *db;
} SourceManager;

/* Source functions */
Source* source_new(const gchar *name, SourceType type);
void source_free(Source *source);
void source_add_child(Source *parent, Source *child);
void source_remove_child(Source *parent, Source *child);
void source_update_count(Source *source, gint count);

/* Source manager functions */
SourceManager* source_manager_new(Database *database);
void source_manager_free(SourceManager *manager);
void source_manager_add_source(SourceManager *manager, Source *source);
void source_manager_remove_source(SourceManager *manager, Source *source);
void source_manager_populate(SourceManager *manager);
Source* source_manager_get_active(SourceManager *manager);
void source_manager_set_active(SourceManager *manager, Source *source);
GtkTreeStore* source_manager_get_tree_store(SourceManager *manager);

/* Default sources */
void source_manager_add_default_sources(SourceManager *manager);
Source* source_create_music_library(Database *database);
Source* source_create_video_library(Database *database);
Source* source_create_radio_library(Database *database);
Source* source_create_podcast_library(Database *database);
Source* source_create_play_queue(void);

/* Source queries */
GList* source_get_tracks(Source *source, Database *database);
gint source_get_count(Source *source);
gint64 source_get_duration(Source *source);
Source* source_get_by_iter(SourceManager *manager, GtkTreeIter *iter);

#endif /* SOURCE_H */
