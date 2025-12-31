#include "ui.h"
#include "source.h"
#include "browser.h"
#include "coverart.h"
#include "smartplaylist.h"
#include "radio.h"
#include "import.h"
#include "albumview.h"
#include <string.h>

enum {
    COL_ID = 0,
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
static void on_source_selected(GtkTreeSelection *selection, gpointer user_data);
static void on_browser_selection_changed(GtkTreeSelection *selection, gpointer user_data);
static void ui_internal_update_track_list(MediaPlayerUI *ui);
static void ui_update_track_list_with_tracks(MediaPlayerUI *ui, GList *tracks);
static void ui_show_radio_stations(MediaPlayerUI *ui);
static void on_import_media_clicked(GtkMenuItem *item, gpointer user_data);
static void on_search_changed(GtkSearchEntry *entry, gpointer user_data);
static void on_preferences_clicked(GtkMenuItem *item, gpointer user_data);

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

static GtkWidget* create_hamburger_menu(MediaPlayerUI *ui) {
    GtkWidget *menu = gtk_menu_new();
    
    /* Media section */
    GtkWidget *new_playlist_item = gtk_menu_item_new_with_label("New Playlist");
    GtkWidget *new_smart_playlist_item = gtk_menu_item_new_with_label("New Smart Playlist...");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), new_playlist_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), new_smart_playlist_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    
    GtkWidget *import_item = gtk_menu_item_new_with_label("Import Media...");
    GtkWidget *import_playlist_item = gtk_menu_item_new_with_label("Import Playlist...");
    GtkWidget *open_location_item = gtk_menu_item_new_with_label("Open Location...");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), import_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), import_playlist_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), open_location_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    
    /* Connect import handler */
    g_signal_connect(import_item, "activate", G_CALLBACK(on_import_media_clicked), ui);
    
    GtkWidget *add_station_item = gtk_menu_item_new_with_label("Add Station");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), add_station_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    
    GtkWidget *preferences_item = gtk_menu_item_new_with_label("Preferences");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), preferences_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    
    /* Connect preferences handler */
    g_signal_connect(preferences_item, "activate", G_CALLBACK(on_preferences_clicked), ui);
    
    GtkWidget *about_item = gtk_menu_item_new_with_label("About Banshee");
    GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), about_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit_item);
    
    g_signal_connect(quit_item, "activate", G_CALLBACK(gtk_main_quit), NULL);
    
    gtk_widget_show_all(menu);
    return menu;
}

static void on_volume_changed(GtkRange *range, gpointer user_data) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    gdouble value = gtk_range_get_value(range);
    player_set_volume(ui->player, value / 100.0);
}

static void on_import_media_clicked(GtkMenuItem *item, gpointer user_data) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Select Music Folder",
        GTK_WINDOW(ui->window),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);
    
    /* Set default to Music folder */
    const gchar *home = g_get_home_dir();
    gchar *music_dir = g_build_filename(home, "Music", NULL);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), music_dir);
    g_free(music_dir);
    
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT) {
        gchar *folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_widget_destroy(dialog);
        
        /* Show progress window */
        GtkWidget *progress_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(progress_window), "Importing Media");
        gtk_window_set_transient_for(GTK_WINDOW(progress_window), GTK_WINDOW(ui->window));
        gtk_window_set_modal(GTK_WINDOW(progress_window), TRUE);
        gtk_window_set_default_size(GTK_WINDOW(progress_window), 400, 100);
        gtk_container_set_border_width(GTK_CONTAINER(progress_window), 20);
        
        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_container_add(GTK_CONTAINER(progress_window), vbox);
        
        GtkWidget *label = gtk_label_new("Scanning for media files...");
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
        
        GtkWidget *spinner = gtk_spinner_new();
        gtk_spinner_start(GTK_SPINNER(spinner));
        gtk_box_pack_start(GTK_BOX(vbox), spinner, FALSE, FALSE, 0);
        
        gtk_widget_show_all(progress_window);
        
        /* Process events to show the window */
        while (gtk_events_pending()) {
            gtk_main_iteration();
        }
        
        /* Import media and extract cover art */
        import_media_from_directory_with_covers(folder, ui->database, ui->coverart_manager);
        
        g_free(folder);
        gtk_widget_destroy(progress_window);
        
        /* Refresh the track list and album view */
        ui_internal_update_track_list(ui);
        
        /* Refresh album view if an artist is selected */
        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->artist_browser));
        GtkTreeModel *model;
        GtkTreeIter iter;
        if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
            gchar *artist;
            gtk_tree_model_get(model, &iter, 0, &artist, -1);
            if (artist) {
                album_view_set_artist(ui->album_view, artist);
                g_free(artist);
            }
        }
    } else {
        gtk_widget_destroy(dialog);
    }
}

static void on_volume_button_clicked(GtkButton *button, gpointer user_data) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    GtkWidget *popover = gtk_popover_new(GTK_WIDGET(button));
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    
    GtkWidget *label = gtk_label_new("Volume");
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    
    ui->volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0, 100, 1);
    gtk_widget_set_size_request(ui->volume_scale, -1, 120);
    gtk_range_set_inverted(GTK_RANGE(ui->volume_scale), TRUE);
    gtk_range_set_value(GTK_RANGE(ui->volume_scale), 50);
    gtk_scale_set_draw_value(GTK_SCALE(ui->volume_scale), TRUE);
    gtk_scale_set_value_pos(GTK_SCALE(ui->volume_scale), GTK_POS_BOTTOM);
    g_signal_connect(ui->volume_scale, "value-changed", G_CALLBACK(on_volume_changed), ui);
    gtk_box_pack_start(GTK_BOX(box), ui->volume_scale, TRUE, TRUE, 0);
    
    gtk_container_add(GTK_CONTAINER(popover), box);
    gtk_widget_show_all(box);
    gtk_popover_popup(GTK_POPOVER(popover));
}

static GtkWidget* create_headerbar(MediaPlayerUI *ui) {
    GtkWidget *headerbar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(headerbar), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(headerbar), "Banshee");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(headerbar), "Media Player");
    
    /* Left side - playback controls */
    GtkWidget *controls_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(controls_box), "linked");
    
    ui->prev_button = gtk_button_new_from_icon_name("media-skip-backward-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(ui->prev_button, "Previous");
    g_signal_connect(ui->prev_button, "clicked", G_CALLBACK(ui_on_prev_clicked), ui);
    gtk_box_pack_start(GTK_BOX(controls_box), ui->prev_button, FALSE, FALSE, 0);
    
    ui->play_button = gtk_button_new_from_icon_name("media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(ui->play_button, "Play");
    g_signal_connect(ui->play_button, "clicked", G_CALLBACK(ui_on_play_clicked), ui);
    gtk_box_pack_start(GTK_BOX(controls_box), ui->play_button, FALSE, FALSE, 0);
    
    ui->pause_button = gtk_button_new_from_icon_name("media-playback-pause-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(ui->pause_button, "Pause");
    g_signal_connect(ui->pause_button, "clicked", G_CALLBACK(ui_on_pause_clicked), ui);
    gtk_box_pack_start(GTK_BOX(controls_box), ui->pause_button, FALSE, FALSE, 0);
    
    ui->next_button = gtk_button_new_from_icon_name("media-skip-forward-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(ui->next_button, "Next");
    g_signal_connect(ui->next_button, "clicked", G_CALLBACK(ui_on_next_clicked), ui);
    gtk_box_pack_start(GTK_BOX(controls_box), ui->next_button, FALSE, FALSE, 0);
    
    gtk_header_bar_pack_start(GTK_HEADER_BAR(headerbar), controls_box);
    
    /* Center - progress bar and media info */
    GtkWidget *media_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_size_request(media_box, 400, -1);
    
    ui->seek_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_scale_set_draw_value(GTK_SCALE(ui->seek_scale), FALSE);
    g_signal_connect(ui->seek_scale, "value-changed", G_CALLBACK(on_seek_changed), ui);
    gtk_widget_set_size_request(ui->seek_scale, 350, -1);
    gtk_box_pack_start(GTK_BOX(media_box), ui->seek_scale, FALSE, FALSE, 0);
    
    GtkWidget *info_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    ui->now_playing_label = gtk_label_new("No track playing");
    gtk_widget_set_halign(ui->now_playing_label, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(ui->now_playing_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_size_request(ui->now_playing_label, 250, -1);
    gtk_box_pack_start(GTK_BOX(info_row), ui->now_playing_label, TRUE, TRUE, 0);
    
    ui->time_label = gtk_label_new("00:00 / 00:00");
    gtk_box_pack_end(GTK_BOX(info_row), ui->time_label, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(media_box), info_row, FALSE, FALSE, 0);
    gtk_header_bar_set_custom_title(GTK_HEADER_BAR(headerbar), media_box);
    
    /* Right side - volume and menu */
    GtkWidget *volume_button = gtk_button_new_from_icon_name("audio-volume-high-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(volume_button, "Volume");
    g_signal_connect(volume_button, "clicked", G_CALLBACK(on_volume_button_clicked), ui);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(headerbar), volume_button);
    
    /* Hamburger menu button */
    GtkWidget *menu_button = gtk_menu_button_new();
    gtk_button_set_image(GTK_BUTTON(menu_button),
        gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_widget_set_tooltip_text(menu_button, "Menu");
    
    GtkWidget *menu = create_hamburger_menu(ui);
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(menu_button), menu);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(headerbar), menu_button);
    
    return headerbar;
}

static void init_source_manager(MediaPlayerUI *ui) {
    ui->source_manager = source_manager_new(ui->database);
    source_manager_add_default_sources(ui->source_manager);
    
    /* Create sidebar with source tree */
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, 180, -1);
    
    ui->sidebar_treeview = gtk_tree_view_new_with_model(
        GTK_TREE_MODEL(ui->source_manager->tree_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(ui->sidebar_treeview), FALSE);
    
    /* Icon column */
    GtkCellRenderer *icon_renderer = gtk_cell_renderer_pixbuf_new();
    GtkCellRenderer *text_renderer = gtk_cell_renderer_text_new();
    
    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start(column, icon_renderer, FALSE);
    gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
    gtk_tree_view_column_add_attribute(column, icon_renderer, "icon-name", 1);
    gtk_tree_view_column_add_attribute(column, text_renderer, "text", 0);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ui->sidebar_treeview), column);
    
    /* Connect selection signal */
    GtkTreeSelection *selection = gtk_tree_view_get_selection(
        GTK_TREE_VIEW(ui->sidebar_treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    g_signal_connect(selection, "changed", G_CALLBACK(on_source_selected), ui);
    
    gtk_container_add(GTK_CONTAINER(scrolled), ui->sidebar_treeview);
    ui->sidebar = scrolled;
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
    gtk_box_pack_start(GTK_BOX(ui->browser_container),
                      browser_view_get_widget(ui->artist_browser), TRUE, TRUE, 0);
    
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
    
    /* Block track selection signal while updating */
    GtkTreeSelection *track_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->track_listview));
    g_signal_handler_block(track_sel, ui->track_selection_handler_id);
    
    /* Filter tracks by album */
    GList *tracks = database_get_tracks_by_album(ui->database, artist, album);
    ui_update_track_list_with_tracks(ui, tracks);
    g_list_free_full(tracks, (GDestroyNotify)database_free_track);
    
    /* Unblock track selection signal */
    g_signal_handler_unblock(track_sel, ui->track_selection_handler_id);
}

static void on_chapter_seek(gpointer user_data, gdouble time) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    
    /* Seek to chapter start time */
    gint64 seek_pos = (gint64)(time * GST_SECOND);
    player_seek(ui->player, seek_pos);
    
    g_print("Seeking to chapter at %.1f seconds\n", time);
}

static void on_chapters_button_clicked(GtkWidget *button, gpointer user_data) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    (void)button;
    
    if (!ui->current_chapters) {
        return;
    }
    
    /* Create popover if it doesn't exist */
    if (!ui->chapter_popover) {
        ui->chapter_view = chapter_view_new();
        chapter_view_set_seek_callback(ui->chapter_view, on_chapter_seek, ui);
        
        ui->chapter_popover = gtk_popover_new(ui->chapters_button);
        gtk_container_add(GTK_CONTAINER(ui->chapter_popover), chapter_view_get_widget(ui->chapter_view));
        gtk_widget_set_size_request(chapter_view_get_widget(ui->chapter_view), 300, 400);
    }
    
    /* Update chapters and show popover */
    chapter_view_set_chapters(ui->chapter_view, ui->current_chapters);
    gtk_widget_show_all(ui->chapter_popover);
    gtk_popover_popup(GTK_POPOVER(ui->chapter_popover));
}

static void on_funding_url_clicked(GtkWidget *button, gpointer user_data) {
    (void)user_data;
    const gchar *url = (const gchar *)g_object_get_data(G_OBJECT(button), "funding_url");
    if (url) {
        gchar *command = g_strdup_printf("xdg-open '%s'", url);
        g_spawn_command_line_async(command, NULL);
        g_free(command);
        g_print("Opening funding URL: %s\n", url);
    }
}

static void on_transcript_button_clicked(GtkWidget *button, gpointer user_data) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    (void)button;
    
    if (!ui->current_transcript_url) {
        return;
    }
    
    /* Create popover if it doesn't exist */
    if (!ui->transcript_popover) {
        ui->transcript_view = transcript_view_new();
        
        ui->transcript_popover = gtk_popover_new(ui->transcript_button);
        gtk_container_add(GTK_CONTAINER(ui->transcript_popover), transcript_view_get_widget(ui->transcript_view));
        gtk_widget_set_size_request(transcript_view_get_widget(ui->transcript_view), 500, 600);
    }
    
    /* Load transcript if not already loaded */
    transcript_view_load_from_url(ui->transcript_view, ui->current_transcript_url, ui->current_transcript_type);
    
    gtk_widget_show_all(ui->transcript_popover);
    gtk_popover_popup(GTK_POPOVER(ui->transcript_popover));
}

static void on_funding_button_clicked(GtkWidget *button, gpointer user_data) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    (void)button;
    
    if (!ui->current_funding) {
        return;
    }
    
    /* Create popover if it doesn't exist */
    if (!ui->funding_popover) {
        GtkWidget *funding_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_start(funding_box, 10);
        gtk_widget_set_margin_end(funding_box, 10);
        gtk_widget_set_margin_top(funding_box, 10);
        gtk_widget_set_margin_bottom(funding_box, 10);
        
        /* Title */
        GtkWidget *title = gtk_label_new("Support This Podcast");
        PangoAttrList *attrs = pango_attr_list_new();
        pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
        pango_attr_list_insert(attrs, pango_attr_scale_new(PANGO_SCALE_LARGE));
        gtk_label_set_attributes(GTK_LABEL(title), attrs);
        pango_attr_list_unref(attrs);
        gtk_box_pack_start(GTK_BOX(funding_box), title, FALSE, FALSE, 0);
        
        /* Add funding options */
        for (GList *l = ui->current_funding; l != NULL; l = l->next) {
            PodcastFunding *funding = (PodcastFunding *)l->data;
            
            GtkWidget *funding_button = gtk_button_new();
            gtk_button_set_relief(GTK_BUTTON(funding_button), GTK_RELIEF_NONE);
            
            GtkWidget *funding_label_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
            
            /* Platform name */
            GtkWidget *platform_label = gtk_label_new(funding->platform ? funding->platform : "Support");
            PangoAttrList *platform_attrs = pango_attr_list_new();
            pango_attr_list_insert(platform_attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
            gtk_label_set_attributes(GTK_LABEL(platform_label), platform_attrs);
            pango_attr_list_unref(platform_attrs);
            gtk_box_pack_start(GTK_BOX(funding_label_box), platform_label, FALSE, FALSE, 0);
            
            /* Message */
            if (funding->message) {
                GtkWidget *message_label = gtk_label_new(funding->message);
                gtk_label_set_line_wrap(GTK_LABEL(message_label), TRUE);
                gtk_widget_set_opacity(message_label, 0.7);
                gtk_box_pack_start(GTK_BOX(funding_label_box), message_label, FALSE, FALSE, 0);
            }
            
            gtk_container_add(GTK_CONTAINER(funding_button), funding_label_box);
            
            /* Store URL in button data */
            g_object_set_data_full(G_OBJECT(funding_button), "funding_url", 
                                  g_strdup(funding->url), g_free);
            
            g_signal_connect(funding_button, "clicked", G_CALLBACK(on_funding_url_clicked), NULL);
            
            gtk_box_pack_start(GTK_BOX(funding_box), funding_button, FALSE, FALSE, 0);
        }
        
        ui->funding_popover = gtk_popover_new(ui->funding_button);
        gtk_container_add(GTK_CONTAINER(ui->funding_popover), funding_box);
        gtk_widget_set_size_request(funding_box, 250, -1);
    }
    
    gtk_widget_show_all(ui->funding_popover);
    gtk_popover_popup(GTK_POPOVER(ui->funding_popover));
}

static void on_podcast_episode_play(gpointer user_data, const gchar *uri, const gchar *title, GList *chapters, 
                                   const gchar *transcript_url, const gchar *transcript_type, GList *funding) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    
    /* Stop current playback and load podcast episode */
    player_stop(ui->player);
    player_set_uri(ui->player, uri);
    player_play(ui->player);
    
    /* Update UI */
    gchar *status = g_strdup_printf("Playing: %s", title);
    gtk_label_set_text(GTK_LABEL(ui->now_playing_label), status);
    g_free(status);
    
    /* Store chapters and enable chapters button */
    if (ui->current_chapters) {
        g_list_free_full(ui->current_chapters, (GDestroyNotify)podcast_chapter_free);
        ui->current_chapters = NULL;
    }
    
    if (chapters && g_list_length(chapters) > 0) {
        /* Copy chapters for UI use */
        for (GList *l = chapters; l != NULL; l = l->next) {
            PodcastChapter *orig = (PodcastChapter *)l->data;
            PodcastChapter *copy = g_new0(PodcastChapter, 1);
            copy->start_time = orig->start_time;
            copy->title = g_strdup(orig->title);
            copy->img = g_strdup(orig->img);
            copy->url = g_strdup(orig->url);
            ui->current_chapters = g_list_append(ui->current_chapters, copy);
        }
        
        gtk_widget_show(ui->chapters_button);
        gtk_widget_set_sensitive(ui->chapters_button, TRUE);
        g_print("Episode has %d chapters - click 'Chapters' button to view\n", g_list_length(chapters));
    } else {
        gtk_widget_hide(ui->chapters_button);
        gtk_widget_set_sensitive(ui->chapters_button, FALSE);
    }
    
    /* Handle transcript */
    g_free(ui->current_transcript_url);
    g_free(ui->current_transcript_type);
    ui->current_transcript_url = g_strdup(transcript_url);
    ui->current_transcript_type = g_strdup(transcript_type);
    
    if (transcript_url && strlen(transcript_url) > 0) {
        gtk_widget_show(ui->transcript_button);
        gtk_widget_set_sensitive(ui->transcript_button, TRUE);
        g_print("Episode has transcript - click 'Transcript' button to view\n");
    } else {
        gtk_widget_hide(ui->transcript_button);
        gtk_widget_set_sensitive(ui->transcript_button, FALSE);
    }
    
    /* Handle funding */
    if (ui->current_funding) {
        g_list_free_full(ui->current_funding, (GDestroyNotify)podcast_funding_free);
        ui->current_funding = NULL;
    }
    
    /* Recreate funding popover for new episode */
    if (ui->funding_popover) {
        gtk_widget_destroy(ui->funding_popover);
        ui->funding_popover = NULL;
    }
    
    if (funding && g_list_length(funding) > 0) {
        /* Copy funding list for UI use */
        for (GList *l = funding; l != NULL; l = l->next) {
            PodcastFunding *orig = (PodcastFunding *)l->data;
            PodcastFunding *copy = g_new0(PodcastFunding, 1);
            copy->url = g_strdup(orig->url);
            copy->message = g_strdup(orig->message);
            copy->platform = g_strdup(orig->platform);
            ui->current_funding = g_list_append(ui->current_funding, copy);
        }
        
        gtk_widget_show(ui->funding_button);
        gtk_widget_set_sensitive(ui->funding_button, TRUE);
        g_print("Episode has %d funding options - click 'Support' button to view\n", g_list_length(funding));
    } else {
        gtk_widget_hide(ui->funding_button);
        gtk_widget_set_sensitive(ui->funding_button, FALSE);
    }
}

static GtkWidget* create_track_list(MediaPlayerUI *ui) {
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    
    ui->track_store = gtk_list_store_new(NUM_COLS,
                                          G_TYPE_INT,     /* ID */
                                          G_TYPE_STRING,  /* Title */
                                          G_TYPE_STRING,  /* Artist */
                                          G_TYPE_STRING,  /* Album */
                                          G_TYPE_STRING); /* Duration */
    
    ui->track_listview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ui->track_store));
    g_object_unref(ui->track_store);
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    
    GtkTreeViewColumn *col_title = gtk_tree_view_column_new_with_attributes(
        "Title", renderer, "text", COL_TITLE, NULL);
    gtk_tree_view_column_set_resizable(col_title, TRUE);
    gtk_tree_view_column_set_expand(col_title, TRUE);
    gtk_tree_view_column_set_min_width(col_title, 200);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ui->track_listview), col_title);
    
    GtkTreeViewColumn *col_artist = gtk_tree_view_column_new_with_attributes(
        "Artist", renderer, "text", COL_ARTIST, NULL);
    gtk_tree_view_column_set_resizable(col_artist, TRUE);
    gtk_tree_view_column_set_expand(col_artist, TRUE);
    gtk_tree_view_column_set_min_width(col_artist, 150);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ui->track_listview), col_artist);
    
    GtkTreeViewColumn *col_album = gtk_tree_view_column_new_with_attributes(
        "Album", renderer, "text", COL_ALBUM, NULL);
    gtk_tree_view_column_set_resizable(col_album, TRUE);
    gtk_tree_view_column_set_expand(col_album, TRUE);
    gtk_tree_view_column_set_min_width(col_album, 150);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ui->track_listview), col_album);
    
    GtkTreeViewColumn *col_duration = gtk_tree_view_column_new_with_attributes(
        "Duration", renderer, "text", COL_DURATION, NULL);
    gtk_tree_view_column_set_min_width(col_duration, 60);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ui->track_listview), col_duration);
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->track_listview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    ui->track_selection_handler_id = g_signal_connect(selection, "changed", G_CALLBACK(ui_on_track_selected), ui);
    
    gtk_container_add(GTK_CONTAINER(scrolled), ui->track_listview);
    return scrolled;
}


static GtkWidget* create_control_box(MediaPlayerUI *ui) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(vbox, 10);
    gtk_widget_set_margin_end(vbox, 10);
    gtk_widget_set_margin_top(vbox, 5);
    gtk_widget_set_margin_bottom(vbox, 10);
    
    /* Search row */
    GtkWidget *search_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    GtkWidget *search_label = gtk_label_new("Search:");
    gtk_box_pack_start(GTK_BOX(search_row), search_label, FALSE, FALSE, 0);
    
    ui->search_entry = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ui->search_entry), "Search library...");
    g_signal_connect(ui->search_entry, "search-changed", G_CALLBACK(on_search_changed), ui);
    gtk_box_pack_start(GTK_BOX(search_row), ui->search_entry, TRUE, TRUE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), search_row, FALSE, FALSE, 0);
    
    /* Episode buttons row */
    GtkWidget *control_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    /* Spacer to push buttons to the right */
    GtkWidget *spacer = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(control_row), spacer, TRUE, TRUE, 0);
    
    /* Chapters button */
    ui->chapters_button = gtk_button_new_with_label("Chapters");
    gtk_widget_set_sensitive(ui->chapters_button, FALSE);  /* Disabled until chapters available */
    gtk_widget_hide(ui->chapters_button);  /* Hidden until podcast content is playing */
    g_signal_connect(ui->chapters_button, "clicked", G_CALLBACK(on_chapters_button_clicked), ui);
    gtk_box_pack_end(GTK_BOX(control_row), ui->chapters_button, FALSE, FALSE, 5);
    
    /* Transcript button */
    ui->transcript_button = gtk_button_new_with_label("Transcript");
    gtk_widget_set_sensitive(ui->transcript_button, FALSE);  /* Disabled until transcript available */
    gtk_widget_hide(ui->transcript_button);  /* Hidden until podcast content is playing */
    g_signal_connect(ui->transcript_button, "clicked", G_CALLBACK(on_transcript_button_clicked), ui);
    gtk_box_pack_end(GTK_BOX(control_row), ui->transcript_button, FALSE, FALSE, 5);
    
    /* Funding button */
    ui->funding_button = gtk_button_new_with_label("Support");
    gtk_widget_set_sensitive(ui->funding_button, FALSE);  /* Disabled until funding available */
    gtk_widget_hide(ui->funding_button);  /* Hidden until podcast content is playing */
    g_signal_connect(ui->funding_button, "clicked", G_CALLBACK(on_funding_button_clicked), ui);
    gtk_box_pack_end(GTK_BOX(control_row), ui->funding_button, FALSE, FALSE, 5);
    
    gtk_box_pack_start(GTK_BOX(vbox), control_row, FALSE, FALSE, 0);
    
    return vbox;
}

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    const gchar *search_text = gtk_entry_get_text(GTK_ENTRY(entry));
    
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
                /* Check if an artist is selected */
                GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->artist_browser));
                gchar *artist = browser_model_get_selection(ui->artist_model, selection);
                
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

static void on_source_selected(GtkTreeSelection *selection, gpointer user_data) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
        Source *source = source_get_by_iter(ui->source_manager, &iter);
        
        if (source) {
            source_manager_set_active(ui->source_manager, source);
            
            /* Clear search entry when switching sources */
            gtk_entry_set_text(GTK_ENTRY(ui->search_entry), "");
            
            /* Get the content stack */
            GtkStack *content_stack = GTK_STACK(g_object_get_data(G_OBJECT(ui->window), "content_stack"));
            
            /* Block track selection signal while updating list */
            GtkTreeSelection *track_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->track_listview));
            g_signal_handler_block(track_sel, ui->track_selection_handler_id);
            
            /* Clear current track list */
            gtk_list_store_clear(ui->track_store);
            
            switch (source->type) {
                case SOURCE_TYPE_LIBRARY:
                    /* Switch to music view */
                    gtk_stack_set_visible_child_name(content_stack, "music");
                    
                    /* Update search placeholder */
                    gtk_entry_set_placeholder_text(GTK_ENTRY(ui->search_entry), "Search library...");
                    
                    /* Hide episode buttons for music library */
                    gtk_widget_hide(ui->chapters_button);
                    gtk_widget_hide(ui->transcript_button);
                    gtk_widget_hide(ui->funding_button);
                    
                    if (source->media_types & MEDIA_TYPE_VIDEO) {
                        /* Video library - no browsers, just video list */
                        gtk_widget_hide(ui->browser_container);
                        gtk_widget_hide(ui->album_container);
                        /* For now, show empty since we don't have video support yet */
                        /* In future: ui_internal_update_video_list(ui); */
                    } else {
                        /* Music library with artist browser and album view */
                        gtk_widget_show(ui->browser_container);
                        gtk_widget_show(ui->album_container);
                        browser_model_reload(ui->artist_model);
                        album_view_set_artist(ui->album_view, NULL);
                        ui_internal_update_track_list(ui);
                    }
                    /* Unblock signal after updating */
                    g_signal_handler_unblock(track_sel, ui->track_selection_handler_id);
                    break;
                    
                case SOURCE_TYPE_PLAYLIST: {
                    /* Switch to music view */
                    gtk_stack_set_visible_child_name(content_stack, "music");
                    
                    /* Update search placeholder */
                    gtk_entry_set_placeholder_text(GTK_ENTRY(ui->search_entry), "Search playlist...");
                    
                    /* Hide episode buttons for playlist */
                    gtk_widget_hide(ui->chapters_button);
                    gtk_widget_hide(ui->transcript_button);
                    gtk_widget_hide(ui->funding_button);
                    
                    /* Hide browsers for playlists */
                    gtk_widget_hide(ui->browser_container);
                    gtk_widget_hide(ui->album_container);
                    GList *tracks = database_get_playlist_tracks(ui->database, source->playlist_id);
                    ui_update_track_list_with_tracks(ui, tracks);
                    g_list_free_full(tracks, (GDestroyNotify)database_free_track);
                    /* Unblock signal after updating */
                    g_signal_handler_unblock(track_sel, ui->track_selection_handler_id);
                    break;
                }
                    
                case SOURCE_TYPE_SMART_PLAYLIST: {
                    /* Switch to music view */
                    gtk_stack_set_visible_child_name(content_stack, "music");
                    
                    /* Update search placeholder */
                    gtk_entry_set_placeholder_text(GTK_ENTRY(ui->search_entry), "Search smart playlist...");
                    
                    /* Hide episode buttons for smart playlist */
                    gtk_widget_hide(ui->chapters_button);
                    gtk_widget_hide(ui->transcript_button);
                    gtk_widget_hide(ui->funding_button);
                    
                    gtk_widget_hide(ui->browser_container);
                    gtk_widget_hide(ui->album_container);
                    SmartPlaylist *sp = (SmartPlaylist *)source->user_data;
                    if (sp) {
                        GList *tracks = smartplaylist_get_tracks(sp, ui->database);
                        ui_update_track_list_with_tracks(ui, tracks);
                        g_list_free_full(tracks, (GDestroyNotify)database_free_track);
                    }
                    /* Unblock signal after updating */
                    g_signal_handler_unblock(track_sel, ui->track_selection_handler_id);
                    break;
                }
                    
                case SOURCE_TYPE_RADIO:
                    /* Switch to music view and show radio stations */
                    gtk_stack_set_visible_child_name(content_stack, "music");
                    
                    /* Update search placeholder */
                    gtk_entry_set_placeholder_text(GTK_ENTRY(ui->search_entry), "Search stations...");
                    
                    /* Hide episode buttons for radio */
                    gtk_widget_hide(ui->chapters_button);
                    gtk_widget_hide(ui->transcript_button);
                    gtk_widget_hide(ui->funding_button);
                    
                    gtk_widget_hide(ui->browser_container);
                    gtk_widget_hide(ui->album_container);
                    ui_show_radio_stations(ui);
                    /* Unblock signal after updating */
                    g_signal_handler_unblock(track_sel, ui->track_selection_handler_id);
                    break;
                    
                case SOURCE_TYPE_PODCAST:
                    /* Switch to podcast view */
                    gtk_stack_set_visible_child_name(content_stack, "podcast");
                    
                    /* Update search placeholder */
                    gtk_entry_set_placeholder_text(GTK_ENTRY(ui->search_entry), "Search podcasts...");
                    
                    podcast_view_refresh_podcasts(ui->podcast_view);
                    
                    /* Hide global episode buttons since they're now in podcast toolbar */
                    gtk_widget_hide(ui->chapters_button);
                    gtk_widget_hide(ui->transcript_button);
                    gtk_widget_hide(ui->funding_button);
                    
                    /* Unblock signal after updating */
                    g_signal_handler_unblock(track_sel, ui->track_selection_handler_id);
                    break;
                    
                default:
                    /* Switch to music view */
                    gtk_stack_set_visible_child_name(content_stack, "music");
                    
                    /* Hide episode buttons for default sources */
                    gtk_widget_hide(ui->chapters_button);
                    gtk_widget_hide(ui->transcript_button);
                    gtk_widget_hide(ui->funding_button);
                    
                    gtk_widget_hide(ui->browser_container);
                    gtk_widget_hide(ui->album_container);
                    /* Unblock signal */
                    g_signal_handler_unblock(track_sel, ui->track_selection_handler_id);
                    break;
            }
        }
    }
}

static void on_browser_selection_changed(GtkTreeSelection *selection, gpointer user_data) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    
    /* Get the selected artist from the browser */
    gchar *artist = browser_model_get_selection(ui->artist_model, selection);
    
    /* Update album view with this artist's albums */
    album_view_set_artist(ui->album_view, artist && g_strcmp0(artist, "All Artists") != 0 ? artist : NULL);
    
    /* Block track selection signal while updating */
    GtkTreeSelection *track_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->track_listview));
    g_signal_handler_block(track_sel, ui->track_selection_handler_id);
    
    if (artist && g_strcmp0(artist, "All Artists") != 0) {
        /* Filter by artist */
        GList *tracks = database_get_tracks_by_artist(ui->database, artist);
        ui_update_track_list_with_tracks(ui, tracks);
        g_list_free_full(tracks, (GDestroyNotify)database_free_track);
    } else {
        /* Show all tracks */
        ui_internal_update_track_list(ui);
    }
    
    /* Unblock track selection signal */
    g_signal_handler_unblock(track_sel, ui->track_selection_handler_id);
    
    g_free(artist);
}

MediaPlayerUI* ui_new(MediaPlayer *player, Database *database) {
    MediaPlayerUI *ui = g_new0(MediaPlayerUI, 1);
    ui->database = database;
    ui->player = player;
    
    /* Initialize managers */
    ui->coverart_manager = coverart_manager_new();
    ui->podcast_manager = podcast_manager_new(database);
    
    /* Create window */
    ui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ui->window), "Banshee Media Player");
    gtk_window_set_default_size(GTK_WINDOW(ui->window), 1200, 700);
    gtk_window_set_icon_name(GTK_WINDOW(ui->window), "multimedia-player");
    
    /* Create and set headerbar */
    GtkWidget *headerbar = create_headerbar(ui);
    gtk_window_set_titlebar(GTK_WINDOW(ui->window), headerbar);
    
    /* Main vertical box */
    ui->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(ui->window), ui->main_box);
    
    /* Initialize source manager and sidebar */
    init_source_manager(ui);
    
    /* Initialize browsers */
    init_browsers(ui);
    
    /* Initialize podcast view */
    ui->podcast_view = podcast_view_new(ui->podcast_manager, ui->database);
    
    /* Create main horizontal paned (sidebar | content) */
    ui->main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(ui->main_paned), 180);
    gtk_box_pack_start(GTK_BOX(ui->main_box), ui->main_paned, TRUE, TRUE, 0);
    
    /* Add sidebar to left pane */
    gtk_paned_pack1(GTK_PANED(ui->main_paned), ui->sidebar, FALSE, FALSE);
    
    /* Create a stack to hold different content views */
    GtkWidget *content_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(content_stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(content_stack), 150);
    gtk_paned_pack2(GTK_PANED(ui->main_paned), content_stack, TRUE, TRUE);
    
    /* Create outer vertical paned for music library: top for browsers/albums, bottom for track list */
    GtkWidget *outer_vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_set_position(GTK_PANED(outer_vpaned), 250);
    gtk_stack_add_named(GTK_STACK(content_stack), outer_vpaned, "music");
    
    /* Create horizontal paned for artist browser and album view */
    ui->content_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(ui->content_paned), 200);
    gtk_paned_pack1(GTK_PANED(outer_vpaned), ui->content_paned, FALSE, TRUE);
    
    /* Add artist browser to left */
    gtk_paned_pack1(GTK_PANED(ui->content_paned), ui->browser_container, FALSE, TRUE);
    
    /* Add album view to right */
    gtk_paned_pack2(GTK_PANED(ui->content_paned), ui->album_container, TRUE, TRUE);
    
    /* Add track list below (spanning full width) */
    ui->content_area = create_track_list(ui);
    gtk_paned_pack2(GTK_PANED(outer_vpaned), ui->content_area, TRUE, TRUE);
    
    /* Add podcast view to stack */
    gtk_stack_add_named(GTK_STACK(content_stack), podcast_view_get_widget(ui->podcast_view), "podcast");
    
    /* Store content stack reference for switching views */
    g_object_set_data(G_OBJECT(ui->window), "content_stack", content_stack);
    
    /* Set album selection callback */
    album_view_set_selection_callback(ui->album_view, on_album_selected, ui);
    
    /* Set podcast playback callback */
    podcast_view_set_play_callback(ui->podcast_view, on_podcast_episode_play, ui);
    
    /* Initialize chapter view fields */
    ui->chapter_view = NULL;
    ui->chapter_popover = NULL;
    ui->current_chapters = NULL;
    
    /* Initialize transcript view fields */
    ui->transcript_view = NULL;
    ui->transcript_popover = NULL;
    ui->current_transcript_url = NULL;
    ui->current_transcript_type = NULL;
    
    /* Initialize funding view fields */
    ui->funding_popover = NULL;
    ui->current_funding = NULL;
    
    /* Playback controls */
    ui->control_box = create_control_box(ui);
    gtk_box_pack_start(GTK_BOX(ui->main_box), ui->control_box, FALSE, FALSE, 0);
    
    /* Status bar */
    ui->statusbar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(ui->main_box), ui->statusbar, FALSE, FALSE, 0);
    
    /* Connect window signals */
    g_signal_connect(ui->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    gtk_widget_show_all(ui->window);
    
    /* Initialize the view with the default active source (Music library) */
    Source *active = source_manager_get_active(ui->source_manager);
    if (active && active->type == SOURCE_TYPE_LIBRARY && !(active->media_types & MEDIA_TYPE_VIDEO)) {
        /* Show music library with artist browser and album view */
        gtk_widget_show(ui->browser_container);
        gtk_widget_show(ui->album_container);
        browser_model_reload(ui->artist_model);
        album_view_set_artist(ui->album_view, NULL);  /* Show all albums */
        ui_internal_update_track_list(ui);
    } else {
        /* Hide browsers and album view for other sources */
        gtk_widget_hide(ui->browser_container);
        gtk_widget_hide(ui->album_container);
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
    if (!ui) return;
    
    gtk_list_store_clear(ui->track_store);
    
    for (GList *l = tracks; l != NULL; l = l->next) {
        Track *track = (Track *)l->data;
        if (!track) continue;
        
        gchar time_str[16];
        format_time(track->duration, time_str, sizeof(time_str));
        
        GtkTreeIter iter;
        gtk_list_store_append(ui->track_store, &iter);
        gtk_list_store_set(ui->track_store, &iter,
                          COL_ID, track->id,
                          COL_TITLE, track->title,
                          COL_ARTIST, track->artist,
                          COL_ALBUM, track->album,
                          COL_DURATION, time_str,
                          -1);
    }
}

void ui_show_radio_stations(MediaPlayerUI *ui) {
    if (!ui) return;
    
    gtk_list_store_clear(ui->track_store);
    
    GList *stations = radio_station_get_all(ui->database);
    if (!stations) {
        /* Add default stations if none exist */
        stations = radio_station_get_defaults();
        for (GList *l = stations; l != NULL; l = l->next) {
            RadioStation *station = (RadioStation *)l->data;
            radio_station_save(station, ui->database);
        }
    }
    
    for (GList *l = stations; l != NULL; l = l->next) {
        RadioStation *station = (RadioStation *)l->data;
        
        gchar *bitrate_str = g_strdup_printf("%d kbps", station->bitrate);
        
        GtkTreeIter iter;
        gtk_list_store_append(ui->track_store, &iter);
        gtk_list_store_set(ui->track_store, &iter,
                          COL_ID, station->id,
                          COL_TITLE, station->name,
                          COL_ARTIST, station->genre ? station->genre : "Unknown",
                          COL_ALBUM, bitrate_str,
                          COL_DURATION, "",
                          -1);
        
        g_free(bitrate_str);
    }
    
    g_list_free_full(stations, (GDestroyNotify)radio_station_free);
}

void ui_on_track_selected(GtkTreeSelection *selection, gpointer user_data) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    GtkTreeIter iter;
    GtkTreeModel *model;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gint track_id;
        gtk_tree_model_get(model, &iter, COL_ID, &track_id, -1);
        
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
                
                /* Hide episode buttons for radio stations */
                gtk_widget_hide(ui->chapters_button);
                gtk_widget_hide(ui->transcript_button);
                gtk_widget_hide(ui->funding_button);
                
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
                
                /* Hide episode buttons for music tracks */
                gtk_widget_hide(ui->chapters_button);
                gtk_widget_hide(ui->transcript_button);
                gtk_widget_hide(ui->funding_button);
                
                database_free_track(track);
            }
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
            }
        }
    }
}

void ui_update_position(MediaPlayerUI *ui, gint64 position, gint64 duration) {
    if (!ui) return;
    
    if (duration > 0) {
        gdouble value = (gdouble)position / (gdouble)duration * 100.0;
        gtk_range_set_value(GTK_RANGE(ui->seek_scale), value);
        
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
    
    /* Free current chapters */
    if (ui->current_chapters) {
        g_list_free_full(ui->current_chapters, (GDestroyNotify)podcast_chapter_free);
    }
    
    /* Free chapter view */
    if (ui->chapter_view) {
        chapter_view_free(ui->chapter_view);
    }
    
    /* Free transcript view */
    if (ui->transcript_view) {
        transcript_view_free(ui->transcript_view);
    }
    
    g_free(ui->current_transcript_url);
    g_free(ui->current_transcript_type);
    
    /* Free current funding */
    if (ui->current_funding) {
        g_list_free_full(ui->current_funding, (GDestroyNotify)podcast_funding_free);
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

/* Callback for preferences menu item */
static void on_preferences_clicked(GtkMenuItem *item, gpointer user_data) {
    MediaPlayerUI *ui = (MediaPlayerUI *)user_data;
    ui_show_preferences_dialog(ui);
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
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Preferences",
        GTK_WINDOW(ui->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_OK", GTK_RESPONSE_OK,
        NULL);
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content_area), 10);
    
    /* Create notebook for tabbed preferences */
    GtkWidget *notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(content_area), notebook, TRUE, TRUE, 0);
    
    /* Podcast preferences tab */
    GtkWidget *podcast_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(podcast_box), 10);
    
    /* Update interval setting */
    GtkWidget *update_frame = gtk_frame_new("RSS Feed Updates");
    gtk_box_pack_start(GTK_BOX(podcast_box), update_frame, FALSE, FALSE, 0);
    
    GtkWidget *update_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(update_box), 10);
    gtk_container_add(GTK_CONTAINER(update_frame), update_box);
    
    GtkWidget *update_label_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(update_box), update_label_box, FALSE, FALSE, 0);
    
    GtkWidget *update_label = gtk_label_new("Check for new episodes every:");
    gtk_box_pack_start(GTK_BOX(update_label_box), update_label, FALSE, FALSE, 0);
    
    /* Spin button for days */
    GtkWidget *update_spin = gtk_spin_button_new_with_range(1, 30, 1);
    gtk_widget_set_size_request(update_spin, 80, -1);
    
    /* Load current preference - with validation */
    gint current_days = 7;  /* default */
    if (ui->database && ui->database->db) {
        current_days = database_get_preference_int(ui->database, "podcast_update_interval_days", 7);
    }
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(update_spin), (gdouble)current_days);
    
    gtk_box_pack_start(GTK_BOX(update_label_box), update_spin, FALSE, FALSE, 0);
    
    GtkWidget *days_label = gtk_label_new("day(s)");
    gtk_box_pack_start(GTK_BOX(update_label_box), days_label, FALSE, FALSE, 0);
    
    GtkWidget *help_label = gtk_label_new("Banshee will automatically check for new podcast episodes at this interval.");
    gtk_label_set_line_wrap(GTK_LABEL(help_label), TRUE);
    gtk_widget_set_halign(help_label, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(help_label), "dim-label");
    gtk_box_pack_start(GTK_BOX(update_box), help_label, FALSE, FALSE, 0);
    
    /* Add podcast tab to notebook */
    GtkWidget *podcast_label = gtk_label_new("Podcasts");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), podcast_box, podcast_label);
    
    /* General preferences tab (for future expansion) */
    GtkWidget *general_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(general_box), 10);
    
    GtkWidget *general_label_widget = gtk_label_new("General settings will appear here.");
    gtk_box_pack_start(GTK_BOX(general_box), general_label_widget, FALSE, FALSE, 0);
    
    GtkWidget *general_label = gtk_label_new("General");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), general_box, general_label);
    
    gtk_widget_show_all(dialog);
    
    /* Run dialog and save preferences if OK clicked */
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (response == GTK_RESPONSE_OK) {
        /* Save podcast update interval */
        gint days = (gint)gtk_spin_button_get_value(GTK_SPIN_BUTTON(update_spin));
        gchar *days_str = g_strdup_printf("%d", days);
        
        if (ui->database && ui->database->db) {
            gboolean success = database_set_preference(ui->database, "podcast_update_interval_days", days_str);
            if (success) {
                g_print("Preferences saved: Podcast update interval set to %d day(s)\n", days);
            } else {
                g_printerr("Failed to save preferences\n");
            }
        } else {
            g_printerr("Cannot save preferences: database is not available\n");
        }
        
        g_free(days_str);
    }
    
    gtk_widget_destroy(dialog);
}
