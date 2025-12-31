#include "browser.h"
#include <string.h>

enum {
    COL_BROWSER_ID = 0,
    COL_BROWSER_NAME,
    COL_BROWSER_COUNT,
    NUM_BROWSER_COLS
};

BrowserModel* browser_model_new(BrowserType type, Database *database) {
    BrowserModel *model = g_new0(BrowserModel, 1);
    model->type = type;
    model->database = database;
    model->current_filter = NULL;
    
    model->store = gtk_list_store_new(NUM_BROWSER_COLS,
                                       G_TYPE_INT,     /* ID */
                                       G_TYPE_STRING,  /* Name */
                                       G_TYPE_INT);    /* Count */
    
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
    
    gtk_list_store_clear(model->store);
    
    GtkTreeIter iter;
    gtk_list_store_append(model->store, &iter);
    gtk_list_store_set(model->store, &iter,
                      COL_BROWSER_ID, 0,
                      COL_BROWSER_NAME, "All",
                      COL_BROWSER_COUNT, 0,
                      -1);
    
    const char *sql = NULL;
    switch (model->type) {
        case BROWSER_TYPE_ARTIST:
            sql = "SELECT DISTINCT Artist, COUNT(*) FROM tracks WHERE Artist IS NOT NULL AND Artist != '' GROUP BY Artist ORDER BY Artist";
            break;
        case BROWSER_TYPE_ALBUM:
            sql = model->current_filter
                ? "SELECT DISTINCT Album, COUNT(*) FROM tracks WHERE Album IS NOT NULL AND Album != '' AND Artist = ? GROUP BY Album ORDER BY Album"
                : "SELECT DISTINCT Album, COUNT(*) FROM tracks WHERE Album IS NOT NULL AND Album != '' GROUP BY Album ORDER BY Album";
            break;
        case BROWSER_TYPE_GENRE:
            sql = "SELECT DISTINCT Genre, COUNT(*) FROM tracks WHERE Genre IS NOT NULL AND Genre != '' GROUP BY Genre ORDER BY Genre";
            break;
        case BROWSER_TYPE_YEAR:
            sql = "SELECT DISTINCT CAST(Year AS TEXT), COUNT(*) FROM tracks WHERE Year > 0 GROUP BY Year ORDER BY Year DESC";
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
            
            gtk_list_store_append(model->store, &iter);
            gtk_list_store_set(model->store, &iter,
                              COL_BROWSER_ID, g_str_hash(name),
                              COL_BROWSER_NAME, name,
                              COL_BROWSER_COUNT, count,
                              -1);
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

gchar* browser_model_get_selection(BrowserModel *model, GtkTreeSelection *selection) {
    GtkTreeIter iter;
    GtkTreeModel *tree_model;
    
    if (gtk_tree_selection_get_selected(selection, &tree_model, &iter)) {
        gchar *name;
        gtk_tree_model_get(tree_model, &iter, COL_BROWSER_NAME, &name, -1);
        
        if (g_strcmp0(name, "All") == 0) {
            g_free(name);
            return NULL;
        }
        return name;
    }
    return NULL;
}

BrowserView* browser_view_new(BrowserModel *model) {
    BrowserView *view = g_new0(BrowserView, 1);
    view->model = model;
    
    view->scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view->scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(view->scrolled_window, 150, -1);
    
    view->tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model->store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view->tree_view), TRUE);
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        "Name", renderer, "text", COL_BROWSER_NAME, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view->tree_view), column);
    
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 1.0, NULL);
    column = gtk_tree_view_column_new_with_attributes(
        "#", renderer, "text", COL_BROWSER_COUNT, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view->tree_view), column);
    
    gtk_container_add(GTK_CONTAINER(view->scrolled_window), view->tree_view);
    
    return view;
}

void browser_view_free(BrowserView *view) {
    if (!view) return;
    g_free(view);
}

void browser_view_set_selection_callback(BrowserView *view, GCallback callback, gpointer user_data) {
    if (!view) return;
    view->selection_changed = callback;
    view->user_data = user_data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view->tree_view));
    g_signal_connect(selection, "changed", callback, user_data);
}

GtkWidget* browser_view_get_widget(BrowserView *view) {
    return view ? view->scrolled_window : NULL;
}

GList* browser_get_artists(Database *db) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT DISTINCT Artist FROM tracks WHERE Artist IS NOT NULL AND Artist != '' ORDER BY Artist";
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
        ? "SELECT DISTINCT Album FROM tracks WHERE Album IS NOT NULL AND Album != '' AND Artist = ? ORDER BY Album"
        : "SELECT DISTINCT Album FROM tracks WHERE Album IS NOT NULL AND Album != '' ORDER BY Album";
    
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
    
    const char *sql = "SELECT DISTINCT Genre FROM tracks WHERE Genre IS NOT NULL AND Genre != '' ORDER BY Genre";
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
