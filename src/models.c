#include "models.h"
#include <string.h>

/* ============================================================================
 * BansheeTrackObject Implementation
 * ============================================================================ */

struct _BansheeTrackObject {
    GObject parent_instance;
    
    gint id;
    gint track_number;
    gchar *title;
    gchar *artist;
    gchar *album;
    gchar *duration_str;
    gint duration_seconds;
    gchar *file_path;
};

enum {
    TRACK_PROP_0,
    TRACK_PROP_ID,
    TRACK_PROP_TRACK_NUMBER,
    TRACK_PROP_TITLE,
    TRACK_PROP_ARTIST,
    TRACK_PROP_ALBUM,
    TRACK_PROP_DURATION_STR,
    TRACK_PROP_DURATION_SECONDS,
    TRACK_PROP_FILE_PATH,
    TRACK_N_PROPERTIES
};

static GParamSpec *track_properties[TRACK_N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE(BansheeTrackObject, banshee_track_object, G_TYPE_OBJECT)

static void banshee_track_object_finalize(GObject *object) {
    BansheeTrackObject *self = BANSHEE_TRACK_OBJECT(object);
    
    g_free(self->title);
    g_free(self->artist);
    g_free(self->album);
    g_free(self->duration_str);
    g_free(self->file_path);
    
    G_OBJECT_CLASS(banshee_track_object_parent_class)->finalize(object);
}

static void banshee_track_object_get_property(GObject *object, guint prop_id,
                                               GValue *value, GParamSpec *pspec) {
    BansheeTrackObject *self = BANSHEE_TRACK_OBJECT(object);
    
    switch (prop_id) {
        case TRACK_PROP_ID:
            g_value_set_int(value, self->id);
            break;
        case TRACK_PROP_TRACK_NUMBER:
            g_value_set_int(value, self->track_number);
            break;
        case TRACK_PROP_TITLE:
            g_value_set_string(value, self->title);
            break;
        case TRACK_PROP_ARTIST:
            g_value_set_string(value, self->artist);
            break;
        case TRACK_PROP_ALBUM:
            g_value_set_string(value, self->album);
            break;
        case TRACK_PROP_DURATION_STR:
            g_value_set_string(value, self->duration_str);
            break;
        case TRACK_PROP_DURATION_SECONDS:
            g_value_set_int(value, self->duration_seconds);
            break;
        case TRACK_PROP_FILE_PATH:
            g_value_set_string(value, self->file_path);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void banshee_track_object_set_property(GObject *object, guint prop_id,
                                               const GValue *value, GParamSpec *pspec) {
    BansheeTrackObject *self = BANSHEE_TRACK_OBJECT(object);
    
    switch (prop_id) {
        case TRACK_PROP_ID:
            self->id = g_value_get_int(value);
            break;
        case TRACK_PROP_TRACK_NUMBER:
            self->track_number = g_value_get_int(value);
            break;
        case TRACK_PROP_TITLE:
            g_free(self->title);
            self->title = g_value_dup_string(value);
            break;
        case TRACK_PROP_ARTIST:
            g_free(self->artist);
            self->artist = g_value_dup_string(value);
            break;
        case TRACK_PROP_ALBUM:
            g_free(self->album);
            self->album = g_value_dup_string(value);
            break;
        case TRACK_PROP_DURATION_STR:
            g_free(self->duration_str);
            self->duration_str = g_value_dup_string(value);
            break;
        case TRACK_PROP_DURATION_SECONDS:
            self->duration_seconds = g_value_get_int(value);
            break;
        case TRACK_PROP_FILE_PATH:
            g_free(self->file_path);
            self->file_path = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void banshee_track_object_class_init(BansheeTrackObjectClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = banshee_track_object_finalize;
    object_class->get_property = banshee_track_object_get_property;
    object_class->set_property = banshee_track_object_set_property;
    
    track_properties[TRACK_PROP_ID] =
        g_param_spec_int("id", "ID", "Track database ID",
                         0, G_MAXINT, 0,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    track_properties[TRACK_PROP_TRACK_NUMBER] =
        g_param_spec_int("track-number", "Track Number", "Track number in album",
                         0, G_MAXINT, 0,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    track_properties[TRACK_PROP_TITLE] =
        g_param_spec_string("title", "Title", "Track title",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    track_properties[TRACK_PROP_ARTIST] =
        g_param_spec_string("artist", "Artist", "Track artist",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    track_properties[TRACK_PROP_ALBUM] =
        g_param_spec_string("album", "Album", "Track album",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    track_properties[TRACK_PROP_DURATION_STR] =
        g_param_spec_string("duration-str", "Duration String", "Formatted duration",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    track_properties[TRACK_PROP_DURATION_SECONDS] =
        g_param_spec_int("duration-seconds", "Duration Seconds", "Duration in seconds",
                         0, G_MAXINT, 0,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    track_properties[TRACK_PROP_FILE_PATH] =
        g_param_spec_string("file-path", "File Path", "Path to media file",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    g_object_class_install_properties(object_class, TRACK_N_PROPERTIES, track_properties);
}

static void banshee_track_object_init(BansheeTrackObject *self) {
    self->id = 0;
    self->track_number = 0;
    self->title = NULL;
    self->artist = NULL;
    self->album = NULL;
    self->duration_str = NULL;
    self->duration_seconds = 0;
    self->file_path = NULL;
}

BansheeTrackObject* banshee_track_object_new(gint id, gint track_number,
                                              const gchar *title, const gchar *artist,
                                              const gchar *album, const gchar *duration_str,
                                              gint duration_seconds, const gchar *file_path) {
    return g_object_new(BANSHEE_TYPE_TRACK_OBJECT,
                        "id", id,
                        "track-number", track_number,
                        "title", title,
                        "artist", artist,
                        "album", album,
                        "duration-str", duration_str,
                        "duration-seconds", duration_seconds,
                        "file-path", file_path,
                        NULL);
}

gint banshee_track_object_get_id(BansheeTrackObject *self) {
    g_return_val_if_fail(BANSHEE_IS_TRACK_OBJECT(self), 0);
    return self->id;
}

gint banshee_track_object_get_track_number(BansheeTrackObject *self) {
    g_return_val_if_fail(BANSHEE_IS_TRACK_OBJECT(self), 0);
    return self->track_number;
}

const gchar* banshee_track_object_get_title(BansheeTrackObject *self) {
    g_return_val_if_fail(BANSHEE_IS_TRACK_OBJECT(self), NULL);
    return self->title;
}

const gchar* banshee_track_object_get_artist(BansheeTrackObject *self) {
    g_return_val_if_fail(BANSHEE_IS_TRACK_OBJECT(self), NULL);
    return self->artist;
}

const gchar* banshee_track_object_get_album(BansheeTrackObject *self) {
    g_return_val_if_fail(BANSHEE_IS_TRACK_OBJECT(self), NULL);
    return self->album;
}

const gchar* banshee_track_object_get_duration_str(BansheeTrackObject *self) {
    g_return_val_if_fail(BANSHEE_IS_TRACK_OBJECT(self), NULL);
    return self->duration_str;
}

gint banshee_track_object_get_duration_seconds(BansheeTrackObject *self) {
    g_return_val_if_fail(BANSHEE_IS_TRACK_OBJECT(self), 0);
    return self->duration_seconds;
}

const gchar* banshee_track_object_get_file_path(BansheeTrackObject *self) {
    g_return_val_if_fail(BANSHEE_IS_TRACK_OBJECT(self), NULL);
    return self->file_path;
}

/* ============================================================================
 * BansheeBrowserItem Implementation
 * ============================================================================ */

struct _BansheeBrowserItem {
    GObject parent_instance;
    
    gint id;
    gchar *name;
    gint count;
};

enum {
    BROWSER_PROP_0,
    BROWSER_PROP_ID,
    BROWSER_PROP_NAME,
    BROWSER_PROP_COUNT,
    BROWSER_N_PROPERTIES
};

static GParamSpec *browser_properties[BROWSER_N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE(BansheeBrowserItem, banshee_browser_item, G_TYPE_OBJECT)

static void banshee_browser_item_finalize(GObject *object) {
    BansheeBrowserItem *self = BANSHEE_BROWSER_ITEM(object);
    g_free(self->name);
    G_OBJECT_CLASS(banshee_browser_item_parent_class)->finalize(object);
}

static void banshee_browser_item_get_property(GObject *object, guint prop_id,
                                               GValue *value, GParamSpec *pspec) {
    BansheeBrowserItem *self = BANSHEE_BROWSER_ITEM(object);
    
    switch (prop_id) {
        case BROWSER_PROP_ID:
            g_value_set_int(value, self->id);
            break;
        case BROWSER_PROP_NAME:
            g_value_set_string(value, self->name);
            break;
        case BROWSER_PROP_COUNT:
            g_value_set_int(value, self->count);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void banshee_browser_item_set_property(GObject *object, guint prop_id,
                                               const GValue *value, GParamSpec *pspec) {
    BansheeBrowserItem *self = BANSHEE_BROWSER_ITEM(object);
    
    switch (prop_id) {
        case BROWSER_PROP_ID:
            self->id = g_value_get_int(value);
            break;
        case BROWSER_PROP_NAME:
            g_free(self->name);
            self->name = g_value_dup_string(value);
            break;
        case BROWSER_PROP_COUNT:
            self->count = g_value_get_int(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void banshee_browser_item_class_init(BansheeBrowserItemClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = banshee_browser_item_finalize;
    object_class->get_property = banshee_browser_item_get_property;
    object_class->set_property = banshee_browser_item_set_property;
    
    browser_properties[BROWSER_PROP_ID] =
        g_param_spec_int("id", "ID", "Item ID",
                         0, G_MAXINT, 0,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    browser_properties[BROWSER_PROP_NAME] =
        g_param_spec_string("name", "Name", "Item name",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    browser_properties[BROWSER_PROP_COUNT] =
        g_param_spec_int("count", "Count", "Item count",
                         0, G_MAXINT, 0,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    g_object_class_install_properties(object_class, BROWSER_N_PROPERTIES, browser_properties);
}

static void banshee_browser_item_init(BansheeBrowserItem *self) {
    self->id = 0;
    self->name = NULL;
    self->count = 0;
}

BansheeBrowserItem* banshee_browser_item_new(gint id, const gchar *name, gint count) {
    return g_object_new(BANSHEE_TYPE_BROWSER_ITEM,
                        "id", id,
                        "name", name,
                        "count", count,
                        NULL);
}

gint banshee_browser_item_get_id(BansheeBrowserItem *self) {
    g_return_val_if_fail(BANSHEE_IS_BROWSER_ITEM(self), 0);
    return self->id;
}

const gchar* banshee_browser_item_get_name(BansheeBrowserItem *self) {
    g_return_val_if_fail(BANSHEE_IS_BROWSER_ITEM(self), NULL);
    return self->name;
}

gint banshee_browser_item_get_count(BansheeBrowserItem *self) {
    g_return_val_if_fail(BANSHEE_IS_BROWSER_ITEM(self), 0);
    return self->count;
}

/* ============================================================================
 * BansheeSourceObject Implementation
 * ============================================================================ */

struct _BansheeSourceObject {
    GObject parent_instance;
    
    gchar *name;
    gchar *icon_name;
    gpointer source_ptr;  /* Pointer to the original Source struct */
    GListStore *children; /* Child sources for hierarchical display */
};

enum {
    SOURCE_PROP_0,
    SOURCE_PROP_NAME,
    SOURCE_PROP_ICON_NAME,
    SOURCE_PROP_SOURCE_PTR,
    SOURCE_N_PROPERTIES
};

static GParamSpec *source_properties[SOURCE_N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE(BansheeSourceObject, banshee_source_object, G_TYPE_OBJECT)

static void banshee_source_object_finalize(GObject *object) {
    BansheeSourceObject *self = BANSHEE_SOURCE_OBJECT(object);
    g_free(self->name);
    g_free(self->icon_name);
    g_clear_object(&self->children);
    G_OBJECT_CLASS(banshee_source_object_parent_class)->finalize(object);
}

static void banshee_source_object_get_property(GObject *object, guint prop_id,
                                                GValue *value, GParamSpec *pspec) {
    BansheeSourceObject *self = BANSHEE_SOURCE_OBJECT(object);
    
    switch (prop_id) {
        case SOURCE_PROP_NAME:
            g_value_set_string(value, self->name);
            break;
        case SOURCE_PROP_ICON_NAME:
            g_value_set_string(value, self->icon_name);
            break;
        case SOURCE_PROP_SOURCE_PTR:
            g_value_set_pointer(value, self->source_ptr);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void banshee_source_object_set_property(GObject *object, guint prop_id,
                                                const GValue *value, GParamSpec *pspec) {
    BansheeSourceObject *self = BANSHEE_SOURCE_OBJECT(object);
    
    switch (prop_id) {
        case SOURCE_PROP_NAME:
            g_free(self->name);
            self->name = g_value_dup_string(value);
            break;
        case SOURCE_PROP_ICON_NAME:
            g_free(self->icon_name);
            self->icon_name = g_value_dup_string(value);
            break;
        case SOURCE_PROP_SOURCE_PTR:
            self->source_ptr = g_value_get_pointer(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void banshee_source_object_class_init(BansheeSourceObjectClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = banshee_source_object_finalize;
    object_class->get_property = banshee_source_object_get_property;
    object_class->set_property = banshee_source_object_set_property;
    
    source_properties[SOURCE_PROP_NAME] =
        g_param_spec_string("name", "Name", "Source name",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    source_properties[SOURCE_PROP_ICON_NAME] =
        g_param_spec_string("icon-name", "Icon Name", "Icon name for display",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    source_properties[SOURCE_PROP_SOURCE_PTR] =
        g_param_spec_pointer("source-ptr", "Source Pointer", "Pointer to Source struct",
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    g_object_class_install_properties(object_class, SOURCE_N_PROPERTIES, source_properties);
}

static void banshee_source_object_init(BansheeSourceObject *self) {
    self->name = NULL;
    self->icon_name = NULL;
    self->source_ptr = NULL;
    self->children = g_list_store_new(BANSHEE_TYPE_SOURCE_OBJECT);
}

BansheeSourceObject* banshee_source_object_new(const gchar *name, const gchar *icon_name,
                                                gpointer source_ptr) {
    return g_object_new(BANSHEE_TYPE_SOURCE_OBJECT,
                        "name", name,
                        "icon-name", icon_name,
                        "source-ptr", source_ptr,
                        NULL);
}

const gchar* banshee_source_object_get_name(BansheeSourceObject *self) {
    g_return_val_if_fail(BANSHEE_IS_SOURCE_OBJECT(self), NULL);
    return self->name;
}

const gchar* banshee_source_object_get_icon_name(BansheeSourceObject *self) {
    g_return_val_if_fail(BANSHEE_IS_SOURCE_OBJECT(self), NULL);
    return self->icon_name;
}

gpointer banshee_source_object_get_source(BansheeSourceObject *self) {
    g_return_val_if_fail(BANSHEE_IS_SOURCE_OBJECT(self), NULL);
    return self->source_ptr;
}

GListModel* banshee_source_object_get_children(BansheeSourceObject *self) {
    g_return_val_if_fail(BANSHEE_IS_SOURCE_OBJECT(self), NULL);
    return G_LIST_MODEL(self->children);
}

void banshee_source_object_add_child(BansheeSourceObject *self, BansheeSourceObject *child) {
    g_return_if_fail(BANSHEE_IS_SOURCE_OBJECT(self));
    g_return_if_fail(BANSHEE_IS_SOURCE_OBJECT(child));
    g_list_store_append(self->children, child);
}

/* ============================================================================
 * BansheePodcastObject Implementation
 * ============================================================================ */

struct _BansheePodcastObject {
    GObject parent_instance;
    
    gint id;
    gchar *title;
    gchar *author;
};

enum {
    PODCAST_PROP_0,
    PODCAST_PROP_ID,
    PODCAST_PROP_TITLE,
    PODCAST_PROP_AUTHOR,
    PODCAST_N_PROPERTIES
};

static GParamSpec *podcast_properties[PODCAST_N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE(BansheePodcastObject, banshee_podcast_object, G_TYPE_OBJECT)

static void banshee_podcast_object_finalize(GObject *object) {
    BansheePodcastObject *self = BANSHEE_PODCAST_OBJECT(object);
    g_free(self->title);
    g_free(self->author);
    G_OBJECT_CLASS(banshee_podcast_object_parent_class)->finalize(object);
}

static void banshee_podcast_object_get_property(GObject *object, guint prop_id,
                                                 GValue *value, GParamSpec *pspec) {
    BansheePodcastObject *self = BANSHEE_PODCAST_OBJECT(object);
    
    switch (prop_id) {
        case PODCAST_PROP_ID:
            g_value_set_int(value, self->id);
            break;
        case PODCAST_PROP_TITLE:
            g_value_set_string(value, self->title);
            break;
        case PODCAST_PROP_AUTHOR:
            g_value_set_string(value, self->author);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void banshee_podcast_object_set_property(GObject *object, guint prop_id,
                                                 const GValue *value, GParamSpec *pspec) {
    BansheePodcastObject *self = BANSHEE_PODCAST_OBJECT(object);
    
    switch (prop_id) {
        case PODCAST_PROP_ID:
            self->id = g_value_get_int(value);
            break;
        case PODCAST_PROP_TITLE:
            g_free(self->title);
            self->title = g_value_dup_string(value);
            break;
        case PODCAST_PROP_AUTHOR:
            g_free(self->author);
            self->author = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void banshee_podcast_object_class_init(BansheePodcastObjectClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = banshee_podcast_object_finalize;
    object_class->get_property = banshee_podcast_object_get_property;
    object_class->set_property = banshee_podcast_object_set_property;
    
    podcast_properties[PODCAST_PROP_ID] =
        g_param_spec_int("id", "ID", "Podcast ID",
                         0, G_MAXINT, 0,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    podcast_properties[PODCAST_PROP_TITLE] =
        g_param_spec_string("title", "Title", "Podcast title",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    podcast_properties[PODCAST_PROP_AUTHOR] =
        g_param_spec_string("author", "Author", "Podcast author",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    g_object_class_install_properties(object_class, PODCAST_N_PROPERTIES, podcast_properties);
}

static void banshee_podcast_object_init(BansheePodcastObject *self) {
    self->id = 0;
    self->title = NULL;
    self->author = NULL;
}

BansheePodcastObject* banshee_podcast_object_new(gint id, const gchar *title, const gchar *author) {
    return g_object_new(BANSHEE_TYPE_PODCAST_OBJECT,
                        "id", id,
                        "title", title,
                        "author", author,
                        NULL);
}

gint banshee_podcast_object_get_id(BansheePodcastObject *self) {
    g_return_val_if_fail(BANSHEE_IS_PODCAST_OBJECT(self), 0);
    return self->id;
}

const gchar* banshee_podcast_object_get_title(BansheePodcastObject *self) {
    g_return_val_if_fail(BANSHEE_IS_PODCAST_OBJECT(self), NULL);
    return self->title;
}

const gchar* banshee_podcast_object_get_author(BansheePodcastObject *self) {
    g_return_val_if_fail(BANSHEE_IS_PODCAST_OBJECT(self), NULL);
    return self->author;
}

/* ============================================================================
 * BansheeEpisodeObject Implementation
 * ============================================================================ */

struct _BansheeEpisodeObject {
    GObject parent_instance;
    
    gint id;
    gchar *title;
    gchar *date;
    gchar *duration;
    gboolean downloaded;
};

enum {
    EPISODE_PROP_0,
    EPISODE_PROP_ID,
    EPISODE_PROP_TITLE,
    EPISODE_PROP_DATE,
    EPISODE_PROP_DURATION,
    EPISODE_PROP_DOWNLOADED,
    EPISODE_N_PROPERTIES
};

static GParamSpec *episode_properties[EPISODE_N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE(BansheeEpisodeObject, banshee_episode_object, G_TYPE_OBJECT)

static void banshee_episode_object_finalize(GObject *object) {
    BansheeEpisodeObject *self = BANSHEE_EPISODE_OBJECT(object);
    g_free(self->title);
    g_free(self->date);
    g_free(self->duration);
    G_OBJECT_CLASS(banshee_episode_object_parent_class)->finalize(object);
}

static void banshee_episode_object_get_property(GObject *object, guint prop_id,
                                                 GValue *value, GParamSpec *pspec) {
    BansheeEpisodeObject *self = BANSHEE_EPISODE_OBJECT(object);
    
    switch (prop_id) {
        case EPISODE_PROP_ID:
            g_value_set_int(value, self->id);
            break;
        case EPISODE_PROP_TITLE:
            g_value_set_string(value, self->title);
            break;
        case EPISODE_PROP_DATE:
            g_value_set_string(value, self->date);
            break;
        case EPISODE_PROP_DURATION:
            g_value_set_string(value, self->duration);
            break;
        case EPISODE_PROP_DOWNLOADED:
            g_value_set_boolean(value, self->downloaded);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void banshee_episode_object_set_property(GObject *object, guint prop_id,
                                                 const GValue *value, GParamSpec *pspec) {
    BansheeEpisodeObject *self = BANSHEE_EPISODE_OBJECT(object);
    
    switch (prop_id) {
        case EPISODE_PROP_ID:
            self->id = g_value_get_int(value);
            break;
        case EPISODE_PROP_TITLE:
            g_free(self->title);
            self->title = g_value_dup_string(value);
            break;
        case EPISODE_PROP_DATE:
            g_free(self->date);
            self->date = g_value_dup_string(value);
            break;
        case EPISODE_PROP_DURATION:
            g_free(self->duration);
            self->duration = g_value_dup_string(value);
            break;
        case EPISODE_PROP_DOWNLOADED:
            self->downloaded = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void banshee_episode_object_class_init(BansheeEpisodeObjectClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = banshee_episode_object_finalize;
    object_class->get_property = banshee_episode_object_get_property;
    object_class->set_property = banshee_episode_object_set_property;
    
    episode_properties[EPISODE_PROP_ID] =
        g_param_spec_int("id", "ID", "Episode ID",
                         0, G_MAXINT, 0,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    episode_properties[EPISODE_PROP_TITLE] =
        g_param_spec_string("title", "Title", "Episode title",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    episode_properties[EPISODE_PROP_DATE] =
        g_param_spec_string("date", "Date", "Episode publish date",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    episode_properties[EPISODE_PROP_DURATION] =
        g_param_spec_string("duration", "Duration", "Episode duration",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    episode_properties[EPISODE_PROP_DOWNLOADED] =
        g_param_spec_boolean("downloaded", "Downloaded", "Whether episode is downloaded",
                             FALSE,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    g_object_class_install_properties(object_class, EPISODE_N_PROPERTIES, episode_properties);
}

static void banshee_episode_object_init(BansheeEpisodeObject *self) {
    self->id = 0;
    self->title = NULL;
    self->date = NULL;
    self->duration = NULL;
    self->downloaded = FALSE;
}

BansheeEpisodeObject* banshee_episode_object_new(gint id, const gchar *title,
                                                  const gchar *date, const gchar *duration,
                                                  gboolean downloaded) {
    return g_object_new(BANSHEE_TYPE_EPISODE_OBJECT,
                        "id", id,
                        "title", title,
                        "date", date,
                        "duration", duration,
                        "downloaded", downloaded,
                        NULL);
}

gint banshee_episode_object_get_id(BansheeEpisodeObject *self) {
    g_return_val_if_fail(BANSHEE_IS_EPISODE_OBJECT(self), 0);
    return self->id;
}

const gchar* banshee_episode_object_get_title(BansheeEpisodeObject *self) {
    g_return_val_if_fail(BANSHEE_IS_EPISODE_OBJECT(self), NULL);
    return self->title;
}

const gchar* banshee_episode_object_get_date(BansheeEpisodeObject *self) {
    g_return_val_if_fail(BANSHEE_IS_EPISODE_OBJECT(self), NULL);
    return self->date;
}

const gchar* banshee_episode_object_get_duration(BansheeEpisodeObject *self) {
    g_return_val_if_fail(BANSHEE_IS_EPISODE_OBJECT(self), NULL);
    return self->duration;
}

gboolean banshee_episode_object_get_downloaded(BansheeEpisodeObject *self) {
    g_return_val_if_fail(BANSHEE_IS_EPISODE_OBJECT(self), FALSE);
    return self->downloaded;
}

/* ============================================================================
 * BansheeVideoObject Implementation
 * ============================================================================ */

struct _BansheeVideoObject {
    GObject parent_instance;
    
    gint id;
    gchar *title;
    gchar *artist;
    gchar *duration;
    gchar *file_path;
};

enum {
    VIDEO_PROP_0,
    VIDEO_PROP_ID,
    VIDEO_PROP_TITLE,
    VIDEO_PROP_ARTIST,
    VIDEO_PROP_DURATION,
    VIDEO_PROP_FILE_PATH,
    VIDEO_N_PROPERTIES
};

static GParamSpec *video_properties[VIDEO_N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE(BansheeVideoObject, banshee_video_object, G_TYPE_OBJECT)

static void banshee_video_object_finalize(GObject *object) {
    BansheeVideoObject *self = BANSHEE_VIDEO_OBJECT(object);
    g_free(self->title);
    g_free(self->artist);
    g_free(self->duration);
    g_free(self->file_path);
    G_OBJECT_CLASS(banshee_video_object_parent_class)->finalize(object);
}

static void banshee_video_object_get_property(GObject *object, guint prop_id,
                                               GValue *value, GParamSpec *pspec) {
    BansheeVideoObject *self = BANSHEE_VIDEO_OBJECT(object);
    
    switch (prop_id) {
        case VIDEO_PROP_ID:
            g_value_set_int(value, self->id);
            break;
        case VIDEO_PROP_TITLE:
            g_value_set_string(value, self->title);
            break;
        case VIDEO_PROP_ARTIST:
            g_value_set_string(value, self->artist);
            break;
        case VIDEO_PROP_DURATION:
            g_value_set_string(value, self->duration);
            break;
        case VIDEO_PROP_FILE_PATH:
            g_value_set_string(value, self->file_path);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void banshee_video_object_set_property(GObject *object, guint prop_id,
                                               const GValue *value, GParamSpec *pspec) {
    BansheeVideoObject *self = BANSHEE_VIDEO_OBJECT(object);
    
    switch (prop_id) {
        case VIDEO_PROP_ID:
            self->id = g_value_get_int(value);
            break;
        case VIDEO_PROP_TITLE:
            g_free(self->title);
            self->title = g_value_dup_string(value);
            break;
        case VIDEO_PROP_ARTIST:
            g_free(self->artist);
            self->artist = g_value_dup_string(value);
            break;
        case VIDEO_PROP_DURATION:
            g_free(self->duration);
            self->duration = g_value_dup_string(value);
            break;
        case VIDEO_PROP_FILE_PATH:
            g_free(self->file_path);
            self->file_path = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void banshee_video_object_class_init(BansheeVideoObjectClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = banshee_video_object_finalize;
    object_class->get_property = banshee_video_object_get_property;
    object_class->set_property = banshee_video_object_set_property;
    
    video_properties[VIDEO_PROP_ID] =
        g_param_spec_int("id", "ID", "Video ID",
                         0, G_MAXINT, 0,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    video_properties[VIDEO_PROP_TITLE] =
        g_param_spec_string("title", "Title", "Video title",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    video_properties[VIDEO_PROP_ARTIST] =
        g_param_spec_string("artist", "Artist", "Video artist",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    video_properties[VIDEO_PROP_DURATION] =
        g_param_spec_string("duration", "Duration", "Video duration",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    video_properties[VIDEO_PROP_FILE_PATH] =
        g_param_spec_string("file-path", "File Path", "Path to video file",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    g_object_class_install_properties(object_class, VIDEO_N_PROPERTIES, video_properties);
}

static void banshee_video_object_init(BansheeVideoObject *self) {
    self->id = 0;
    self->title = NULL;
    self->artist = NULL;
    self->duration = NULL;
    self->file_path = NULL;
}

BansheeVideoObject* banshee_video_object_new(gint id, const gchar *title,
                                              const gchar *artist, const gchar *duration,
                                              const gchar *file_path) {
    return g_object_new(BANSHEE_TYPE_VIDEO_OBJECT,
                        "id", id,
                        "title", title,
                        "artist", artist,
                        "duration", duration,
                        "file-path", file_path,
                        NULL);
}

gint banshee_video_object_get_id(BansheeVideoObject *self) {
    g_return_val_if_fail(BANSHEE_IS_VIDEO_OBJECT(self), 0);
    return self->id;
}

const gchar* banshee_video_object_get_title(BansheeVideoObject *self) {
    g_return_val_if_fail(BANSHEE_IS_VIDEO_OBJECT(self), NULL);
    return self->title;
}

const gchar* banshee_video_object_get_artist(BansheeVideoObject *self) {
    g_return_val_if_fail(BANSHEE_IS_VIDEO_OBJECT(self), NULL);
    return self->artist;
}

const gchar* banshee_video_object_get_duration(BansheeVideoObject *self) {
    g_return_val_if_fail(BANSHEE_IS_VIDEO_OBJECT(self), NULL);
    return self->duration;
}

const gchar* banshee_video_object_get_file_path(BansheeVideoObject *self) {
    g_return_val_if_fail(BANSHEE_IS_VIDEO_OBJECT(self), NULL);
    return self->file_path;
}

/* ============================================================================
 * BansheeChapterObject Implementation
 * ============================================================================ */

struct _BansheeChapterObject {
    GObject parent_instance;
    gdouble start_time;
    gchar *title;
    gchar *img;
    gchar *url;
};

enum {
    CHAPTER_PROP_0,
    CHAPTER_PROP_START_TIME,
    CHAPTER_PROP_TITLE,
    CHAPTER_PROP_IMG,
    CHAPTER_PROP_URL,
    CHAPTER_N_PROPERTIES
};

static GParamSpec *chapter_properties[CHAPTER_N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE(BansheeChapterObject, banshee_chapter_object, G_TYPE_OBJECT)

static void banshee_chapter_object_finalize(GObject *object) {
    BansheeChapterObject *self = BANSHEE_CHAPTER_OBJECT(object);
    g_free(self->title);
    g_free(self->img);
    g_free(self->url);
    G_OBJECT_CLASS(banshee_chapter_object_parent_class)->finalize(object);
}

static void banshee_chapter_object_get_property(GObject *object, guint prop_id,
                                                 GValue *value, GParamSpec *pspec) {
    BansheeChapterObject *self = BANSHEE_CHAPTER_OBJECT(object);
    
    switch (prop_id) {
        case CHAPTER_PROP_START_TIME:
            g_value_set_double(value, self->start_time);
            break;
        case CHAPTER_PROP_TITLE:
            g_value_set_string(value, self->title);
            break;
        case CHAPTER_PROP_IMG:
            g_value_set_string(value, self->img);
            break;
        case CHAPTER_PROP_URL:
            g_value_set_string(value, self->url);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void banshee_chapter_object_set_property(GObject *object, guint prop_id,
                                                 const GValue *value, GParamSpec *pspec) {
    BansheeChapterObject *self = BANSHEE_CHAPTER_OBJECT(object);
    
    switch (prop_id) {
        case CHAPTER_PROP_START_TIME:
            self->start_time = g_value_get_double(value);
            break;
        case CHAPTER_PROP_TITLE:
            g_free(self->title);
            self->title = g_value_dup_string(value);
            break;
        case CHAPTER_PROP_IMG:
            g_free(self->img);
            self->img = g_value_dup_string(value);
            break;
        case CHAPTER_PROP_URL:
            g_free(self->url);
            self->url = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void banshee_chapter_object_class_init(BansheeChapterObjectClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = banshee_chapter_object_finalize;
    object_class->get_property = banshee_chapter_object_get_property;
    object_class->set_property = banshee_chapter_object_set_property;
    
    chapter_properties[CHAPTER_PROP_START_TIME] =
        g_param_spec_double("start-time", "Start Time", "Chapter start time in seconds",
                            0.0, G_MAXDOUBLE, 0.0,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    chapter_properties[CHAPTER_PROP_TITLE] =
        g_param_spec_string("title", "Title", "Chapter title",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    chapter_properties[CHAPTER_PROP_IMG] =
        g_param_spec_string("img", "Image", "Chapter image URL",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    chapter_properties[CHAPTER_PROP_URL] =
        g_param_spec_string("url", "URL", "Chapter URL",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    g_object_class_install_properties(object_class, CHAPTER_N_PROPERTIES, chapter_properties);
}

static void banshee_chapter_object_init(BansheeChapterObject *self) {
    self->start_time = 0.0;
    self->title = NULL;
    self->img = NULL;
    self->url = NULL;
}

BansheeChapterObject* banshee_chapter_object_new(gdouble start_time, const gchar *title,
                                                  const gchar *img, const gchar *url) {
    return g_object_new(BANSHEE_TYPE_CHAPTER_OBJECT,
                        "start-time", start_time,
                        "title", title,
                        "img", img,
                        "url", url,
                        NULL);
}

gdouble banshee_chapter_object_get_start_time(BansheeChapterObject *self) {
    g_return_val_if_fail(BANSHEE_IS_CHAPTER_OBJECT(self), 0.0);
    return self->start_time;
}

const gchar* banshee_chapter_object_get_title(BansheeChapterObject *self) {
    g_return_val_if_fail(BANSHEE_IS_CHAPTER_OBJECT(self), NULL);
    return self->title;
}

const gchar* banshee_chapter_object_get_img(BansheeChapterObject *self) {
    g_return_val_if_fail(BANSHEE_IS_CHAPTER_OBJECT(self), NULL);
    return self->img;
}

const gchar* banshee_chapter_object_get_url(BansheeChapterObject *self) {
    g_return_val_if_fail(BANSHEE_IS_CHAPTER_OBJECT(self), NULL);
    return self->url;
}