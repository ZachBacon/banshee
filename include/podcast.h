#ifndef PODCAST_H
#define PODCAST_H

#include <glib.h>
#include <gtk/gtk.h>
#include "database.h"

/* Podcast 2.0 namespace support */
#define PODCAST_NAMESPACE "https://podcastindex.org/namespace/1.0"

/* Podcast structure */
typedef struct {
    gint id;
    gchar *title;
    gchar *feed_url;
    gchar *link;
    gchar *description;
    gchar *author;
    gchar *image_url;
    gchar *language;
    gint64 last_updated;
    gint64 last_fetched;
    gboolean auto_download;
} Podcast;

/* Episode structure with Podcast 2.0 features */
typedef struct {
    gint id;
    gint podcast_id;
    gchar *guid;
    gchar *title;
    gchar *description;
    gchar *enclosure_url;
    gint64 enclosure_length;
    gchar *enclosure_type;
    gint64 published_date;
    gint duration;  /* seconds */
    gboolean downloaded;
    gchar *local_file_path;
    gint play_position;  /* seconds */
    gboolean played;
    
    /* Podcast 2.0 features */
    gchar *transcript_url;
    gchar *transcript_type;
    gchar *chapters_url;
    gchar *chapters_type;
    GList *persons;  /* List of PodcastPerson */
    GList *funding;  /* List of PodcastFunding */
    GList *value;    /* List of PodcastValue (Value4Value) */
    gchar *location_name;
    gdouble location_lat;
    gdouble location_lon;
    gboolean locked;
    gchar *season;
    gchar *episode_num;
} PodcastEpisode;

/* Podcast Person (host, guest, etc.) */
typedef struct {
    gchar *name;
    gchar *role;
    gchar *group;
    gchar *img;
    gchar *href;
} PodcastPerson;

/* Funding information */
typedef struct {
    gchar *url;
    gchar *message;
    gchar *platform;  /* Platform name (e.g., "Patreon", "Ko-fi") */
} PodcastFunding;

/* Value for Value (Lightning Network) */
typedef struct {
    gchar *type;
    gchar *method;
    gchar *suggested;
    GList *recipients;  /* List of ValueRecipient */
} PodcastValue;

typedef struct {
    gchar *name;
    gchar *type;
    gchar *address;
    gint split;
    gchar *custom_key;
    gchar *custom_value;
} ValueRecipient;

/* Chapter information */
typedef struct {
    gdouble start_time;
    gchar *title;
    gchar *img;
    gchar *url;
} PodcastChapter;

/* Podcast Manager */
typedef struct {
    Database *database;
    GList *podcasts;
    GThreadPool *download_pool;
    gchar *download_dir;
} PodcastManager;

/* Podcast Manager */
PodcastManager* podcast_manager_new(Database *database);
void podcast_manager_free(PodcastManager *manager);

/* Podcast operations */
gboolean podcast_manager_subscribe(PodcastManager *manager, const gchar *feed_url);
gboolean podcast_manager_unsubscribe(PodcastManager *manager, gint podcast_id);
void podcast_manager_update_feed(PodcastManager *manager, gint podcast_id);
void podcast_manager_update_all_feeds(PodcastManager *manager);
GList* podcast_manager_get_podcasts(PodcastManager *manager);
GList* podcast_manager_get_episodes(PodcastManager *manager, gint podcast_id);

/* Episode operations */
void podcast_episode_download(PodcastManager *manager, PodcastEpisode *episode);
void podcast_episode_delete(PodcastManager *manager, PodcastEpisode *episode);
void podcast_episode_mark_played(PodcastManager *manager, gint episode_id, gboolean played);
void podcast_episode_update_position(PodcastManager *manager, gint episode_id, gint position);

/* Chapter operations */
GList* podcast_episode_get_chapters(PodcastManager *manager, gint episode_id);
PodcastChapter* podcast_chapter_at_time(GList *chapters, gdouble time);

/* Memory management */
void podcast_free(Podcast *podcast);
void podcast_episode_free(PodcastEpisode *episode);
void podcast_person_free(PodcastPerson *person);
void podcast_funding_free(PodcastFunding *funding);
void podcast_value_free(PodcastValue *value);
void podcast_chapter_free(PodcastChapter *chapter);

/* Copy functions for deep copying */
PodcastChapter* podcast_chapter_copy(const PodcastChapter *chapter);
PodcastFunding* podcast_funding_copy(const PodcastFunding *funding);

/* RSS Feed parsing */
Podcast* podcast_parse_feed(const gchar *feed_url);
GList* podcast_parse_episodes(const gchar *xml_data, gint podcast_id);

/* HTTP fetching utility */
gchar* fetch_url(const gchar *url);

#endif /* PODCAST_H */
