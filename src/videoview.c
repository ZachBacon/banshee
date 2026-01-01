#include "videoview.h"
#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_WIN32
#include <gdk/gdkwin32.h>
#endif
#include <string.h>

/* Function to notify app about video playback state */
extern void app_set_video_playing(gboolean playing);

typedef struct {
    VideoSelectedCallback callback;
    gpointer user_data;
} VideoCallbackData;

/* Frame monitoring */
static guint frame_count = 0;
static GTimer *frame_timer = NULL;

static gboolean on_frame_rendered(GtkWidget *widget, GdkFrameClock *clock, gpointer user_data) {
    (void)widget;
    (void)clock;
    (void)user_data;
    
    frame_count++;
    
    if (!frame_timer) {
        frame_timer = g_timer_new();
    }
    
    gdouble elapsed = g_timer_elapsed(frame_timer, NULL);
    if (elapsed >= 1.0) {
        g_print("Video: Rendered %u frames in %.2f seconds (%.1f fps)\n", 
                frame_count, elapsed, frame_count / elapsed);
        frame_count = 0;
        g_timer_reset(frame_timer);
    }
    
    return G_SOURCE_CONTINUE;
}

/* Callback when gtksink widget becomes available */
static void on_video_widget_ready(GtkWidget *widget, gpointer user_data) {
    VideoView *view = (VideoView *)user_data;
    
    g_print("VideoView: on_video_widget_ready called\n");
    
    if (!view) {
        g_print("VideoView: ERROR - view is NULL\n");
        return;
    }
    
    if (!widget) {
        g_print("VideoView: ERROR - widget is NULL\n");
        return;
    }
    
    g_print("VideoView: gtksink widget ready, adding to overlay\n");
    
    /* Store the widget */
    view->video_widget = widget;
    
    /* Add to overlay if we have one */
    if (!view->overlay_container) {
        g_print("VideoView: ERROR - overlay_container is NULL\n");
        return;
    }
    
    if (!GTK_IS_OVERLAY(view->overlay_container)) {
        g_print("VideoView: ERROR - overlay_container is not a GtkOverlay\n");
        return;
    }
    
    /* Add video widget to overlay - works smoothly now with GStreamer in separate thread */
    g_print("VideoView: Adding video widget to overlay...\n");
    
    gtk_overlay_add_overlay(GTK_OVERLAY(view->overlay_container), view->video_widget);
    gtk_widget_set_valign(view->video_widget, GTK_ALIGN_FILL);
    gtk_widget_set_halign(view->video_widget, GTK_ALIGN_FILL);
    gtk_widget_show(view->video_widget);
    
    /* Now switch to show the overlay with embedded video */
    if (view->content_stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(view->content_stack), "playback");
        g_print("VideoView: Switched to playback view - video widget embedded\n");
    }
    
    g_print("VideoView: Video widget embedded in overlay\n");
}

static void format_video_time(gint seconds, gchar *buffer, gsize buffer_size) {
    gint hours = seconds / 3600;
    gint mins = (seconds % 3600) / 60;
    gint secs = seconds % 60;
    
    if (hours > 0) {
        g_snprintf(buffer, buffer_size, "%d:%02d:%02d", hours, mins, secs);
    } else {
        g_snprintf(buffer, buffer_size, "%d:%02d", mins, secs);
    }
}

static void on_video_row_activated(GtkTreeView *tree_view, GtkTreePath *path,
                                   GtkTreeViewColumn *column, gpointer user_data) {
    VideoView *view = (VideoView *)user_data;
    (void)tree_view;
    (void)column;
    
    GtkTreeIter iter;
    
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(view->video_store), &iter, path)) {
        gint video_id;
        gchar *file_path;
        
        gtk_tree_model_get(GTK_TREE_MODEL(view->video_store), &iter,
                          VIDEO_COL_ID, &video_id,
                          VIDEO_COL_FILE_PATH, &file_path,
                          -1);
        
        if (file_path && view->player) {
            view->video_playing = TRUE;
            
            /* Register callback to be notified when gtksink widget is ready */
            player_set_video_widget_ready_callback(view->player, 
                                                   G_CALLBACK(on_video_widget_ready), 
                                                   view);
            
            /* Set URI and start playback */
            player_set_uri(view->player, file_path);
            player_play(view->player);
            
            g_print("VideoView: Started video playback, waiting for widget...\n");
            
            g_free(file_path);
        }
    }
}

VideoView* video_view_new(Database *database, MediaPlayer *player) {
    VideoView *view = g_new0(VideoView, 1);
    view->database = database;
    view->player = player;
    view->video_playing = FALSE;
    view->overlay_container = NULL;
    view->content_stack = NULL;
    
    /* Create main container as a stack with video list and playback overlay */
    view->main_container = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(view->main_container), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    
    /* Create the video list page */
    GtkWidget *list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    /* Create list store for videos */
    view->video_store = gtk_list_store_new(VIDEO_COL_COUNT,
                                           G_TYPE_INT,      /* ID */
                                           G_TYPE_STRING,   /* Title */
                                           G_TYPE_STRING,   /* Artist */
                                           G_TYPE_STRING,   /* Duration */
                                           G_TYPE_STRING);  /* File Path */
    
    /* Create tree view */
    view->video_listview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(view->video_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view->video_listview), TRUE);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view->video_listview), TRUE);
    gtk_tree_view_set_search_column(GTK_TREE_VIEW(view->video_listview), VIDEO_COL_TITLE);
    
    /* Add columns */
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    
    /* Title column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Title", renderer,
                                                      "text", VIDEO_COL_TITLE,
                                                      NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view->video_listview), column);
    
    /* Artist column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Artist", renderer,
                                                      "text", VIDEO_COL_ARTIST,
                                                      NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view->video_listview), column);
    
    /* Duration column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Duration", renderer,
                                                      "text", VIDEO_COL_DURATION,
                                                      NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view->video_listview), column);
    
    /* Connect row activation signal (double-click) */
    g_signal_connect(view->video_listview, "row-activated",
                    G_CALLBACK(on_video_row_activated), view);
    
    /* Scrolled window for video list */
    view->scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view->scrolled_window),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(view->scrolled_window), view->video_listview);
    
    gtk_box_pack_start(GTK_BOX(list_box), view->scrolled_window, TRUE, TRUE, 0);
    
    /* Add list page to stack */
    gtk_stack_add_named(GTK_STACK(view->main_container), list_box, "list");
    
    /* Create overlay page for video playback */
    GtkWidget *playback_overlay = gtk_overlay_new();
    gtk_stack_add_named(GTK_STACK(view->main_container), playback_overlay, "playback");
    
    /* Store overlay reference */
    view->overlay_container = playback_overlay;
    view->content_stack = view->main_container;
    
    /* Show list by default */
    gtk_stack_set_visible_child_name(GTK_STACK(view->main_container), "list");
    
    /* Don't create a video widget yet - we'll get it from gtksink when playing */
    view->video_widget = NULL;
    
    gtk_widget_show_all(view->main_container);
    
    return view;
}

void video_view_free(VideoView *view) {
    if (!view) return;
    
    if (view->video_store) {
        g_object_unref(view->video_store);
    }
    
    g_free(view);
}

GtkWidget* video_view_get_widget(VideoView *view) {
    return view ? view->main_container : NULL;
}

void video_view_clear(VideoView *view) {
    if (!view || !view->video_store) return;
    gtk_list_store_clear(view->video_store);
}

void video_view_load_videos(VideoView *view) {
    if (!view || !view->database) return;
    
    /* Clear existing videos */
    video_view_clear(view);
    
    /* Get videos from database */
    GList *videos = database_get_all_videos(view->database);
    
    for (GList *l = videos; l != NULL; l = l->next) {
        Track *video = (Track *)l->data;
        GtkTreeIter iter;
        
        gchar duration_str[32];
        format_video_time(video->duration, duration_str, sizeof(duration_str));
        
        gtk_list_store_append(view->video_store, &iter);
        gtk_list_store_set(view->video_store, &iter,
                          VIDEO_COL_ID, video->id,
                          VIDEO_COL_TITLE, video->title ? video->title : "Unknown",
                          VIDEO_COL_ARTIST, video->artist ? video->artist : "Unknown",
                          VIDEO_COL_DURATION, duration_str,
                          VIDEO_COL_FILE_PATH, video->file_path,
                          -1);
    }
    
    g_list_free_full(videos, (GDestroyNotify)database_free_track);
}

void video_view_set_overlay_container(VideoView *view, GtkWidget *overlay_container) {
    if (view) {
        view->overlay_container = overlay_container;
    }
}

void video_view_set_content_stack(VideoView *view, GtkWidget *content_stack) {
    if (view) {
        view->content_stack = content_stack;
    }
}

void video_view_show_video(VideoView *view) {
    if (!view) return;
    
    view->video_playing = TRUE;
    app_set_video_playing(TRUE);
    
    /* View switching now happens in on_video_widget_ready after widget is added */
    g_print("Video: Playback started, waiting for widget...\n");
}

void video_view_hide_video(VideoView *view) {
    if (!view) return;
    
    view->video_playing = FALSE;
    app_set_video_playing(FALSE);
    
    /* Hide the video widget */
    if (view->video_widget) {
        gtk_widget_hide(view->video_widget);
    }
    
    /* Switch back to video list */
    if (view->content_stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(view->content_stack), "list");
    }
    
    /* Stop playback */
    if (view->player) {
        player_stop(view->player);
    }
}

gboolean video_view_is_showing_video(VideoView *view) {
    if (!view) return FALSE;
    return view->video_playing;
}

void video_view_set_selection_callback(VideoView *view, VideoSelectedCallback callback, gpointer user_data) {
    if (!view) return;
    
    VideoCallbackData *data = g_new0(VideoCallbackData, 1);
    data->callback = callback;
    data->user_data = user_data;
    
    g_object_set_data_full(G_OBJECT(view->video_listview), "callback-data", data, g_free);
}
