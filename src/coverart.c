#include "coverart.h"
#include <string.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/tag/tag.h>

CoverArtManager* coverart_manager_new(void) {
    CoverArtManager *manager = g_new0(CoverArtManager, 1);
    
    manager->cache_dir = g_build_filename(g_get_user_cache_dir(), "banshee", "covers", NULL);
    g_mkdir_with_parents(manager->cache_dir, 0755);
    
    manager->cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
    g_mutex_init(&manager->cache_mutex);
    
    return manager;
}

void coverart_manager_free(CoverArtManager *manager) {
    if (!manager) return;
    
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
    
    gchar *key = coverart_generate_cache_key(artist, album);
    
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

static gpointer coverart_fetch_thread(gpointer data) {
    FetchData *fetch = (FetchData *)data;
    
    GdkPixbuf *pixbuf = coverart_get(fetch->manager, fetch->artist, fetch->album, fetch->size);
    
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
    
    g_free(fetch->artist);
    g_free(fetch->album);
    g_free(fetch);
    
    return NULL;
}

void coverart_fetch_async(CoverArtManager *manager, const gchar *artist, const gchar *album,
                          gint size, CoverArtFetchCallback callback, gpointer user_data) {
    if (!manager) return;
    
    FetchData *fetch = g_new0(FetchData, 1);
    fetch->manager = manager;
    fetch->artist = g_strdup(artist);
    fetch->album = g_strdup(album);
    fetch->size = size;
    fetch->callback = callback;
    fetch->user_data = user_data;
    
    g_thread_new("coverart-fetch", coverart_fetch_thread, fetch);
}

GtkWidget* coverart_widget_new(gint size) {
    GtkWidget *image = gtk_image_new();
    gtk_widget_set_size_request(image, size, size);
    return image;
}

void coverart_widget_set_image(GtkWidget *widget, GdkPixbuf *pixbuf) {
    if (!GTK_IS_IMAGE(widget)) return;
    
    if (pixbuf) {
        gtk_image_set_from_pixbuf(GTK_IMAGE(widget), pixbuf);
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
