#ifndef BROWSER_H
#define BROWSER_H

#include <gtk/gtk.h>
#include "database.h"
#include "models.h"

/* Browser filter types */
typedef enum {
    BROWSER_TYPE_ARTIST,
    BROWSER_TYPE_ALBUM,
    BROWSER_TYPE_GENRE,
    BROWSER_TYPE_YEAR
} BrowserType;

/* Browser item structure (legacy - kept for compatibility) */
typedef struct {
    gint id;
    gchar *name;
    gint count;
    gint64 duration;
} BrowserItem;

/* Browser model - now uses GListStore */
typedef struct {
    GListStore *store;           /* GListStore of ShriekBrowserItem objects */
    BrowserType type;
    Database *database;
    gchar *current_filter;
} BrowserModel;

/* Browser view widget - now uses GtkColumnView */
typedef struct {
    GtkWidget *scrolled_window;
    GtkWidget *column_view;      /* GtkColumnView instead of GtkTreeView */
    GtkSingleSelection *selection_model;
    BrowserModel *model;
    GCallback selection_changed;
    gpointer user_data;
    gulong selection_handler_id;
} BrowserView;

/* Browser model functions */
BrowserModel* browser_model_new(BrowserType type, Database *database);
void browser_model_free(BrowserModel *model);
void browser_model_reload(BrowserModel *model);
void browser_model_set_filter(BrowserModel *model, const gchar *filter);

/* GTK4 compatible selection getter */
gchar* browser_model_get_selected_name(BrowserModel *model, GtkSingleSelection *selection);

/* Legacy compatibility - wraps GTK4 API internally */
gchar* browser_model_get_selection(BrowserModel *model, gpointer selection);

/* Browser view functions */
BrowserView* browser_view_new(BrowserModel *model);
void browser_view_free(BrowserView *view);
void browser_view_set_selection_callback(BrowserView *view, GCallback callback, gpointer user_data);
GtkWidget* browser_view_get_widget(BrowserView *view);
GtkSingleSelection* browser_view_get_selection_model(BrowserView *view);

/* Utility functions */
GList* browser_get_artists(Database *db);
GList* browser_get_albums(Database *db, const gchar *artist_filter);
GList* browser_get_genres(Database *db);
void browser_item_free(BrowserItem *item);

#endif /* BROWSER_H */
