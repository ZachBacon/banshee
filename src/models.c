#include "models.h"
#include <string.h>

/* ============================================================================
 * ShriekTrackObject Implementation
 * ============================================================================ */

struct _ShriekTrackObject {
    GObject parent_instance;
    
    gint id;
    gint track_number;
    gchar *title;
    gchar *artist;
    gchar *album;
    gchar *duration_str;
    gint duration_seconds;
    gchar *file_path;
    gint play_count;
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
    TRACK_PROP_PLAY_COUNT,
    TRACK_N_PROPERTIES
};

static GParamSpec *track_properties[TRACK_N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE(ShriekTrackObject, shriek_track_object, G_TYPE_OBJECT)

static void shriek_track_object_finalize(GObject *object) {
    ShriekTrackObject *self = SHRIEK_TRACK_OBJECT(object);
    
    g_free(self->title);
    g_free(self->artist);
    g_free(self->album);
    g_free(self->duration_str);
    g_free(self->file_path);
    
    G_OBJECT_CLASS(shriek_track_object_parent_class)->finalize(object);
}

static void shriek_track_object_get_property(GObject *object, guint prop_id,
                                               GValue *value, GParamSpec *pspec) {
    ShriekTrackObject *self = SHRIEK_TRACK_OBJECT(object);
    
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
        case TRACK_PROP_PLAY_COUNT:
            g_value_set_int(value, self->play_count);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void shriek_track_object_set_property(GObject *object, guint prop_id,
                                               const GValue *value, GParamSpec *pspec) {
    ShriekTrackObject *self = SHRIEK_TRACK_OBJECT(object);
    
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
        case TRACK_PROP_PLAY_COUNT:
            self->play_count = g_value_get_int(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void shriek_track_object_class_init(ShriekTrackObjectClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = shriek_track_object_finalize;
    object_class->get_property = shriek_track_object_get_property;
    object_class->set_property = shriek_track_object_set_property;
    
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
    
    track_properties[TRACK_PROP_PLAY_COUNT] =
        g_param_spec_int("play-count", "Play Count", "Number of times track has been played",
                         0, G_MAXINT, 0,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    
    g_object_class_install_properties(object_class, TRACK_N_PROPERTIES, track_properties);
}

static void shriek_track_object_init(ShriekTrackObject *self) {
    self->id = 0;
    self->track_number = 0;
    self->title = NULL;
    self->artist = NULL;
    self->album = NULL;
    self->duration_str = NULL;
    self->duration_seconds = 0;
    self->file_path = NULL;
    self->play_count = 0;
}

ShriekTrackObject* shriek_track_object_new(gint id, gint track_number,
                                              const gchar *title, const gchar *artist,
                                              const gchar *album, const gchar *duration_str,
                                              gint duration_seconds, const gchar *file_path,
                                              gint play_count) {
    return g_object_new(SHRIEK_TYPE_TRACK_OBJECT,
                        "id", id,
                        "track-number", track_number,
                        "title", title,
                        "artist", artist,
                        "album", album,
                        "duration-str", duration_str,
                        "duration-seconds", duration_seconds,
                        "file-path", file_path,
                        "play-count", play_count,
                        NULL);
}

gint shriek_track_object_get_id(ShriekTrackObject *self) {
    g_return_val_if_fail(SHRIEK_IS_TRACK_OBJECT(self), 0);
    return self->id;
}

gint shriek_track_object_get_track_number(ShriekTrackObject *self) {
    g_return_val_if_fail(SHRIEK_IS_TRACK_OBJECT(self), 0);
    return self->track_number;
}

const gchar* shriek_track_object_get_title(ShriekTrackObject *self) {
    g_return_val_if_fail(SHRIEK_IS_TRACK_OBJECT(self), NULL);
    return self->title;
}

const gchar* shriek_track_object_get_artist(ShriekTrackObject *self) {
    g_return_val_if_fail(SHRIEK_IS_TRACK_OBJECT(self), NULL);
    return self->artist;
}

const gchar* shriek_track_object_get_album(ShriekTrackObject *self) {
    g_return_val_if_fail(SHRIEK_IS_TRACK_OBJECT(self), NULL);
    return self->album;
}

const gchar* shriek_track_object_get_duration_str(ShriekTrackObject *self) {
    g_return_val_if_fail(SHRIEK_IS_TRACK_OBJECT(self), NULL);
    return self->duration_str;
}

gint shriek_track_object_get_duration_seconds(ShriekTrackObject *self) {
    g_return_val_if_fail(SHRIEK_IS_TRACK_OBJECT(self), 0);
    return self->duration_seconds;
}

const gchar* shriek_track_object_get_file_path(ShriekTrackObject *self) {
    g_return_val_if_fail(SHRIEK_IS_TRACK_OBJECT(self), NULL);
    return self->file_path;
}

gint shriek_track_object_get_play_count(ShriekTrackObject *self) {
    g_return_val_if_fail(SHRIEK_IS_TRACK_OBJECT(self), 0);
    return self->play_count;
}

/* ============================================================================
 * ShriekBrowserItem Implementation
 * ============================================================================ */

struct _ShriekBrowserItem {
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

G_DEFINE_TYPE(ShriekBrowserItem, shriek_browser_item, G_TYPE_OBJECT)

static void shriek_browser_item_finalize(GObject *object) {
    ShriekBrowserItem *self = SHRIEK_BROWSER_ITEM(object);
    g_free(self->name);
    G_OBJECT_CLASS(shriek_browser_item_parent_class)->finalize(object);
}

static void shriek_browser_item_get_property(GObject *object, guint prop_id,
                                               GValue *value, GParamSpec *pspec) {
    ShriekBrowserItem *self = SHRIEK_BROWSER_ITEM(object);
    
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

static void shriek_browser_item_set_property(GObject *object, guint prop_id,
                                               const GValue *value, GParamSpec *pspec) {
    ShriekBrowserItem *self = SHRIEK_BROWSER_ITEM(object);
    
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

static void shriek_browser_item_class_init(ShriekBrowserItemClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = shriek_browser_item_finalize;
    object_class->get_property = shriek_browser_item_get_property;
    object_class->set_property = shriek_browser_item_set_property;
    
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

static void shriek_browser_item_init(ShriekBrowserItem *self) {
    self->id = 0;
    self->name = NULL;
    self->count = 0;
}

ShriekBrowserItem* shriek_browser_item_new(gint id, const gchar *name, gint count) {
    return g_object_new(SHRIEK_TYPE_BROWSER_ITEM,
                        "id", id,
                        "name", name,
                        "count", count,
                        NULL);
}

gint shriek_browser_item_get_id(ShriekBrowserItem *self) {
    g_return_val_if_fail(SHRIEK_IS_BROWSER_ITEM(self), 0);
    return self->id;
}

const gchar* shriek_browser_item_get_name(ShriekBrowserItem *self) {
    g_return_val_if_fail(SHRIEK_IS_BROWSER_ITEM(self), NULL);
    return self->name;
}

gint shriek_browser_item_get_count(ShriekBrowserItem *self) {
    g_return_val_if_fail(SHRIEK_IS_BROWSER_ITEM(self), 0);
    return self->count;
}

/* ============================================================================
 * ShriekSourceObject Implementation
 * ============================================================================ */

struct _ShriekSourceObject {
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

G_DEFINE_TYPE(ShriekSourceObject, shriek_source_object, G_TYPE_OBJECT)

static void shriek_source_object_finalize(GObject *object) {
    ShriekSourceObject *self = SHRIEK_SOURCE_OBJECT(object);
    g_free(self->name);
    g_free(self->icon_name);
    g_clear_object(&self->children);
    G_OBJECT_CLASS(shriek_source_object_parent_class)->finalize(object);
}

static void shriek_source_object_get_property(GObject *object, guint prop_id,
                                                GValue *value, GParamSpec *pspec) {
    ShriekSourceObject *self = SHRIEK_SOURCE_OBJECT(object);
    
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

static void shriek_source_object_set_property(GObject *object, guint prop_id,
                                                const GValue *value, GParamSpec *pspec) {
    ShriekSourceObject *self = SHRIEK_SOURCE_OBJECT(object);
    
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

static void shriek_source_object_class_init(ShriekSourceObjectClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = shriek_source_object_finalize;
    object_class->get_property = shriek_source_object_get_property;
    object_class->set_property = shriek_source_object_set_property;
    
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

static void shriek_source_object_init(ShriekSourceObject *self) {
    self->name = NULL;
    self->icon_name = NULL;
    self->source_ptr = NULL;
    self->children = g_list_store_new(SHRIEK_TYPE_SOURCE_OBJECT);
}

ShriekSourceObject* shriek_source_object_new(const gchar *name, const gchar *icon_name,
                                                gpointer source_ptr) {
    return g_object_new(SHRIEK_TYPE_SOURCE_OBJECT,
                        "name", name,
                        "icon-name", icon_name,
                        "source-ptr", source_ptr,
                        NULL);
}

const gchar* shriek_source_object_get_name(ShriekSourceObject *self) {
    g_return_val_if_fail(SHRIEK_IS_SOURCE_OBJECT(self), NULL);
    return self->name;
}

const gchar* shriek_source_object_get_icon_name(ShriekSourceObject *self) {
    g_return_val_if_fail(SHRIEK_IS_SOURCE_OBJECT(self), NULL);
    return self->icon_name;
}

gpointer shriek_source_object_get_source(ShriekSourceObject *self) {
    g_return_val_if_fail(SHRIEK_IS_SOURCE_OBJECT(self), NULL);
    return self->source_ptr;
}

GListModel* shriek_source_object_get_children(ShriekSourceObject *self) {
    g_return_val_if_fail(SHRIEK_IS_SOURCE_OBJECT(self), NULL);
    return G_LIST_MODEL(self->children);
}

void shriek_source_object_add_child(ShriekSourceObject *self, ShriekSourceObject *child) {
    g_return_if_fail(SHRIEK_IS_SOURCE_OBJECT(self));
    g_return_if_fail(SHRIEK_IS_SOURCE_OBJECT(child));
    g_list_store_append(self->children, child);
}

/* ============================================================================
 * ShriekPodcastObject Implementation
 * ============================================================================ */

struct _ShriekPodcastObject {
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

G_DEFINE_TYPE(ShriekPodcastObject, shriek_podcast_object, G_TYPE_OBJECT)

static void shriek_podcast_object_finalize(GObject *object) {
    ShriekPodcastObject *self = SHRIEK_PODCAST_OBJECT(object);
    g_free(self->title);
    g_free(self->author);
    G_OBJECT_CLASS(shriek_podcast_object_parent_class)->finalize(object);
}

static void shriek_podcast_object_get_property(GObject *object, guint prop_id,
                                                 GValue *value, GParamSpec *pspec) {
    ShriekPodcastObject *self = SHRIEK_PODCAST_OBJECT(object);
    
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

static void shriek_podcast_object_set_property(GObject *object, guint prop_id,
                                                 const GValue *value, GParamSpec *pspec) {
    ShriekPodcastObject *self = SHRIEK_PODCAST_OBJECT(object);
    
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

static void shriek_podcast_object_class_init(ShriekPodcastObjectClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = shriek_podcast_object_finalize;
    object_class->get_property = shriek_podcast_object_get_property;
    object_class->set_property = shriek_podcast_object_set_property;
    
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

static void shriek_podcast_object_init(ShriekPodcastObject *self) {
    self->id = 0;
    self->title = NULL;
    self->author = NULL;
}

ShriekPodcastObject* shriek_podcast_object_new(gint id, const gchar *title, const gchar *author) {
    return g_object_new(SHRIEK_TYPE_PODCAST_OBJECT,
                        "id", id,
                        "title", title,
                        "author", author,
                        NULL);
}

gint shriek_podcast_object_get_id(ShriekPodcastObject *self) {
    g_return_val_if_fail(SHRIEK_IS_PODCAST_OBJECT(self), 0);
    return self->id;
}

const gchar* shriek_podcast_object_get_title(ShriekPodcastObject *self) {
    g_return_val_if_fail(SHRIEK_IS_PODCAST_OBJECT(self), NULL);
    return self->title;
}

const gchar* shriek_podcast_object_get_author(ShriekPodcastObject *self) {
    g_return_val_if_fail(SHRIEK_IS_PODCAST_OBJECT(self), NULL);
    return self->author;
}

/* ============================================================================
 * ShriekEpisodeObject Implementation
 * ============================================================================ */

struct _ShriekEpisodeObject {
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

G_DEFINE_TYPE(ShriekEpisodeObject, shriek_episode_object, G_TYPE_OBJECT)

static void shriek_episode_object_finalize(GObject *object) {
    ShriekEpisodeObject *self = SHRIEK_EPISODE_OBJECT(object);
    g_free(self->title);
    g_free(self->date);
    g_free(self->duration);
    G_OBJECT_CLASS(shriek_episode_object_parent_class)->finalize(object);
}

static void shriek_episode_object_get_property(GObject *object, guint prop_id,
                                                 GValue *value, GParamSpec *pspec) {
    ShriekEpisodeObject *self = SHRIEK_EPISODE_OBJECT(object);
    
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

static void shriek_episode_object_set_property(GObject *object, guint prop_id,
                                                 const GValue *value, GParamSpec *pspec) {
    ShriekEpisodeObject *self = SHRIEK_EPISODE_OBJECT(object);
    
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

static void shriek_episode_object_class_init(ShriekEpisodeObjectClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = shriek_episode_object_finalize;
    object_class->get_property = shriek_episode_object_get_property;
    object_class->set_property = shriek_episode_object_set_property;
    
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

static void shriek_episode_object_init(ShriekEpisodeObject *self) {
    self->id = 0;
    self->title = NULL;
    self->date = NULL;
    self->duration = NULL;
    self->downloaded = FALSE;
}

ShriekEpisodeObject* shriek_episode_object_new(gint id, const gchar *title,
                                                  const gchar *date, const gchar *duration,
                                                  gboolean downloaded) {
    return g_object_new(SHRIEK_TYPE_EPISODE_OBJECT,
                        "id", id,
                        "title", title,
                        "date", date,
                        "duration", duration,
                        "downloaded", downloaded,
                        NULL);
}

gint shriek_episode_object_get_id(ShriekEpisodeObject *self) {
    g_return_val_if_fail(SHRIEK_IS_EPISODE_OBJECT(self), 0);
    return self->id;
}

const gchar* shriek_episode_object_get_title(ShriekEpisodeObject *self) {
    g_return_val_if_fail(SHRIEK_IS_EPISODE_OBJECT(self), NULL);
    return self->title;
}

const gchar* shriek_episode_object_get_date(ShriekEpisodeObject *self) {
    g_return_val_if_fail(SHRIEK_IS_EPISODE_OBJECT(self), NULL);
    return self->date;
}

const gchar* shriek_episode_object_get_duration(ShriekEpisodeObject *self) {
    g_return_val_if_fail(SHRIEK_IS_EPISODE_OBJECT(self), NULL);
    return self->duration;
}

gboolean shriek_episode_object_get_downloaded(ShriekEpisodeObject *self) {
    g_return_val_if_fail(SHRIEK_IS_EPISODE_OBJECT(self), FALSE);
    return self->downloaded;
}

/* ============================================================================
 * ShriekVideoObject Implementation
 * ============================================================================ */

struct _ShriekVideoObject {
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

G_DEFINE_TYPE(ShriekVideoObject, shriek_video_object, G_TYPE_OBJECT)

static void shriek_video_object_finalize(GObject *object) {
    ShriekVideoObject *self = SHRIEK_VIDEO_OBJECT(object);
    g_free(self->title);
    g_free(self->artist);
    g_free(self->duration);
    g_free(self->file_path);
    G_OBJECT_CLASS(shriek_video_object_parent_class)->finalize(object);
}

static void shriek_video_object_get_property(GObject *object, guint prop_id,
                                               GValue *value, GParamSpec *pspec) {
    ShriekVideoObject *self = SHRIEK_VIDEO_OBJECT(object);
    
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

static void shriek_video_object_set_property(GObject *object, guint prop_id,
                                               const GValue *value, GParamSpec *pspec) {
    ShriekVideoObject *self = SHRIEK_VIDEO_OBJECT(object);
    
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

static void shriek_video_object_class_init(ShriekVideoObjectClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = shriek_video_object_finalize;
    object_class->get_property = shriek_video_object_get_property;
    object_class->set_property = shriek_video_object_set_property;
    
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

static void shriek_video_object_init(ShriekVideoObject *self) {
    self->id = 0;
    self->title = NULL;
    self->artist = NULL;
    self->duration = NULL;
    self->file_path = NULL;
}

ShriekVideoObject* shriek_video_object_new(gint id, const gchar *title,
                                              const gchar *artist, const gchar *duration,
                                              const gchar *file_path) {
    return g_object_new(SHRIEK_TYPE_VIDEO_OBJECT,
                        "id", id,
                        "title", title,
                        "artist", artist,
                        "duration", duration,
                        "file-path", file_path,
                        NULL);
}

gint shriek_video_object_get_id(ShriekVideoObject *self) {
    g_return_val_if_fail(SHRIEK_IS_VIDEO_OBJECT(self), 0);
    return self->id;
}

const gchar* shriek_video_object_get_title(ShriekVideoObject *self) {
    g_return_val_if_fail(SHRIEK_IS_VIDEO_OBJECT(self), NULL);
    return self->title;
}

const gchar* shriek_video_object_get_artist(ShriekVideoObject *self) {
    g_return_val_if_fail(SHRIEK_IS_VIDEO_OBJECT(self), NULL);
    return self->artist;
}

const gchar* shriek_video_object_get_duration(ShriekVideoObject *self) {
    g_return_val_if_fail(SHRIEK_IS_VIDEO_OBJECT(self), NULL);
    return self->duration;
}

const gchar* shriek_video_object_get_file_path(ShriekVideoObject *self) {
    g_return_val_if_fail(SHRIEK_IS_VIDEO_OBJECT(self), NULL);
    return self->file_path;
}

/* ============================================================================
 * ShriekChapterObject Implementation
 * ============================================================================ */

struct _ShriekChapterObject {
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

G_DEFINE_TYPE(ShriekChapterObject, shriek_chapter_object, G_TYPE_OBJECT)

static void shriek_chapter_object_finalize(GObject *object) {
    ShriekChapterObject *self = SHRIEK_CHAPTER_OBJECT(object);
    g_free(self->title);
    g_free(self->img);
    g_free(self->url);
    G_OBJECT_CLASS(shriek_chapter_object_parent_class)->finalize(object);
}

static void shriek_chapter_object_get_property(GObject *object, guint prop_id,
                                                 GValue *value, GParamSpec *pspec) {
    ShriekChapterObject *self = SHRIEK_CHAPTER_OBJECT(object);
    
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

static void shriek_chapter_object_set_property(GObject *object, guint prop_id,
                                                 const GValue *value, GParamSpec *pspec) {
    ShriekChapterObject *self = SHRIEK_CHAPTER_OBJECT(object);
    
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

static void shriek_chapter_object_class_init(ShriekChapterObjectClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->finalize = shriek_chapter_object_finalize;
    object_class->get_property = shriek_chapter_object_get_property;
    object_class->set_property = shriek_chapter_object_set_property;
    
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

static void shriek_chapter_object_init(ShriekChapterObject *self) {
    self->start_time = 0.0;
    self->title = NULL;
    self->img = NULL;
    self->url = NULL;
}

ShriekChapterObject* shriek_chapter_object_new(gdouble start_time, const gchar *title,
                                                  const gchar *img, const gchar *url) {
    return g_object_new(SHRIEK_TYPE_CHAPTER_OBJECT,
                        "start-time", start_time,
                        "title", title,
                        "img", img,
                        "url", url,
                        NULL);
}

gdouble shriek_chapter_object_get_start_time(ShriekChapterObject *self) {
    g_return_val_if_fail(SHRIEK_IS_CHAPTER_OBJECT(self), 0.0);
    return self->start_time;
}

const gchar* shriek_chapter_object_get_title(ShriekChapterObject *self) {
    g_return_val_if_fail(SHRIEK_IS_CHAPTER_OBJECT(self), NULL);
    return self->title;
}

const gchar* shriek_chapter_object_get_img(ShriekChapterObject *self) {
    g_return_val_if_fail(SHRIEK_IS_CHAPTER_OBJECT(self), NULL);
    return self->img;
}

const gchar* shriek_chapter_object_get_url(ShriekChapterObject *self) {
    g_return_val_if_fail(SHRIEK_IS_CHAPTER_OBJECT(self), NULL);
    return self->url;
}