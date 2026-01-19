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
    BansheeBrowserItem *item = gtk_list_item_get_item(list_item);
    if (item) {
        gtk_label_set_text(GTK_LABEL(label), banshee_browser_item_get_name(item));
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
    BansheeBrowserItem *item = gtk_list_item_get_item(list_item);
    if (item) {
        gint count = banshee_browser_item_get_count(item);
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
    
    /* GTK4: Use GListStore with BansheeBrowserItem GObjects */
    model->store = g_list_store_new(BANSHEE_TYPE_BROWSER_ITEM);
    
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
    BansheeBrowserItem *all_item = banshee_browser_item_new(0, "All", 0);
    g_list_store_append(model->store, all_item);
    g_object_unref(all_item);
    
    const char *sql = NULL;
    switch (model->type) {
        case BROWSER_TYPE_ARTIST:
            sql = "SELECT DISTINCT Artist, COUNT(*) FROM tracks WHERE Artist IS NOT NULL AND Artist != '' AND ("
                  "LOWER(file_path) LIKE '%.mp3' OR LOWER(file_path) LIKE '%.ogg' OR LOWER(file_path) LIKE '%.flac' OR "
                  "LOWER(file_path) LIKE '%.wav' OR LOWER(file_path) LIKE '%.m4a' OR LOWER(file_path) LIKE '%.aac' OR "
                  "LOWER(file_path) LIKE '%.opus' OR LOWER(file_path) LIKE '%.wma' OR LOWER(file_path) LIKE '%.ape' OR "
                  "LOWER(file_path) LIKE '%.mpc') GROUP BY Artist ORDER BY Artist";
            break;
        case BROWSER_TYPE_ALBUM:
            sql = model->current_filter
                ? "SELECT DISTINCT Album, COUNT(*) FROM tracks WHERE Album IS NOT NULL AND Album != '' AND Artist = ? AND ("
                  "LOWER(file_path) LIKE '%.mp3' OR LOWER(file_path) LIKE '%.ogg' OR LOWER(file_path) LIKE '%.flac' OR "
                  "LOWER(file_path) LIKE '%.wav' OR LOWER(file_path) LIKE '%.m4a' OR LOWER(file_path) LIKE '%.aac' OR "
                  "LOWER(file_path) LIKE '%.opus' OR LOWER(file_path) LIKE '%.wma' OR LOWER(file_path) LIKE '%.ape' OR "
                  "LOWER(file_path) LIKE '%.mpc') GROUP BY Album ORDER BY Album"
                : "SELECT DISTINCT Album, COUNT(*) FROM tracks WHERE Album IS NOT NULL AND Album != '' AND ("
                  "LOWER(file_path) LIKE '%.mp3' OR LOWER(file_path) LIKE '%.ogg' OR LOWER(file_path) LIKE '%.flac' OR "
                  "LOWER(file_path) LIKE '%.wav' OR LOWER(file_path) LIKE '%.m4a' OR LOWER(file_path) LIKE '%.aac' OR "
                  "LOWER(file_path) LIKE '%.opus' OR LOWER(file_path) LIKE '%.wma' OR LOWER(file_path) LIKE '%.ape' OR "
                  "LOWER(file_path) LIKE '%.mpc') GROUP BY Album ORDER BY Album";
            break;
        case BROWSER_TYPE_GENRE:
            sql = "SELECT DISTINCT Genre, COUNT(*) FROM tracks WHERE Genre IS NOT NULL AND Genre != '' AND ("
                  "LOWER(file_path) LIKE '%.mp3' OR LOWER(file_path) LIKE '%.ogg' OR LOWER(file_path) LIKE '%.flac' OR "
                  "LOWER(file_path) LIKE '%.wav' OR LOWER(file_path) LIKE '%.m4a' OR LOWER(file_path) LIKE '%.aac' OR "
                  "LOWER(file_path) LIKE '%.opus' OR LOWER(file_path) LIKE '%.wma' OR LOWER(file_path) LIKE '%.ape' OR "
                  "LOWER(file_path) LIKE '%.mpc') GROUP BY Genre ORDER BY Genre";
            break;
        case BROWSER_TYPE_YEAR:
            sql = "SELECT DISTINCT CAST(Year AS TEXT), COUNT(*) FROM tracks WHERE Year > 0 AND ("
                  "LOWER(file_path) LIKE '%.mp3' OR LOWER(file_path) LIKE '%.ogg' OR LOWER(file_path) LIKE '%.flac' OR "
                  "LOWER(file_path) LIKE '%.wav' OR LOWER(file_path) LIKE '%.m4a' OR LOWER(file_path) LIKE '%.aac' OR "
                  "LOWER(file_path) LIKE '%.opus' OR LOWER(file_path) LIKE '%.wma' OR LOWER(file_path) LIKE '%.ape' OR "
                  "LOWER(file_path) LIKE '%.mpc') GROUP BY Year ORDER BY Year DESC";
            break;
    }
    
    if (!sql) return;
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(model->database->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (model->current_filter && model->type == BROWSER_TYPE_ALBUM) {
            sqlite3_bind_text(stmt, 1, model->current_filter, -1, SQLITE_TRANSIENT);
        }
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const gchar *name = (const gchar *)sqlite3_column_text(stmt, 0);
            gint count = sqlite3_column_int(stmt, 1);
            
            BansheeBrowserItem *item = banshee_browser_item_new(g_str_hash(name), name, count);
            g_list_store_append(model->store, item);
            g_object_unref(item);
        }
        sqlite3_finalize(stmt);
    }
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
    
    BansheeBrowserItem *item = gtk_single_selection_get_selected_item(selection);
    if (!item) return NULL;
    
    const gchar *name = banshee_browser_item_get_name(item);
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
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT DISTINCT Artist FROM tracks WHERE Artist IS NOT NULL AND Artist != '' AND ("
                      "LOWER(file_path) LIKE '%.mp3' OR LOWER(file_path) LIKE '%.ogg' OR "
                      "LOWER(file_path) LIKE '%.flac' OR LOWER(file_path) LIKE '%.wav' OR "
                      "LOWER(file_path) LIKE '%.m4a' OR LOWER(file_path) LIKE '%.aac' OR "
                      "LOWER(file_path) LIKE '%.opus' OR LOWER(file_path) LIKE '%.wma' OR "
                      "LOWER(file_path) LIKE '%.ape' OR LOWER(file_path) LIKE '%.mpc') "
                      "ORDER BY Artist";
    sqlite3_stmt *stmt;
    GList *list = NULL;
    
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const gchar *artist = (const gchar *)sqlite3_column_text(stmt, 0);
            list = g_list_append(list, g_strdup(artist));
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

GList* browser_get_albums(Database *db, const gchar *artist_filter) {
    if (!db || !db->db) return NULL;
    
    const char *sql = artist_filter
        ? "SELECT DISTINCT Album FROM tracks WHERE Album IS NOT NULL AND Album != '' AND Artist = ? AND ("
          "LOWER(file_path) LIKE '%.mp3' OR LOWER(file_path) LIKE '%.ogg' OR "
          "LOWER(file_path) LIKE '%.flac' OR LOWER(file_path) LIKE '%.wav' OR "
          "LOWER(file_path) LIKE '%.m4a' OR LOWER(file_path) LIKE '%.aac' OR "
          "LOWER(file_path) LIKE '%.opus' OR LOWER(file_path) LIKE '%.wma' OR "
          "LOWER(file_path) LIKE '%.ape' OR LOWER(file_path) LIKE '%.mpc') ORDER BY Album"
        : "SELECT DISTINCT Album FROM tracks WHERE Album IS NOT NULL AND Album != '' AND ("
          "LOWER(file_path) LIKE '%.mp3' OR LOWER(file_path) LIKE '%.ogg' OR "
          "LOWER(file_path) LIKE '%.flac' OR LOWER(file_path) LIKE '%.wav' OR "
          "LOWER(file_path) LIKE '%.m4a' OR LOWER(file_path) LIKE '%.aac' OR "
          "LOWER(file_path) LIKE '%.opus' OR LOWER(file_path) LIKE '%.wma' OR "
          "LOWER(file_path) LIKE '%.ape' OR LOWER(file_path) LIKE '%.mpc') ORDER BY Album";
    
    sqlite3_stmt *stmt;
    GList *list = NULL;
    
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (artist_filter) {
            sqlite3_bind_text(stmt, 1, artist_filter, -1, SQLITE_TRANSIENT);
        }
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const gchar *album = (const gchar *)sqlite3_column_text(stmt, 0);
            list = g_list_append(list, g_strdup(album));
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

GList* browser_get_genres(Database *db) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT DISTINCT Genre FROM tracks WHERE Genre IS NOT NULL AND Genre != '' AND ("
                      "LOWER(file_path) LIKE '%.mp3' OR LOWER(file_path) LIKE '%.ogg' OR "
                      "LOWER(file_path) LIKE '%.flac' OR LOWER(file_path) LIKE '%.wav' OR "
                      "LOWER(file_path) LIKE '%.m4a' OR LOWER(file_path) LIKE '%.aac' OR "
                      "LOWER(file_path) LIKE '%.opus' OR LOWER(file_path) LIKE '%.wma' OR "
                      "LOWER(file_path) LIKE '%.ape' OR LOWER(file_path) LIKE '%.mpc') "
                      "ORDER BY Genre";
    sqlite3_stmt *stmt;
    GList *list = NULL;
    
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const gchar *genre = (const gchar *)sqlite3_column_text(stmt, 0);
            list = g_list_append(list, g_strdup(genre));
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

void browser_item_free(BrowserItem *item) {
    if (!item) return;
    g_free(item->name);
    g_free(item);
}
