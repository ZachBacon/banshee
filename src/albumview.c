#include "albumview.h"
#include <string.h>

/* ========== AlbumItem GObject Implementation ========== */

G_DEFINE_TYPE(AlbumItem, album_item, G_TYPE_OBJECT)

static void album_item_finalize(GObject *object) {
    AlbumItem *item = ALBUM_ITEM(object);
    g_free(item->artist);
    g_free(item->album);
    g_clear_object(&item->cover);
    G_OBJECT_CLASS(album_item_parent_class)->finalize(object);
}

static void album_item_class_init(AlbumItemClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = album_item_finalize;
}

static void album_item_init(AlbumItem *item) {
    item->artist = NULL;
    item->album = NULL;
    item->cover = NULL;
    item->picture = NULL;
}

AlbumItem* album_item_new(const gchar *artist, const gchar *album) {
    AlbumItem *item = g_object_new(ALBUM_TYPE_ITEM, NULL);
    item->artist = g_strdup(artist);
    item->album = g_strdup(album);
    return item;
}

void album_item_set_cover(AlbumItem *item, GdkPixbuf *pixbuf) {
    if (!item) return;
    g_clear_object(&item->cover);
    if (pixbuf) {
        /* GTK4: Use GBytes-based texture creation instead of deprecated gdk_texture_new_for_pixbuf */
        GBytes *bytes = gdk_pixbuf_read_pixel_bytes(pixbuf);
        int width = gdk_pixbuf_get_width(pixbuf);
        int height = gdk_pixbuf_get_height(pixbuf);
        int stride = gdk_pixbuf_get_rowstride(pixbuf);
        gboolean has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);
        
        GdkTexture *texture = gdk_memory_texture_new(width, height,
            has_alpha ? GDK_MEMORY_R8G8B8A8 : GDK_MEMORY_R8G8B8,
            bytes, stride);
        g_bytes_unref(bytes);
        item->cover = GDK_PAINTABLE(texture);
        g_debug("album_item_set_cover: Created texture for %s", item->album ? item->album : "Unknown");
    }
    
    /* Directly update the picture widget if we have a reference to it */
    if (item->picture && GTK_IS_PICTURE(item->picture)) {
        g_debug("album_item_set_cover: Directly updating picture widget for %s", 
                item->album ? item->album : "Unknown");
        gtk_picture_set_paintable(GTK_PICTURE(item->picture), item->cover);
    } else {
        g_debug("album_item_set_cover: No picture widget reference for %s", 
                item->album ? item->album : "Unknown");
    }
}

/* ========== Cover Art Loading ========== */

typedef struct {
    AlbumItem *item;
} AlbumLoadData;

static void on_coverart_loaded(GdkPixbuf *pixbuf, gpointer user_data) {
    AlbumLoadData *data = (AlbumLoadData *)user_data;
    
    if (data && data->item && pixbuf) {
        g_debug("on_coverart_loaded: Cover art loaded for %s - %s", 
                data->item->artist ? data->item->artist : "Unknown",
                data->item->album ? data->item->album : "Unknown");
        
        /* Update the cover on the item - this will also update the picture widget
         * directly if one is bound */
        album_item_set_cover(data->item, pixbuf);
    } else {
        g_debug("on_coverart_loaded: No pixbuf or item for callback");
    }
    
    if (data) {
        if (data->item) {
            g_object_unref(data->item);
        }
        g_free(data);
    }
}

/* ========== GtkGridView Factory Callbacks ========== */

static void setup_album_item(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    
    /* Create a box to hold the cover image and label */
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    
    /* Cover art image - use GtkPicture for proper scaling */
    GtkWidget *picture = gtk_picture_new();
    gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_COVER);
    gtk_widget_set_size_request(picture, COVER_ART_SIZE_MEDIUM, COVER_ART_SIZE_MEDIUM);
    gtk_widget_set_name(picture, "album-cover");
    gtk_box_append(GTK_BOX(box), picture);
    
    /* Album title label */
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 20);
    gtk_label_set_wrap(GTK_LABEL(label), FALSE);
    gtk_widget_set_name(label, "album-label");
    gtk_box_append(GTK_BOX(box), label);
    
    gtk_list_item_set_child(list_item, box);
}

static void bind_album_item(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    
    GtkWidget *box = gtk_list_item_get_child(list_item);
    AlbumItem *item = gtk_list_item_get_item(list_item);
    
    if (!box || !item) return;
    
    /* Find the picture and label widgets */
    GtkWidget *picture = gtk_widget_get_first_child(box);
    GtkWidget *label = gtk_widget_get_next_sibling(picture);
    
    /* Store reference to picture widget for async cover art updates */
    item->picture = picture;
    g_debug("bind_album_item: Stored picture reference for %s", item->album ? item->album : "Unknown");
    
    /* Set the cover art */
    if (item->cover) {
        gtk_picture_set_paintable(GTK_PICTURE(picture), item->cover);
        g_debug("bind_album_item: Set existing cover for %s", item->album ? item->album : "Unknown");
    } else {
        gtk_picture_set_paintable(GTK_PICTURE(picture), NULL);
        g_debug("bind_album_item: No cover yet for %s", item->album ? item->album : "Unknown");
    }
    
    /* Set the album name */
    gtk_label_set_text(GTK_LABEL(label), item->album ? item->album : "Unknown Album");
}

static void unbind_album_item(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    
    /* Clear the picture reference when item is unbound */
    AlbumItem *item = gtk_list_item_get_item(list_item);
    if (item) {
        item->picture = NULL;
        g_debug("unbind_album_item: Cleared picture reference for %s", item->album ? item->album : "Unknown");
    }
}

static void teardown_album_item(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)list_item;
    (void)user_data;
    /* Nothing special to do on teardown */
}

/* ========== AlbumView Implementation ========== */

AlbumView* album_view_new(CoverArtManager *coverart_manager, Database *database) {
    AlbumView *view = g_new0(AlbumView, 1);
    view->coverart_manager = coverart_manager;
    view->database = database;
    view->current_artist = NULL;
    
    /* Create GListStore for album items */
    view->store = g_list_store_new(ALBUM_TYPE_ITEM);
    
    /* Create selection model */
    view->selection = gtk_single_selection_new(G_LIST_MODEL(view->store));
    gtk_single_selection_set_autoselect(view->selection, FALSE);
    gtk_single_selection_set_can_unselect(view->selection, TRUE);
    
    /* Create factory for list items */
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_album_item), view);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_album_item), view);
    g_signal_connect(factory, "unbind", G_CALLBACK(unbind_album_item), view);
    g_signal_connect(factory, "teardown", G_CALLBACK(teardown_album_item), view);
    
    /* Create GtkGridView */
    view->grid_view = gtk_grid_view_new(GTK_SELECTION_MODEL(view->selection), factory);
    gtk_grid_view_set_min_columns(GTK_GRID_VIEW(view->grid_view), 1);
    gtk_grid_view_set_max_columns(GTK_GRID_VIEW(view->grid_view), 10);
    gtk_grid_view_set_single_click_activate(GTK_GRID_VIEW(view->grid_view), TRUE);
    
    /* Scrolled window */
    view->scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view->scrolled_window),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(view->scrolled_window), view->grid_view);
    
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
    g_list_store_remove_all(view->store);
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
    GdkPixbuf *default_cover = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 
                                              COVER_ART_SIZE_MEDIUM, COVER_ART_SIZE_MEDIUM);
    gdk_pixbuf_fill(default_cover, 0x666666FF);
    
    for (GList *l = albums; l != NULL; l = l->next) {
        typedef struct { gchar *artist; gchar *album; } AlbumInfo;
        AlbumInfo *info = (AlbumInfo *)l->data;
        
        /* Create album item and add to store */
        AlbumItem *item = album_item_new(info->artist, info->album);
        album_item_set_cover(item, default_cover);
        g_list_store_append(view->store, item);
        
        /* Load cover art asynchronously - pass database to extract from audio if needed */
        if (view->coverart_manager) {
            AlbumLoadData *data = g_new0(AlbumLoadData, 1);
            data->item = g_object_ref(item);
            
            coverart_fetch_async_with_db(view->coverart_manager, view->database,
                               info->artist, info->album,
                               COVER_ART_SIZE_MEDIUM, on_coverart_loaded, data);
        }
        
        g_object_unref(item);  /* Store holds reference */
        g_free(info->artist);
        g_free(info->album);
        g_free(info);
    }
    
    g_list_free(albums);
    g_object_unref(default_cover);
}

typedef struct {
    AlbumSelectedCallback callback;
    gpointer user_data;
    AlbumView *view;
} SelectionCallbackData;

static void on_album_activated(GtkGridView *grid_view, guint position, gpointer user_data) {
    SelectionCallbackData *data = (SelectionCallbackData *)user_data;
    
    if (!data || !data->callback || !data->view) return;
    
    AlbumItem *item = g_list_model_get_item(G_LIST_MODEL(data->view->store), position);
    if (item) {
        data->callback(item->artist, item->album, data->user_data);
        g_object_unref(item);
    }
}

void album_view_set_selection_callback(AlbumView *view, AlbumSelectedCallback callback, gpointer user_data) {
    if (!view || !view->grid_view) return;
    
    SelectionCallbackData *data = g_new0(SelectionCallbackData, 1);
    data->callback = callback;
    data->user_data = user_data;
    data->view = view;
    
    g_signal_connect_data(view->grid_view, "activate",
        G_CALLBACK(on_album_activated),
        data, (GClosureNotify)g_free, (GConnectFlags)0);
}
