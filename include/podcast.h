#ifndef PODCAST_H
#define PODCAST_H

#include <glib.h>
#include <gtk/gtk.h>
#include "database.h"  /* For Database and podcast type forward declarations */

/* Podcast 2.0 namespace support */
#define PODCAST_NAMESPACE "https://podcastindex.org/namespace/1.0"

/* Podcast structure */
struct Podcast {
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
    GList *funding;  /* List of PodcastFunding */
    GList *images;   /* List of PodcastImage */
    GList *value;    /* List of PodcastValue (Value4Value) */
    GList *live_items;     /* List of PodcastLiveItem */
    gboolean has_active_live;  /* TRUE if any live item has status=live */
};

/* Episode structure with Podcast 2.0 features */
struct PodcastEpisode {
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
    GList *images;   /* List of PodcastImage */
    gchar *location_name;
    gdouble location_lat;
    gdouble location_lon;
    gboolean locked;
    gchar *season;
    gchar *episode_num;
};

/* Podcast Person (host, guest, etc.) */
typedef struct {
    gchar *name;
    gchar *role;
    gchar *group;
    gchar *img;
    gchar *href;
} PodcastPerson;

/* Podcast Image (Podcast 2.0) */
typedef struct {
    gchar *href;           /* Required: URL to the image */
    gchar *alt;            /* Recommended: Accessibility text */
    gchar *aspect_ratio;   /* Recommended: e.g., "1/1", "16/9" */
    gint width;            /* Recommended: Width in pixels */
    gint height;           /* Optional: Height in pixels */
    gchar *type;           /* Optional: MIME type */
    gchar *purpose;        /* Optional: Space-separated tokens */
} PodcastImage;

/* Funding information */
struct PodcastFunding {
    gchar *url;
    gchar *message;
    gchar *platform;  /* Platform name (e.g., "Patreon", "Ko-fi") */
};

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
    gboolean fee;
    gchar *custom_key;
    gchar *custom_value;
} ValueRecipient;

/* Content Link for Live Items */
typedef struct {
    gchar *href;      /* Required: URL to the content */
    gchar *text;      /* Optional: Display text */
} PodcastContentLink;

/* Live Item status enum */
typedef enum {
    LIVE_STATUS_PENDING,
    LIVE_STATUS_LIVE,
    LIVE_STATUS_ENDED
} LiveItemStatus;

/* Live Item structure (podcast:liveItem) */
typedef struct {
    gint id;
    gint podcast_id;
    gchar *guid;
    gchar *title;
    gchar *description;
    gchar *enclosure_url;
    gchar *enclosure_type;
    gint64 enclosure_length;
    gint64 start_time;       /* ISO8601 timestamp as Unix time */
    gint64 end_time;         /* ISO8601 timestamp as Unix time */
    LiveItemStatus status;   /* pending, live, or ended */
    GList *content_links;    /* List of PodcastContentLink */
    GList *persons;          /* List of PodcastPerson */
    gchar *image_url;        /* Image URL for this live item */
} PodcastLiveItem;

/* Chapter information */
typedef struct {
    gdouble start_time;
    gchar *title;
    gchar *img;
    gchar *url;
} PodcastChapter;

/* Forward declaration for PodcastManager */
typedef struct _PodcastManager PodcastManager;

/* Download progress callback */
typedef void (*DownloadProgressCallback)(gpointer user_data, gint episode_id, gdouble progress, const gchar *status);
typedef void (*DownloadCompleteCallback)(gpointer user_data, gint episode_id, gboolean success, const gchar *error_msg);

/* Download task structure */
typedef struct {
    PodcastEpisode *episode;
    PodcastManager *manager;
    DownloadProgressCallback progress_callback;
    DownloadCompleteCallback complete_callback;
    gpointer user_data;
    gboolean cancelled;
} DownloadTask;

/* Podcast Manager */
struct _PodcastManager {
    Database *database;
    GList *podcasts;
    GThreadPool *download_pool;
    gchar *download_dir;
    GHashTable *active_downloads; /* episode_id -> DownloadTask */
    GMutex downloads_mutex;
    guint update_timer_id;  /* Timer for automatic feed updates */
    gint update_interval_days;  /* Update interval in days */
    volatile gboolean update_cancelled;  /* Flag to cancel feed updates */
    gboolean update_in_progress;  /* Flag indicating update is running */
    void *curl_handle;  /* Reusable curl handle for feed updates (CURL*) */
};

/* Podcast Manager */
PodcastManager* podcast_manager_new(Database *database);
void podcast_manager_free(PodcastManager *manager);

/* Auto-update timer management */
void podcast_manager_start_auto_update(PodcastManager *manager, gint interval_days);
void podcast_manager_stop_auto_update(PodcastManager *manager);

/* Podcast operations */
gboolean podcast_manager_subscribe(PodcastManager *manager, const gchar *feed_url);
gboolean podcast_manager_unsubscribe(PodcastManager *manager, gint podcast_id);
void podcast_manager_update_feed(PodcastManager *manager, gint podcast_id);
void podcast_manager_update_all_feeds(PodcastManager *manager);
void podcast_manager_cancel_updates(PodcastManager *manager);
gboolean podcast_manager_is_updating(PodcastManager *manager);
GList* podcast_manager_get_podcasts(PodcastManager *manager);
GList* podcast_manager_get_episodes(PodcastManager *manager, gint podcast_id);

/* Episode operations */
void podcast_episode_download(PodcastManager *manager, PodcastEpisode *episode, 
                             DownloadProgressCallback progress_cb, 
                             DownloadCompleteCallback complete_cb,
                             gpointer user_data);
void podcast_episode_cancel_download(PodcastManager *manager, gint episode_id);
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
void podcast_image_free(PodcastImage *image);
void podcast_funding_free(PodcastFunding *funding);
void podcast_value_free(PodcastValue *value);
void value_recipient_free(ValueRecipient *recipient);
void podcast_chapter_free(PodcastChapter *chapter);
void podcast_content_link_free(PodcastContentLink *link);
void podcast_live_item_free(PodcastLiveItem *live_item);

/* Copy functions for deep copying */
PodcastChapter* podcast_chapter_copy(const PodcastChapter *chapter);
PodcastImage* podcast_image_copy(const PodcastImage *image);
PodcastFunding* podcast_funding_copy(const PodcastFunding *funding);
PodcastValue* podcast_value_copy(const PodcastValue *value);
ValueRecipient* value_recipient_copy(const ValueRecipient *recipient);
PodcastContentLink* podcast_content_link_copy(const PodcastContentLink *link);
PodcastLiveItem* podcast_live_item_copy(const PodcastLiveItem *live_item);

/* Live item operations */
GList* podcast_manager_get_live_items(PodcastManager *manager, gint podcast_id);
gboolean podcast_has_active_live_item(Podcast *podcast);
const gchar* podcast_live_status_to_string(LiveItemStatus status);
LiveItemStatus podcast_live_status_from_string(const gchar *status_str);

/* RSS Feed parsing */
Podcast* podcast_parse_feed(const gchar *feed_url);
GList* podcast_parse_episodes(const gchar *xml_data, gint podcast_id);

/* HTTP fetching utility */
gchar* fetch_url(const gchar *url);
gchar* fetch_binary_url(const gchar *url, gsize *out_size);

/* Podcast image utilities */
PodcastImage* podcast_get_best_image(GList *images, const gchar *purpose);
const gchar* podcast_get_display_image_url(Podcast *podcast);
const gchar* podcast_episode_get_display_image_url(PodcastEpisode *episode, Podcast *podcast);

#endif /* PODCAST_H */
