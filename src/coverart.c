#include "coverart.h"
#include "database.h"
#include <string.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/tag/tag.h>

/* Thread pool function wrapper */
static void coverart_fetch_pool_func(gpointer data, gpointer user_data);

CoverArtManager* coverart_manager_new(void) {
    CoverArtManager *manager = g_new0(CoverArtManager, 1);
    
    manager->cache_dir = g_build_filename(g_get_user_cache_dir(), "banshee", "covers", NULL);
    g_mkdir_with_parents(manager->cache_dir, 0755);
    
    manager->cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
    g_mutex_init(&manager->cache_mutex);
    
    /* Create thread pool with max 4 concurrent threads */
    GError *error = NULL;
    manager->fetch_pool = g_thread_pool_new(coverart_fetch_pool_func, manager, 4, FALSE, &error);
    if (error) {
        g_warning("Failed to create cover art thread pool: %s", error->message);
        g_error_free(error);
    }
    
    return manager;
}

void coverart_manager_free(CoverArtManager *manager) {
    if (!manager) return;
    
    /* Free thread pool - wait for pending tasks */
    if (manager->fetch_pool) {
        g_thread_pool_free(manager->fetch_pool, FALSE, TRUE);
    }
    
    g_free(manager->cache_dir);
    g_hash_table_destroy(manager->cache);
    g_mutex_clear(&manager->cache_mutex);
    g_free(manager);
}

static gchar* coverart_generate_cache_key(const gchar *artist, const gchar *album) {
    return g_strdup_printf("%s-%s", artist ? artist : "Unknown", album ? album : "Unknown");
}

gchar* coverart_get_cache_path(CoverArtManager *manager, const gchar *artist, const gchar *album) {
    if (!manager) return NULL;
    
    gchar *key = coverart_generate_cache_key(artist, album);
    gchar *encoded = g_compute_checksum_for_string(G_CHECKSUM_MD5, key, -1);
    g_free(key);
    
    gchar *path = g_build_filename(manager->cache_dir, encoded, NULL);
    g_free(encoded);
    
    return path;
}

gboolean coverart_exists(CoverArtManager *manager, const gchar *artist, const gchar *album) {
    if (!manager) return FALSE;
    
    gchar *path = coverart_get_cache_path(manager, artist, album);
    gboolean exists = g_file_test(path, G_FILE_TEST_EXISTS);
    g_free(path);
    
    return exists;
}

GdkPixbuf* coverart_get(CoverArtManager *manager, const gchar *artist, const gchar *album, gint size) {
    if (!manager) return NULL;
    
    /* Include size in cache key so different sizes are cached separately */
    gchar *base_key = coverart_generate_cache_key(artist, album);
    gchar *key = g_strdup_printf("%s@%d", base_key, size);
    g_free(base_key);
    
    g_mutex_lock(&manager->cache_mutex);
    GdkPixbuf *cached = g_hash_table_lookup(manager->cache, key);
    if (cached) {
        g_object_ref(cached);
        g_mutex_unlock(&manager->cache_mutex);
        g_free(key);
        return cached;
    }
    g_mutex_unlock(&manager->cache_mutex);
    
    gchar *path = coverart_get_cache_path(manager, artist, album);
    GdkPixbuf *pixbuf = NULL;
    
    if (g_file_test(path, G_FILE_TEST_EXISTS)) {
        GError *error = NULL;
        pixbuf = gdk_pixbuf_new_from_file_at_scale(path, size, size, TRUE, &error);
        if (error) {
            g_warning("Failed to load cover art: %s", error->message);
            g_error_free(error);
        } else {
            g_mutex_lock(&manager->cache_mutex);
            g_hash_table_insert(manager->cache, g_strdup(key), g_object_ref(pixbuf));
            g_mutex_unlock(&manager->cache_mutex);
        }
    }
    
    g_free(key);
    g_free(path);
    return pixbuf;
}

GdkPixbuf* coverart_get_from_file(const gchar *file_path, gint size) {
    if (!file_path) return NULL;
    
    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(file_path, size, size, TRUE, &error);
    
    if (error) {
        g_warning("Failed to load image: %s", error->message);
        g_error_free(error);
        return NULL;
    }
    
    return pixbuf;
}

/* Extract album art from audio file tags */
GdkPixbuf* coverart_extract_from_audio(const gchar *audio_file_path, gint size) {
    if (!audio_file_path) return NULL;
    
    GstDiscoverer *discoverer = NULL;
    GstDiscovererInfo *info = NULL;
    GdkPixbuf *pixbuf = NULL;
    GError *error = NULL;
    
    /* Initialize GStreamer if not already done */
    if (!gst_is_initialized()) {
        gst_init(NULL, NULL);
    }
    
    /* Create discoverer */
    discoverer = gst_discoverer_new(5 * GST_SECOND, &error);
    if (error) {
        g_warning("Failed to create discoverer: %s", error->message);
        g_error_free(error);
        return NULL;
    }
    
    /* Build URI from file path */
    gchar *uri = g_filename_to_uri(audio_file_path, NULL, &error);
    if (error) {
        g_warning("Failed to convert path to URI: %s", error->message);
        g_error_free(error);
        g_object_unref(discoverer);
        return NULL;
    }
    
    /* Discover file */
    info = gst_discoverer_discover_uri(discoverer, uri, &error);
    if (error) {
        g_warning("Failed to discover file %s: %s", audio_file_path, error->message);
        g_error_free(error);
        g_free(uri);
        g_object_unref(discoverer);
        return NULL;
    }
    
    /* Get tags */
    const GstTagList *tags = gst_discoverer_info_get_tags(info);
    if (tags) {
        GstSample *sample = NULL;
        
        /* Try to get image tag */
        if (gst_tag_list_get_sample(tags, GST_TAG_IMAGE, &sample) ||
            gst_tag_list_get_sample(tags, GST_TAG_PREVIEW_IMAGE, &sample)) {
            
            GstBuffer *buffer = gst_sample_get_buffer(sample);
            if (buffer) {
                GstMapInfo map;
                if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
                    /* Create pixbuf from image data */
                    GInputStream *stream = g_memory_input_stream_new_from_data(
                        g_memdup2(map.data, map.size), map.size, g_free);
                    
                    GError *pixbuf_error = NULL;
                    pixbuf = gdk_pixbuf_new_from_stream_at_scale(stream, size, size, TRUE, NULL, &pixbuf_error);
                    
                    if (pixbuf_error) {
                        g_warning("Failed to create pixbuf from image data: %s", pixbuf_error->message);
                        g_error_free(pixbuf_error);
                    }
                    
                    gst_buffer_unmap(buffer, &map);
                    g_object_unref(stream);
                }
            }
            
            gst_sample_unref(sample);
        }
    }
    
    /* Cleanup */
    gst_discoverer_info_unref(info);
    g_free(uri);
    g_object_unref(discoverer);
    
    return pixbuf;
}

/* Search for cover art in the same directory as the audio file */
GdkPixbuf* coverart_search_directory(const gchar *audio_file_path, gint size) {
    if (!audio_file_path) return NULL;
    
    gchar *dir_path = g_path_get_dirname(audio_file_path);
    GDir *dir = g_dir_open(dir_path, 0, NULL);
    
    if (!dir) {
        g_free(dir_path);
        return NULL;
    }
    
    /* Common cover art filenames */
    const gchar *cover_names[] = {
        "cover.jpg", "cover.png", "Cover.jpg", "Cover.png",
        "folder.jpg", "folder.png", "Folder.jpg", "Folder.png",
        "album.jpg", "album.png", "Album.jpg", "Album.png",
        "front.jpg", "front.png", "Front.jpg", "Front.png",
        NULL
    };
    
    GdkPixbuf *pixbuf = NULL;
    
    /* First try common names */
    for (int i = 0; cover_names[i] != NULL; i++) {
        gchar *cover_path = g_build_filename(dir_path, cover_names[i], NULL);
        if (g_file_test(cover_path, G_FILE_TEST_EXISTS)) {
            pixbuf = coverart_get_from_file(cover_path, size);
            g_free(cover_path);
            if (pixbuf) break;
        }
        g_free(cover_path);
    }
    
    /* If not found, search for any jpg/png file */
    if (!pixbuf) {
        const gchar *filename;
        while ((filename = g_dir_read_name(dir)) != NULL) {
            gchar *lower = g_ascii_strdown(filename, -1);
            if (g_str_has_suffix(lower, ".jpg") || 
                g_str_has_suffix(lower, ".jpeg") || 
                g_str_has_suffix(lower, ".png")) {
                gchar *cover_path = g_build_filename(dir_path, filename, NULL);
                pixbuf = coverart_get_from_file(cover_path, size);
                g_free(cover_path);
                if (pixbuf) {
                    g_free(lower);
                    break;
                }
            }
            g_free(lower);
        }
    }
    
    g_dir_close(dir);
    g_free(dir_path);
    
    return pixbuf;
}

/* Extract or find album art for a track and cache it */
gboolean coverart_extract_and_cache(CoverArtManager *manager, const gchar *audio_file_path,
                                    const gchar *artist, const gchar *album) {
    if (!manager || !audio_file_path) return FALSE;
    
    /* Check if already cached */
    if (coverart_exists(manager, artist, album)) {
        return TRUE;
    }
    
    GdkPixbuf *pixbuf = NULL;
    
    /* First try to extract from the audio file itself */
    pixbuf = coverart_extract_from_audio(audio_file_path, COVER_ART_SIZE_LARGE);
    
    /* If not found, search in the directory */
    if (!pixbuf) {
        pixbuf = coverart_search_directory(audio_file_path, COVER_ART_SIZE_LARGE);
    }
    
    /* Cache if found */
    if (pixbuf) {
        gboolean success = coverart_save(manager, artist, album, pixbuf);
        g_object_unref(pixbuf);
        return success;
    }
    
    return FALSE;
}

gboolean coverart_save(CoverArtManager *manager, const gchar *artist, const gchar *album, GdkPixbuf *pixbuf) {
    if (!manager || !pixbuf) return FALSE;
    
    gchar *path = coverart_get_cache_path(manager, artist, album);
    GError *error = NULL;
    
    gboolean success = gdk_pixbuf_save(pixbuf, path, "jpeg", &error, "quality", "90", NULL);
    
    if (error) {
        g_warning("Failed to save cover art: %s", error->message);
        g_error_free(error);
    } else {
        gchar *key = coverart_generate_cache_key(artist, album);
        g_mutex_lock(&manager->cache_mutex);
        g_hash_table_insert(manager->cache, key, g_object_ref(pixbuf));
        g_mutex_unlock(&manager->cache_mutex);
    }
    
    g_free(path);
    return success;
}

typedef struct {
    CoverArtManager *manager;
    Database *database;
    gchar *artist;
    gchar *album;
    gint size;
    CoverArtFetchCallback callback;
    gpointer user_data;
} FetchData;

typedef struct {
    CoverArtFetchCallback callback;
    GdkPixbuf *pixbuf;
    gpointer user_data;
} CallbackData;

static gboolean invoke_callback_in_main(gpointer data) {
    CallbackData *cb_data = (CallbackData *)data;
    if (cb_data->callback) {
        cb_data->callback(cb_data->pixbuf, cb_data->user_data);
    }
    if (cb_data->pixbuf) {
        g_object_unref(cb_data->pixbuf);
    }
    g_free(cb_data);
    return G_SOURCE_REMOVE;
}

/* Thread pool function for fetching cover art */
static void coverart_fetch_pool_func(gpointer data, gpointer user_data) {
    FetchData *fetch = (FetchData *)data;
    (void)user_data;  /* Manager passed via FetchData */
    
    g_debug("Fetching cover art for: %s - %s (size %d)", 
            fetch->artist ? fetch->artist : "Unknown",
            fetch->album ? fetch->album : "Unknown",
            fetch->size);
    
    GdkPixbuf *pixbuf = coverart_get(fetch->manager, fetch->artist, fetch->album, fetch->size);
    
    if (pixbuf) {
        g_debug("Found cover art in cache for: %s - %s", 
                fetch->artist ? fetch->artist : "Unknown",
                fetch->album ? fetch->album : "Unknown");
    }
    
    /* If not in cache, try to extract from audio file */
    if (!pixbuf && fetch->database) {
        g_debug("Trying to extract from audio file for: %s - %s", 
                fetch->artist ? fetch->artist : "Unknown",
                fetch->album ? fetch->album : "Unknown");
        
        GList *tracks = database_get_tracks_by_album(fetch->database, fetch->artist, fetch->album);
        if (tracks) {
            Track *track = (Track *)tracks->data;
            if (track && track->file_path) {
                g_debug("Found track: %s", track->file_path);
                /* Try to extract and cache */
                if (coverart_extract_and_cache(fetch->manager, track->file_path, 
                                               fetch->artist, fetch->album)) {
                    /* Now try to get it from cache */
                    pixbuf = coverart_get(fetch->manager, fetch->artist, fetch->album, fetch->size);
                    if (pixbuf) {
                        g_debug("Successfully extracted cover art from: %s", track->file_path);
                    }
                } else {
                    g_debug("Failed to extract cover art from: %s", track->file_path);
                }
            }
            g_list_free_full(tracks, (GDestroyNotify)track_free);
        } else {
            g_debug("No tracks found for: %s - %s", 
                    fetch->artist ? fetch->artist : "Unknown",
                    fetch->album ? fetch->album : "Unknown");
        }
    } else if (!pixbuf) {
        g_debug("No database available to look up tracks");
    }
    
    if (!pixbuf) {
        g_debug("Creating default gray cover for: %s - %s", 
                fetch->artist ? fetch->artist : "Unknown",
                fetch->album ? fetch->album : "Unknown");
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, fetch->size, fetch->size);
        gdk_pixbuf_fill(pixbuf, 0x333333FF);
    }
    
    if (fetch->callback && pixbuf) {
        CallbackData *cb_data = g_new0(CallbackData, 1);
        cb_data->callback = fetch->callback;
        cb_data->pixbuf = pixbuf;  /* Transfer ownership */
        cb_data->user_data = fetch->user_data;
        g_main_context_invoke(NULL, invoke_callback_in_main, cb_data);
    } else if (pixbuf) {
        g_object_unref(pixbuf);
    }
    
    g_free(fetch->artist);
    g_free(fetch->album);
    g_free(fetch);
}

void coverart_fetch_async(CoverArtManager *manager, const gchar *artist, const gchar *album,
                          gint size, CoverArtFetchCallback callback, gpointer user_data) {
    coverart_fetch_async_with_db(manager, NULL, artist, album, size, callback, user_data);
}

void coverart_fetch_async_with_db(CoverArtManager *manager, Database *database,
                                  const gchar *artist, const gchar *album,
                                  gint size, CoverArtFetchCallback callback, gpointer user_data) {
    if (!manager || !manager->fetch_pool) return;
    
    FetchData *fetch = g_new0(FetchData, 1);
    fetch->manager = manager;
    fetch->database = database;
    fetch->artist = g_strdup(artist);
    fetch->album = g_strdup(album);
    fetch->size = size;
    fetch->callback = callback;
    fetch->user_data = user_data;
    
    GError *error = NULL;
    g_thread_pool_push(manager->fetch_pool, fetch, &error);
    if (error) {
        g_warning("Failed to push cover art fetch to pool: %s", error->message);
        g_error_free(error);
        g_free(fetch->artist);
        g_free(fetch->album);
        g_free(fetch);
    }
}

/* URL-based cover art functions for podcasts */
gchar* coverart_get_url_cache_path(CoverArtManager *manager, const gchar *url) {
    if (!manager || !url) return NULL;
    
    gchar *encoded = g_compute_checksum_for_string(G_CHECKSUM_MD5, url, -1);
    gchar *path = g_build_filename(manager->cache_dir, encoded, NULL);
    g_free(encoded);
    
    return path;
}

gboolean coverart_cache_url_image(CoverArtManager *manager, const gchar *url, GdkPixbuf *pixbuf) {
    if (!manager || !url || !pixbuf) return FALSE;
    
    gchar *path = coverart_get_url_cache_path(manager, url);
    GError *error = NULL;
    
    gboolean success = gdk_pixbuf_save(pixbuf, path, "jpeg", &error, "quality", "90", NULL);
    
    if (error) {
        g_warning("Failed to cache URL image: %s", error->message);
        g_error_free(error);
    } else {
        g_mutex_lock(&manager->cache_mutex);
        g_hash_table_insert(manager->cache, g_strdup(url), g_object_ref(pixbuf));
        g_mutex_unlock(&manager->cache_mutex);
    }
    
    g_free(path);
    return success;
}

/* Forward declaration for external fetch_binary_url function from podcast.c */
extern gchar* fetch_binary_url(const gchar *url, gsize *out_size);

GdkPixbuf* coverart_get_from_url(CoverArtManager *manager, const gchar *url, gint size) {
    if (!manager || !url) return NULL;
    
    /* Check memory cache first */
    g_mutex_lock(&manager->cache_mutex);
    GdkPixbuf *cached = g_hash_table_lookup(manager->cache, url);
    if (cached) {
        g_object_ref(cached);
        g_mutex_unlock(&manager->cache_mutex);
        return cached;
    }
    g_mutex_unlock(&manager->cache_mutex);
    
    /* Check disk cache */
    gchar *cache_path = coverart_get_url_cache_path(manager, url);
    GdkPixbuf *pixbuf = NULL;
    
    if (g_file_test(cache_path, G_FILE_TEST_EXISTS)) {
        GError *error = NULL;
        pixbuf = gdk_pixbuf_new_from_file_at_scale(cache_path, size, size, TRUE, &error);
        if (error) {
            g_warning("Failed to load cached image: %s", error->message);
            g_error_free(error);
        } else {
            g_mutex_lock(&manager->cache_mutex);
            g_hash_table_insert(manager->cache, g_strdup(url), g_object_ref(pixbuf));
            g_mutex_unlock(&manager->cache_mutex);
        }
        g_free(cache_path);
        return pixbuf;
    }
    
    /* Download image from URL */
    gsize data_size = 0;
    gchar *image_data = fetch_binary_url(url, &data_size);
    if (image_data && data_size > 0) {
        GInputStream *stream = g_memory_input_stream_new_from_data(
            image_data, data_size, g_free);
        
        GError *error = NULL;
        pixbuf = gdk_pixbuf_new_from_stream_at_scale(stream, size, size, TRUE, NULL, &error);
        
        if (error) {
            g_warning("Failed to create pixbuf from URL data: %s", error->message);
            g_error_free(error);
        } else {
            /* Cache the image */
            coverart_cache_url_image(manager, url, pixbuf);
        }
        
        g_object_unref(stream);
        /* image_data freed by g_memory_input_stream_new_from_data */
    }
    
    g_free(cache_path);
    return pixbuf;
}

typedef struct {
    CoverArtManager *manager;
    gchar *url;
    gint size;
    CoverArtFetchCallback callback;
    gpointer user_data;
} URLFetchData;

static gpointer coverart_fetch_url_thread(gpointer data) {
    URLFetchData *fetch = (URLFetchData *)data;
    
    GdkPixbuf *pixbuf = coverart_get_from_url(fetch->manager, fetch->url, fetch->size);
    
    if (!pixbuf) {
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, fetch->size, fetch->size);
        gdk_pixbuf_fill(pixbuf, 0x333333FF);
    }
    
    if (fetch->callback && pixbuf) {
        CallbackData *cb_data = g_new0(CallbackData, 1);
        cb_data->callback = fetch->callback;
        cb_data->pixbuf = pixbuf;  /* Transfer ownership */
        cb_data->user_data = fetch->user_data;
        g_main_context_invoke(NULL, invoke_callback_in_main, cb_data);
    } else if (pixbuf) {
        g_object_unref(pixbuf);
    }
    
    g_free(fetch->url);
    g_free(fetch);
    
    return NULL;
}

void coverart_fetch_from_url_async(CoverArtManager *manager, const gchar *url,
                                   gint size, CoverArtFetchCallback callback, gpointer user_data) {
    if (!manager || !url) return;
    
    URLFetchData *fetch = g_new0(URLFetchData, 1);
    fetch->manager = manager;
    fetch->url = g_strdup(url);
    fetch->size = size;
    fetch->callback = callback;
    fetch->user_data = user_data;
    
    g_thread_new("coverart-url-fetch", coverart_fetch_url_thread, fetch);
}

GtkWidget* coverart_widget_new(gint size) {
    GtkWidget *image = gtk_image_new();
    gtk_widget_set_size_request(image, size, size);
    gtk_image_set_pixel_size(GTK_IMAGE(image), size);
    return image;
}

void coverart_widget_set_image(GtkWidget *widget, GdkPixbuf *pixbuf) {
    if (!GTK_IS_IMAGE(widget)) return;
    
    if (pixbuf) {
        /* GTK4: Use GdkTexture instead of deprecated gtk_image_set_from_pixbuf */
        GBytes *bytes = gdk_pixbuf_read_pixel_bytes(pixbuf);
        int width = gdk_pixbuf_get_width(pixbuf);
        int height = gdk_pixbuf_get_height(pixbuf);
        int stride = gdk_pixbuf_get_rowstride(pixbuf);
        gboolean has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);
        
        GdkTexture *texture = gdk_memory_texture_new(width, height,
            has_alpha ? GDK_MEMORY_R8G8B8A8 : GDK_MEMORY_R8G8B8,
            bytes, stride);
        g_bytes_unref(bytes);
        gtk_image_set_from_paintable(GTK_IMAGE(widget), GDK_PAINTABLE(texture));
        g_object_unref(texture);
    } else {
        gtk_image_clear(GTK_IMAGE(widget));
    }
}

void coverart_widget_set_from_manager(GtkWidget *widget, CoverArtManager *manager,
                                      const gchar *artist, const gchar *album, gint size) {
    if (!GTK_IS_IMAGE(widget) || !manager) return;
    
    GdkPixbuf *pixbuf = coverart_get(manager, artist, album, size);
    
    if (!pixbuf) {
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, size, size);
        gdk_pixbuf_fill(pixbuf, 0x333333FF);
    }
    
    coverart_widget_set_image(widget, pixbuf);
    
    if (pixbuf) g_object_unref(pixbuf);
}

typedef struct {
    GtkWidget *widget;
    gchar *url;
    gint size;
} URLWidgetData;

static gboolean update_widget_with_pixbuf_main_thread(gpointer data) {
    CallbackData *cb_data = (CallbackData *)data;
    URLWidgetData *widget_data = (URLWidgetData *)cb_data->user_data;
    
    if (GTK_IS_IMAGE(widget_data->widget) && cb_data->pixbuf) {
        /* Ensure the pixbuf is the right size */
        GdkPixbuf *scaled = gdk_pixbuf_scale_simple(cb_data->pixbuf, widget_data->size, widget_data->size, GDK_INTERP_BILINEAR);
        coverart_widget_set_image(widget_data->widget, scaled);
        g_object_unref(scaled);
    }
    
    /* Cleanup */
    g_object_unref(widget_data->widget); /* Release our reference */
    g_free(widget_data->url);
    g_free(widget_data);
    
    if (cb_data->pixbuf) {
        g_object_unref(cb_data->pixbuf);
    }
    g_free(cb_data);
    return G_SOURCE_REMOVE;
}

static void widget_url_fetch_callback(GdkPixbuf *pixbuf, gpointer user_data) {
    URLWidgetData *widget_data = (URLWidgetData *)user_data;
    
    CallbackData *cb_data = g_new0(CallbackData, 1);
    cb_data->pixbuf = pixbuf ? g_object_ref(pixbuf) : NULL;
    cb_data->user_data = widget_data;
    
    g_main_context_invoke(NULL, update_widget_with_pixbuf_main_thread, cb_data);
}

void coverart_widget_set_from_url(GtkWidget *widget, const gchar *url) {
    if (!GTK_IS_IMAGE(widget) || !url) return;
    
    /* Get the size from the widget's size request */
    gint width, height;
    gtk_widget_get_size_request(widget, &width, &height);
    gint size = (width > 0) ? width : COVER_ART_SIZE_SMALL;
    
    /* Set a temporary placeholder while loading */
    GdkPixbuf *placeholder = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, size, size);
    gdk_pixbuf_fill(placeholder, 0x4A90E2FF); /* Blue placeholder for podcast images */
    coverart_widget_set_image(widget, placeholder);
    g_object_unref(placeholder);
    
    /* Prepare data for async fetch */
    URLWidgetData *widget_data = g_new0(URLWidgetData, 1);
    widget_data->widget = g_object_ref(widget); /* Keep a reference */
    widget_data->url = g_strdup(url);
    widget_data->size = size;
    
    /* Start async fetch - use a dummy manager (this could be improved) */
    static CoverArtManager *dummy_manager = NULL;
    if (!dummy_manager) {
        dummy_manager = coverart_manager_new();
    }
    
    /* Use existing async URL fetch */
    coverart_fetch_from_url_async(dummy_manager, url, size, widget_url_fetch_callback, widget_data);
}

gboolean coverart_widget_set_from_album(GtkWidget *widget, CoverArtManager *manager,
                                        const gchar *artist, const gchar *album) {
    if (!GTK_IS_IMAGE(widget) || !manager) return FALSE;
    
    /* Get the size from the widget's size request */
    gint width, height;
    gtk_widget_get_size_request(widget, &width, &height);
    gint size = (width > 0) ? width : COVER_ART_SIZE_SMALL;
    
    GdkPixbuf *pixbuf = coverart_get(manager, artist, album, size);
    
    if (pixbuf) {
        /* Ensure the pixbuf is exactly the right size */
        GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pixbuf, size, size, GDK_INTERP_BILINEAR);
        coverart_widget_set_image(widget, scaled);
        g_object_unref(pixbuf);
        g_object_unref(scaled);
        return TRUE;
    }
    
    return FALSE;
}

void coverart_widget_set_default(GtkWidget *widget) {
    if (!GTK_IS_IMAGE(widget)) return;
    
    /* Get the size from the widget's size request */
    gint width, height;
    gtk_widget_get_size_request(widget, &width, &height);
    gint size = (width > 0) ? width : COVER_ART_SIZE_SMALL;
    
    GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, size, size);
    gdk_pixbuf_fill(pixbuf, 0x333333FF); /* Default gray color */
    
    coverart_widget_set_image(widget, pixbuf);
    g_object_unref(pixbuf);
}
