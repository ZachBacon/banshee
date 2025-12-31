#ifndef ALBUMVIEW_H
#define ALBUMVIEW_H

#include <gtk/gtk.h>
#include "database.h"
#include "coverart.h"

typedef struct {
    GtkWidget *scrolled_window;
    GtkWidget *icon_view;
    GtkListStore *store;
    CoverArtManager *coverart_manager;
    Database *database;
    gchar *current_artist;
} AlbumView;

enum {
    ALBUM_COL_PIXBUF = 0,
    ALBUM_COL_ARTIST,
    ALBUM_COL_ALBUM,
    ALBUM_COL_COUNT
};

/* Album view functions */
AlbumView* album_view_new(CoverArtManager *coverart_manager, Database *database);
void album_view_free(AlbumView *view);
GtkWidget* album_view_get_widget(AlbumView *view);

/* Update album view */
void album_view_set_artist(AlbumView *view, const gchar *artist);
void album_view_clear(AlbumView *view);

/* Selection callback */
typedef void (*AlbumSelectedCallback)(const gchar *artist, const gchar *album, gpointer user_data);
void album_view_set_selection_callback(AlbumView *view, AlbumSelectedCallback callback, gpointer user_data);

#endif /* ALBUMVIEW_H */
