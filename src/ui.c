#include "ui.h"
#include "source.h"
#include "browser.h"
#include "coverart.h"
#include "smartplaylist.h"
#include "radio.h"
#include "import.h"
#include "albumview.h"
#include "videoview.h"
#include <string.h>

enum {
    COL_ID = 0,
    COL_TRACK_NUM,
    COL_TITLE,
    COL_ARTIST,
    COL_ALBUM,
    COL_DURATION,
    NUM_COLS
};

static void format_time(gint seconds, gchar *buffer, gsize buffer_size) {
    gint mins = seconds / 60;
    gint secs = seconds % 60;
    g_snprintf(buffer, buffer_size, "%02d:%02d", mins, secs);
}

/* Forward declarations */
static void on_source_selected(GtkSelectionModel *selection, guint position, guint n_items, gpointer user_data);
static void on_browser_selection_changed(GtkSelectionModel *selection, guint position, guint n_items, gpointer user_data);
static void ui_internal_update_track_list(MediaPlayerUI *ui);
static void ui_update_track_list_with_tracks(MediaPlayerUI *ui, GList *tracks);
static void ui_show_radio_stations(MediaPlayerUI *ui);
static void on_import_audio_action(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_import_video_action(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_search_changed(GtkSearchEntry *entry, gpointer user_data);
static void on_preferences_action(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void ui_update_cover_art(MediaPlayerUI *ui, const gchar *artist, const gchar *album, const gchar *podcast_image_url);

static void on_seek_changed(GtkRange *range, gpointer user_data) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    if (!ui->player) return;
    
    gint64 duration = player_get_duration(ui->player);
    if (duration > 0) {
        gdouble value = gtk_range_get_value(range);
        gint64 position = (gint64)(duration * value / 100.0);
        player_seek(ui->player, position);
    }
}

/* GTK4: Use GtkPopoverMenu with GMenu instead of GtkMenu */
static void on_quit_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    if (ui && ui->app) {
        g_application_quit(G_APPLICATION(ui->app));
    }
}

static void on_import_audio_action(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_import_video_action(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_preferences_action(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static GtkWidget* create_hamburger_menu(MediaPlayerUI *ui) {
    GMenu *menu = g_menu_new();
    
    /* Media section */
    GMenu *media_section = g_menu_new();
    g_menu_append(media_section, "New Playlist", "win.new-playlist");
    g_menu_append(media_section, "New Smart Playlist...", "win.new-smart-playlist");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(media_section));
    
    /* Import section */
    GMenu *import_section = g_menu_new();
    g_menu_append(import_section, "Import Audio...", "win.import-audio");
    g_menu_append(import_section, "Import Video...", "win.import-video");
    g_menu_append(import_section, "Import Playlist...", "win.import-playlist");
    g_menu_append(import_section, "Open Location...", "win.open-location");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(import_section));
    
    /* Radio section */
    GMenu *radio_section = g_menu_new();
    g_menu_append(radio_section, "Add Station", "win.add-station");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(radio_section));
    
    /* Preferences section */
    GMenu *prefs_section = g_menu_new();
    g_menu_append(prefs_section, "Preferences", "win.preferences");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(prefs_section));
    
    /* App section */
    GMenu *app_section = g_menu_new();
    g_menu_append(app_section, "About Banshee", "win.about");
    g_menu_append(app_section, "Quit", "win.quit");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(app_section));
    
    GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
    
    /* Add window actions */
    GtkWidget *window = ui->window;
    
    static const GActionEntry win_actions[] = {
        { "import-audio", on_import_audio_action, NULL, NULL, NULL },
        { "import-video", on_import_video_action, NULL, NULL, NULL },
        { "preferences", on_preferences_action, NULL, NULL, NULL },
        { "quit", on_quit_action, NULL, NULL, NULL },
    };
    
    GActionMap *action_map = G_ACTION_MAP(window);
    g_action_map_add_action_entries(action_map, win_actions, G_N_ELEMENTS(win_actions), ui);
    
    g_object_unref(menu);
    
    return popover;
}

static void on_volume_changed(GtkRange *range, gpointer user_data) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    gdouble value = gtk_range_get_value(range);
    player_set_volume(ui->player, value / 100.0);
}

static void on_volume_button_clicked(GtkButton *button, gpointer user_data) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    GtkWidget *popover = gtk_popover_new();
    gtk_widget_set_parent(popover, GTK_WIDGET(button));
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    
    GtkWidget *label = gtk_label_new("Volume");
    gtk_box_append(GTK_BOX(box), label);
    
    ui->volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0, 100, 1);
    gtk_widget_set_size_request(ui->volume_scale, -1, 120);
    gtk_range_set_inverted(GTK_RANGE(ui->volume_scale), TRUE);
    /* Initialize with player's current volume */
    gdouble current_volume = player_get_volume(ui->player) * 100.0;
    gtk_range_set_value(GTK_RANGE(ui->volume_scale), current_volume);
    gtk_scale_set_draw_value(GTK_SCALE(ui->volume_scale), TRUE);
    gtk_scale_set_value_pos(GTK_SCALE(ui->volume_scale), GTK_POS_BOTTOM);
    g_signal_connect(ui->volume_scale, "value-changed", G_CALLBACK(on_volume_changed), ui);
    gtk_widget_set_vexpand(ui->volume_scale, TRUE);
    gtk_box_append(GTK_BOX(box), ui->volume_scale);
    
    gtk_popover_set_child(GTK_POPOVER(popover), box);
    gtk_popover_popup(GTK_POPOVER(popover));
}

static GtkWidget* create_headerbar(MediaPlayerUI *ui) {
    GtkWidget *headerbar = gtk_header_bar_new();
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(headerbar), TRUE);
    
    /* Title widget - GTK4 uses set_title_widget instead of set_title/subtitle */
    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *title_label = gtk_label_new("Banshee");
    gtk_widget_add_css_class(title_label, "title");
    GtkWidget *subtitle_label = gtk_label_new("Media Player");
    gtk_widget_add_css_class(subtitle_label, "subtitle");
    gtk_box_append(GTK_BOX(title_box), title_label);
    gtk_box_append(GTK_BOX(title_box), subtitle_label);
    
    /* Left side - playback controls */
    GtkWidget *controls_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(controls_box, "linked");
    
    ui->prev_button = gtk_button_new_from_icon_name("media-skip-backward-symbolic");
    gtk_widget_set_tooltip_text(ui->prev_button, "Previous");
    g_signal_connect(ui->prev_button, "clicked", G_CALLBACK(ui_on_prev_clicked), ui);
    gtk_box_append(GTK_BOX(controls_box), ui->prev_button);
    
    ui->play_button = gtk_button_new_from_icon_name("media-playback-start-symbolic");
    gtk_widget_set_tooltip_text(ui->play_button, "Play");
    g_signal_connect(ui->play_button, "clicked", G_CALLBACK(ui_on_play_clicked), ui);
    gtk_box_append(GTK_BOX(controls_box), ui->play_button);
    
    ui->pause_button = gtk_button_new_from_icon_name("media-playback-pause-symbolic");
    gtk_widget_set_tooltip_text(ui->pause_button, "Pause");
    g_signal_connect(ui->pause_button, "clicked", G_CALLBACK(ui_on_pause_clicked), ui);
    gtk_box_append(GTK_BOX(controls_box), ui->pause_button);
    
    ui->next_button = gtk_button_new_from_icon_name("media-skip-forward-symbolic");
    gtk_widget_set_tooltip_text(ui->next_button, "Next");
    g_signal_connect(ui->next_button, "clicked", G_CALLBACK(ui_on_next_clicked), ui);
    gtk_box_append(GTK_BOX(controls_box), ui->next_button);
    
    gtk_header_bar_pack_start(GTK_HEADER_BAR(headerbar), controls_box);
    
/* Center - cover art and progress bar with media info */
    GtkWidget *media_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_size_request(media_box, 450, -1);
    
    /* Cover art */
    ui->header_cover_art = coverart_widget_new(64);
    gtk_widget_set_halign(ui->header_cover_art, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(ui->header_cover_art, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(ui->header_cover_art, 64, 64);
    gtk_box_append(GTK_BOX(media_box), ui->header_cover_art);
    
    /* Progress and info container */
    GtkWidget *progress_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(progress_box, TRUE);
    
    ui->seek_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_scale_set_draw_value(GTK_SCALE(ui->seek_scale), FALSE);
    ui->seek_handler_id = g_signal_connect(ui->seek_scale, "value-changed", G_CALLBACK(on_seek_changed), ui);
    gtk_widget_set_size_request(ui->seek_scale, 300, -1);
    gtk_box_append(GTK_BOX(progress_box), ui->seek_scale);

    GtkWidget *info_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    ui->now_playing_label = gtk_label_new("No track playing");
    gtk_widget_set_halign(ui->now_playing_label, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(ui->now_playing_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_size_request(ui->now_playing_label, 200, -1);
    gtk_widget_set_hexpand(ui->now_playing_label, TRUE);
    gtk_box_append(GTK_BOX(info_row), ui->now_playing_label);

    ui->time_label = gtk_label_new("00:00 / 00:00");
    gtk_box_append(GTK_BOX(info_row), ui->time_label);

    gtk_box_append(GTK_BOX(progress_box), info_row);
    gtk_box_append(GTK_BOX(media_box), progress_box);
    
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(headerbar), media_box);
    
    /* Right side - volume and menu */
    GtkWidget *volume_button = gtk_button_new_from_icon_name("audio-volume-high-symbolic");
    gtk_widget_set_tooltip_text(volume_button, "Volume");
    g_signal_connect(volume_button, "clicked", G_CALLBACK(on_volume_button_clicked), ui);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(headerbar), volume_button);
    
    /* Hamburger menu button */
    GtkWidget *menu_button = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_button), "open-menu-symbolic");
    gtk_widget_set_tooltip_text(menu_button, "Menu");
    
    GtkWidget *menu_popover = create_hamburger_menu(ui);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(menu_button), menu_popover);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(headerbar), menu_button);
    
    return headerbar;
}

/* Import action handlers for GTK4 */
static void on_import_folder_ready(GObject *source, GAsyncResult *result, gpointer user_data);

typedef struct {
    MediaPlayerUI *ui;
    gboolean is_video;
} ImportData;

static void on_import_audio_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Select Audio Folder");
    
    ImportData *data = g_new0(ImportData, 1);
    data->ui = ui;
    data->is_video = FALSE;
    
    gtk_file_dialog_select_folder(dialog, GTK_WINDOW(ui->window), NULL, on_import_folder_ready, data);
    g_object_unref(dialog);
}

static void on_import_video_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Select Video Folder");
    
    ImportData *data = g_new0(ImportData, 1);
    data->ui = ui;
    data->is_video = TRUE;
    
    gtk_file_dialog_select_folder(dialog, GTK_WINDOW(ui->window), NULL, on_import_folder_ready, data);
    g_object_unref(dialog);
}

static void on_import_folder_ready(GObject *source, GAsyncResult *result, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    ImportData *data = (ImportData *)user_data;
    MediaPlayerUI *ui = data->ui;
    gboolean is_video = data->is_video;
    g_free(data);
    
    GError *error = NULL;
    GFile *folder = gtk_file_dialog_select_folder_finish(dialog, result, &error);
    
    if (error) {
        if (!g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED)) {
            g_warning("Failed to select folder: %s", error->message);
        }
        g_error_free(error);
        return;
    }
    
    if (!folder) return;
    
    gchar *folder_path = g_file_get_path(folder);
    g_object_unref(folder);
    
    if (!folder_path) return;
    
    /* Import files */
    if (is_video) {
        import_video_from_directory_with_covers(folder_path, ui->database, ui->coverart_manager);
        if (ui->video_view) {
            video_view_load_videos(ui->video_view);
        }
    } else {
        import_audio_from_directory_with_covers(folder_path, ui->database, ui->coverart_manager);
        ui_internal_update_track_list(ui);
        if (ui->artist_model) {
            browser_model_reload(ui->artist_model);
        }
    }
    
    g_free(folder_path);
    
    /* Update source counts */
    if (ui->source_manager) {
        source_manager_populate(ui->source_manager);
    }
}

static void on_preferences_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    (void)action;
    (void)parameter;
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    ui_show_preferences_dialog(ui);
}

static void init_source_manager(MediaPlayerUI *ui) {
    ui->source_manager = source_manager_new(ui->database);
    source_manager_add_default_sources(ui->source_manager);
    
    /* Create sidebar using source_manager's GTK4 API */
    ui->sidebar = source_manager_create_sidebar(ui->source_manager);
    gtk_widget_set_size_request(ui->sidebar, 180, -1);
    
    /* Get the list view from the scrolled window for reference */
    ui->sidebar_listview = gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(ui->sidebar));
    
    /* Connect selection signal using GTK4 GtkSelectionModel */
    GtkSelectionModel *selection = source_manager_get_selection(ui->source_manager);
    ui->source_selection_handler_id = g_signal_connect(
        selection, "selection-changed", 
        G_CALLBACK(on_source_selected), ui);
}

static void init_browsers(MediaPlayerUI *ui) {
    /* Create browser models */
    ui->artist_model = browser_model_new(BROWSER_TYPE_ARTIST, ui->database);
    ui->album_model = browser_model_new(BROWSER_TYPE_ALBUM, ui->database);
    ui->genre_model = browser_model_new(BROWSER_TYPE_GENRE, ui->database);
    
    /* Create browser views */
    ui->artist_browser = browser_view_new(ui->artist_model);
    ui->album_browser = browser_view_new(ui->album_model);
    ui->genre_browser = browser_view_new(ui->genre_model);
    
    /* Single artist browser like original Banshee (not iTunes 3-row style) */
    ui->browser_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(ui->browser_container, 180, -1);
    
    /* Only pack artist browser - cleaner, more like Banshee */
    GtkWidget *artist_widget = browser_view_get_widget(ui->artist_browser);
    gtk_widget_set_vexpand(artist_widget, TRUE);
    gtk_box_append(GTK_BOX(ui->browser_container), artist_widget);
    
    /* Connect selection signal for artist browser only */
    browser_view_set_selection_callback(ui->artist_browser,
                    G_CALLBACK(on_browser_selection_changed), ui);
    
    /* Create album grid view */
    ui->album_view = album_view_new(ui->coverart_manager, ui->database);
    ui->album_container = album_view_get_widget(ui->album_view);
    /* No fixed size - let the paned handle it */
}

static void on_album_selected(const gchar *artist, const gchar *album, gpointer user_data) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    
    /* Block track selection signal while updating - GTK4: use track_selection */
    if (ui->track_selection && ui->track_selection_handler_id > 0) {
        g_signal_handler_block(ui->track_selection, ui->track_selection_handler_id);
    }
    
    /* Filter tracks by album */
    GList *tracks = database_get_tracks_by_album(ui->database, artist, album);
    ui_update_track_list_with_tracks(ui, tracks);
    g_list_free_full(tracks, (GDestroyNotify)database_free_track);
    
    /* Unblock track selection signal */
    if (ui->track_selection && ui->track_selection_handler_id > 0) {
        g_signal_handler_unblock(ui->track_selection, ui->track_selection_handler_id);
    }
}

static void on_funding_url_clicked(GtkWidget *button, gpointer user_data) {
    (void)user_data;
    const gchar *url = (const gchar *)g_object_get_data(G_OBJECT(button), "funding_url");
    if (url) {
        gchar *command = g_strdup_printf("xdg-open '%s'", url);
        g_spawn_command_line_async(command, NULL);
        g_free(command);
    }
}

static void on_podcast_seek(gpointer user_data, gdouble time_seconds) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    
    /* Convert seconds to nanoseconds (GStreamer uses nanoseconds) */
    gint64 position_ns = (gint64)(time_seconds * GST_SECOND);
    
    /* Seek the player */
    if (ui->player) {
        player_seek(ui->player, position_ns);
    } else {
        /* Warning: No player available for seeking */
    }
}

static void on_podcast_episode_play(gpointer user_data, const gchar *uri, const gchar *title, GList *chapters, 
                                   const gchar *transcript_url, const gchar *transcript_type, GList *funding) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    (void)transcript_type;
    (void)transcript_url;
    (void)chapters;
    (void)funding;
    
    /* Stop current playback and load podcast episode */
    player_stop(ui->player);
    player_set_uri(ui->player, uri);
    player_play(ui->player);
    
    /* Get podcast information for cover art */
    PodcastView *podcast_view = ui->podcast_view;
    gchar *podcast_title = NULL;
    const gchar *image_url = NULL;
    
    /* Get currently selected podcast */
    Podcast *current_podcast = podcast_view_get_selected_podcast(podcast_view);
    if (current_podcast) {
        podcast_title = g_strdup(current_podcast->title);
        
        /* Get the best image URL for this podcast */
        image_url = podcast_get_display_image_url(current_podcast);
    }
    
    /* Update UI with podcast-specific function */
    ui_update_now_playing_podcast(ui, podcast_title, title, image_url);
    
    /* Cleanup */
    g_free(podcast_title);
    /* image_url is const, don't free it */
    
    /* Episode buttons are now in podcast toolbar, not global UI */
    if (chapters && g_list_length(chapters) > 0) {
        /* Episode has chapters - toolbar button will be active */
    }
    
    if (transcript_url && strlen(transcript_url) > 0) {
        /* Episode has transcript - toolbar button will be active */
    }
    
    if (funding && g_list_length(funding) > 0) {
        /* Episode has funding options - toolbar button will be active */
    }
}

/* ============================================================================
 * Track list column factories for GTK4 GtkColumnView
 * ============================================================================ */

static void setup_track_num_label(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory; (void)user_data;
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 1.0);
    gtk_widget_set_margin_start(label, 4);
    gtk_widget_set_margin_end(label, 4);
    gtk_list_item_set_child(list_item, label);
}

static void bind_track_num_label(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory; (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    BansheeTrackObject *track = gtk_list_item_get_item(list_item);
    if (track) {
        gint num = banshee_track_object_get_track_number(track);
        gchar *text = g_strdup_printf("%d", num);
        gtk_label_set_text(GTK_LABEL(label), text);
        g_free(text);
    }
}

static void setup_title_label(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory; (void)user_data;
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_margin_start(label, 4);
    gtk_widget_set_margin_end(label, 4);
    gtk_list_item_set_child(list_item, label);
}

static void bind_title_label(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory; (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    BansheeTrackObject *track = gtk_list_item_get_item(list_item);
    if (track) {
        gtk_label_set_text(GTK_LABEL(label), banshee_track_object_get_title(track));
    }
}

static void setup_artist_label(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory; (void)user_data;
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_margin_start(label, 4);
    gtk_widget_set_margin_end(label, 4);
    gtk_list_item_set_child(list_item, label);
}

static void bind_artist_label(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory; (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    BansheeTrackObject *track = gtk_list_item_get_item(list_item);
    if (track) {
        gtk_label_set_text(GTK_LABEL(label), banshee_track_object_get_artist(track));
    }
}

static void setup_album_label(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory; (void)user_data;
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_margin_start(label, 4);
    gtk_widget_set_margin_end(label, 4);
    gtk_list_item_set_child(list_item, label);
}

static void bind_album_label(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory; (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    BansheeTrackObject *track = gtk_list_item_get_item(list_item);
    if (track) {
        gtk_label_set_text(GTK_LABEL(label), banshee_track_object_get_album(track));
    }
}

static void setup_duration_label(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory; (void)user_data;
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 1.0);
    gtk_widget_set_margin_start(label, 4);
    gtk_widget_set_margin_end(label, 4);
    gtk_list_item_set_child(list_item, label);
}

static void bind_duration_label(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory; (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    BansheeTrackObject *track = gtk_list_item_get_item(list_item);
    if (track) {
        gtk_label_set_text(GTK_LABEL(label), banshee_track_object_get_duration_str(track));
    }
}

static GtkWidget* create_track_list(MediaPlayerUI *ui) {
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    
    /* GTK4: Use GListStore with BansheeTrackObject GObjects */
    ui->track_store = g_list_store_new(BANSHEE_TYPE_TRACK_OBJECT);
    
    /* Create selection model */
    ui->track_selection = gtk_single_selection_new(G_LIST_MODEL(g_object_ref(ui->track_store)));
    gtk_single_selection_set_autoselect(ui->track_selection, FALSE);
    gtk_single_selection_set_can_unselect(ui->track_selection, TRUE);
    
    /* Create GtkColumnView */
    ui->track_listview = gtk_column_view_new(GTK_SELECTION_MODEL(ui->track_selection));
    gtk_column_view_set_show_column_separators(GTK_COLUMN_VIEW(ui->track_listview), FALSE);
    gtk_column_view_set_show_row_separators(GTK_COLUMN_VIEW(ui->track_listview), FALSE);
    
    /* Track # column */
    GtkListItemFactory *num_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(num_factory, "setup", G_CALLBACK(setup_track_num_label), NULL);
    g_signal_connect(num_factory, "bind", G_CALLBACK(bind_track_num_label), NULL);
    GtkColumnViewColumn *num_col = gtk_column_view_column_new("#", num_factory);
    gtk_column_view_column_set_fixed_width(num_col, 50);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(ui->track_listview), num_col);
    g_object_unref(num_col);
    
    /* Title column */
    GtkListItemFactory *title_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(title_factory, "setup", G_CALLBACK(setup_title_label), NULL);
    g_signal_connect(title_factory, "bind", G_CALLBACK(bind_title_label), NULL);
    GtkColumnViewColumn *title_col = gtk_column_view_column_new("Title", title_factory);
    gtk_column_view_column_set_expand(title_col, TRUE);
    gtk_column_view_column_set_resizable(title_col, TRUE);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(ui->track_listview), title_col);
    g_object_unref(title_col);
    
    /* Artist column */
    GtkListItemFactory *artist_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(artist_factory, "setup", G_CALLBACK(setup_artist_label), NULL);
    g_signal_connect(artist_factory, "bind", G_CALLBACK(bind_artist_label), NULL);
    GtkColumnViewColumn *artist_col = gtk_column_view_column_new("Artist", artist_factory);
    gtk_column_view_column_set_expand(artist_col, TRUE);
    gtk_column_view_column_set_resizable(artist_col, TRUE);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(ui->track_listview), artist_col);
    g_object_unref(artist_col);
    
    /* Album column */
    GtkListItemFactory *album_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(album_factory, "setup", G_CALLBACK(setup_album_label), NULL);
    g_signal_connect(album_factory, "bind", G_CALLBACK(bind_album_label), NULL);
    GtkColumnViewColumn *album_col = gtk_column_view_column_new("Album", album_factory);
    gtk_column_view_column_set_expand(album_col, TRUE);
    gtk_column_view_column_set_resizable(album_col, TRUE);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(ui->track_listview), album_col);
    g_object_unref(album_col);
    
    /* Duration column */
    GtkListItemFactory *duration_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(duration_factory, "setup", G_CALLBACK(setup_duration_label), NULL);
    g_signal_connect(duration_factory, "bind", G_CALLBACK(bind_duration_label), NULL);
    GtkColumnViewColumn *duration_col = gtk_column_view_column_new("Duration", duration_factory);
    gtk_column_view_column_set_fixed_width(duration_col, 80);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(ui->track_listview), duration_col);
    g_object_unref(duration_col);
    
    /* Connect selection signal */
    ui->track_selection_handler_id = g_signal_connect(ui->track_selection, "selection-changed", 
                                                       G_CALLBACK(ui_on_track_selected), ui);
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), ui->track_listview);
    return scrolled;
}


static GtkWidget* create_control_box(MediaPlayerUI *ui) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(vbox, 10);
    gtk_widget_set_margin_end(vbox, 10);
    gtk_widget_set_margin_top(vbox, 5);
    gtk_widget_set_margin_bottom(vbox, 2);
    
    /* Search row */
    GtkWidget *search_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    GtkWidget *search_label = gtk_label_new("Search:");
    gtk_box_append(GTK_BOX(search_row), search_label);
    
    ui->search_entry = gtk_search_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(ui->search_entry), "");
    gtk_widget_set_hexpand(ui->search_entry, TRUE);
    g_signal_connect(ui->search_entry, "search-changed", G_CALLBACK(on_search_changed), ui);
    gtk_box_append(GTK_BOX(search_row), ui->search_entry);
    
    gtk_box_append(GTK_BOX(vbox), search_row);
    
    /* Episode buttons removed - now in podcast toolbar */
    
    return vbox;
}

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    const gchar *search_text = gtk_editable_get_text(GTK_EDITABLE(entry));
    
    /* Get the active source to determine which view is shown */
    Source *active = source_manager_get_active(ui->source_manager);
    if (!active) return;
    
    /* Get the content stack */
    GtkStack *content_stack = GTK_STACK(g_object_get_data(G_OBJECT(ui->window), "content_stack"));
    const gchar *visible_child = gtk_stack_get_visible_child_name(content_stack);
    
    if (g_strcmp0(visible_child, "podcast") == 0) {
        /* Search in podcast view */
        podcast_view_filter(ui->podcast_view, search_text);
    } else {
        /* Search in music library track list */
        if (!search_text || strlen(search_text) == 0) {
            /* No search - show all tracks based on current filter */
            if (active->type == SOURCE_TYPE_LIBRARY) {
                /* Check if an artist is selected - use GTK4 selection model */
                GtkSingleSelection *selection = browser_view_get_selection_model(ui->artist_browser);
                gchar *artist = browser_model_get_selected_name(ui->artist_model, selection);
                
                if (artist && g_strcmp0(artist, "All Artists") != 0) {
                    GList *tracks = database_get_tracks_by_artist(ui->database, artist);
                    ui_update_track_list_with_tracks(ui, tracks);
                    g_list_free_full(tracks, (GDestroyNotify)database_free_track);
                } else {
                    ui_internal_update_track_list(ui);
                }
                g_free(artist);
            } else if (active->type == SOURCE_TYPE_PLAYLIST) {
                GList *tracks = database_get_playlist_tracks(ui->database, active->playlist_id);
                ui_update_track_list_with_tracks(ui, tracks);
                g_list_free_full(tracks, (GDestroyNotify)database_free_track);
            } else if (active->type == SOURCE_TYPE_SMART_PLAYLIST) {
                SmartPlaylist *sp = (SmartPlaylist *)active->user_data;
                if (sp) {
                    GList *tracks = smartplaylist_get_tracks(sp, ui->database);
                    ui_update_track_list_with_tracks(ui, tracks);
                    g_list_free_full(tracks, (GDestroyNotify)database_free_track);
                }
            }
        } else {
            /* Search tracks */
            GList *tracks = database_search_tracks(ui->database, search_text);
            ui_update_track_list_with_tracks(ui, tracks);
            g_list_free_full(tracks, (GDestroyNotify)database_free_track);
        }
    }
}

static void on_source_selected(GtkSelectionModel *selection, guint position, guint n_items, gpointer user_data) {
    (void)position;
    (void)n_items;
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    
    /* Get selected source using the new GTK4 API */
    Source *source = source_get_by_selection(ui->source_manager);
    
    if (source) {
        source_manager_set_active(ui->source_manager, source);
        
        /* Clear search entry when switching sources */
        gtk_editable_set_text(GTK_EDITABLE(ui->search_entry), "");
        
        /* Get the content stack */
        GtkStack *content_stack = GTK_STACK(g_object_get_data(G_OBJECT(ui->window), "content_stack"));
        
        /* Block track selection signal while updating list - GTK4 */
        if (ui->track_selection && ui->track_selection_handler_id > 0) {
            g_signal_handler_block(ui->track_selection, ui->track_selection_handler_id);
        }
        
        /* Clear current track list - GTK4: use g_list_store_remove_all */
        g_list_store_remove_all(ui->track_store);
        
        switch (source->type) {
            case SOURCE_TYPE_LIBRARY:
                /* Switch to music view */
                gtk_stack_set_visible_child_name(content_stack, "music");
                
                /* Update search placeholder */
                gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(ui->search_entry), "Search library...");
                
                if (source->media_types & MEDIA_TYPE_VIDEO) {
                    /* Video library - switch to video view */
                    gtk_stack_set_visible_child_name(content_stack, "video");
                    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(ui->search_entry), "Search videos...");
                    video_view_load_videos(ui->video_view);
                } else {
                    /* Music library with artist browser and album view */
                    gtk_widget_set_visible(ui->browser_container, TRUE);
                    gtk_widget_set_visible(ui->album_container, TRUE);
                    browser_model_reload(ui->artist_model);
                    album_view_set_artist(ui->album_view, NULL);
                    ui_internal_update_track_list(ui);
                }
                /* Unblock signal after updating */
                if (ui->track_selection && ui->track_selection_handler_id > 0) {
                    g_signal_handler_unblock(ui->track_selection, ui->track_selection_handler_id);
                }
                break;
                
            case SOURCE_TYPE_PLAYLIST: {
                /* Switch to music view */
                gtk_stack_set_visible_child_name(content_stack, "music");
                
                /* Update search placeholder */
                gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(ui->search_entry), "Search playlist...");
                
                /* Hide browsers for playlists */
                gtk_widget_set_visible(ui->browser_container, FALSE);
                gtk_widget_set_visible(ui->album_container, FALSE);
                GList *tracks = database_get_playlist_tracks(ui->database, source->playlist_id);
                ui_update_track_list_with_tracks(ui, tracks);
                g_list_free_full(tracks, (GDestroyNotify)database_free_track);
                /* Unblock signal after updating */
                if (ui->track_selection && ui->track_selection_handler_id > 0) {
                    g_signal_handler_unblock(ui->track_selection, ui->track_selection_handler_id);
                }
                break;
            }
                
            case SOURCE_TYPE_SMART_PLAYLIST: {
                /* Switch to music view */
                gtk_stack_set_visible_child_name(content_stack, "music");
                
                /* Update search placeholder */
                gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(ui->search_entry), "Search smart playlist...");
                
                gtk_widget_set_visible(ui->browser_container, FALSE);
                gtk_widget_set_visible(ui->album_container, FALSE);
                SmartPlaylist *sp = (SmartPlaylist *)source->user_data;
                if (sp) {
                    GList *tracks = smartplaylist_get_tracks(sp, ui->database);
                    ui_update_track_list_with_tracks(ui, tracks);
                    g_list_free_full(tracks, (GDestroyNotify)database_free_track);
                }
                /* Unblock signal after updating */
                if (ui->track_selection && ui->track_selection_handler_id > 0) {
                    g_signal_handler_unblock(ui->track_selection, ui->track_selection_handler_id);
                }
                break;
            }
                
            case SOURCE_TYPE_RADIO:
                /* Switch to music view and show radio stations */
                gtk_stack_set_visible_child_name(content_stack, "music");
                    
                    /* Update search placeholder */
                    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(ui->search_entry), "Search stations...");
                    
                    gtk_widget_set_visible(ui->browser_container, FALSE);
                    gtk_widget_set_visible(ui->album_container, FALSE);
                    ui_show_radio_stations(ui);
                    /* Unblock signal after updating */
                    if (ui->track_selection && ui->track_selection_handler_id > 0) {
                        g_signal_handler_unblock(ui->track_selection, ui->track_selection_handler_id);
                    }
                    break;
                    
                case SOURCE_TYPE_PODCAST:
                    /* Switch to podcast view */
                    gtk_stack_set_visible_child_name(content_stack, "podcast");
                    
                    /* Update search placeholder */
                    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(ui->search_entry), "Search podcasts...");
                    
                    podcast_view_refresh_podcasts(ui->podcast_view);
                    
                    /* Unblock signal after updating */
                    if (ui->track_selection && ui->track_selection_handler_id > 0) {
                        g_signal_handler_unblock(ui->track_selection, ui->track_selection_handler_id);
                    }
                    break;
                    
                default:
                    /* Switch to music view */
                    gtk_stack_set_visible_child_name(content_stack, "music");
                    
                    gtk_widget_set_visible(ui->browser_container, FALSE);
                    gtk_widget_set_visible(ui->album_container, FALSE);
                    /* Unblock signal */
                    if (ui->track_selection && ui->track_selection_handler_id > 0) {
                        g_signal_handler_unblock(ui->track_selection, ui->track_selection_handler_id);
                    }
                    break;
            }
        }
}

static void on_browser_selection_changed(GtkSelectionModel *selection, guint position, guint n_items, gpointer user_data) {
    (void)position;
    (void)n_items;
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    
    /* Get the selected artist from the browser using GTK4 API */
    GtkSingleSelection *single_sel = GTK_SINGLE_SELECTION(selection);
    gchar *artist = browser_model_get_selected_name(ui->artist_model, single_sel);
    
    /* Update album view with this artist's albums */
    album_view_set_artist(ui->album_view, artist);
    
    /* Block track selection signal while updating - GTK4 */
    if (ui->track_selection && ui->track_selection_handler_id > 0) {
        g_signal_handler_block(ui->track_selection, ui->track_selection_handler_id);
    }
    
    if (artist) {
        /* Filter by artist */
        GList *tracks = database_get_tracks_by_artist(ui->database, artist);
        ui_update_track_list_with_tracks(ui, tracks);
        g_list_free_full(tracks, (GDestroyNotify)database_free_track);
    } else {
        /* Show all tracks (NULL means "All" was selected) */
        ui_internal_update_track_list(ui);
    }
    
    /* Unblock track selection signal */
    if (ui->track_selection && ui->track_selection_handler_id > 0) {
        g_signal_handler_unblock(ui->track_selection, ui->track_selection_handler_id);
    }
    
    g_free(artist);
}

MediaPlayerUI* ui_new(MediaPlayer *player, Database *database, GtkApplication *app) {
    MediaPlayerUI *ui = g_new0(MediaPlayerUI, 1);
    ui->database = database;
    ui->player = player;
    ui->app = app;
    
    /* Initialize managers */
    ui->coverart_manager = coverart_manager_new();
    ui->podcast_manager = podcast_manager_new(database);
    
    /* Start automatic podcast feed updates based on preference (in minutes) */
    if (ui->podcast_manager && database && database->db) {
        gint update_interval = database_get_preference_int(database, "podcast_update_interval_minutes", 1440);  /* Default: 24 hours */
        podcast_manager_start_auto_update(ui->podcast_manager, update_interval);
    }
    
    /* Restore saved volume preference */
    if (database && database->db && player) {
        gdouble saved_volume = database_get_preference_double(database, "volume", 0.5);
        player_set_volume(player, saved_volume);
    }
    
    /* Create window - GTK4 uses GtkApplicationWindow */
    ui->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(ui->window), "Banshee Media Player");
    gtk_window_set_default_size(GTK_WINDOW(ui->window), 1200, 700);
    gtk_window_set_icon_name(GTK_WINDOW(ui->window), "multimedia-player");
    
    /* Create and set headerbar */
    GtkWidget *headerbar = create_headerbar(ui);
    gtk_window_set_titlebar(GTK_WINDOW(ui->window), headerbar);
    
    /* Main vertical box */
    ui->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(ui->window), ui->main_box);
    
    /* Initialize source manager and sidebar */
    init_source_manager(ui);
    
    /* Initialize browsers */
    init_browsers(ui);
    
    /* Initialize podcast view */
    ui->podcast_view = podcast_view_new(ui->podcast_manager, ui->database);
    
    /* Initialize video view */
    ui->video_view = video_view_new(ui->database, ui->player);
    
    /* Create main horizontal paned (sidebar | content) */
    ui->main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(ui->main_paned), 180);
    gtk_widget_set_vexpand(ui->main_paned, TRUE);
    gtk_box_append(GTK_BOX(ui->main_box), ui->main_paned);
    
    /* Add sidebar to left pane */
    gtk_paned_set_start_child(GTK_PANED(ui->main_paned), ui->sidebar);
    gtk_paned_set_resize_start_child(GTK_PANED(ui->main_paned), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(ui->main_paned), FALSE);
    
    /* Create a stack to hold different content views */
    GtkWidget *content_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(content_stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(content_stack), 150);
    gtk_paned_set_end_child(GTK_PANED(ui->main_paned), content_stack);
    gtk_paned_set_resize_end_child(GTK_PANED(ui->main_paned), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(ui->main_paned), TRUE);
    
    /* Create overlay container for music view (allows video to paint over playlist) */
    GtkWidget *music_overlay = gtk_overlay_new();
    gtk_stack_add_named(GTK_STACK(content_stack), music_overlay, "music");
    
    /* Create outer vertical paned for music library: top for browsers/albums, bottom for track list */
    GtkWidget *outer_vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_set_position(GTK_PANED(outer_vpaned), 250);
    gtk_overlay_set_child(GTK_OVERLAY(music_overlay), outer_vpaned);
    
    /* Create horizontal paned for artist browser and album view */
    ui->content_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(ui->content_paned), 200);
    gtk_paned_set_start_child(GTK_PANED(outer_vpaned), ui->content_paned);
    gtk_paned_set_resize_start_child(GTK_PANED(outer_vpaned), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(outer_vpaned), TRUE);
    
    /* Add artist browser to left */
    gtk_paned_set_start_child(GTK_PANED(ui->content_paned), ui->browser_container);
    gtk_paned_set_resize_start_child(GTK_PANED(ui->content_paned), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(ui->content_paned), TRUE);
    
    /* Add album view to right */
    gtk_paned_set_end_child(GTK_PANED(ui->content_paned), ui->album_container);
    gtk_paned_set_resize_end_child(GTK_PANED(ui->content_paned), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(ui->content_paned), TRUE);
    
    /* Add track list below (spanning full width) */
    ui->content_area = create_track_list(ui);
    gtk_paned_set_end_child(GTK_PANED(outer_vpaned), ui->content_area);
    gtk_paned_set_resize_end_child(GTK_PANED(outer_vpaned), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(outer_vpaned), TRUE);
    
    /* Add podcast view to stack */
    gtk_stack_add_named(GTK_STACK(content_stack), podcast_view_get_widget(ui->podcast_view), "podcast");
    
    /* Add video view to stack */
    gtk_stack_add_named(GTK_STACK(content_stack), video_view_get_widget(ui->video_view), "video");
    
    /* Video view now manages its own internal overlay and stack */
    
    /* Store content stack reference for switching views */
    g_object_set_data(G_OBJECT(ui->window), "content_stack", content_stack);
    
    /* Set album selection callback */
    album_view_set_selection_callback(ui->album_view, on_album_selected, ui);
    
    /* Set podcast playback callback */
    podcast_view_set_play_callback(ui->podcast_view, on_podcast_episode_play, ui);
    
    /* Set podcast seek callback */
    podcast_view_set_seek_callback(ui->podcast_view, on_podcast_seek, ui);
    
    /* Playback controls */
    ui->control_box = create_control_box(ui);
    gtk_box_append(GTK_BOX(ui->main_box), ui->control_box);
    
    /* GTK4: widgets are visible by default, no gtk_widget_show_all needed */
    gtk_window_present(GTK_WINDOW(ui->window));
    
    /* Initialize the view with the default active source (Music library) */
    Source *active = source_manager_get_active(ui->source_manager);
    if (active && active->type == SOURCE_TYPE_LIBRARY && !(active->media_types & MEDIA_TYPE_VIDEO)) {
        /* Show music library with artist browser and album view */
        gtk_widget_set_visible(ui->browser_container, TRUE);
        gtk_widget_set_visible(ui->album_container, TRUE);
        browser_model_reload(ui->artist_model);
        album_view_set_artist(ui->album_view, NULL);  /* Show all albums */
        ui_internal_update_track_list(ui);
    } else {
        /* Hide browsers and album view for other sources */
        gtk_widget_set_visible(ui->browser_container, FALSE);
        gtk_widget_set_visible(ui->album_container, FALSE);
    }
    
    return ui;
}

void ui_update_track_list(MediaPlayerUI *ui, GList *tracks) {
    ui_update_track_list_with_tracks(ui, tracks);
}

static void ui_internal_update_track_list(MediaPlayerUI *ui) {
    if (!ui || !ui->database) return;
    
    GList *tracks = database_get_all_tracks(ui->database);
    ui_update_track_list_with_tracks(ui, tracks);
    g_list_free_full(tracks, (GDestroyNotify)database_free_track);
}

static void ui_update_track_list_with_tracks(MediaPlayerUI *ui, GList *tracks) {
    if (!ui || !ui->track_store) return;
    
    /* Block selection signal while updating */
    if (ui->track_selection && ui->track_selection_handler_id > 0) {
        g_signal_handler_block(ui->track_selection, ui->track_selection_handler_id);
    }
    
    /* GTK4: Use g_list_store_remove_all instead of gtk_list_store_clear */
    g_list_store_remove_all(ui->track_store);
    
    for (GList *l = tracks; l != NULL; l = l->next) {
        Track *track = (Track *)l->data;
        if (!track) continue;
        
        gchar time_str[16];
        format_time(track->duration, time_str, sizeof(time_str));
        
        /* GTK4: Create a BansheeTrackObject and append to GListStore */
        BansheeTrackObject *track_obj = banshee_track_object_new(
            track->id,
            track->track_number,
            track->title,
            track->artist,
            track->album,
            time_str,
            track->duration,
            track->file_path
        );
        g_list_store_append(ui->track_store, track_obj);
        g_object_unref(track_obj);
    }
    
    /* Unblock selection signal */
    if (ui->track_selection && ui->track_selection_handler_id > 0) {
        g_signal_handler_unblock(ui->track_selection, ui->track_selection_handler_id);
    }
}

void ui_show_radio_stations(MediaPlayerUI *ui) {
    if (!ui || !ui->track_store) return;
    
    /* Block selection signal while updating */
    if (ui->track_selection && ui->track_selection_handler_id > 0) {
        g_signal_handler_block(ui->track_selection, ui->track_selection_handler_id);
    }
    
    /* GTK4: Use g_list_store_remove_all instead of gtk_list_store_clear */
    g_list_store_remove_all(ui->track_store);
    
    GList *stations = radio_station_get_all(ui->database);
    if (!stations) {
        /* Add default stations if none exist */
        stations = radio_station_get_defaults();
        for (GList *l = stations; l != NULL; l = l->next) {
            RadioStation *station = (RadioStation *)l->data;
            radio_station_save(station, ui->database);
        }
    }
    
    gint station_num = 1;
    for (GList *l = stations; l != NULL; l = l->next) {
        RadioStation *station = (RadioStation *)l->data;
        
        gchar *bitrate_str = g_strdup_printf("%d kbps", station->bitrate);
        
        /* GTK4: Create a BansheeTrackObject for radio station */
        BansheeTrackObject *track_obj = banshee_track_object_new(
            station->id,
            station_num++,
            station->name,
            station->genre ? station->genre : "Unknown",
            bitrate_str,
            "",  /* No duration for radio */
            0,
            station->url
        );
        g_list_store_append(ui->track_store, track_obj);
        g_object_unref(track_obj);
        
        g_free(bitrate_str);
    }
    
    g_list_free_full(stations, (GDestroyNotify)radio_station_free);
    
    /* Unblock selection signal */
    if (ui->track_selection && ui->track_selection_handler_id > 0) {
        g_signal_handler_unblock(ui->track_selection, ui->track_selection_handler_id);
    }
}

void ui_on_track_selected(GtkSelectionModel *selection, guint position, guint n_items, gpointer user_data) {
    (void)position;
    (void)n_items;
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    
    /* GTK4: Get selected item from GtkSingleSelection */
    BansheeTrackObject *track_obj = gtk_single_selection_get_selected_item(GTK_SINGLE_SELECTION(selection));
    if (!track_obj) return;
    
    gint track_id = banshee_track_object_get_id(track_obj);
    
    /* Hide video view if currently showing video (UI only, player will be reused) */
    if (ui->video_view && video_view_is_showing_video(ui->video_view)) {
        video_view_hide_video_ui(ui->video_view);
    }
    
    /* Check if this is a radio station or a track */
    Source *active_source = source_manager_get_active(ui->source_manager);
    if (active_source && active_source->type == SOURCE_TYPE_RADIO) {
        /* It's a radio station */
        RadioStation *station = radio_station_load(track_id, ui->database);
        if (station && station->url) {
            player_set_uri(ui->player, station->url);
            player_play(ui->player);
            
            gchar *label = g_strdup_printf("%s - %s", station->name ? station->name : "Radio",
                                           station->genre ? station->genre : "Streaming");
            gtk_label_set_text(GTK_LABEL(ui->now_playing_label), label);
            g_free(label);
            
            /* Update cover art for radio (typically no cover) */
            ui_update_cover_art(ui, NULL, NULL, NULL);
            
            radio_station_free(station);
        }
    } else {
        /* It's a music track */
        Track *track = database_get_track(ui->database, track_id);
        if (track && track->file_path) {
            /* Store current playlist for prev/next navigation */
            if (ui->current_playlist) {
                g_list_free_full(ui->current_playlist, (GDestroyNotify)database_free_track);
            }
            ui->current_playlist = database_get_all_tracks(ui->database);
            
            /* Find the index of this track in the playlist */
            ui->current_track_index = 0;
            for (GList *l = ui->current_playlist; l != NULL; l = l->next) {
                Track *t = (Track *)l->data;
                if (t && t->id == track_id) {
                    break;
                }
                ui->current_track_index++;
            }
            
            player_set_uri(ui->player, track->file_path);
            player_play(ui->player);
            
            gchar *label = g_strdup_printf("%s - %s", track->artist ? track->artist : "Unknown",
                                           track->title ? track->title : "Unknown");
            gtk_label_set_text(GTK_LABEL(ui->now_playing_label), label);
            g_free(label);
            
            /* Update cover art for this track */
            ui_update_cover_art(ui, track->artist, track->album, NULL);
            
            database_free_track(track);
        }
    }
}

void ui_on_play_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    player_play(ui->player);
}

void ui_on_pause_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    player_pause(ui->player);
}

void ui_on_stop_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    player_stop(ui->player);
    
    /* Hide video overlay if video is playing */
    if (ui->video_view && video_view_is_showing_video(ui->video_view)) {
        video_view_hide_video(ui->video_view);
    }
}

void ui_on_prev_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    
    if (ui->current_playlist && ui->current_track_index > 0) {
        ui->current_track_index--;
        Track *track = g_list_nth_data(ui->current_playlist, ui->current_track_index);
        if (track) {
            player_set_uri(ui->player, track->file_path);
            player_play(ui->player);
            
            gchar *label = g_strdup_printf("%s - %s", track->artist ? track->artist : "Unknown",
                                           track->title ? track->title : "Unknown");
            gtk_label_set_text(GTK_LABEL(ui->now_playing_label), label);
            g_free(label);
            
            /* Update cover art */
            ui_update_cover_art(ui, track->artist, track->album, NULL);
        }
    }
}

void ui_on_next_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    
    if (ui->current_playlist) {
        gint playlist_length = g_list_length(ui->current_playlist);
        if (ui->current_track_index < playlist_length - 1) {
            ui->current_track_index++;
            Track *track = g_list_nth_data(ui->current_playlist, ui->current_track_index);
            if (track) {
                player_set_uri(ui->player, track->file_path);
                player_play(ui->player);
                
                gchar *label = g_strdup_printf("%s - %s", track->artist ? track->artist : "Unknown",
                                               track->title ? track->title : "Unknown");
                gtk_label_set_text(GTK_LABEL(ui->now_playing_label), label);
                g_free(label);
                
                /* Update cover art */
                ui_update_cover_art(ui, track->artist, track->album, NULL);
            }
        }
    }
}

void ui_update_position(MediaPlayerUI *ui, gint64 position, gint64 duration) {
    if (!ui) return;
    
    if (duration > 0) {
        gdouble value = (gdouble)position / (gdouble)duration * 100.0;
        
        /* Block the seek handler to prevent feedback loop */
        g_signal_handler_block(ui->seek_scale, ui->seek_handler_id);
        gtk_range_set_value(GTK_RANGE(ui->seek_scale), value);
        g_signal_handler_unblock(ui->seek_scale, ui->seek_handler_id);
        
        gchar pos_str[16], dur_str[16];
        format_time(position / GST_SECOND, pos_str, sizeof(pos_str));
        format_time(duration / GST_SECOND, dur_str, sizeof(dur_str));
        
        gchar *time_text = g_strdup_printf("%s / %s", pos_str, dur_str);
        gtk_label_set_text(GTK_LABEL(ui->time_label), time_text);
        g_free(time_text);
    }
}

void ui_free(MediaPlayerUI *ui) {
    if (!ui) return;
    
    /* Save current volume preference */
    if (ui->database && ui->database->db && ui->player) {
        gdouble volume = player_get_volume(ui->player);
        gchar *volume_str = g_strdup_printf("%.6f", volume);
        database_set_preference(ui->database, "volume", volume_str);
        g_free(volume_str);
    }
    
    /* Free podcast manager */
    if (ui->podcast_manager) {
        podcast_manager_free(ui->podcast_manager);
    }
    
    /* Free video view */
    if (ui->video_view) {
        video_view_free(ui->video_view);
    }
    
    if (ui->source_manager) {
        source_manager_free(ui->source_manager);
    }
    
    if (ui->coverart_manager) {
        coverart_manager_free(ui->coverart_manager);
    }
    
    if (ui->artist_model) browser_model_free(ui->artist_model);
    if (ui->album_model) browser_model_free(ui->album_model);
    if (ui->genre_model) browser_model_free(ui->genre_model);
    
    if (ui->artist_browser) browser_view_free(ui->artist_browser);
    if (ui->album_browser) browser_view_free(ui->album_browser);
    if (ui->genre_browser) browser_view_free(ui->genre_browser);
    
    g_free(ui);
}

/* Preferences dialog response data */
typedef struct {
    MediaPlayerUI *ui;
    GtkWidget *hours_spin;
    GtkWidget *mins_spin;
    GtkWidget *dialog;
} PrefsDialogData;

static void on_prefs_dialog_response(GtkWidget *button, PrefsDialogData *data) {
    if (data && data->ui && data->ui->database && data->ui->database->db) {
        /* Calculate total minutes from hours and minutes */
        gint hours = (gint)gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->hours_spin));
        gint mins = (gint)gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->mins_spin));
        gint total_minutes = hours * 60 + mins;
        
        /* Enforce minimum of 15 minutes (unless disabled with 0) */
        if (total_minutes > 0 && total_minutes < 15) {
            total_minutes = 15;
        }
        
        gchar *minutes_str = g_strdup_printf("%d", total_minutes);
        
        gboolean success = database_set_preference(data->ui->database, "podcast_update_interval_minutes", minutes_str);
        if (success) {
            if (total_minutes == 0) {
                g_print("Preferences saved: Podcast auto-update disabled\n");
            } else {
                g_print("Preferences saved: Podcast update interval set to %d hour(s) %d minute(s)\n", 
                        total_minutes / 60, total_minutes % 60);
            }
            
            /* Restart auto-update timer with new interval */
            if (data->ui->podcast_manager) {
                podcast_manager_start_auto_update(data->ui->podcast_manager, total_minutes);
            }
        } else {
            g_printerr("Failed to save preferences\n");
        }
        
        g_free(minutes_str);
    }
    
    gtk_window_destroy(GTK_WINDOW(data->dialog));
    g_free(data);
}

static void on_prefs_dialog_cancel(GtkWidget *button, PrefsDialogData *data) {
    gtk_window_destroy(GTK_WINDOW(data->dialog));
    g_free(data);
}

/* Preferences dialog */
void ui_show_preferences_dialog(MediaPlayerUI *ui) {
    if (!ui) {
        g_printerr("ui_show_preferences_dialog: ui is NULL\n");
        return;
    }
    
    if (!ui->database) {
        g_printerr("ui_show_preferences_dialog: ui->database is NULL\n");
        return;
    }
    
    /* Create window for preferences (GTK4 doesn't have GtkDialog with run) */
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Preferences");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(ui->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    
    /* Main content box */
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(main_box, 10);
    gtk_widget_set_margin_end(main_box, 10);
    gtk_widget_set_margin_top(main_box, 10);
    gtk_widget_set_margin_bottom(main_box, 10);
    gtk_window_set_child(GTK_WINDOW(dialog), main_box);
    
    /* Create notebook for tabbed preferences */
    GtkWidget *notebook = gtk_notebook_new();
    gtk_widget_set_vexpand(notebook, TRUE);
    gtk_box_append(GTK_BOX(main_box), notebook);
    
    /* Podcast preferences tab */
    GtkWidget *podcast_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(podcast_box, 10);
    gtk_widget_set_margin_end(podcast_box, 10);
    gtk_widget_set_margin_top(podcast_box, 10);
    gtk_widget_set_margin_bottom(podcast_box, 10);
    
    /* Update interval setting */
    GtkWidget *update_frame = gtk_frame_new("RSS Feed Updates");
    gtk_box_append(GTK_BOX(podcast_box), update_frame);
    
    GtkWidget *update_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(update_box, 10);
    gtk_widget_set_margin_end(update_box, 10);
    gtk_widget_set_margin_top(update_box, 10);
    gtk_widget_set_margin_bottom(update_box, 10);
    gtk_frame_set_child(GTK_FRAME(update_frame), update_box);
    
    GtkWidget *update_label_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(update_box), update_label_box);
    
    GtkWidget *update_label = gtk_label_new("Check for new episodes every:");
    gtk_box_append(GTK_BOX(update_label_box), update_label);
    
    /* Load current preference in minutes - with validation */
    gint current_minutes = 1440;  /* default: 24 hours */
    if (ui->database && ui->database->db) {
        current_minutes = database_get_preference_int(ui->database, "podcast_update_interval_minutes", 1440);
    }
    
    /* Calculate hours and remaining minutes */
    gint current_hours = current_minutes / 60;
    gint current_mins = current_minutes % 60;
    
    /* Spin button for hours (0-168 = up to 1 week) */
    GtkWidget *hours_spin = gtk_spin_button_new_with_range(0, 168, 1);
    gtk_widget_set_size_request(hours_spin, 70, -1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(hours_spin), (gdouble)current_hours);
    gtk_box_append(GTK_BOX(update_label_box), hours_spin);
    
    GtkWidget *hours_label = gtk_label_new("hour(s)");
    gtk_box_append(GTK_BOX(update_label_box), hours_label);
    
    /* Spin button for minutes (0-59) */
    GtkWidget *mins_spin = gtk_spin_button_new_with_range(0, 59, 5);
    gtk_widget_set_size_request(mins_spin, 70, -1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mins_spin), (gdouble)current_mins);
    gtk_box_append(GTK_BOX(update_label_box), mins_spin);
    
    GtkWidget *mins_label = gtk_label_new("min(s)");
    gtk_box_append(GTK_BOX(update_label_box), mins_label);
    
    GtkWidget *help_label = gtk_label_new("Banshee will automatically check for new podcast episodes at this interval.\nMinimum: 15 minutes. Set to 0 hours 0 minutes to disable.");
    gtk_label_set_wrap(GTK_LABEL(help_label), TRUE);
    gtk_widget_set_halign(help_label, GTK_ALIGN_START);
    gtk_widget_add_css_class(help_label, "dim-label");
    gtk_box_append(GTK_BOX(update_box), help_label);
    
    /* Add podcast tab to notebook */
    GtkWidget *podcast_label = gtk_label_new("Podcasts");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), podcast_box, podcast_label);
    
    /* General preferences tab (for future expansion) */
    GtkWidget *general_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(general_box, 10);
    gtk_widget_set_margin_end(general_box, 10);
    gtk_widget_set_margin_top(general_box, 10);
    gtk_widget_set_margin_bottom(general_box, 10);
    
    GtkWidget *general_label_widget = gtk_label_new("General settings will appear here.");
    gtk_box_append(GTK_BOX(general_box), general_label_widget);
    
    GtkWidget *general_label = gtk_label_new("General");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), general_box, general_label);
    
    /* Add button box at bottom */
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(main_box), button_box);
    
    GtkWidget *cancel_button = gtk_button_new_with_label("Cancel");
    gtk_box_append(GTK_BOX(button_box), cancel_button);
    
    GtkWidget *ok_button = gtk_button_new_with_label("OK");
    gtk_widget_add_css_class(ok_button, "suggested-action");
    gtk_box_append(GTK_BOX(button_box), ok_button);
    
    /* Create data struct for callbacks */
    PrefsDialogData *data = g_new0(PrefsDialogData, 1);
    data->ui = ui;
    data->hours_spin = hours_spin;
    data->mins_spin = mins_spin;
    data->dialog = dialog;
    
    g_signal_connect(ok_button, "clicked", G_CALLBACK(on_prefs_dialog_response), data);
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_prefs_dialog_cancel), data);
    
    /* GTK4: widgets are visible by default */
    gtk_window_present(GTK_WINDOW(dialog));
}

/* Cover art update functions */
static void ui_update_cover_art(MediaPlayerUI *ui, const gchar *artist, const gchar *album, const gchar *podcast_image_url) {
    if (!ui || !ui->header_cover_art) return;
    
    /* Try podcast image first if provided */
    if (podcast_image_url && strlen(podcast_image_url) > 0) {
        coverart_widget_set_from_url(ui->header_cover_art, podcast_image_url);
        return;
    }
    
    /* Try album art for music */
    if (artist && album) {
        gboolean has_art = coverart_widget_set_from_album(ui->header_cover_art, ui->coverart_manager, artist, album);
        if (has_art) return;
    }
    
    /* Fall back to default image */
    coverart_widget_set_default(ui->header_cover_art);
}

void ui_update_now_playing(MediaPlayerUI *ui) {
    if (!ui) return;
    
    /* Get current track information */
    Source *active_source = source_manager_get_active(ui->source_manager);
    if (!active_source) {
        ui_update_cover_art(ui, NULL, NULL, NULL);
        return;
    }
    
    if (active_source->type == SOURCE_TYPE_RADIO) {
        /* Radio station - no cover art typically */
        ui_update_cover_art(ui, NULL, NULL, NULL);
    } else {
        /* Music track - get current selection using GTK4 */
        BansheeTrackObject *track_obj = gtk_single_selection_get_selected_item(ui->track_selection);
        if (track_obj) {
            gint track_id = banshee_track_object_get_id(track_obj);
            
            Track *track = database_get_track(ui->database, track_id);
            if (track) {
                ui_update_cover_art(ui, track->artist, track->album, NULL);
                database_free_track(track);
            } else {
                ui_update_cover_art(ui, NULL, NULL, NULL);
            }
        } else {
            ui_update_cover_art(ui, NULL, NULL, NULL);
        }
    }
}

void ui_update_now_playing_podcast(MediaPlayerUI *ui, const gchar *podcast_title, const gchar *episode_title, const gchar *image_url) {
    if (!ui) return;
    
    /* Update now playing label */
    if (podcast_title && episode_title) {
        gchar *label = g_strdup_printf("%s - %s", podcast_title, episode_title);
        gtk_label_set_text(GTK_LABEL(ui->now_playing_label), label);
        g_free(label);
    } else if (episode_title) {
        gtk_label_set_text(GTK_LABEL(ui->now_playing_label), episode_title);
    } else {
        gtk_label_set_text(GTK_LABEL(ui->now_playing_label), "Podcast Episode");
    }
    
    /* Update cover art */
    ui_update_cover_art(ui, NULL, NULL, image_url);
}

void ui_update_now_playing_video(MediaPlayerUI *ui, const gchar *video_title) {
    if (!ui) return;
    
    /* Update now playing label with video icon prefix */
    if (video_title) {
        gchar *label = g_strdup_printf("🎬 %s", video_title);
        gtk_label_set_text(GTK_LABEL(ui->now_playing_label), label);
        g_free(label);
    } else {
        gtk_label_set_text(GTK_LABEL(ui->now_playing_label), "🎬 Video");
    }
}
