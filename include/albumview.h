#ifndef ALBUMVIEW_H
#define ALBUMVIEW_H

#include <gtk/gtk.h>
#include "database.h"
#include "coverart.h"

/* Album item object for GListStore */
#define ALBUM_TYPE_ITEM (album_item_get_type())
G_DECLARE_FINAL_TYPE(AlbumItem, album_item, ALBUM, ITEM, GObject)

struct _AlbumItem {
    GObject parent_instance;
    gchar *artist;
    gchar *album;
    GdkPaintable *cover;
    GtkWidget *picture;  /* Weak reference to bound picture widget */
};

AlbumItem* album_item_new(const gchar *artist, const gchar *album);
void album_item_set_cover(AlbumItem *item, GdkPixbuf *pixbuf);

typedef struct {
    GtkWidget *scrolled_window;
    GtkWidget *grid_view;
    GListStore *store;
    GtkSingleSelection *selection;
    CoverArtManager *coverart_manager;
    Database *database;
    gchar *current_artist;
} AlbumView;

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
