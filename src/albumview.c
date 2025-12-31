#include "albumview.h"
#include <string.h>

typedef struct {
    GtkListStore *store;  /* Hold reference to store, not view */
    GtkTreeRowReference *row_ref;  /* Use row reference instead of iter */
    gchar *artist;
    gchar *album;
} AlbumLoadData;

static void on_coverart_loaded(GdkPixbuf *pixbuf, gpointer user_data) {
    AlbumLoadData *data = (AlbumLoadData *)user_data;
    
    if (!data) return;
    
    if (pixbuf && data->store && gtk_tree_row_reference_valid(data->row_ref)) {
        GtkTreePath *path = gtk_tree_row_reference_get_path(data->row_ref);
        if (path) {
            GtkTreeIter iter;
            if (gtk_tree_model_get_iter(GTK_TREE_MODEL(data->store), &iter, path)) {
                gtk_list_store_set(data->store, &iter,
                                  ALBUM_COL_PIXBUF, pixbuf,
                                  -1);
            }
            gtk_tree_path_free(path);
        }
    }
    
    /* Don't unref pixbuf here - caller owns it */
    
    /* Clean up data */
    if (data->store) {
        g_object_unref(data->store);
    }
    if (data->row_ref) {
        gtk_tree_row_reference_free(data->row_ref);
    }
    g_free(data->artist);
    g_free(data->album);
    g_free(data);
}

AlbumView* album_view_new(CoverArtManager *coverart_manager, Database *database) {
    AlbumView *view = g_new0(AlbumView, 1);
    view->coverart_manager = coverart_manager;
    view->database = database;
    view->current_artist = NULL;
    
    /* Create list store for albums */
    view->store = gtk_list_store_new(ALBUM_COL_COUNT,
                                     GDK_TYPE_PIXBUF,  /* Album cover */
                                     G_TYPE_STRING,     /* Artist */
                                     G_TYPE_STRING);    /* Album name */
    
    /* Create icon view */
    view->icon_view = gtk_icon_view_new_with_model(GTK_TREE_MODEL(view->store));
    gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(view->icon_view), ALBUM_COL_PIXBUF);
    gtk_icon_view_set_text_column(GTK_ICON_VIEW(view->icon_view), ALBUM_COL_ALBUM);
    gtk_icon_view_set_item_width(GTK_ICON_VIEW(view->icon_view), 140);
    gtk_icon_view_set_spacing(GTK_ICON_VIEW(view->icon_view), 6);
    gtk_icon_view_set_item_padding(GTK_ICON_VIEW(view->icon_view), 6);
    gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(view->icon_view), GTK_SELECTION_SINGLE);
    
    /* Scrolled window */
    view->scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view->scrolled_window),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(view->scrolled_window), view->icon_view);
    
    return view;
}

void album_view_free(AlbumView *view) {
    if (!view) return;
    
    g_free(view->current_artist);
    g_object_unref(view->store);
    g_free(view);
}

GtkWidget* album_view_get_widget(AlbumView *view) {
    return view ? view->scrolled_window : NULL;
}

void album_view_clear(AlbumView *view) {
    if (!view || !view->store) return;
    gtk_list_store_clear(view->store);
}

void album_view_set_artist(AlbumView *view, const gchar *artist) {
    if (!view || !view->database) return;
    
    /* Clear existing albums */
    album_view_clear(view);
    
    /* Update current artist */
    g_free(view->current_artist);
    view->current_artist = artist ? g_strdup(artist) : NULL;
    
    /* Get albums for this artist */
    GList *albums = database_get_albums_by_artist(view->database, artist);
    
    /* Create default cover pixbuf */
    GdkPixbuf *default_cover = gdk_pixbuf_new_from_file_at_scale(
        "/usr/share/icons/hicolor/128x128/mimetypes/audio-x-generic.png",
        128, 128, TRUE, NULL);
    
    if (!default_cover) {
        /* Fallback: create a simple colored square */
        default_cover = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 128, 128);
        gdk_pixbuf_fill(default_cover, 0x666666FF);
    }
    
    for (GList *l = albums; l != NULL; l = l->next) {
        typedef struct { gchar *artist; gchar *album; } AlbumInfo;
        AlbumInfo *info = (AlbumInfo *)l->data;
        
        GtkTreeIter iter;
        gtk_list_store_append(view->store, &iter);
        gtk_list_store_set(view->store, &iter,
                          ALBUM_COL_PIXBUF, default_cover,
                          ALBUM_COL_ARTIST, info->artist,
                          ALBUM_COL_ALBUM, info->album,
                          -1);
        
        /* Load cover art asynchronously */
        if (view->coverart_manager) {
            AlbumLoadData *data = g_new0(AlbumLoadData, 1);
            data->store = view->store;
            g_object_ref(data->store);  /* Take reference to keep store alive */
            
            GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(view->store), &iter);
            data->row_ref = gtk_tree_row_reference_new(GTK_TREE_MODEL(view->store), path);
            gtk_tree_path_free(path);
            
            data->artist = g_strdup(info->artist);
            data->album = g_strdup(info->album);
            
            coverart_fetch_async(view->coverart_manager, info->artist, info->album,
                               COVER_ART_SIZE_MEDIUM, on_coverart_loaded, data);
        }
        
        g_free(info->artist);
        g_free(info->album);
        g_free(info);
    }
    
    g_list_free(albums);
    
    if (default_cover) {
        g_object_unref(default_cover);
    }
}

typedef struct {
    AlbumSelectedCallback callback;
    gpointer user_data;
    AlbumView *view;
} SelectionCallbackData;

static void on_album_selection_changed(GtkIconView *icon_view, gpointer user_data) {
    SelectionCallbackData *data = (SelectionCallbackData *)user_data;
    GList *selected = gtk_icon_view_get_selected_items(icon_view);
    
    if (selected && data->callback) {
        GtkTreePath *path = (GtkTreePath *)selected->data;
        GtkTreeIter iter;
        
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(data->view->store), &iter, path)) {
            gchar *artist = NULL;
            gchar *album = NULL;
            
            gtk_tree_model_get(GTK_TREE_MODEL(data->view->store), &iter,
                              ALBUM_COL_ARTIST, &artist,
                              ALBUM_COL_ALBUM, &album,
                              -1);
            
            data->callback(artist, album, data->user_data);
            
            g_free(artist);
            g_free(album);
        }
    }
    
    g_list_free_full(selected, (GDestroyNotify)gtk_tree_path_free);
}

void album_view_set_selection_callback(AlbumView *view, AlbumSelectedCallback callback, gpointer user_data) {
    if (!view || !view->icon_view) return;
    
    SelectionCallbackData *data = g_new0(SelectionCallbackData, 1);
    data->callback = callback;
    data->user_data = user_data;
    data->view = view;
    
    g_signal_connect_data(view->icon_view, "selection-changed",
        G_CALLBACK(on_album_selection_changed),
        data, (GClosureNotify)g_free, (GConnectFlags)0);
}
