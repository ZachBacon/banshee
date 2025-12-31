#define _XOPEN_SOURCE
#include "podcast.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <curl/curl.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
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
        const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        for (int i = 0; i < 12; i++) {
            if (strcmp(month_name, months[i]) == 0) {
                tm->tm_mon = i;
                break;
            }
        }
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
        
        const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        for (int i = 0; i < 12; i++) {
            if (strcmp(month_name, months[i]) == 0) {
                tm->tm_mon = i;
                break;
            }
        }
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

gchar* fetch_url(const gchar *url) {
    CURL *curl;
    CURLcode res;
    MemoryBuffer chunk = {NULL, 0};
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (!curl) return NULL;
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Banshee/1.0 (Podcast 2.0)");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    
    if (res != CURLE_OK) {
        g_free(chunk.data);
        return NULL;
    }
    
    return chunk.data;
}

PodcastManager* podcast_manager_new(Database *database) {
    PodcastManager *manager = g_new0(PodcastManager, 1);
    manager->database = database;
    
    /* Load existing podcasts from database */
    manager->podcasts = database_get_podcasts(database);
    
    if (manager->podcasts) {
        g_print("Loaded %d podcasts from database\n", g_list_length(manager->podcasts));
    }
    
    /* Create download directory */
    manager->download_dir = g_build_filename(g_get_user_data_dir(), "banshee", "podcasts", NULL);
    g_mkdir_with_parents(manager->download_dir, 0755);
    
    /* Thread pool will be created later when needed */
    manager->download_pool = NULL;
    
    return manager;
}

void podcast_manager_free(PodcastManager *manager) {
    if (!manager) return;
    
    g_list_free_full(manager->podcasts, (GDestroyNotify)podcast_free);
    if (manager->download_pool) {
        g_thread_pool_free(manager->download_pool, FALSE, TRUE);
    }
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

void podcast_value_free(PodcastValue *value) {
    if (!value) return;
    g_free(value->type);
    g_free(value->method);
    g_free(value->suggested);
    g_list_free_full(value->recipients, g_free);
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

Podcast* podcast_parse_feed(const gchar *feed_url) {
    gchar *xml_data = fetch_url(feed_url);
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
            if (xmlStrcmp(image_node->name, (const xmlChar *)"image") == 0) {
                xmlChar *url = get_node_content(image_node, "url");
                if (url) {
                    podcast->image_url = g_strdup((gchar *)url);
                    xmlFree(url);
                }
                break;
            } else if (xmlStrcmp(image_node->name, (const xmlChar *)"image") == 0 && 
                       image_node->ns && xmlStrcmp(image_node->ns->prefix, (const xmlChar *)"itunes") == 0) {
                xmlChar *href = xmlGetProp(image_node, (const xmlChar *)"href");
                if (href) {
                    podcast->image_url = g_strdup((gchar *)href);
                    xmlFree(href);
                }
                break;
            }
        }
        image_node = image_node->next;
    }
    
    xmlFreeDoc(doc);
    return podcast;
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
                    episode->duration = atoi(duration_str);
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
                    transcript_node->ns && xmlStrcmp(transcript_node->ns->href, (const xmlChar *)PODCAST_NAMESPACE) == 0) {
                    xmlChar *url = xmlGetProp(transcript_node, (const xmlChar *)"url");
                    xmlChar *type = xmlGetProp(transcript_node, (const xmlChar *)"type");
                    
                    if (url) {
                        episode->transcript_url = g_strdup((gchar *)url);
                        xmlFree(url);
                        g_print("Found transcript URL: %s\n", episode->transcript_url);
                    }
                    if (type) {
                        episode->transcript_type = g_strdup((gchar *)type);
                        xmlFree(type);
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
                    chapters_node->ns && xmlStrcmp(chapters_node->ns->href, (const xmlChar *)PODCAST_NAMESPACE) == 0) {
                    xmlChar *url = xmlGetProp(chapters_node, (const xmlChar *)"url");
                    xmlChar *type = xmlGetProp(chapters_node, (const xmlChar *)"type");
                    
                    if (url) {
                        episode->chapters_url = g_strdup((gchar *)url);
                        xmlFree(url);
                        g_print("Found chapters URL: %s\n", episode->chapters_url);
                    }
                    if (type) {
                        episode->chapters_type = g_strdup((gchar *)type);
                        xmlFree(type);
                    }
                    break;
                }
                chapters_node = chapters_node->next;
            }
            
            /* Look for <podcast:funding> elements */
            xmlNodePtr funding_node = item->children;
            while (funding_node) {
                if (funding_node->type == XML_ELEMENT_NODE && 
                    xmlStrcmp(funding_node->name, (const xmlChar *)"funding") == 0 &&
                    funding_node->ns && xmlStrcmp(funding_node->ns->href, (const xmlChar *)PODCAST_NAMESPACE) == 0) {
                    
                    PodcastFunding *funding = g_new0(PodcastFunding, 1);
                    
                    xmlChar *url = xmlGetProp(funding_node, (const xmlChar *)"url");
                    xmlChar *message = xmlGetProp(funding_node, (const xmlChar *)"message");
                    xmlChar *platform = xmlNodeGetContent(funding_node);
                    
                    if (url) {
                        funding->url = g_strdup((gchar *)url);
                        xmlFree(url);
                    }
                    if (message) {
                        funding->message = g_strdup((gchar *)message);
                        xmlFree(message);
                    }
                    if (platform) {
                        funding->platform = g_strdup((gchar *)platform);
                        xmlFree(platform);
                    }
                    
                    if (funding->url) {
                        episode->funding = g_list_append(episode->funding, funding);
                        g_print("Found funding: %s -> %s (%s)\n", 
                                funding->platform ? funding->platform : "Unknown",
                                funding->url, funding->message ? funding->message : "No message");
                    } else {
                        podcast_funding_free(funding);
                    }
                }
                funding_node = funding_node->next;
            }
            if ((content = get_node_ns_content(item, PODCAST_NAMESPACE, "season"))) {
                episode->season = g_strdup((gchar *)content);
                xmlFree(content);
            }
            if ((content = get_node_ns_content(item, PODCAST_NAMESPACE, "episode"))) {
                episode->episode_num = g_strdup((gchar *)content);
                xmlFree(content);
            }
            
            /* Parse locked */
            xmlNodePtr locked_node = item->children;
            while (locked_node) {
                if (locked_node->type == XML_ELEMENT_NODE && 
                    xmlStrcmp(locked_node->name, (const xmlChar *)"locked") == 0 &&
                    locked_node->ns && xmlStrcmp(locked_node->ns->href, (const xmlChar *)PODCAST_NAMESPACE) == 0) {
                    xmlChar *locked_val = xmlNodeGetContent(locked_node);
                    if (locked_val) {
                        episode->locked = (xmlStrcmp(locked_val, (const xmlChar *)"yes") == 0);
                        xmlFree(locked_val);
                    }
                    break;
                }
                locked_node = locked_node->next;
            }
            
            episodes = g_list_append(episodes, episode);
        }
        item = item->next;
    }
    
    xmlFreeDoc(doc);
    return episodes;
}

gboolean podcast_manager_subscribe(PodcastManager *manager, const gchar *feed_url) {
    if (!manager || !feed_url) return FALSE;
    
    g_print("Subscribing to podcast: %s\n", feed_url);
    
    Podcast *podcast = podcast_parse_feed(feed_url);
    if (!podcast) {
        g_warning("Failed to parse podcast feed");
        return FALSE;
    }
    
    g_print("Subscribed to: %s\n", podcast->title);
    
    /* Check if already subscribed */
    for (GList *l = manager->podcasts; l != NULL; l = l->next) {
        Podcast *existing = (Podcast *)l->data;
        if (g_strcmp0(existing->feed_url, feed_url) == 0) {
            g_print("Already subscribed to: %s\n", existing->title);
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
    
    /* Fetch and parse episodes */
    gchar *xml_data = fetch_url(feed_url);
    if (xml_data) {
        GList *episodes = podcast_parse_episodes(xml_data, podcast_id);
        g_free(xml_data);
        
        /* Save episodes to database */
        for (GList *l = episodes; l != NULL; l = l->next) {
            PodcastEpisode *episode = (PodcastEpisode *)l->data;
            database_add_podcast_episode(manager->database, podcast_id, episode->guid,
                                        episode->title, episode->description,
                                        episode->enclosure_url, episode->enclosure_length,
                                        episode->enclosure_type, episode->published_date,
                                        episode->duration);
        }
        
        g_print("Added %d episodes\n", g_list_length(episodes));
        g_list_free_full(episodes, (GDestroyNotify)podcast_episode_free);
    }
    
    return TRUE;
}

gboolean podcast_manager_unsubscribe(PodcastManager *manager, gint podcast_id) {
    if (!manager) return FALSE;
    
    /* TODO: Remove from database and delete downloaded episodes */
    
    return TRUE;
}

void podcast_manager_update_feed(PodcastManager *manager, gint podcast_id) {
    if (!manager) return;
    
    /* TODO: Fetch feed, parse episodes, update database */
}

void podcast_manager_update_all_feeds(PodcastManager *manager) {
    if (!manager) return;
    
    for (GList *l = manager->podcasts; l != NULL; l = l->next) {
        Podcast *podcast = (Podcast *)l->data;
        podcast_manager_update_feed(manager, podcast->id);
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

static size_t write_file_callback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

void podcast_episode_download(PodcastManager *manager, PodcastEpisode *episode) {
    if (!manager || !episode || !episode->enclosure_url) return;
    
    /* Create download directory if it doesn't exist */
    g_mkdir_with_parents(manager->download_dir, 0755);
    
    /* Generate local filename */
    gchar *basename = g_path_get_basename(episode->enclosure_url);
    gchar *safe_basename = g_strdup(basename);
    
    /* Clean up filename - remove query strings */
    gchar *query = strchr(safe_basename, '?');
    if (query) *query = '\0';
    
    gchar *local_path = g_build_filename(manager->download_dir, safe_basename, NULL);
    g_free(basename);
    g_free(safe_basename);
    
    g_print("Downloading %s to %s\n", episode->enclosure_url, local_path);
    
    /* Download file using curl */
    CURL *curl = curl_easy_init();
    if (!curl) {
        g_warning("Failed to initialize curl");
        g_free(local_path);
        return;
    }
    
    FILE *fp = fopen(local_path, "wb");
    if (!fp) {
        g_warning("Failed to open file for writing: %s", local_path);
        curl_easy_cleanup(curl);
        g_free(local_path);
        return;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, episode->enclosure_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);  /* 5 minute timeout */
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Banshee Media Player/1.0");
    
    CURLcode res = curl_easy_perform(curl);
    
    fclose(fp);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        g_warning("Download failed: %s", curl_easy_strerror(res));
        unlink(local_path);
        g_free(local_path);
        return;
    }
    
    g_print("Download complete: %s\n", local_path);
    
    /* Update database */
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE podcast_episodes SET downloaded=1, local_file_path=? WHERE id=?";
    
    if (sqlite3_prepare_v2(manager->database->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, local_path, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, episode->id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    g_free(local_path);
}

void podcast_episode_delete(PodcastManager *manager, PodcastEpisode *episode) {
    /* TODO: Delete local file */
    (void)manager;
    (void)episode;
}

void podcast_episode_mark_played(PodcastManager *manager, gint episode_id, gboolean played) {
    /* TODO: Update database */
    (void)manager;
    (void)episode_id;
    (void)played;
}

void podcast_episode_update_position(PodcastManager *manager, gint episode_id, gint position) {
    /* TODO: Update playback position in database */
    (void)manager;
    (void)episode_id;
    (void)position;
}

static GList* parse_chapters_json(const gchar *json_data) {
    GList *chapters = NULL;
    
    /* Simple JSON parsing for chapters */
    xmlDocPtr doc = xmlReadMemory(json_data, strlen(json_data), NULL, NULL, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc) return NULL;
    
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) {
        xmlFreeDoc(doc);
        return NULL;
    }
    
    /* Look for chapters array */
    for (xmlNodePtr node = root->children; node; node = node->next) {
        if (node->type != XML_ELEMENT_NODE) continue;
        
        if (xmlStrcmp(node->name, (const xmlChar *)"chapters") == 0) {
            for (xmlNodePtr chapter_node = node->children; chapter_node; chapter_node = chapter_node->next) {
                if (chapter_node->type != XML_ELEMENT_NODE) continue;
                
                PodcastChapter *chapter = g_new0(PodcastChapter, 1);
                
                for (xmlNodePtr prop = chapter_node->children; prop; prop = prop->next) {
                    if (prop->type != XML_ELEMENT_NODE) continue;
                    
                    xmlChar *content = xmlNodeGetContent(prop);
                    if (xmlStrcmp(prop->name, (const xmlChar *)"startTime") == 0) {
                        chapter->start_time = g_ascii_strtod((const gchar *)content, NULL);
                    } else if (xmlStrcmp(prop->name, (const xmlChar *)"title") == 0) {
                        chapter->title = g_strdup((const gchar *)content);
                    } else if (xmlStrcmp(prop->name, (const xmlChar *)"img") == 0) {
                        chapter->img = g_strdup((const gchar *)content);
                    } else if (xmlStrcmp(prop->name, (const xmlChar *)"url") == 0) {
                        chapter->url = g_strdup((const gchar *)content);
                    }
                    xmlFree(content);
                }
                
                chapters = g_list_append(chapters, chapter);
            }
        }
    }
    
    xmlFreeDoc(doc);
    return chapters;
}

GList* podcast_episode_get_chapters(PodcastManager *manager, gint episode_id) {
    if (!manager || !manager->database) return NULL;
    
    /* Get episode details */
    GList *episodes = database_get_podcast_episodes(manager->database, episode_id);
    if (!episodes) return NULL;
    
    PodcastEpisode *episode = (PodcastEpisode *)episodes->data;
    GList *chapters = NULL;
    
    /* Try to fetch external chapters file */
    if (episode->chapters_url) {
        g_print("Fetching chapters from: %s\n", episode->chapters_url);
        gchar *chapters_data = fetch_url(episode->chapters_url);
        if (chapters_data) {
            if (g_str_has_suffix(episode->chapters_url, ".json") || 
                (episode->chapters_type && strstr(episode->chapters_type, "json"))) {
                chapters = parse_chapters_json(chapters_data);
            }
            g_free(chapters_data);
        }
    }
    
    /* If no external chapters, try to get from media file tags */
    if (!chapters && episode->local_file_path && g_file_test(episode->local_file_path, G_FILE_TEST_EXISTS)) {
        /* TODO: Extract embedded chapters from media file using GStreamer */
        g_print("Checking for embedded chapters in: %s\n", episode->local_file_path);
    }
    
    g_list_free_full(episodes, (GDestroyNotify)podcast_episode_free);
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
