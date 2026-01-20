#define _XOPEN_SOURCE
#include "podcast.h"
#include "database.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <curl/curl.h>
#include <json-glib/json-glib.h>
#include <glib/gstdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sqlite3.h>
#include <stdint.h>

#ifdef _WIN32
/* Helper to convert month name to number, returns -1 if not found */
static int parse_month_name(const char *month_name) {
    static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < 12; i++) {
        if (strcmp(month_name, months[i]) == 0) {
            return i;
        }
    }
    return -1;  /* Month not found */
}

/* Minimal strptime implementation for Windows */
static char *strptime(const char *buf, const char *fmt, struct tm *tm) {
    /* Simple implementation for the specific formats used in this file */
    if (strcmp(fmt, "%a, %d %b %Y %H:%M:%S") == 0) {
        /* RFC 822: "Mon, 30 Dec 2025 10:00:00 GMT" */
        char day_name[4], month_name[4];
        int n = sscanf(buf, "%3s, %d %3s %d %d:%d:%d",
                       day_name, &tm->tm_mday, month_name, &tm->tm_year,
                       &tm->tm_hour, &tm->tm_min, &tm->tm_sec);
        if (n != 7) return NULL;
        
        /* Convert month name to number */
        int mon = parse_month_name(month_name);
        if (mon < 0) return NULL;  /* Unknown month */
        tm->tm_mon = mon;
        tm->tm_year -= 1900;  /* tm_year is years since 1900 */
        tm->tm_isdst = -1;
        return (char *)buf + strlen(buf);
    } else if (strcmp(fmt, "%d %b %Y %H:%M:%S") == 0) {
        /* Alternative format: "30 Dec 2025 10:00:00" */
        char month_name[4];
        int n = sscanf(buf, "%d %3s %d %d:%d:%d",
                       &tm->tm_mday, month_name, &tm->tm_year,
                       &tm->tm_hour, &tm->tm_min, &tm->tm_sec);
        if (n != 6) return NULL;
        
        int mon = parse_month_name(month_name);
        if (mon < 0) return NULL;  /* Unknown month */
        tm->tm_mon = mon;
        tm->tm_year -= 1900;
        tm->tm_isdst = -1;
        return (char *)buf + strlen(buf);
    }
    return NULL;
}
#endif

/* Memory buffer for CURL */
typedef struct {
    gchar *data;
    gsize size;
} MemoryBuffer;

static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryBuffer *mem = (MemoryBuffer *)userp;
    
    gchar *ptr = g_realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) return 0;
    
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    
    return realsize;
}

/* Internal fetch function that can optionally reuse a curl handle */
static gchar* fetch_url_with_handle(const gchar *url, CURL *reuse_handle) {
    CURL *curl;
    CURLcode res;
    MemoryBuffer chunk = {NULL, 0};
    gboolean own_handle = FALSE;
    
    if (reuse_handle) {
        curl = reuse_handle;
        curl_easy_reset(curl);  /* Reset options but keep connection */
    } else {
        curl = curl_easy_init();
        own_handle = TRUE;
    }
    
    if (!curl) return NULL;
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Shriek/1.0 (Podcast 2.0)");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    res = curl_easy_perform(curl);
    
    if (own_handle) {
        curl_easy_cleanup(curl);
    }
    
    if (res != CURLE_OK) {
        g_warning("Failed to fetch URL '%s': %s", url, curl_easy_strerror(res));
        g_free(chunk.data);
        return NULL;
    }
    
    return chunk.data;
}

gchar* fetch_url(const gchar *url) {
    return fetch_url_with_handle(url, NULL);
}

gchar* fetch_binary_url(const gchar *url, gsize *out_size) {
    CURL *curl;
    CURLcode res;
    MemoryBuffer chunk = {NULL, 0};
    
    curl = curl_easy_init();
    if (!curl) return NULL;
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Shriek/1.0 (Podcast 2.0)");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        g_warning("Failed to fetch binary URL '%s': %s", url, curl_easy_strerror(res));
        g_free(chunk.data);
        return NULL;
    }
    
    if (out_size) {
        *out_size = chunk.size;
    }
    
    return chunk.data;
}

PodcastManager* podcast_manager_new(Database *database) {
    PodcastManager *manager = g_new0(PodcastManager, 1);
    manager->database = database;
    manager->update_timer_id = 0;
    manager->update_interval_minutes = 0;
    
    /* Initialize curl globally (thread-safe, only once) */
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    /* Load existing podcasts from database */
    manager->podcasts = database_get_podcasts(database);
    
    /* Create download directory in user's Music folder (cross-platform) */
    const gchar *music_dir = g_get_user_special_dir(G_USER_DIRECTORY_MUSIC);
    if (music_dir) {
        manager->download_dir = g_build_filename(music_dir, "Podcasts", NULL);
    } else {
        /* Fallback to home directory if Music folder not available */
        manager->download_dir = g_build_filename(g_get_home_dir(), "Music", "Podcasts", NULL);
    }
    g_mkdir_with_parents(manager->download_dir, 0755);
    
    /* Initialize downloads tracking - NULL destroy func since we manage task lifecycle */
    manager->active_downloads = g_hash_table_new(g_direct_hash, g_direct_equal);
    g_mutex_init(&manager->downloads_mutex);
    
    /* Create thread pool for downloads (max 3 concurrent downloads) */
    manager->download_pool = NULL;
    
    /* Create reusable curl handle for feed updates */
    manager->curl_handle = curl_easy_init();
    
    return manager;
}

void podcast_manager_free(PodcastManager *manager) {
    if (!manager) return;
    
    /* Stop auto-update timer */
    podcast_manager_stop_auto_update(manager);
    
    /* Cleanup reusable curl handle */
    if (manager->curl_handle) {
        curl_easy_cleanup((CURL *)manager->curl_handle);
    }
    
    /* Cleanup curl globally */
    curl_global_cleanup();
    
    g_list_free_full(manager->podcasts, (GDestroyNotify)podcast_free);
    if (manager->download_pool) {
        g_thread_pool_free(manager->download_pool, FALSE, TRUE);
    }
    if (manager->active_downloads) {
        g_hash_table_destroy(manager->active_downloads);
    }
    g_mutex_clear(&manager->downloads_mutex);
    g_free(manager->download_dir);
    g_free(manager);
}

void podcast_free(Podcast *podcast) {
    if (!podcast) return;
    
    g_free(podcast->title);
    g_free(podcast->feed_url);
    g_free(podcast->link);
    g_free(podcast->description);
    g_free(podcast->author);
    g_free(podcast->image_url);
    g_free(podcast->language);
    g_list_free_full(podcast->funding, (GDestroyNotify)podcast_funding_free);
    g_list_free_full(podcast->images, (GDestroyNotify)podcast_image_free);
    g_list_free_full(podcast->value, (GDestroyNotify)podcast_value_free);
    g_list_free_full(podcast->live_items, (GDestroyNotify)podcast_live_item_free);
    
    g_free(podcast);
}

void podcast_episode_free(PodcastEpisode *episode) {
    if (!episode) return;
    
    g_free(episode->guid);
    g_free(episode->title);
    g_free(episode->description);
    g_free(episode->enclosure_url);
    g_free(episode->enclosure_type);
    g_free(episode->local_file_path);
    g_free(episode->transcript_url);
    g_free(episode->transcript_type);
    g_free(episode->chapters_url);
    g_free(episode->chapters_type);
    g_free(episode->location_name);
    g_free(episode->season);
    g_free(episode->episode_num);
    
    g_list_free_full(episode->persons, (GDestroyNotify)podcast_person_free);
    g_list_free_full(episode->funding, (GDestroyNotify)podcast_funding_free);
    g_list_free_full(episode->value, (GDestroyNotify)podcast_value_free);
    g_list_free_full(episode->images, (GDestroyNotify)podcast_image_free);
    
    g_free(episode);
}

void podcast_person_free(PodcastPerson *person) {
    if (!person) return;
    g_free(person->name);
    g_free(person->role);
    g_free(person->group);
    g_free(person->img);
    g_free(person->href);
    g_free(person);
}

void podcast_funding_free(PodcastFunding *funding) {
    if (!funding) return;
    g_free(funding->url);
    g_free(funding->message);
    g_free(funding->platform);
    g_free(funding);
}

void podcast_image_free(PodcastImage *image) {
    if (!image) return;
    g_free(image->href);
    g_free(image->alt);
    g_free(image->aspect_ratio);
    g_free(image->type);
    g_free(image->purpose);
    g_free(image);
}

void value_recipient_free(ValueRecipient *recipient) {
    if (!recipient) return;
    g_free(recipient->name);
    g_free(recipient->type);
    g_free(recipient->address);
    g_free(recipient->custom_key);
    g_free(recipient->custom_value);
    g_free(recipient);
}

void podcast_value_free(PodcastValue *value) {
    if (!value) return;
    g_free(value->type);
    g_free(value->method);
    g_free(value->suggested);
    g_list_free_full(value->recipients, (GDestroyNotify)value_recipient_free);
    g_free(value);
}

void podcast_chapter_free(PodcastChapter *chapter) {
    if (!chapter) return;
    g_free(chapter->title);
    g_free(chapter->img);
    g_free(chapter->url);
    g_free(chapter);
}

/* Copy functions for deep copying structures */
PodcastChapter* podcast_chapter_copy(const PodcastChapter *chapter) {
    if (!chapter) return NULL;
    
    PodcastChapter *copy = g_new0(PodcastChapter, 1);
    copy->start_time = chapter->start_time;
    copy->title = g_strdup(chapter->title);
    copy->img = g_strdup(chapter->img);
    copy->url = g_strdup(chapter->url);
    
    return copy;
}

PodcastFunding* podcast_funding_copy(const PodcastFunding *funding) {
    if (!funding) return NULL;
    
    PodcastFunding *copy = g_new0(PodcastFunding, 1);
    copy->url = g_strdup(funding->url);
    copy->message = g_strdup(funding->message);
    copy->platform = g_strdup(funding->platform);
    
    return copy;
}

PodcastImage* podcast_image_copy(const PodcastImage *image) {
    if (!image) return NULL;
    
    PodcastImage *copy = g_new0(PodcastImage, 1);
    copy->href = g_strdup(image->href);
    copy->alt = g_strdup(image->alt);
    copy->aspect_ratio = g_strdup(image->aspect_ratio);
    copy->width = image->width;
    copy->height = image->height;
    copy->type = g_strdup(image->type);
    copy->purpose = g_strdup(image->purpose);
    
    return copy;
}

ValueRecipient* value_recipient_copy(const ValueRecipient *recipient) {
    if (!recipient) return NULL;
    
    ValueRecipient *copy = g_new0(ValueRecipient, 1);
    copy->name = g_strdup(recipient->name);
    copy->type = g_strdup(recipient->type);
    copy->address = g_strdup(recipient->address);
    copy->split = recipient->split;
    copy->fee = recipient->fee;
    copy->custom_key = g_strdup(recipient->custom_key);
    copy->custom_value = g_strdup(recipient->custom_value);
    
    return copy;
}

PodcastValue* podcast_value_copy(const PodcastValue *value) {
    if (!value) return NULL;
    
    PodcastValue *copy = g_new0(PodcastValue, 1);
    copy->type = g_strdup(value->type);
    copy->method = g_strdup(value->method);
    copy->suggested = g_strdup(value->suggested);
    copy->recipients = g_list_copy_deep(value->recipients, (GCopyFunc)value_recipient_copy, NULL);
    
    return copy;
}

void podcast_content_link_free(PodcastContentLink *link) {
    if (!link) return;
    g_free(link->href);
    g_free(link->text);
    g_free(link);
}

PodcastContentLink* podcast_content_link_copy(const PodcastContentLink *link) {
    if (!link) return NULL;
    
    PodcastContentLink *copy = g_new0(PodcastContentLink, 1);
    copy->href = g_strdup(link->href);
    copy->text = g_strdup(link->text);
    
    return copy;
}

void podcast_live_item_free(PodcastLiveItem *live_item) {
    if (!live_item) return;
    g_free(live_item->guid);
    g_free(live_item->title);
    g_free(live_item->description);
    g_free(live_item->enclosure_url);
    g_free(live_item->enclosure_type);
    g_free(live_item->image_url);
    g_list_free_full(live_item->content_links, (GDestroyNotify)podcast_content_link_free);
    g_list_free_full(live_item->persons, (GDestroyNotify)podcast_person_free);
    g_free(live_item);
}

PodcastLiveItem* podcast_live_item_copy(const PodcastLiveItem *live_item) {
    if (!live_item) return NULL;
    
    PodcastLiveItem *copy = g_new0(PodcastLiveItem, 1);
    copy->id = live_item->id;
    copy->podcast_id = live_item->podcast_id;
    copy->guid = g_strdup(live_item->guid);
    copy->title = g_strdup(live_item->title);
    copy->description = g_strdup(live_item->description);
    copy->enclosure_url = g_strdup(live_item->enclosure_url);
    copy->enclosure_type = g_strdup(live_item->enclosure_type);
    copy->enclosure_length = live_item->enclosure_length;
    copy->start_time = live_item->start_time;
    copy->end_time = live_item->end_time;
    copy->status = live_item->status;
    copy->image_url = g_strdup(live_item->image_url);
    copy->content_links = g_list_copy_deep(live_item->content_links, (GCopyFunc)podcast_content_link_copy, NULL);
    copy->persons = NULL;  /* Deep copy persons if needed */
    
    return copy;
}

/* Live item status conversion functions */
const gchar* podcast_live_status_to_string(LiveItemStatus status) {
    switch (status) {
        case LIVE_STATUS_PENDING: return "pending";
        case LIVE_STATUS_LIVE: return "live";
        case LIVE_STATUS_ENDED: return "ended";
        default: return "pending";
    }
}

LiveItemStatus podcast_live_status_from_string(const gchar *status_str) {
    if (!status_str) return LIVE_STATUS_PENDING;
    if (g_strcmp0(status_str, "live") == 0) return LIVE_STATUS_LIVE;
    if (g_strcmp0(status_str, "ended") == 0) return LIVE_STATUS_ENDED;
    return LIVE_STATUS_PENDING;
}

gboolean podcast_has_active_live_item(Podcast *podcast) {
    if (!podcast || !podcast->live_items) return FALSE;
    
    for (GList *l = podcast->live_items; l != NULL; l = l->next) {
        PodcastLiveItem *item = (PodcastLiveItem *)l->data;
        if (item->status == LIVE_STATUS_LIVE) {
            return TRUE;
        }
    }
    return FALSE;
}

static xmlChar* get_node_content(xmlNodePtr node, const gchar *name) {
    xmlNodePtr cur = node->children;
    while (cur) {
        if (cur->type == XML_ELEMENT_NODE && xmlStrcmp(cur->name, (const xmlChar *)name) == 0) {
            return xmlNodeGetContent(cur);
        }
        cur = cur->next;
    }
    return NULL;
}

static xmlChar* get_node_ns_content(xmlNodePtr node, const gchar *ns, const gchar *name) {
    xmlNodePtr cur = node->children;
    while (cur) {
        if (cur->type == XML_ELEMENT_NODE && 
            xmlStrcmp(cur->name, (const xmlChar *)name) == 0) {
            if (cur->ns && xmlStrcmp(cur->ns->href, (const xmlChar *)ns) == 0) {
                return xmlNodeGetContent(cur);
            }
        }
        cur = cur->next;
    }
    return NULL;
}

static xmlChar* get_node_ns_prefix_content(xmlNodePtr node, const gchar *prefix, const gchar *name) {
    xmlNodePtr cur = node->children;
    while (cur) {
        if (cur->type == XML_ELEMENT_NODE && 
            xmlStrcmp(cur->name, (const xmlChar *)name) == 0) {
            if (cur->ns && cur->ns->prefix && xmlStrcmp(cur->ns->prefix, (const xmlChar *)prefix) == 0) {
                return xmlNodeGetContent(cur);
            }
        }
        cur = cur->next;
    }
    return NULL;
}

/* Helper to get XML property as GLib string (caller must free) */
static gchar* xml_get_prop_string(xmlNodePtr node, const gchar *name) {
    xmlChar *val = xmlGetProp(node, (const xmlChar *)name);
    if (!val) return NULL;
    gchar *result = g_strdup((gchar *)val);
    xmlFree(val);
    return result;
}

/* Helper to check if node is in the Podcast 2.0 namespace */
static gboolean is_podcast_namespace(xmlNodePtr node) {
    return node->ns && 
           (xmlStrcmp(node->ns->href, (const xmlChar *)PODCAST_NAMESPACE) == 0 ||
            (node->ns->prefix && xmlStrcmp(node->ns->prefix, (const xmlChar *)"podcast") == 0));
}

/* Parse a <podcast:image> element into PodcastImage struct */
static PodcastImage* parse_podcast_image_node(xmlNodePtr node) {
    if (!node) return NULL;
    
    PodcastImage *image = g_new0(PodcastImage, 1);
    
    image->href = xml_get_prop_string(node, "href");
    image->alt = xml_get_prop_string(node, "alt");
    image->aspect_ratio = xml_get_prop_string(node, "aspect-ratio");
    image->type = xml_get_prop_string(node, "type");
    image->purpose = xml_get_prop_string(node, "purpose");
    
    gchar *width_str = xml_get_prop_string(node, "width");
    gchar *height_str = xml_get_prop_string(node, "height");
    
    if (width_str) {
        image->width = (gint)g_ascii_strtoll(width_str, NULL, 10);
        g_free(width_str);
    }
    if (height_str) {
        image->height = (gint)g_ascii_strtoll(height_str, NULL, 10);
        g_free(height_str);
    }
    
    /* Return NULL if no href (required field) */
    if (!image->href) {
        podcast_image_free(image);
        return NULL;
    }
    
    return image;
}

/* Parse a <podcast:funding> element into PodcastFunding struct */
static PodcastFunding* parse_podcast_funding_node(xmlNodePtr node) {
    if (!node) return NULL;
    
    PodcastFunding *funding = g_new0(PodcastFunding, 1);
    
    funding->url = xml_get_prop_string(node, "url");
    
    xmlChar *content = xmlNodeGetContent(node);
    if (content) {
        funding->message = g_strdup((gchar *)content);
        xmlFree(content);
    }
    
    /* Return NULL if no URL (required field) */
    if (!funding->url) {
        podcast_funding_free(funding);
        return NULL;
    }
    
    return funding;
}

/* Parse a <podcast:valueRecipient> element into ValueRecipient struct */
static ValueRecipient* parse_value_recipient_node(xmlNodePtr node) {
    if (!node) return NULL;
    
    ValueRecipient *recipient = g_new0(ValueRecipient, 1);
    
    recipient->name = xml_get_prop_string(node, "name");
    recipient->type = xml_get_prop_string(node, "type");
    recipient->address = xml_get_prop_string(node, "address");
    recipient->custom_key = xml_get_prop_string(node, "customKey");
    recipient->custom_value = xml_get_prop_string(node, "customValue");
    
    gchar *split_str = xml_get_prop_string(node, "split");
    gchar *fee_str = xml_get_prop_string(node, "fee");
    
    if (split_str) {
        recipient->split = (gint)g_ascii_strtoll(split_str, NULL, 10);
        g_free(split_str);
    }
    if (fee_str) {
        recipient->fee = (g_strcmp0(fee_str, "true") == 0);
        g_free(fee_str);
    }
    
    return recipient;
}

/* Parse a <podcast:value> element into PodcastValue struct */
static PodcastValue* parse_podcast_value_node(xmlNodePtr node) {
    if (!node) return NULL;
    
    PodcastValue *value = g_new0(PodcastValue, 1);
    
    value->type = xml_get_prop_string(node, "type");
    value->method = xml_get_prop_string(node, "method");
    value->suggested = xml_get_prop_string(node, "suggested");
    
    /* Parse valueRecipient child elements */
    xmlNodePtr child = node->children;
    while (child) {
        if (child->type == XML_ELEMENT_NODE && 
            xmlStrcmp(child->name, (const xmlChar *)"valueRecipient") == 0 &&
            is_podcast_namespace(child)) {
            
            ValueRecipient *recipient = parse_value_recipient_node(child);
            if (recipient) {
                value->recipients = g_list_prepend(value->recipients, recipient);
                g_debug("  Found value recipient: %s (%s) - %d%% split",
                        recipient->name ? recipient->name : "Unknown",
                        recipient->address ? recipient->address : "No address",
                        recipient->split);
            }
        }
        child = child->next;
    }
    value->recipients = g_list_reverse(value->recipients);
    
    /* Return NULL if missing required fields */
    if (!value->type || !value->method) {
        podcast_value_free(value);
        return NULL;
    }
    
    g_debug("Found value: type=%s, method=%s, suggested=%s", 
            value->type, value->method, 
            value->suggested ? value->suggested : "None");
    
    return value;
}

/* Parse all Podcast 2.0 elements from a parent node (channel or item) */
static void parse_podcast_ns_elements(xmlNodePtr parent, 
                                      GList **images_out,
                                      GList **funding_out,
                                      GList **value_out) {
    xmlNodePtr cur = parent->children;
    
    while (cur) {
        if (cur->type == XML_ELEMENT_NODE && is_podcast_namespace(cur)) {
            if (xmlStrcmp(cur->name, (const xmlChar *)"image") == 0) {
                PodcastImage *image = parse_podcast_image_node(cur);
                if (image && images_out) {
                    *images_out = g_list_prepend(*images_out, image);
                }
            } else if (xmlStrcmp(cur->name, (const xmlChar *)"funding") == 0) {
                PodcastFunding *funding = parse_podcast_funding_node(cur);
                if (funding && funding_out) {
                    *funding_out = g_list_prepend(*funding_out, funding);
                }
            } else if (xmlStrcmp(cur->name, (const xmlChar *)"value") == 0) {
                PodcastValue *value = parse_podcast_value_node(cur);
                if (value && value_out) {
                    *value_out = g_list_prepend(*value_out, value);
                }
            }
        }
        cur = cur->next;
    }
    
    /* Reverse lists to maintain order */
    if (images_out) *images_out = g_list_reverse(*images_out);
    if (funding_out) *funding_out = g_list_reverse(*funding_out);
    if (value_out) *value_out = g_list_reverse(*value_out);
}

/* Parse a <podcast:contentLink> element into PodcastContentLink struct */
static PodcastContentLink* parse_podcast_content_link_node(xmlNodePtr node) {
    if (!node) return NULL;
    
    PodcastContentLink *link = g_new0(PodcastContentLink, 1);
    
    link->href = xml_get_prop_string(node, "href");
    
    xmlChar *content = xmlNodeGetContent(node);
    if (content) {
        link->text = g_strdup((gchar *)content);
        xmlFree(content);
    }
    
    /* Return NULL if no href (required field) */
    if (!link->href) {
        podcast_content_link_free(link);
        return NULL;
    }
    
    return link;
}

/* Parse a <podcast:liveItem> element into PodcastLiveItem struct */
static PodcastLiveItem* parse_podcast_live_item_node(xmlNodePtr node, gint podcast_id) {
    if (!node) return NULL;
    
    PodcastLiveItem *live_item = g_new0(PodcastLiveItem, 1);
    live_item->podcast_id = podcast_id;
    
    /* Parse required attributes */
    gchar *status_str = xml_get_prop_string(node, "status");
    live_item->status = podcast_live_status_from_string(status_str);
    g_free(status_str);
    
    gchar *start_str = xml_get_prop_string(node, "start");
    if (start_str) {
        GDateTime *dt = g_date_time_new_from_iso8601(start_str, NULL);
        if (dt) {
            live_item->start_time = g_date_time_to_unix(dt);
            g_date_time_unref(dt);
        }
        g_free(start_str);
    }
    
    gchar *end_str = xml_get_prop_string(node, "end");
    if (end_str) {
        GDateTime *dt = g_date_time_new_from_iso8601(end_str, NULL);
        if (dt) {
            live_item->end_time = g_date_time_to_unix(dt);
            g_date_time_unref(dt);
        }
        g_free(end_str);
    }
    
    /* Parse child elements */
    xmlNodePtr child = node->children;
    while (child) {
        if (child->type == XML_ELEMENT_NODE) {
            /* Standard RSS elements */
            if (xmlStrcmp(child->name, (const xmlChar *)"title") == 0 && !child->ns) {
                xmlChar *content = xmlNodeGetContent(child);
                if (content) {
                    live_item->title = g_strdup((gchar *)content);
                    xmlFree(content);
                }
            } else if (xmlStrcmp(child->name, (const xmlChar *)"description") == 0 && !child->ns) {
                xmlChar *content = xmlNodeGetContent(child);
                if (content) {
                    live_item->description = g_strdup((gchar *)content);
                    xmlFree(content);
                }
            } else if (xmlStrcmp(child->name, (const xmlChar *)"guid") == 0 && !child->ns) {
                xmlChar *content = xmlNodeGetContent(child);
                if (content) {
                    live_item->guid = g_strdup((gchar *)content);
                    xmlFree(content);
                }
            } else if (xmlStrcmp(child->name, (const xmlChar *)"enclosure") == 0 && !child->ns) {
                live_item->enclosure_url = xml_get_prop_string(child, "url");
                live_item->enclosure_type = xml_get_prop_string(child, "type");
                gchar *length_str = xml_get_prop_string(child, "length");
                if (length_str) {
                    live_item->enclosure_length = g_ascii_strtoll(length_str, NULL, 10);
                    g_free(length_str);
                }
            }
            /* Podcast namespace elements */
            else if (is_podcast_namespace(child)) {
                if (xmlStrcmp(child->name, (const xmlChar *)"contentLink") == 0) {
                    PodcastContentLink *link = parse_podcast_content_link_node(child);
                    if (link) {
                        live_item->content_links = g_list_prepend(live_item->content_links, link);
                    }
                } else if (xmlStrcmp(child->name, (const xmlChar *)"images") == 0 ||
                           xmlStrcmp(child->name, (const xmlChar *)"image") == 0) {
                    gchar *href = xml_get_prop_string(child, "href");
                    if (!href) {
                        href = xml_get_prop_string(child, "srcset");
                        /* Get first URL from srcset if present */
                        if (href) {
                            gchar *space = strchr(href, ' ');
                            if (space) *space = '\0';
                        }
                    }
                    if (href) {
                        g_free(live_item->image_url);
                        live_item->image_url = href;
                    }
                }
            }
        }
        child = child->next;
    }
    
    live_item->content_links = g_list_reverse(live_item->content_links);
    
    g_debug("Found live item: %s (status=%s, start=%ld)", 
            live_item->title ? live_item->title : "Untitled",
            podcast_live_status_to_string(live_item->status),
            (long)live_item->start_time);
    
    return live_item;
}

/* Parse all live items from a channel */
static GList* parse_podcast_live_items(xmlNodePtr channel, gint podcast_id) {
    GList *live_items = NULL;
    
    xmlNodePtr cur = channel->children;
    while (cur) {
        if (cur->type == XML_ELEMENT_NODE && 
            xmlStrcmp(cur->name, (const xmlChar *)"liveItem") == 0 &&
            is_podcast_namespace(cur)) {
            
            PodcastLiveItem *live_item = parse_podcast_live_item_node(cur, podcast_id);
            if (live_item) {
                live_items = g_list_prepend(live_items, live_item);
            }
        }
        cur = cur->next;
    }
    
    return g_list_reverse(live_items);
}

/* Internal version that can reuse a curl handle */
static Podcast* podcast_parse_feed_internal(const gchar *feed_url, CURL *curl_handle) {
    gchar *xml_data = fetch_url_with_handle(feed_url, curl_handle);
    if (!xml_data) {
        g_warning("Failed to fetch feed: %s", feed_url);
        return NULL;
    }
    
    xmlDocPtr doc = xmlReadMemory(xml_data, strlen(xml_data), feed_url, NULL, 0);
    g_free(xml_data);
    
    if (!doc) {
        g_warning("Failed to parse XML feed");
        return NULL;
    }
    
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root || xmlStrcmp(root->name, (const xmlChar *)"rss") != 0) {
        xmlFreeDoc(doc);
        return NULL;
    }
    
    xmlNodePtr channel = root->children;
    while (channel && xmlStrcmp(channel->name, (const xmlChar *)"channel") != 0) {
        channel = channel->next;
    }
    
    if (!channel) {
        xmlFreeDoc(doc);
        return NULL;
    }
    
    Podcast *podcast = g_new0(Podcast, 1);
    podcast->feed_url = g_strdup(feed_url);
    podcast->last_fetched = g_get_real_time() / G_USEC_PER_SEC;
    
    xmlChar *content;
    if ((content = get_node_content(channel, "title"))) {
        podcast->title = g_strdup((gchar *)content);
        xmlFree(content);
    }
    if ((content = get_node_content(channel, "link"))) {
        podcast->link = g_strdup((gchar *)content);
        xmlFree(content);
    }
    if ((content = get_node_content(channel, "description"))) {
        podcast->description = g_strdup((gchar *)content);
        xmlFree(content);
    }
    if ((content = get_node_content(channel, "language"))) {
        podcast->language = g_strdup((gchar *)content);
        xmlFree(content);
    }
    
    /* Get image */
    xmlNodePtr image_node = channel->children;
    while (image_node) {
        if (image_node->type == XML_ELEMENT_NODE) {
            if (xmlStrcmp(image_node->name, (const xmlChar *)"image") == 0 && !image_node->ns) {
                xmlChar *url = get_node_content(image_node, "url");
                if (url) {
                    podcast->image_url = g_strdup((gchar *)url);
                    xmlFree(url);
                }
                break;
            } else if (xmlStrcmp(image_node->name, (const xmlChar *)"image") == 0 && 
                       image_node->ns && xmlStrcmp(image_node->ns->prefix, (const xmlChar *)"itunes") == 0) {
                gchar *href = xml_get_prop_string(image_node, "href");
                if (href) {
                    podcast->image_url = href;
                }
                break;
            }
        }
        image_node = image_node->next;
    }
    
    /* Parse all Podcast 2.0 elements (images, funding, value) using helper function */
    parse_podcast_ns_elements(channel, &podcast->images, &podcast->funding, &podcast->value);
    
    /* Parse live items */
    podcast->live_items = parse_podcast_live_items(channel, podcast->id);
    podcast->has_active_live = podcast_has_active_live_item(podcast);
    
    if (podcast->has_active_live) {
        g_debug("Podcast '%s' has an active live stream!", podcast->title);
    }
    
    xmlFreeDoc(doc);
    return podcast;
}

/* Public wrapper that creates a new curl handle */
Podcast* podcast_parse_feed(const gchar *feed_url) {
    return podcast_parse_feed_internal(feed_url, NULL);
}

GList* podcast_parse_episodes(const gchar *xml_data, gint podcast_id) {
    GList *episodes = NULL;
    
    xmlDocPtr doc = xmlReadMemory(xml_data, strlen(xml_data), NULL, NULL, 0);
    if (!doc) return NULL;
    
    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr channel = root->children;
    while (channel && xmlStrcmp(channel->name, (const xmlChar *)"channel") != 0) {
        channel = channel->next;
    }
    
    if (!channel) {
        xmlFreeDoc(doc);
        return NULL;
    }
    
    /* Parse items (episodes) */
    xmlNodePtr item = channel->children;
    while (item) {
        if (item->type == XML_ELEMENT_NODE && xmlStrcmp(item->name, (const xmlChar *)"item") == 0) {
            PodcastEpisode *episode = g_new0(PodcastEpisode, 1);
            episode->podcast_id = podcast_id;
            
            xmlChar *content;
            if ((content = get_node_content(item, "title"))) {
                episode->title = g_strdup((gchar *)content);
                xmlFree(content);
            }
            if ((content = get_node_content(item, "guid"))) {
                episode->guid = g_strdup((gchar *)content);
                xmlFree(content);
            }
            if ((content = get_node_content(item, "description"))) {
                episode->description = g_strdup((gchar *)content);
                xmlFree(content);
            }
            
            /* Parse pubDate */
            if ((content = get_node_content(item, "pubDate"))) {
                /* Parse RFC 822 date format (e.g., "Mon, 30 Dec 2025 10:00:00 GMT") */
                const gchar *date_str = (const gchar *)content;
                
                /* Try strptime for RFC 822 format */
                struct tm tm = {0};
                gchar *result = strptime(date_str, "%a, %d %b %Y %H:%M:%S", &tm);
                if (result) {
                    /* Successfully parsed, convert to unix timestamp */
                    episode->published_date = (gint64)mktime(&tm);
                } else {
                    /* Try alternative format without day of week */
                    result = strptime(date_str, "%d %b %Y %H:%M:%S", &tm);
                    if (result) {
                        episode->published_date = (gint64)mktime(&tm);
                    } else {
                        /* Try ISO 8601 as fallback */
                        GDateTime *dt = g_date_time_new_from_iso8601(date_str, NULL);
                        if (dt) {
                            episode->published_date = g_date_time_to_unix(dt);
                            g_date_time_unref(dt);
                        } else {
                            /* Last resort: use current time and warn */
                            g_warning("Failed to parse date: %s", date_str);
                            episode->published_date = g_get_real_time() / G_USEC_PER_SEC;
                        }
                    }
                }
                xmlFree(content);
            } else {
                /* No date provided, use current time */
                episode->published_date = g_get_real_time() / G_USEC_PER_SEC;
            }
            
            /* Parse duration from itunes:duration */
            if ((content = get_node_content(item, "duration"))) {
                /* Parse duration in format "HH:MM:SS" or seconds */
                gchar *duration_str = (gchar *)content;
                if (strchr(duration_str, ':')) {
                    /* Parse HH:MM:SS or MM:SS format */
                    gint hours = 0, minutes = 0, seconds = 0;
                    gint parts = sscanf(duration_str, "%d:%d:%d", &hours, &minutes, &seconds);
                    if (parts == 3) {
                        episode->duration = hours * 3600 + minutes * 60 + seconds;
                    } else if (parts == 2) {
                        /* MM:SS format (hours was actually minutes) */
                        episode->duration = hours * 60 + minutes;
                    }
                } else {
                    /* Plain seconds */
                    episode->duration = (gint)g_ascii_strtoll(duration_str, NULL, 10);
                }
                xmlFree(content);
            }
            
            /* Parse enclosure */
            xmlNodePtr enclosure = item->children;
            while (enclosure) {
                if (enclosure->type == XML_ELEMENT_NODE && 
                    xmlStrcmp(enclosure->name, (const xmlChar *)"enclosure") == 0) {
                    xmlChar *url = xmlGetProp(enclosure, (const xmlChar *)"url");
                    xmlChar *type = xmlGetProp(enclosure, (const xmlChar *)"type");
                    xmlChar *length = xmlGetProp(enclosure, (const xmlChar *)"length");
                    
                    if (url) episode->enclosure_url = g_strdup((gchar *)url);
                    if (type) episode->enclosure_type = g_strdup((gchar *)type);
                    if (length) episode->enclosure_length = g_ascii_strtoll((gchar *)length, NULL, 10);
                    
                    xmlFree(url);
                    xmlFree(type);
                    xmlFree(length);
                    break;
                }
                enclosure = enclosure->next;
            }
            
            /* Parse Podcast 2.0 namespace elements */
            /* Look for <podcast:transcript> element */
            xmlNodePtr transcript_node = item->children;
            while (transcript_node) {
                if (transcript_node->type == XML_ELEMENT_NODE && 
                    xmlStrcmp(transcript_node->name, (const xmlChar *)"transcript") == 0 &&
                    is_podcast_namespace(transcript_node)) {
                    episode->transcript_url = xml_get_prop_string(transcript_node, "url");
                    episode->transcript_type = xml_get_prop_string(transcript_node, "type");
                    if (episode->transcript_url) {
                        g_debug("Found transcript URL: %s", episode->transcript_url);
                    }
                    break;
                }
                transcript_node = transcript_node->next;
            }
            
            /* Look for <podcast:chapters> element */
            xmlNodePtr chapters_node = item->children;
            while (chapters_node) {
                if (chapters_node->type == XML_ELEMENT_NODE && 
                    xmlStrcmp(chapters_node->name, (const xmlChar *)"chapters") == 0 &&
                    is_podcast_namespace(chapters_node)) {
                    episode->chapters_url = xml_get_prop_string(chapters_node, "url");
                    episode->chapters_type = xml_get_prop_string(chapters_node, "type");
                    break;
                }
                chapters_node = chapters_node->next;
            }
            
            /* Parse season and episode number */
            if ((content = get_node_ns_prefix_content(item, "podcast", "season"))) {
                episode->season = g_strdup((gchar *)content);
                xmlFree(content);
            }
            if ((content = get_node_ns_prefix_content(item, "podcast", "episode"))) {
                episode->episode_num = g_strdup((gchar *)content);
                xmlFree(content);
            }
            
            /* Parse locked */
            xmlNodePtr locked_node = item->children;
            while (locked_node) {
                if (locked_node->type == XML_ELEMENT_NODE && 
                    xmlStrcmp(locked_node->name, (const xmlChar *)"locked") == 0 &&
                    is_podcast_namespace(locked_node)) {
                    xmlChar *locked_val = xmlNodeGetContent(locked_node);
                    if (locked_val) {
                        episode->locked = (xmlStrcmp(locked_val, (const xmlChar *)"yes") == 0);
                        xmlFree(locked_val);
                    }
                    break;
                }
                locked_node = locked_node->next;
            }
            
            /* Parse all Podcast 2.0 elements (images, funding, value) using helper function */
            parse_podcast_ns_elements(item, &episode->images, &episode->funding, &episode->value);
            
            episodes = g_list_prepend(episodes, episode);
        }
        item = item->next;
    }
    
    xmlFreeDoc(doc);
    return g_list_reverse(episodes);
}

gboolean podcast_manager_subscribe(PodcastManager *manager, const gchar *feed_url) {
    if (!manager || !feed_url) return FALSE;
    
    g_debug("Subscribing to podcast: %s", feed_url);
    
    Podcast *podcast = podcast_parse_feed(feed_url);
    if (!podcast) {
        g_warning("Failed to parse podcast feed");
        return FALSE;
    }
    
    g_debug("Subscribed to: %s", podcast->title);
    
    /* Check if already subscribed */
    for (GList *l = manager->podcasts; l != NULL; l = l->next) {
        Podcast *existing = (Podcast *)l->data;
        if (g_strcmp0(existing->feed_url, feed_url) == 0) {
            g_debug("Already subscribed to: %s", existing->title);
            podcast_free(podcast);
            return TRUE;  /* Not an error, just already exists */
        }
    }
    
    /* Save podcast to database */
    gint podcast_id = database_add_podcast(manager->database, podcast->title, podcast->feed_url,
                                          podcast->link, podcast->description, podcast->author,
                                          podcast->image_url, podcast->language);
    
    if (podcast_id < 0) {
        g_warning("Failed to save podcast to database (may already exist)");
        podcast_free(podcast);
        return FALSE;
    }
    
    podcast->id = podcast_id;
    manager->podcasts = g_list_append(manager->podcasts, podcast);
    
    /* Save podcast funding information if available */
    if (podcast->funding) {
        database_save_podcast_funding(manager->database, podcast_id, podcast->funding);
    }
    
    /* Save podcast value information if available */
    if (podcast->value) {
        database_save_podcast_value(manager->database, podcast_id, podcast->value);
    }
    
    /* Save podcast live items if available */
    if (podcast->live_items) {
        database_save_podcast_live_items(manager->database, podcast_id, podcast->live_items);
    }
    
    /* Fetch and parse episodes */
    gchar *xml_data = fetch_url(feed_url);
    if (xml_data) {
        GList *episodes = podcast_parse_episodes(xml_data, podcast_id);
        g_free(xml_data);
        
        /* Save episodes to database */
        for (GList *l = episodes; l != NULL; l = l->next) {
            PodcastEpisode *episode = (PodcastEpisode *)l->data;
            gint episode_id = database_add_podcast_episode(manager->database, podcast_id, episode->guid,
                                        episode->title, episode->description,
                                        episode->enclosure_url, episode->enclosure_length,
                                        episode->enclosure_type, episode->published_date,
                                        episode->duration, episode->chapters_url,
                                        episode->chapters_type, episode->transcript_url,
                                        episode->transcript_type);
            
            /* Save funding information if available */
            if (episode_id > 0 && episode->funding) {
                database_save_episode_funding(manager->database, episode_id, episode->funding);
            }
            
            /* Save value information if available */
            if (episode_id > 0 && episode->value) {
                database_save_episode_value(manager->database, episode_id, episode->value);
            }
        }
        
        g_debug("Added %d episodes", g_list_length(episodes));
        g_list_free_full(episodes, (GDestroyNotify)podcast_episode_free);
    }
    
    return TRUE;
}

gboolean podcast_manager_unsubscribe(PodcastManager *manager, gint podcast_id) {
    if (!manager || !manager->database) return FALSE;
    
    /* Find the podcast in memory and get episodes for cleanup */
    Podcast *podcast = NULL;
    GList *podcast_link = NULL;
    for (GList *l = manager->podcasts; l != NULL; l = l->next) {
        Podcast *p = (Podcast *)l->data;
        if (p->id == podcast_id) {
            podcast = p;
            podcast_link = l;
            break;
        }
    }
    
    if (!podcast) {
        g_warning("Podcast not found with ID: %d", podcast_id);
        return FALSE;
    }
    
    g_debug("Unsubscribing from podcast: %s", podcast->title);
    
    /* Delete downloaded episode files */
    GList *episodes = database_get_podcast_episodes(manager->database, podcast_id);
    for (GList *l = episodes; l != NULL; l = l->next) {
        PodcastEpisode *episode = (PodcastEpisode *)l->data;
        if (episode->downloaded && episode->local_file_path) {
            if (g_file_test(episode->local_file_path, G_FILE_TEST_EXISTS)) {
                g_unlink(episode->local_file_path);
            }
        }
    }
    g_list_free_full(episodes, (GDestroyNotify)podcast_episode_free);
    
    /* Remove from database */
    if (!database_delete_podcast(manager->database, podcast_id)) {
        g_warning("Failed to delete podcast from database");
        return FALSE;
    }
    
    /* Remove from in-memory list */
    manager->podcasts = g_list_remove_link(manager->podcasts, podcast_link);
    podcast_free(podcast);
    g_list_free(podcast_link);
    
    return TRUE;
}

void podcast_manager_update_feed(PodcastManager *manager, gint podcast_id) {
    if (!manager) return;
    
    /* Find the podcast by ID */
    Podcast *podcast = NULL;
    for (GList *l = manager->podcasts; l != NULL; l = l->next) {
        Podcast *p = (Podcast *)l->data;
        if (p->id == podcast_id) {
            podcast = p;
            break;
        }
    }
    
    if (!podcast || !podcast->feed_url) {
        g_warning("Podcast not found or has no feed URL\n");
        return;
    }
    
    g_debug("Updating podcast feed: %s", podcast->title);
    
    /* Re-parse podcast-level information (including funding) using reusable handle */
    Podcast *updated_podcast = podcast_parse_feed_internal(podcast->feed_url, manager->curl_handle);
    if (updated_podcast && updated_podcast->funding) {
        database_save_podcast_funding(manager->database, podcast_id, updated_podcast->funding);
        
        /* Update the in-memory podcast funding */
        if (podcast->funding) {
            g_list_free_full(podcast->funding, (GDestroyNotify)podcast_funding_free);
        }
        podcast->funding = g_list_copy_deep(updated_podcast->funding, (GCopyFunc)podcast_funding_copy, NULL);
    }
    if (updated_podcast && updated_podcast->value) {
        database_save_podcast_value(manager->database, podcast_id, updated_podcast->value);
        
        /* Update the in-memory podcast value */
        if (podcast->value) {
            g_list_free_full(podcast->value, (GDestroyNotify)podcast_value_free);
        }
        podcast->value = g_list_copy_deep(updated_podcast->value, (GCopyFunc)podcast_value_copy, NULL);
    }
    /* Update live items - these change frequently so always update */
    if (updated_podcast) {
        database_save_podcast_live_items(manager->database, podcast_id, updated_podcast->live_items);
        
        /* Update the in-memory podcast live items */
        if (podcast->live_items) {
            g_list_free_full(podcast->live_items, (GDestroyNotify)podcast_live_item_free);
        }
        podcast->live_items = g_list_copy_deep(updated_podcast->live_items, (GCopyFunc)podcast_live_item_copy, NULL);
        podcast->has_active_live = podcast_has_active_live_item(podcast);
        
        if (podcast->has_active_live) {
            g_debug("Podcast '%s' is currently LIVE!", podcast->title);
        }
    }
    if (updated_podcast) {
        podcast_free(updated_podcast);
    }
    
    /* Fetch and parse episodes using reusable handle */
    gchar *xml_data = fetch_url_with_handle(podcast->feed_url, manager->curl_handle);
    if (!xml_data) {
        g_warning("Failed to fetch feed\n");
        return;
    }
    
    GList *episodes = podcast_parse_episodes(xml_data, podcast_id);
    g_free(xml_data);
    
    if (!episodes) {
        g_warning("No episodes found or failed to parse feed\n");
        return;
    }
    
    /* Save/update episodes to database - ON CONFLICT will update existing episodes */
    for (GList *l = episodes; l != NULL; l = l->next) {
        PodcastEpisode *episode = (PodcastEpisode *)l->data;
        gint episode_id = database_add_podcast_episode(manager->database, podcast_id, episode->guid,
                                    episode->title, episode->description,
                                    episode->enclosure_url, episode->enclosure_length,
                                    episode->enclosure_type, episode->published_date,
                                    episode->duration, episode->chapters_url,
                                    episode->chapters_type, episode->transcript_url,
                                    episode->transcript_type);
        
        /* Save funding information if available */
        if (episode_id > 0 && episode->funding) {
            database_save_episode_funding(manager->database, episode_id, episode->funding);
        }
        
        /* Save value information if available */
        if (episode_id > 0 && episode->value) {
            database_save_episode_value(manager->database, episode_id, episode->value);
        }
    }
    
    g_debug("Updated %d episodes", g_list_length(episodes));
    g_list_free_full(episodes, (GDestroyNotify)podcast_episode_free);
    
    /* Update last_fetched timestamp */
    podcast->last_fetched = g_get_real_time() / G_USEC_PER_SEC;
}

void podcast_manager_update_all_feeds(PodcastManager *manager) {
    if (!manager) return;
    
    /* Don't start if already updating */
    if (manager->update_in_progress) {
        g_debug("Feed update already in progress");
        return;
    }
    
    manager->update_in_progress = TRUE;
    manager->update_cancelled = FALSE;
    
    g_debug("Automatically checking for new podcast episodes...");
    
    for (GList *l = manager->podcasts; l != NULL; l = l->next) {
        /* Check for cancellation before each feed */
        if (manager->update_cancelled) {
            g_debug("Feed update cancelled");
            break;
        }
        
        Podcast *podcast = (Podcast *)l->data;
        podcast_manager_update_feed(manager, podcast->id);
    }
    
    manager->update_in_progress = FALSE;
    manager->update_cancelled = FALSE;
}

/* Cancel any ongoing feed updates */
void podcast_manager_cancel_updates(PodcastManager *manager) {
    if (!manager) return;
    
    if (manager->update_in_progress) {
        manager->update_cancelled = TRUE;
        g_debug("Requesting feed update cancellation...");
    }
}

/* Check if feed update is in progress */
gboolean podcast_manager_is_updating(PodcastManager *manager) {
    return manager ? manager->update_in_progress : FALSE;
}

/* Timer callback for automatic feed updates */
static gboolean podcast_update_timer_callback(gpointer user_data) {
    PodcastManager *manager = (PodcastManager *)user_data;
    
    if (manager && manager->podcasts) {
        podcast_manager_update_all_feeds(manager);
    }
    
    return TRUE;  /* Continue timer */
}

/* Start automatic feed update timer */
void podcast_manager_start_auto_update(PodcastManager *manager, gint interval_minutes) {
    if (!manager) return;
    
    /* Stop existing timer if any */
    podcast_manager_stop_auto_update(manager);
    
    manager->update_interval_minutes = interval_minutes;
    
    if (interval_minutes > 0) {
        /* Convert minutes to seconds for GLib timer */
        guint interval_seconds = interval_minutes * 60;
        
        g_print("Podcast auto-update: checking every %d minutes (%d hours, %d mins)\n",
                interval_minutes, interval_minutes / 60, interval_minutes % 60);
        
        manager->update_timer_id = g_timeout_add_seconds(
            interval_seconds,
            podcast_update_timer_callback,
            manager
        );
    }
}

/* Stop automatic feed update timer */
void podcast_manager_stop_auto_update(PodcastManager *manager) {
    if (!manager) return;
    
    if (manager->update_timer_id > 0) {
        g_source_remove(manager->update_timer_id);
        manager->update_timer_id = 0;
    }
}

GList* podcast_manager_get_podcasts(PodcastManager *manager) {
    return manager ? manager->podcasts : NULL;
}

GList* podcast_manager_get_episodes(PodcastManager *manager, gint podcast_id) {
    if (!manager || !manager->database) return NULL;
    
    /* Query episodes from database */
    return database_get_podcast_episodes(manager->database, podcast_id);
}

GList* podcast_manager_get_live_items(PodcastManager *manager, gint podcast_id) {
    if (!manager) return NULL;
    
    /* Find the podcast and return its live items */
    for (GList *l = manager->podcasts; l != NULL; l = l->next) {
        Podcast *p = (Podcast *)l->data;
        if (p->id == podcast_id) {
            /* Return a deep copy of the live items list */
            return g_list_copy_deep(p->live_items, (GCopyFunc)podcast_live_item_copy, NULL);
        }
    }
    
    return NULL;
}

static size_t write_file_callback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

/* Progress callback for curl */
static int download_progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, 
                                      curl_off_t ultotal, curl_off_t ulnow) {
    (void)ultotal;
    (void)ulnow;
    
    DownloadTask *task = (DownloadTask *)clientp;
    
    /* Check if download was cancelled */
    if (task->cancelled) {
        return 1;  /* Non-zero return value cancels download */
    }
    
    /* Calculate progress */
    if (dltotal > 0 && task->progress_callback) {
        gdouble progress = (gdouble)dlnow / (gdouble)dltotal;
        
        /* Format status message */
        gchar *status = g_strdup_printf("Downloading: %.1f MB / %.1f MB", 
                                       dlnow / 1048576.0, dltotal / 1048576.0);
        
        /* Call progress callback on main thread */
        task->progress_callback(task->user_data, task->episode->id, progress, status);
        g_free(status);
    }
    
    return 0;
}

/* Thread function for downloading */
static void download_thread_func(gpointer data, gpointer user_data) {
    DownloadTask *task = (DownloadTask *)data;
    PodcastManager *manager = task->manager;
    PodcastEpisode *episode = task->episode;
    (void)user_data;
    
    gboolean success = FALSE;
    gchar *error_msg = NULL;
    gchar *local_path = NULL;
    
    /* Create download directory if it doesn't exist */
    g_mkdir_with_parents(manager->download_dir, 0755);
    
    /* Generate local filename */
    gchar *basename = g_path_get_basename(episode->enclosure_url);
    gchar *safe_basename = g_strdup(basename);
    
    /* Clean up filename - remove query strings */
    gchar *query = strchr(safe_basename, '?');
    if (query) *query = '\0';
    
    local_path = g_build_filename(manager->download_dir, safe_basename, NULL);
    g_free(basename);
    g_free(safe_basename);
    
    if (task->progress_callback) {
        task->progress_callback(task->user_data, episode->id, 0.0, "Initializing download...");
    }
    
    /* Download file using curl */
    CURL *curl = curl_easy_init();
    if (!curl) {
        error_msg = g_strdup("Failed to initialize curl");
        goto cleanup;
    }
    
    FILE *fp = fopen(local_path, "wb");
    if (!fp) {
        error_msg = g_strdup_printf("Failed to open file for writing: %s", local_path);
        curl_easy_cleanup(curl);
        goto cleanup;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, episode->enclosure_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);  /* 10 minute timeout */
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Shriek Media Player/1.0");
    
    /* Set up progress tracking */
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, download_progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, task);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    
    CURLcode res = curl_easy_perform(curl);
    
    fclose(fp);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        if (task->cancelled) {
            error_msg = g_strdup("Download cancelled");
        } else {
            error_msg = g_strdup_printf("Download failed: %s", curl_easy_strerror(res));
        }
        unlink(local_path);
        goto cleanup;
    }
    
    /* Update database */
    database_update_episode_downloaded(manager->database, episode->id, local_path);
    
    success = TRUE;
    
cleanup:
    /* Remove from active downloads */
    g_mutex_lock(&manager->downloads_mutex);
    g_hash_table_remove(manager->active_downloads, GINT_TO_POINTER(episode->id));
    g_mutex_unlock(&manager->downloads_mutex);
    
    /* Call completion callback */
    if (task->complete_callback) {
        task->complete_callback(task->user_data, episode->id, success, error_msg);
    }
    
    g_free(local_path);
    g_free(error_msg);
    
    /* Free the minimal episode copy created in podcast_episode_download */
    g_free(episode->enclosure_url);
    g_free(episode->title);
    g_free(episode);
    
    g_free(task);
}

void podcast_episode_download(PodcastManager *manager, PodcastEpisode *episode,
                             DownloadProgressCallback progress_cb,
                             DownloadCompleteCallback complete_cb,
                             gpointer user_data) {
    if (!manager || !episode || !episode->enclosure_url) return;
    
    /* Check if already downloading */
    g_mutex_lock(&manager->downloads_mutex);
    if (g_hash_table_contains(manager->active_downloads, GINT_TO_POINTER(episode->id))) {
        g_mutex_unlock(&manager->downloads_mutex);
        g_debug("Episode is already being downloaded");
        return;
    }
    
    /* Create a minimal copy of the episode for the download thread.
     * This avoids ownership issues - caller keeps ownership of original episode. */
    PodcastEpisode *episode_copy = g_new0(PodcastEpisode, 1);
    episode_copy->id = episode->id;
    episode_copy->podcast_id = episode->podcast_id;
    episode_copy->enclosure_url = g_strdup(episode->enclosure_url);
    episode_copy->title = g_strdup(episode->title);
    
    /* Create download task */
    DownloadTask *task = g_new0(DownloadTask, 1);
    task->episode = episode_copy;  /* Thread owns this copy and will free it */
    task->manager = manager;
    task->progress_callback = progress_cb;
    task->complete_callback = complete_cb;
    task->user_data = user_data;
    task->cancelled = FALSE;
    
    /* Add to active downloads */
    g_hash_table_insert(manager->active_downloads, GINT_TO_POINTER(episode->id), task);
    g_mutex_unlock(&manager->downloads_mutex);
    
    /* Create thread pool if it doesn't exist */
    if (!manager->download_pool) {
        GError *error = NULL;
        manager->download_pool = g_thread_pool_new(download_thread_func, NULL, 3, FALSE, &error);
        if (error) {
            g_warning("Failed to create download thread pool: %s", error->message);
            g_error_free(error);
            
            g_mutex_lock(&manager->downloads_mutex);
            g_hash_table_remove(manager->active_downloads, GINT_TO_POINTER(episode->id));
            g_mutex_unlock(&manager->downloads_mutex);
            
            g_free(task);
            return;
        }
    }
    
    /* Queue the download */
    GError *error = NULL;
    g_thread_pool_push(manager->download_pool, task, &error);
    if (error) {
        g_warning("Failed to queue download: %s", error->message);
        g_error_free(error);
        
        g_mutex_lock(&manager->downloads_mutex);
        g_hash_table_remove(manager->active_downloads, GINT_TO_POINTER(episode->id));
        g_mutex_unlock(&manager->downloads_mutex);
        
        g_free(task);
    }
}

void podcast_episode_cancel_download(PodcastManager *manager, gint episode_id) {
    if (!manager) return;
    
    g_mutex_lock(&manager->downloads_mutex);
    DownloadTask *task = g_hash_table_lookup(manager->active_downloads, GINT_TO_POINTER(episode_id));
    if (task) {
        task->cancelled = TRUE;
    }
    g_mutex_unlock(&manager->downloads_mutex);
}

void podcast_episode_delete(PodcastManager *manager, PodcastEpisode *episode) {
    if (!manager || !episode) return;
    
    /* Delete the local file if it exists */
    if (episode->local_file_path && g_file_test(episode->local_file_path, G_FILE_TEST_EXISTS)) {
        if (g_unlink(episode->local_file_path) == 0) {
            g_debug("Deleted episode file: %s", episode->local_file_path);
        } else {
            g_warning("Failed to delete episode file: %s", episode->local_file_path);
        }
    }
    
    /* Update database to clear download status */
    database_clear_episode_download(manager->database, episode->id);
}

void podcast_episode_mark_played(PodcastManager *manager, gint episode_id, gboolean played) {
    if (!manager || !manager->database || episode_id <= 0) return;
    
    /* Get current position to preserve it */
    PodcastEpisode *episode = database_get_episode_by_id(manager->database, episode_id);
    gint position = episode ? episode->play_position : 0;
    
    database_update_episode_progress(manager->database, episode_id, position, played);
    
    if (episode) {
        podcast_episode_free(episode);
    }
}

void podcast_episode_update_position(PodcastManager *manager, gint episode_id, gint position) {
    if (!manager || !manager->database || episode_id <= 0) return;
    
    /* Get current played status to preserve it */
    PodcastEpisode *episode = database_get_episode_by_id(manager->database, episode_id);
    gboolean played = episode ? episode->played : FALSE;
    
    database_update_episode_progress(manager->database, episode_id, position, played);
    
    if (episode) {
        podcast_episode_free(episode);
    }
}

static GList* parse_chapters_json(const gchar *json_data) {
    GList *chapters = NULL;
    GError *error = NULL;
    
    /* Parse JSON data */
    JsonParser *parser = json_parser_new();
    if (!json_parser_load_from_data(parser, json_data, -1, &error)) {
        g_warning("Failed to parse chapters JSON: %s", error ? error->message : "unknown error");
        if (error) g_error_free(error);
        g_object_unref(parser);
        return NULL;
    }
    
    JsonNode *root = json_parser_get_root(parser);
    if (!root || !JSON_NODE_HOLDS_OBJECT(root)) {
        g_warning("Invalid JSON root node");
        g_object_unref(parser);
        return NULL;
    }
    
    JsonObject *root_obj = json_node_get_object(root);
    
    /* Get the chapters array */
    if (!json_object_has_member(root_obj, "chapters")) {
        g_warning("No 'chapters' array found in JSON");
        g_object_unref(parser);
        return NULL;
    }
    
    JsonArray *chapters_array = json_object_get_array_member(root_obj, "chapters");
    guint n_chapters = json_array_get_length(chapters_array);
    
    g_debug("Parsing %u chapters from JSON", n_chapters);
    
    for (guint i = 0; i < n_chapters; i++) {
        JsonObject *chapter_obj = json_array_get_object_element(chapters_array, i);
        if (!chapter_obj) continue;
        
        PodcastChapter *chapter = g_new0(PodcastChapter, 1);
        
        /* Parse startTime */
        if (json_object_has_member(chapter_obj, "startTime")) {
            chapter->start_time = json_object_get_double_member(chapter_obj, "startTime");
        }
        
        /* Parse title */
        if (json_object_has_member(chapter_obj, "title")) {
            const gchar *title = json_object_get_string_member(chapter_obj, "title");
            chapter->title = g_strdup(title);
        }
        
        /* Parse img (optional) */
        if (json_object_has_member(chapter_obj, "img")) {
            const gchar *img = json_object_get_string_member(chapter_obj, "img");
            chapter->img = g_strdup(img);
        }
        
        /* Parse url (optional) */
        if (json_object_has_member(chapter_obj, "url")) {
            const gchar *url = json_object_get_string_member(chapter_obj, "url");
            chapter->url = g_strdup(url);
        }
        
        chapters = g_list_append(chapters, chapter);
        
        if (chapter->title) {
            g_debug("  Chapter %u: %.0fs - %s", i, chapter->start_time, chapter->title);
        }
    }
    
    g_object_unref(parser);
    return chapters;
}

GList* podcast_episode_get_chapters(PodcastManager *manager, gint episode_id) {
    if (!manager || !manager->database) return NULL;
    
    /* Get episode details by ID */
    PodcastEpisode *episode = database_get_episode_by_id(manager->database, episode_id);
    if (!episode) {
        return NULL;
    }
    
    GList *chapters = NULL;
    
    /* Try to fetch external chapters file */
    if (episode->chapters_url) {
        gchar *chapters_data = fetch_url(episode->chapters_url);
        if (chapters_data) {
            if (g_str_has_suffix(episode->chapters_url, ".json") || 
                (episode->chapters_type && strstr(episode->chapters_type, "json"))) {
                chapters = parse_chapters_json(chapters_data);
            }
            g_free(chapters_data);
        } else {
            /* Fetch failed */
        }
    } else {
        /* Episode has no chapters_url */
    }
    
    /* If no external chapters, try to get from media file tags */
    if (!chapters && episode->local_file_path && g_file_test(episode->local_file_path, G_FILE_TEST_EXISTS)) {
        /* TODO: Extract embedded chapters from media file using GStreamer */
    }
    
    podcast_episode_free(episode);
    return chapters;
}

PodcastChapter* podcast_chapter_at_time(GList *chapters, gdouble time) {
    PodcastChapter *current = NULL;
    
    for (GList *l = chapters; l != NULL; l = l->next) {
        PodcastChapter *chapter = (PodcastChapter *)l->data;
        if (chapter->start_time <= time) {
            current = chapter;
        } else {
            break;
        }
    }
    
    return current;
}
/* Podcast image utility functions */
PodcastImage* podcast_get_best_image(GList *images, const gchar *purpose) {
    if (!images) return NULL;
    
    /* If no specific purpose requested, return first image */
    if (!purpose) {
        return (PodcastImage *)images->data;
    }
    
    /* Look for image with matching purpose */
    for (GList *l = images; l != NULL; l = l->next) {
        PodcastImage *image = (PodcastImage *)l->data;
        if (image->purpose && strstr(image->purpose, purpose)) {
            return image;
        }
    }
    
    /* Look for image with "artwork" purpose as fallback */
    if (g_strcmp0(purpose, "artwork") != 0) {
        for (GList *l = images; l != NULL; l = l->next) {
            PodcastImage *image = (PodcastImage *)l->data;
            if (image->purpose && strstr(image->purpose, "artwork")) {
                return image;
            }
        }
    }
    
    /* Return first image with square aspect ratio */
    for (GList *l = images; l != NULL; l = l->next) {
        PodcastImage *image = (PodcastImage *)l->data;
        if (image->aspect_ratio && (g_strcmp0(image->aspect_ratio, "1/1") == 0 || 
                                   g_strcmp0(image->aspect_ratio, "1:1") == 0)) {
            return image;
        }
    }
    
    /* Return any image */
    return (PodcastImage *)images->data;
}

const gchar* podcast_get_display_image_url(Podcast *podcast) {
    if (!podcast) return NULL;
    
    /* Try Podcast 2.0 images first */
    if (podcast->images) {
        PodcastImage *image = podcast_get_best_image(podcast->images, "artwork");
        if (image && image->href) {
            return image->href;
        }
    }
    
    /* Fallback to legacy image URL */
    return podcast->image_url;
}

const gchar* podcast_episode_get_display_image_url(PodcastEpisode *episode, Podcast *podcast) {
    if (!episode) return NULL;
    
    /* Try episode-specific images first */
    if (episode->images) {
        PodcastImage *image = podcast_get_best_image(episode->images, "artwork");
        if (image && image->href) {
            return image->href;
        }
    }
    
    /* Fallback to podcast-level image */
    return podcast ? podcast_get_display_image_url(podcast) : NULL;
}