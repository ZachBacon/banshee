#include "browser.h"
#include "models.h"
#include <string.h>

/* Factory setup callback for name column */
static void setup_name_label(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_list_item_set_child(list_item, label);
}

static void bind_name_label(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    ShriekBrowserItem *item = gtk_list_item_get_item(list_item);
    if (item) {
        gtk_label_set_text(GTK_LABEL(label), shriek_browser_item_get_name(item));
    }
}

/* Factory setup callback for count column */
static void setup_count_label(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 1.0);
    gtk_list_item_set_child(list_item, label);
}

static void bind_count_label(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    ShriekBrowserItem *item = gtk_list_item_get_item(list_item);
    if (item) {
        gint count = shriek_browser_item_get_count(item);
        if (count > 0) {
            gchar *text = g_strdup_printf("%d", count);
            gtk_label_set_text(GTK_LABEL(label), text);
            g_free(text);
        } else {
            gtk_label_set_text(GTK_LABEL(label), "");
        }
    }
}

BrowserModel* browser_model_new(BrowserType type, Database *database) {
    BrowserModel *model = g_new0(BrowserModel, 1);
    model->type = type;
    model->database = database;
    model->current_filter = NULL;
    
    /* GTK4: Use GListStore with ShriekBrowserItem GObjects */
    model->store = g_list_store_new(SHRIEK_TYPE_BROWSER_ITEM);
    
    browser_model_reload(model);
    return model;
}

void browser_model_free(BrowserModel *model) {
    if (!model) return;
    g_free(model->current_filter);
    if (model->store) {
        g_object_unref(model->store);
    }
    g_free(model);
}

void browser_model_reload(BrowserModel *model) {
    if (!model || !model->database) return;
    
    /* Clear the store */
    g_list_store_remove_all(model->store);
    
    /* Add "All" item first */
    ShriekBrowserItem *all_item = shriek_browser_item_new(0, "All", 0);
    g_list_store_append(model->store, all_item);
    g_object_unref(all_item);
    
    GList *results = NULL;
    switch (model->type) {
        case BROWSER_TYPE_ARTIST:
            results = database_browse_artists(model->database);
            break;
        case BROWSER_TYPE_ALBUM:
            results = database_browse_albums(model->database, model->current_filter);
            break;
        case BROWSER_TYPE_GENRE:
            results = database_browse_genres(model->database);
            break;
        case BROWSER_TYPE_YEAR:
            results = database_browse_years(model->database);
            break;
    }
    
    for (GList *l = results; l != NULL; l = l->next) {
        DatabaseBrowseResult *r = (DatabaseBrowseResult *)l->data;
        ShriekBrowserItem *item = shriek_browser_item_new(g_str_hash(r->name), r->name, r->count);
        g_list_store_append(model->store, item);
        g_object_unref(item);
    }
    
    g_list_free_full(results, (GDestroyNotify)database_browse_result_free);
}

void browser_model_set_filter(BrowserModel *model, const gchar *filter) {
    if (!model) return;
    g_free(model->current_filter);
    model->current_filter = filter ? g_strdup(filter) : NULL;
    browser_model_reload(model);
}

/* GTK4 compatible selection getter */
gchar* browser_model_get_selected_name(BrowserModel *model, GtkSingleSelection *selection) {
    (void)model;
    if (!selection) return NULL;
    
    ShriekBrowserItem *item = gtk_single_selection_get_selected_item(selection);
    if (!item) return NULL;
    
    const gchar *name = shriek_browser_item_get_name(item);
    if (g_strcmp0(name, "All") == 0) {
        return NULL;
    }
    return g_strdup(name);
}

/* Legacy compatibility wrapper - now takes gpointer to work with both old and new code */
gchar* browser_model_get_selection(BrowserModel *model, gpointer selection) {
    /* In the new code, selection will be a GtkSingleSelection */
    if (!selection) return NULL;
    
    /* Check if it's a GtkSingleSelection (GTK4) */
    if (GTK_IS_SINGLE_SELECTION(selection)) {
        return browser_model_get_selected_name(model, GTK_SINGLE_SELECTION(selection));
    }
    
    /* Fallback - shouldn't happen in fully migrated code */
    return NULL;
}

BrowserView* browser_view_new(BrowserModel *model) {
    BrowserView *view = g_new0(BrowserView, 1);
    view->model = model;
    
    view->scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view->scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(view->scrolled_window, 150, -1);
    
    /* GTK4: Create selection model wrapping the GListStore */
    view->selection_model = gtk_single_selection_new(G_LIST_MODEL(g_object_ref(model->store)));
    gtk_single_selection_set_autoselect(view->selection_model, FALSE);
    gtk_single_selection_set_can_unselect(view->selection_model, TRUE);
    
    /* GTK4: Create GtkColumnView */
    view->column_view = gtk_column_view_new(GTK_SELECTION_MODEL(view->selection_model));
    gtk_column_view_set_show_column_separators(GTK_COLUMN_VIEW(view->column_view), FALSE);
    gtk_column_view_set_show_row_separators(GTK_COLUMN_VIEW(view->column_view), FALSE);
    
    /* Name column */
    GtkListItemFactory *name_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(name_factory, "setup", G_CALLBACK(setup_name_label), NULL);
    g_signal_connect(name_factory, "bind", G_CALLBACK(bind_name_label), NULL);
    
    GtkColumnViewColumn *name_column = gtk_column_view_column_new("Name", name_factory);
    gtk_column_view_column_set_expand(name_column, TRUE);
    gtk_column_view_column_set_resizable(name_column, TRUE);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(view->column_view), name_column);
    g_object_unref(name_column);
    
    /* Count column */
    GtkListItemFactory *count_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(count_factory, "setup", G_CALLBACK(setup_count_label), NULL);
    g_signal_connect(count_factory, "bind", G_CALLBACK(bind_count_label), NULL);
    
    GtkColumnViewColumn *count_column = gtk_column_view_column_new("#", count_factory);
    gtk_column_view_column_set_fixed_width(count_column, 50);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(view->column_view), count_column);
    g_object_unref(count_column);
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(view->scrolled_window), view->column_view);
    
    return view;
}

void browser_view_free(BrowserView *view) {
    if (!view) return;
    if (view->selection_model && view->selection_handler_id > 0) {
        g_signal_handler_disconnect(view->selection_model, view->selection_handler_id);
    }
    g_free(view);
}

void browser_view_set_selection_callback(BrowserView *view, GCallback callback, gpointer user_data) {
    if (!view) return;
    view->selection_changed = callback;
    view->user_data = user_data;
    
    /* GTK4: Connect to selection model's "selection-changed" signal */
    view->selection_handler_id = g_signal_connect(view->selection_model, "selection-changed", 
                                                   callback, user_data);
}

GtkWidget* browser_view_get_widget(BrowserView *view) {
    return view ? view->scrolled_window : NULL;
}

GtkSingleSelection* browser_view_get_selection_model(BrowserView *view) {
    return view ? view->selection_model : NULL;
}

GList* browser_get_artists(Database *db) {
    return database_get_distinct_artists(db);
}

GList* browser_get_albums(Database *db, const gchar *artist_filter) {
    return database_get_distinct_albums(db, artist_filter);
}

GList* browser_get_genres(Database *db) {
    return database_get_distinct_genres(db);
}

void browser_item_free(BrowserItem *item) {
    if (!item) return;
    g_free(item->name);
    g_free(item);
}
