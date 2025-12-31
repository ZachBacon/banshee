#ifndef COVERART_H
#define COVERART_H

#include <gtk/gtk.h>
#include <glib.h>

#define COVER_ART_SIZE_SMALL 48
#define COVER_ART_SIZE_MEDIUM 128
#define COVER_ART_SIZE_LARGE 300

typedef struct {
    gchar *cache_dir;
    GHashTable *cache;
    GMutex cache_mutex;
} CoverArtManager;

/* Cover art manager */
CoverArtManager* coverart_manager_new(void);
void coverart_manager_free(CoverArtManager *manager);

/* Cover art retrieval */
GdkPixbuf* coverart_get(CoverArtManager *manager, const gchar *artist, const gchar *album, gint size);
GdkPixbuf* coverart_get_from_file(const gchar *file_path, gint size);
gchar* coverart_get_cache_path(CoverArtManager *manager, const gchar *artist, const gchar *album);

/* Cover art extraction */
GdkPixbuf* coverart_extract_from_audio(const gchar *audio_file_path, gint size);
GdkPixbuf* coverart_search_directory(const gchar *audio_file_path, gint size);
gboolean coverart_extract_and_cache(CoverArtManager *manager, const gchar *audio_file_path,
                                    const gchar *artist, const gchar *album);

/* Cover art storage */
gboolean coverart_save(CoverArtManager *manager, const gchar *artist, const gchar *album, GdkPixbuf *pixbuf);
gboolean coverart_exists(CoverArtManager *manager, const gchar *artist, const gchar *album);

/* Cover art fetching (async) */
typedef void (*CoverArtFetchCallback)(GdkPixbuf *pixbuf, gpointer user_data);
void coverart_fetch_async(CoverArtManager *manager, const gchar *artist, const gchar *album, 
                          gint size, CoverArtFetchCallback callback, gpointer user_data);

/* Cover art display widget */
GtkWidget* coverart_widget_new(gint size);
void coverart_widget_set_image(GtkWidget *widget, GdkPixbuf *pixbuf);
void coverart_widget_set_from_manager(GtkWidget *widget, CoverArtManager *manager, 
                                      const gchar *artist, const gchar *album, gint size);

#endif /* COVERART_H */
