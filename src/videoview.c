#include "videoview.h"
#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_WIN32
#include <gdk/gdkwin32.h>
#endif
#include <string.h>

/* Function to notify app about video playback state */
extern void app_set_video_playing(gboolean playing);
extern void app_set_video_now_playing(const gchar *title);

typedef struct {
    VideoSelectedCallback callback;
    gpointer user_data;
} VideoCallbackData;

/* Forward declarations */
static void update_audio_menu(VideoView *view);
static void update_subtitle_menu(VideoView *view);
static void show_controls(VideoView *view);
static void hide_controls(VideoView *view);
static GtkWidget* create_video_controls(VideoView *view);
static void start_position_timer(VideoView *view);
static void stop_position_timer(VideoView *view);

/* Timeout callback to hide controls */
static gboolean hide_controls_timeout(gpointer user_data) {
    VideoView *view = (VideoView *)user_data;
    view->controls_timeout_id = 0;
    hide_controls(view);
    return G_SOURCE_REMOVE;
}

static void reset_controls_timeout(VideoView *view) {
    if (view->controls_timeout_id > 0) {
        g_source_remove(view->controls_timeout_id);
    }
    view->controls_timeout_id = g_timeout_add(3000, hide_controls_timeout, view);
}

static void show_controls(VideoView *view) {
    if (!view || !view->controls_revealer) return;
    gtk_revealer_set_reveal_child(GTK_REVEALER(view->controls_revealer), TRUE);
    view->controls_visible = TRUE;
    reset_controls_timeout(view);
}

static void hide_controls(VideoView *view) {
    if (!view || !view->controls_revealer) return;
    gtk_revealer_set_reveal_child(GTK_REVEALER(view->controls_revealer), FALSE);
    view->controls_visible = FALSE;
}

/* Motion event to show controls when mouse moves over video */
static gboolean on_video_motion(GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
    (void)widget;
    (void)event;
    VideoView *view = (VideoView *)user_data;
    show_controls(view);
    return FALSE;
}

/* Back button clicked */
static void on_back_button_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    VideoView *view = (VideoView *)user_data;
    video_view_hide_video(view);
}

/* Audio stream menu item activated */
static void on_audio_stream_selected(GtkMenuItem *item, gpointer user_data) {
    VideoView *view = (VideoView *)user_data;
    gint index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "stream-index"));
    player_set_audio_stream(view->player, index);
    update_audio_menu(view);
}

/* Subtitle stream menu item activated */
static void on_subtitle_stream_selected(GtkMenuItem *item, gpointer user_data) {
    VideoView *view = (VideoView *)user_data;
    gint index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "stream-index"));
    player_set_subtitle_stream(view->player, index);
    update_subtitle_menu(view);
}

/* Subtitles off selected */
static void on_subtitles_off_selected(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    VideoView *view = (VideoView *)user_data;
    player_set_subtitles_enabled(view->player, FALSE);
    update_subtitle_menu(view);
}

static void update_audio_menu(VideoView *view) {
    if (!view || !view->audio_menu_button) return;
    
    /* Create new menu */
    GtkWidget *menu = gtk_menu_new();
    
    gint n_audio = player_get_audio_stream_count(view->player);
    gint current = player_get_current_audio_stream(view->player);
    
    if (n_audio == 0) {
        GtkWidget *item = gtk_menu_item_new_with_label("No audio tracks");
        gtk_widget_set_sensitive(item, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    } else {
        for (gint i = 0; i < n_audio; i++) {
            StreamInfo *info = player_get_audio_stream_info(view->player, i);
            
            gchar *label;
            if (info && info->language && info->codec) {
                label = g_strdup_printf("%s (%s)", info->language, info->codec);
            } else if (info && info->title) {
                label = g_strdup(info->title);
            } else {
                label = g_strdup_printf("Audio Track %d", i + 1);
            }
            
            GtkWidget *item = gtk_check_menu_item_new_with_label(label);
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), i == current);
            gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(item), TRUE);
            g_object_set_data(G_OBJECT(item), "stream-index", GINT_TO_POINTER(i));
            g_signal_connect(item, "activate", G_CALLBACK(on_audio_stream_selected), view);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
            
            g_free(label);
            player_free_stream_info(info);
        }
    }
    
    gtk_widget_show_all(menu);
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(view->audio_menu_button), menu);
}

/* Wrapper for g_timeout_add */
static gboolean update_audio_menu_timeout(gpointer user_data) {
    update_audio_menu((VideoView *)user_data);
    return G_SOURCE_REMOVE;
}

static void update_subtitle_menu(VideoView *view) {
    if (!view || !view->subtitle_menu_button) return;
    
    /* Create new menu */
    GtkWidget *menu = gtk_menu_new();
    
    gint n_text = player_get_subtitle_stream_count(view->player);
    gint current = player_get_current_subtitle_stream(view->player);
    gboolean subs_enabled = player_get_subtitles_enabled(view->player);
    
    /* Add "Off" option */
    GtkWidget *off_item = gtk_check_menu_item_new_with_label("Off");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(off_item), !subs_enabled || current < 0);
    gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(off_item), TRUE);
    g_signal_connect(off_item, "activate", G_CALLBACK(on_subtitles_off_selected), view);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), off_item);
    
    if (n_text > 0) {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        
        for (gint i = 0; i < n_text; i++) {
            StreamInfo *info = player_get_subtitle_stream_info(view->player, i);
            
            gchar *label;
            if (info && info->language && info->codec) {
                /* Show language, codec, and track number for disambiguation */
                label = g_strdup_printf("%d: %s (%s)", i + 1, info->language, info->codec);
            } else if (info && info->language && info->title && g_strcmp0(info->language, info->title) != 0) {
                /* Show language and title if different */
                label = g_strdup_printf("%d: %s - %s", i + 1, info->language, info->title);
            } else if (info && info->language) {
                /* At minimum, include track number with language */
                label = g_strdup_printf("%d: %s", i + 1, info->language);
            } else if (info && info->title) {
                label = g_strdup_printf("%d: %s", i + 1, info->title);
            } else {
                label = g_strdup_printf("Subtitle Track %d", i + 1);
            }
            
            GtkWidget *item = gtk_check_menu_item_new_with_label(label);
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), 
                                           subs_enabled && i == current);
            gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(item), TRUE);
            g_object_set_data(G_OBJECT(item), "stream-index", GINT_TO_POINTER(i));
            g_signal_connect(item, "activate", G_CALLBACK(on_subtitle_stream_selected), view);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
            
            g_free(label);
            player_free_stream_info(info);
        }
    }
    
    gtk_widget_show_all(menu);
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(view->subtitle_menu_button), menu);
}

/* Wrapper for g_timeout_add */
static gboolean update_subtitle_menu_timeout(gpointer user_data) {
    update_subtitle_menu((VideoView *)user_data);
    return G_SOURCE_REMOVE;
}

static GtkWidget* create_video_controls(VideoView *view) {
    /* Create revealer for smooth show/hide animation */
    view->controls_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(view->controls_revealer), 
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
    gtk_revealer_set_transition_duration(GTK_REVEALER(view->controls_revealer), 200);
    gtk_widget_set_valign(view->controls_revealer, GTK_ALIGN_END);
    gtk_widget_set_halign(view->controls_revealer, GTK_ALIGN_FILL);
    
    /* Main vertical container for controls */
    GtkWidget *controls_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(controls_vbox, 10);
    gtk_widget_set_margin_end(controls_vbox, 10);
    gtk_widget_set_margin_bottom(controls_vbox, 10);
    gtk_widget_set_margin_top(controls_vbox, 10);
    
    /* Add a semi-transparent background */
    GtkStyleContext *context = gtk_widget_get_style_context(controls_vbox);
    gtk_style_context_add_class(context, "video-controls");
    
    /* Apply CSS for background */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        ".video-controls { background-color: rgba(0, 0, 0, 0.7); border-radius: 8px; padding: 8px; }"
        ".video-title { font-weight: bold; font-size: 14px; color: white; }"
        ".video-time { font-size: 12px; color: #cccccc; }", -1, NULL);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
    
    /* Video title label */
    view->video_title_label = gtk_label_new("");
    gtk_label_set_ellipsize(GTK_LABEL(view->video_title_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(view->video_title_label), 80);
    gtk_widget_set_halign(view->video_title_label, GTK_ALIGN_START);
    GtkStyleContext *title_ctx = gtk_widget_get_style_context(view->video_title_label);
    gtk_style_context_add_class(title_ctx, "video-title");
    gtk_style_context_add_provider(title_ctx, GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    gtk_box_pack_start(GTK_BOX(controls_vbox), view->video_title_label, FALSE, FALSE, 0);
    
    /* Time label */
    view->time_label = gtk_label_new("0:00 / 0:00");
    gtk_widget_set_halign(view->time_label, GTK_ALIGN_START);
    GtkStyleContext *time_ctx = gtk_widget_get_style_context(view->time_label);
    gtk_style_context_add_class(time_ctx, "video-time");
    gtk_style_context_add_provider(time_ctx, GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    gtk_box_pack_start(GTK_BOX(controls_vbox), view->time_label, FALSE, FALSE, 0);
    
    /* Control bar (horizontal buttons) */
    GtkWidget *control_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(control_bar, 5);
    
    /* Back button */
    view->back_button = gtk_button_new_from_icon_name("go-previous-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(view->back_button, "Back to video list");
    g_signal_connect(view->back_button, "clicked", G_CALLBACK(on_back_button_clicked), view);
    gtk_box_pack_start(GTK_BOX(control_bar), view->back_button, FALSE, FALSE, 0);
    
    /* Spacer */
    GtkWidget *spacer = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(control_bar), spacer, TRUE, TRUE, 0);
    
    /* Audio stream button */
    view->audio_menu_button = gtk_menu_button_new();
    GtkWidget *audio_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *audio_icon = gtk_image_new_from_icon_name("audio-x-generic-symbolic", GTK_ICON_SIZE_BUTTON);
    GtkWidget *audio_label = gtk_label_new("Audio");
    gtk_box_pack_start(GTK_BOX(audio_box), audio_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(audio_box), audio_label, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(view->audio_menu_button), audio_box);
    gtk_widget_set_tooltip_text(view->audio_menu_button, "Select audio track");
    gtk_box_pack_start(GTK_BOX(control_bar), view->audio_menu_button, FALSE, FALSE, 0);
    
    /* Subtitle button */
    view->subtitle_menu_button = gtk_menu_button_new();
    GtkWidget *sub_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *sub_icon = gtk_image_new_from_icon_name("media-view-subtitles-symbolic", GTK_ICON_SIZE_BUTTON);
    GtkWidget *sub_label = gtk_label_new("Subtitles");
    gtk_box_pack_start(GTK_BOX(sub_box), sub_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sub_box), sub_label, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(view->subtitle_menu_button), sub_box);
    gtk_widget_set_tooltip_text(view->subtitle_menu_button, "Select subtitles");
    gtk_box_pack_start(GTK_BOX(control_bar), view->subtitle_menu_button, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(controls_vbox), control_bar, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(view->controls_revealer), controls_vbox);
    gtk_widget_show_all(view->controls_revealer);
    
    return view->controls_revealer;
}

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
    
    /* Check if we already have this widget embedded */
    if (view->video_widget == widget) {
        g_print("VideoView: Widget already embedded, just showing it\n");
        gtk_widget_show(view->video_widget);
        if (view->content_stack) {
            gtk_stack_set_visible_child_name(GTK_STACK(view->content_stack), "playback");
        }
        /* Update stream menus for new video */
        update_audio_menu(view);
        update_subtitle_menu(view);
        show_controls(view);
        return;
    }
    
    /* Remove old video widget if present */
    if (view->video_widget && gtk_widget_get_parent(view->video_widget) == view->overlay_container) {
        g_print("VideoView: Removing old video widget from overlay\n");
        gtk_container_remove(GTK_CONTAINER(view->overlay_container), view->video_widget);
    }
    
    /* Store the new widget */
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
    
    /* Add video widget to overlay */
    g_print("VideoView: Adding video widget to overlay...\n");
    
    /* Ensure the widget expands to fill the overlay */
    gtk_widget_set_hexpand(view->video_widget, TRUE);
    gtk_widget_set_vexpand(view->video_widget, TRUE);
    gtk_widget_set_valign(view->video_widget, GTK_ALIGN_FILL);
    gtk_widget_set_halign(view->video_widget, GTK_ALIGN_FILL);
    
    /* Enable motion events on video widget for showing controls */
    gtk_widget_add_events(view->video_widget, GDK_POINTER_MOTION_MASK);
    g_signal_connect(view->video_widget, "motion-notify-event", 
                    G_CALLBACK(on_video_motion), view);
    
    /* Add video widget as the main child of the overlay */
    gtk_container_add(GTK_CONTAINER(view->overlay_container), view->video_widget);
    gtk_widget_show(view->video_widget);
    
    /* Add controls overlay on top of video */
    if (view->controls_revealer) {
        gtk_overlay_add_overlay(GTK_OVERLAY(view->overlay_container), view->controls_revealer);
        gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(view->overlay_container), 
                                             view->controls_revealer, FALSE);
    }
    
    /* Now switch to show the overlay with embedded video */
    if (view->content_stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(view->content_stack), "playback");
        g_print("VideoView: Switched to playback view - video widget embedded\n");
    }
    
    /* Update stream menus after a short delay to let GStreamer discover streams */
    g_timeout_add(500, update_audio_menu_timeout, view);
    g_timeout_add(500, update_subtitle_menu_timeout, view);
    
    /* Start position timer to update time display */
    start_position_timer(view);
    
    /* Show controls initially */
    show_controls(view);
    
    g_print("VideoView: Video widget embedded in overlay with controls\n");
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

static gboolean update_video_position(gpointer user_data) {
    VideoView *view = (VideoView *)user_data;
    
    if (!view || !view->player || !view->video_playing) {
        view->position_timeout_id = 0;
        return G_SOURCE_REMOVE;
    }
    
    gint64 position = player_get_position(view->player);
    gint64 duration = player_get_duration(view->player);
    
    if (view->time_label && duration > 0) {
        gchar pos_str[32], dur_str[32], time_str[80];
        format_video_time((gint)(position / GST_SECOND), pos_str, sizeof(pos_str));
        format_video_time((gint)(duration / GST_SECOND), dur_str, sizeof(dur_str));
        g_snprintf(time_str, sizeof(time_str), "%s / %s", pos_str, dur_str);
        gtk_label_set_text(GTK_LABEL(view->time_label), time_str);
    }
    
    return G_SOURCE_CONTINUE;
}

static void start_position_timer(VideoView *view) {
    if (view->position_timeout_id > 0) {
        g_source_remove(view->position_timeout_id);
    }
    view->position_timeout_id = g_timeout_add(500, update_video_position, view);
}

static void stop_position_timer(VideoView *view) {
    if (view->position_timeout_id > 0) {
        g_source_remove(view->position_timeout_id);
        view->position_timeout_id = 0;
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
        gchar *title;
        
        gtk_tree_model_get(GTK_TREE_MODEL(view->video_store), &iter,
                          VIDEO_COL_ID, &video_id,
                          VIDEO_COL_TITLE, &title,
                          VIDEO_COL_FILE_PATH, &file_path,
                          -1);
        
        if (file_path && view->player) {
            view->video_playing = TRUE;
            
            /* Set the video title in the controls */
            if (view->video_title_label && title) {
                gtk_label_set_text(GTK_LABEL(view->video_title_label), title);
            }
            
            /* Update the header bar now playing label */
            app_set_video_now_playing(title);
            
            /* Reset time label */
            if (view->time_label) {
                gtk_label_set_text(GTK_LABEL(view->time_label), "0:00 / 0:00");
            }
            
            /* Register callback to be notified when gtksink widget is ready */
            player_set_video_widget_ready_callback(view->player, 
                                                   G_CALLBACK(on_video_widget_ready), 
                                                   view);
            
            /* Set URI and start playback */
            player_set_uri(view->player, file_path);
            player_play(view->player);
            
            g_print("VideoView: Started video playback, waiting for widget...\n");
            
            g_free(file_path);
            g_free(title);
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
    
    /* Create video controls (will be added to overlay when video plays) */
    create_video_controls(view);
    view->controls_visible = FALSE;
    view->controls_timeout_id = 0;
    
    /* Show list by default */
    gtk_stack_set_visible_child_name(GTK_STACK(view->main_container), "list");
    
    /* Don't create a video widget yet - we'll get it from gtksink when playing */
    view->video_widget = NULL;
    
    gtk_widget_show_all(view->main_container);
    
    return view;
}

void video_view_free(VideoView *view) {
    if (!view) return;
    
    /* Cancel controls timeout if active */
    if (view->controls_timeout_id > 0) {
        g_source_remove(view->controls_timeout_id);
    }
    
    /* Cancel position timeout if active */
    if (view->position_timeout_id > 0) {
        g_source_remove(view->position_timeout_id);
    }
    
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
    
    video_view_hide_video_ui(view);
    
    /* Stop playback - only when explicitly hiding video (e.g., stop button) */
    if (view->player) {
        player_stop(view->player);
    }
}

void video_view_hide_video_ui(VideoView *view) {
    if (!view) return;
    
    view->video_playing = FALSE;
    app_set_video_playing(FALSE);
    
    /* Stop position timer */
    stop_position_timer(view);
    
    /* Hide the video widget */
    if (view->video_widget) {
        gtk_widget_hide(view->video_widget);
    }
    
    /* Switch back to video list */
    if (view->content_stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(view->content_stack), "list");
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
