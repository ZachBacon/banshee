#include "podcastview.h"
#include "transcriptview.h"
#include <string.h>

static void on_chapters_button_clicked(GtkButton *button, gpointer user_data);
static void on_transcript_button_clicked(GtkButton *button, gpointer user_data);
static void on_support_button_clicked(GtkButton *button, gpointer user_data);
static void on_chapter_seek(gpointer user_data, gdouble time);
static void on_funding_url_clicked(GtkWidget *button, gpointer user_data);
static void on_cancel_button_clicked(GtkButton *button, gpointer user_data);

/* Download callbacks */
static void on_download_progress(gpointer user_data, gint episode_id, gdouble progress, const gchar *status);
static void on_download_complete(gpointer user_data, gint episode_id, gboolean success, const gchar *error_msg);

enum {
    PODCAST_COL_ID,
    PODCAST_COL_TITLE,
    PODCAST_COL_AUTHOR,
    PODCAST_COL_COUNT
};

enum {
    EPISODE_COL_ID,
    EPISODE_COL_TITLE,
    EPISODE_COL_DATE,
    EPISODE_COL_DURATION,
    EPISODE_COL_DOWNLOADED,
    EPISODE_COL_COUNT
};

static void on_podcast_selection_changed(GtkTreeSelection *selection, gpointer user_data) {
    PodcastView *view = (PodcastView *)user_data;
    
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gint podcast_id;
        gtk_tree_model_get(model, &iter, PODCAST_COL_ID, &podcast_id, -1);
        
        view->selected_podcast_id = podcast_id;
        podcast_view_refresh_episodes(view, podcast_id);
    }
}

static void on_add_button_clicked(GtkButton *button, gpointer user_data) {
    PodcastView *view = (PodcastView *)user_data;
    (void)button;
    podcast_view_add_subscription(view);
}

static void on_refresh_button_clicked(GtkButton *button, gpointer user_data) {
    PodcastView *view = (PodcastView *)user_data;
    (void)button;
    
    /* Update all podcast feeds from the internet */
    g_print("Refreshing podcast feeds...\n");
    podcast_manager_update_all_feeds(view->podcast_manager);
    
    /* Refresh the UI */
    podcast_view_refresh_podcasts(view);
    
    /* If a podcast is selected, refresh its episodes too */
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view->podcast_listview));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gint podcast_id;
        gtk_tree_model_get(model, &iter, PODCAST_COL_ID, &podcast_id, -1);
        podcast_view_refresh_episodes(view, podcast_id);
    }
}

static void on_download_button_clicked(GtkButton *button, gpointer user_data) {
    PodcastView *view = (PodcastView *)user_data;
    (void)button;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view->episode_listview));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gint episode_id;
        gtk_tree_model_get(model, &iter, EPISODE_COL_ID, &episode_id, -1);
        podcast_view_download_episode(view, episode_id);
    }
}

static void on_cancel_button_clicked(GtkButton *button, gpointer user_data) {
    PodcastView *view = (PodcastView *)user_data;
    (void)button;
    
    if (view->current_download_id > 0) {
        podcast_episode_cancel_download(view->podcast_manager, view->current_download_id);
        gtk_label_set_text(GTK_LABEL(view->progress_label), "Cancelling download...");
    }
}

static void on_episode_row_activated(GtkTreeView *treeview, GtkTreePath *path, 
                                     GtkTreeViewColumn *column, gpointer user_data) {
    PodcastView *view = (PodcastView *)user_data;
    (void)column;
    
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    GtkTreeIter iter;
    
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gint episode_id;
        gtk_tree_model_get(model, &iter, EPISODE_COL_ID, &episode_id, -1);
        podcast_view_play_episode(view, episode_id);
    }
}

/* Helper function to replace all occurrences of a string */
static gchar* str_replace_all(const gchar *input, const gchar *search, const gchar *replace) {
    GString *result = g_string_new("");
    const gchar *p = input;
    gsize search_len = strlen(search);
    gsize replace_len = strlen(replace);
    
    while (*p) {
        if (strncmp(p, search, search_len) == 0) {
            g_string_append_len(result, replace, replace_len);
            p += search_len;
        } else {
            g_string_append_c(result, *p);
            p++;
        }
    }
    
    gchar *output = result->str;
    g_string_free(result, FALSE);
    return output;
}

/* Helper function to strip HTML tags and decode common HTML entities */
static gchar* strip_html_and_decode(const gchar *html) {
    if (!html || strlen(html) == 0) {
        return g_strdup("");
    }
    
    /* Simple HTML tag stripper - remove everything between < and > */
    GString *result = g_string_new("");
    gboolean in_tag = FALSE;
    const gchar *p = html;
    
    while (*p) {
        if (*p == '<') {
            in_tag = TRUE;
        } else if (*p == '>') {
            in_tag = FALSE;
        } else if (!in_tag) {
            g_string_append_c(result, *p);
        }
        p++;
    }
    
    gchar *stripped = result->str;
    g_string_free(result, FALSE);
    
    /* Decode common HTML entities - must decode &amp; last to avoid double-decoding */
    gchar *temp = stripped;
    gchar *decoded = str_replace_all(temp, "&lt;", "<");
    g_free(temp);
    temp = decoded;
    decoded = str_replace_all(temp, "&gt;", ">");
    g_free(temp);
    temp = decoded;
    decoded = str_replace_all(temp, "&quot;", "\"");
    g_free(temp);
    temp = decoded;
    decoded = str_replace_all(temp, "&apos;", "'");
    g_free(temp);
    temp = decoded;
    decoded = str_replace_all(temp, "&#39;", "'");
    g_free(temp);
    temp = decoded;
    decoded = str_replace_all(temp, "&amp;", "&");
    g_free(temp);
    
    /* Remove extra whitespace */
    g_strstrip(decoded);
    
    return decoded;
}

static gboolean on_episode_query_tooltip(GtkWidget *widget, gint x, gint y, 
                                         gboolean keyboard_mode, GtkTooltip *tooltip,
                                         gpointer user_data) {
    PodcastView *view = (PodcastView *)user_data;
    GtkTreeView *treeview = GTK_TREE_VIEW(widget);
    GtkTreeModel *model;
    GtkTreePath *path = NULL;
    GtkTreeIter iter;
    
    (void)keyboard_mode;
    
    /* Get the path at the mouse position */
    if (!gtk_tree_view_get_path_at_pos(treeview, x, y, &path, NULL, NULL, NULL)) {
        return FALSE;
    }
    
    model = gtk_tree_view_get_model(treeview);
    if (!gtk_tree_model_get_iter(model, &iter, path)) {
        gtk_tree_path_free(path);
        return FALSE;
    }
    
    gint episode_id;
    gtk_tree_model_get(model, &iter, EPISODE_COL_ID, &episode_id, -1);
    gtk_tree_path_free(path);
    
    /* Get episode details from database */
    GList *episodes = database_get_podcast_episodes(view->database, view->selected_podcast_id);
    PodcastEpisode *found_episode = NULL;
    
    for (GList *l = episodes; l != NULL; l = l->next) {
        PodcastEpisode *episode = (PodcastEpisode *)l->data;
        if (episode->id == episode_id) {
            found_episode = episode;
            break;
        }
    }
    
    if (!found_episode) {
        g_list_free_full(episodes, (GDestroyNotify)podcast_episode_free);
        return FALSE;
    }
    
    /* Build tooltip markup */
    GString *markup = g_string_new("");
    
    /* Episode title (bold) - escape it for markup */
    gchar *escaped_title = g_markup_escape_text(found_episode->title, -1);
    g_string_append_printf(markup, "<b>%s</b>\n\n", escaped_title);
    g_free(escaped_title);
    
    /* Description - strip HTML and decode entities */
    if (found_episode->description && strlen(found_episode->description) > 0) {
        gchar *plain_desc = strip_html_and_decode(found_episode->description);
        
        /* Limit description length for tooltip */
        gsize desc_len = strlen(plain_desc);
        if (desc_len > 300) {
            gchar *truncated = g_strndup(plain_desc, 297);
            gchar *escaped_desc = g_markup_escape_text(truncated, -1);
            g_string_append_printf(markup, "%s...\n\n", escaped_desc);
            g_free(escaped_desc);
            g_free(truncated);
        } else if (desc_len > 0) {
            gchar *escaped_desc = g_markup_escape_text(plain_desc, -1);
            g_string_append_printf(markup, "%s\n\n", escaped_desc);
            g_free(escaped_desc);
        }
        g_free(plain_desc);
    }
    
    /* Episode metadata */
    if (found_episode->duration > 0) {
        gint hours = found_episode->duration / 3600;
        gint minutes = (found_episode->duration % 3600) / 60;
        gint seconds = found_episode->duration % 60;
        
        g_string_append(markup, "<b>Duration:</b> ");
        if (hours > 0) {
            g_string_append_printf(markup, "%dh %dm %ds\n", hours, minutes, seconds);
        } else {
            g_string_append_printf(markup, "%dm %ds\n", minutes, seconds);
        }
    }
    
    /* Season and episode number */
    if (found_episode->season || found_episode->episode_num) {
        g_string_append(markup, "<b>Episode:</b> ");
        if (found_episode->season) {
            gchar *escaped_season = g_markup_escape_text(found_episode->season, -1);
            g_string_append_printf(markup, "Season %s", escaped_season);
            g_free(escaped_season);
        }
        if (found_episode->episode_num) {
            gchar *escaped_episode = g_markup_escape_text(found_episode->episode_num, -1);
            if (found_episode->season) {
                g_string_append_printf(markup, ", Episode %s", escaped_episode);
            } else {
                g_string_append_printf(markup, "Episode %s", escaped_episode);
            }
            g_free(escaped_episode);
        }
        g_string_append(markup, "\n");
    }
    
    /* Download status */
    if (found_episode->downloaded) {
        g_string_append(markup, "<b>Status:</b> Downloaded\n");
    }
    
    /* Play position */
    if (found_episode->play_position > 0) {
        gint pos_hours = found_episode->play_position / 3600;
        gint pos_minutes = (found_episode->play_position % 3600) / 60;
        gint pos_seconds = found_episode->play_position % 60;
        
        g_string_append(markup, "<b>Position:</b> ");
        if (pos_hours > 0) {
            g_string_append_printf(markup, "%dh %dm %ds\n", pos_hours, pos_minutes, pos_seconds);
        } else {
            g_string_append_printf(markup, "%dm %ds\n", pos_minutes, pos_seconds);
        }
    }
    
    /* Podcast 2.0 features */
    if (found_episode->transcript_url) {
        g_string_append(markup, "📝 <i>Transcript available</i>\n");
    }
    if (found_episode->chapters_url || (found_episode->enclosure_url && strstr(found_episode->enclosure_url, ".mp3"))) {
        g_string_append(markup, "📑 <i>Chapters available</i>\n");
    }
    if (found_episode->funding && g_list_length(found_episode->funding) > 0) {
        g_string_append(markup, "💝 <i>Support options available</i>\n");
    }
    
    gtk_tooltip_set_markup(tooltip, markup->str);
    
    g_string_free(markup, TRUE);
    g_list_free_full(episodes, (GDestroyNotify)podcast_episode_free);
    
    return TRUE;
}

static void on_chapter_seek(gpointer user_data, gdouble time) {
    PodcastView *view = (PodcastView *)user_data;
    
    g_print("Chapter seek requested: %.1f seconds\n", time);
    
    /* Call the seek callback if registered */
    if (view->seek_callback) {
        view->seek_callback(view->seek_callback_data, time);
    } else {
        g_print("Warning: No seek callback registered\n");
    }
}

static void on_chapters_button_clicked(GtkButton *button, gpointer user_data) {
    PodcastView *view = (PodcastView *)user_data;
    (void)button;
    
    if (!view->current_chapters) {
        return;
    }
    
    /* Create popover if it doesn't exist */
    if (!view->chapter_popover) {
        view->chapter_view = chapter_view_new();
        chapter_view_set_seek_callback(view->chapter_view, on_chapter_seek, view);
        
        view->chapter_popover = gtk_popover_new(view->chapters_button);
        gtk_container_add(GTK_CONTAINER(view->chapter_popover), chapter_view_get_widget(view->chapter_view));
        gtk_widget_set_size_request(chapter_view_get_widget(view->chapter_view), 300, 400);
    }
    
    /* Update chapters and show popover */
    chapter_view_set_chapters(view->chapter_view, view->current_chapters);
    gtk_widget_show_all(view->chapter_popover);
    gtk_popover_popup(GTK_POPOVER(view->chapter_popover));
}

static void on_transcript_button_clicked(GtkButton *button, gpointer user_data) {
    PodcastView *view = (PodcastView *)user_data;
    (void)button;
    
    if (!view->current_transcript_url) {
        return;
    }
    
    /* Create popover if it doesn't exist */
    if (!view->transcript_popover) {
        TranscriptView *transcript_view = transcript_view_new();
        
        view->transcript_popover = gtk_popover_new(view->transcript_button);
        gtk_container_add(GTK_CONTAINER(view->transcript_popover), transcript_view_get_widget(transcript_view));
        gtk_widget_set_size_request(transcript_view_get_widget(transcript_view), 500, 600);
        
        /* Store transcript view in popover data for later access */
        g_object_set_data(G_OBJECT(view->transcript_popover), "transcript_view", transcript_view);
    }
    
    /* Load transcript */
    TranscriptView *transcript_view = (TranscriptView *)g_object_get_data(G_OBJECT(view->transcript_popover), "transcript_view");
    transcript_view_load_from_url(transcript_view, view->current_transcript_url, view->current_transcript_type);
    
    gtk_widget_show_all(view->transcript_popover);
    gtk_popover_popup(GTK_POPOVER(view->transcript_popover));
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

static void on_support_button_clicked(GtkButton *button, gpointer user_data) {
    PodcastView *view = (PodcastView *)user_data;
    (void)button;
    
    if (!view->current_funding) {
        return;
    }
    
    /* Create popover if it doesn't exist */
    if (!view->funding_popover) {
        GtkWidget *funding_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_start(funding_box, 10);
        gtk_widget_set_margin_end(funding_box, 10);
        gtk_widget_set_margin_top(funding_box, 10);
        gtk_widget_set_margin_bottom(funding_box, 10);
        
        GtkWidget *title_label = gtk_label_new("Support this Podcast");
        gtk_widget_set_halign(title_label, GTK_ALIGN_START);
        PangoAttrList *attrs = pango_attr_list_new();
        pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
        gtk_label_set_attributes(GTK_LABEL(title_label), attrs);
        pango_attr_list_unref(attrs);
        gtk_box_pack_start(GTK_BOX(funding_box), title_label, FALSE, FALSE, 0);
        
        /* Add funding links */
        for (GList *l = view->current_funding; l != NULL; l = l->next) {
            PodcastFunding *funding = (PodcastFunding *)l->data;
            
            GtkWidget *funding_button = gtk_button_new();
            gtk_button_set_relief(GTK_BUTTON(funding_button), GTK_RELIEF_NONE);
            
            GtkWidget *funding_label_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            
            /* Add icon if we can determine the platform */
            const gchar *icon_name = "web-browser-symbolic";  /* Default icon */
            if (g_str_has_prefix(funding->url, "https://www.patreon.com")) {
                icon_name = "applications-internet-symbolic";
            } else if (g_str_has_prefix(funding->url, "https://ko-fi.com")) {
                icon_name = "face-smile-symbolic";
            }
            
            GtkWidget *icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
            gtk_box_pack_start(GTK_BOX(funding_label_box), icon, FALSE, FALSE, 0);
            
            GtkWidget *label = gtk_label_new(funding->message ? funding->message : funding->url);
            gtk_widget_set_halign(label, GTK_ALIGN_START);
            gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
            gtk_label_set_max_width_chars(GTK_LABEL(label), 40);
            gtk_box_pack_start(GTK_BOX(funding_label_box), label, TRUE, TRUE, 0);
            
            gtk_container_add(GTK_CONTAINER(funding_button), funding_label_box);
            
            /* Store URL in button data */
            g_object_set_data_full(G_OBJECT(funding_button), "funding_url", 
                                   g_strdup(funding->url), g_free);
            
            g_signal_connect(funding_button, "clicked", G_CALLBACK(on_funding_url_clicked), NULL);
            
            gtk_box_pack_start(GTK_BOX(funding_box), funding_button, FALSE, FALSE, 0);
        }
        
        view->funding_popover = gtk_popover_new(view->support_button);
        gtk_container_add(GTK_CONTAINER(view->funding_popover), funding_box);
        gtk_widget_set_size_request(funding_box, 350, -1);
    }
    
    gtk_widget_show_all(view->funding_popover);
    gtk_popover_popup(GTK_POPOVER(view->funding_popover));
}

PodcastView* podcast_view_new(PodcastManager *manager, Database *database) {
    PodcastView *view = g_new0(PodcastView, 1);
    view->podcast_manager = manager;
    view->database = database;
    view->selected_podcast_id = -1;
    view->current_download_id = -1;
    
    /* Initialize download progress tracking */
    view->download_progress = g_hash_table_new(g_direct_hash, g_direct_equal);
    
    /* Initialize episode-specific data */
    view->chapter_view = NULL;
    view->chapter_popover = NULL;
    view->transcript_popover = NULL;
    view->funding_popover = NULL;
    view->current_chapters = NULL;
    view->current_transcript_url = NULL;
    view->current_transcript_type = NULL;
    view->current_funding = NULL;
    
    /* Initialize callbacks */
    view->play_callback = NULL;
    view->play_callback_data = NULL;
    view->seek_callback = NULL;
    view->seek_callback_data = NULL;
    
    /* Main container */
    view->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    /* Toolbar */
    GtkWidget *toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);
    
    view->add_button = GTK_WIDGET(gtk_tool_button_new(NULL, "Subscribe"));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(view->add_button), "list-add");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(view->add_button), -1);
    g_signal_connect(view->add_button, "clicked", G_CALLBACK(on_add_button_clicked), view);
    
    view->remove_button = GTK_WIDGET(gtk_tool_button_new(NULL, "Unsubscribe"));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(view->remove_button), "list-remove");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(view->remove_button), -1);
    
    view->refresh_button = GTK_WIDGET(gtk_tool_button_new(NULL, "Refresh"));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(view->refresh_button), "view-refresh");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(view->refresh_button), -1);
    g_signal_connect(view->refresh_button, "clicked", G_CALLBACK(on_refresh_button_clicked), view);
    
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);
    
    view->download_button = GTK_WIDGET(gtk_tool_button_new(NULL, "Download"));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(view->download_button), "document-save");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(view->download_button), -1);
    g_signal_connect(view->download_button, "clicked", G_CALLBACK(on_download_button_clicked), view);
    
    view->cancel_button = GTK_WIDGET(gtk_tool_button_new(NULL, "Cancel"));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(view->cancel_button), "process-stop");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(view->cancel_button), -1);
    g_signal_connect(view->cancel_button, "clicked", G_CALLBACK(on_cancel_button_clicked), view);
    gtk_widget_set_sensitive(view->cancel_button, FALSE);
    
    /* Add separator before episode-specific buttons */
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);
    
    /* Episode-specific buttons */
    view->chapters_button = GTK_WIDGET(gtk_tool_button_new(NULL, "Chapters"));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(view->chapters_button), "view-list-symbolic");
    gtk_widget_set_sensitive(view->chapters_button, FALSE);  /* Disabled until chapters available */
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(view->chapters_button), -1);
    g_signal_connect(view->chapters_button, "clicked", G_CALLBACK(on_chapters_button_clicked), view);
    
    view->transcript_button = GTK_WIDGET(gtk_tool_button_new(NULL, "Transcript"));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(view->transcript_button), "text-x-generic-symbolic");
    gtk_widget_set_sensitive(view->transcript_button, FALSE);  /* Disabled until transcript available */
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(view->transcript_button), -1);
    g_signal_connect(view->transcript_button, "clicked", G_CALLBACK(on_transcript_button_clicked), view);
    
    view->support_button = GTK_WIDGET(gtk_tool_button_new(NULL, "Support"));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(view->support_button), "emblem-favorite-symbolic");
    gtk_widget_set_sensitive(view->support_button, FALSE);  /* Disabled until funding available */
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(view->support_button), -1);
    g_signal_connect(view->support_button, "clicked", G_CALLBACK(on_support_button_clicked), view);
    
    gtk_box_pack_start(GTK_BOX(view->container), toolbar, FALSE, FALSE, 0);
    
    /* Progress bar for downloads */
    view->progress_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(view->progress_box), 5);
    
    view->progress_label = gtk_label_new("");
    gtk_widget_set_halign(view->progress_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(view->progress_box), view->progress_label, FALSE, FALSE, 0);
    
    view->progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(view->progress_bar), TRUE);
    gtk_box_pack_start(GTK_BOX(view->progress_box), view->progress_bar, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(view->container), view->progress_box, FALSE, FALSE, 0);
    /* Initially hide the progress box */
    gtk_widget_set_visible(view->progress_box, FALSE);
    
    /* Paned container for podcast list and episode list */
    view->paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(view->paned), 250);
    
    /* Podcast list */
    view->podcast_store = gtk_list_store_new(PODCAST_COL_COUNT,
                                             G_TYPE_INT,    /* ID */
                                             G_TYPE_STRING, /* Title */
                                             G_TYPE_STRING  /* Author */
                                            );
    
    view->podcast_listview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(view->podcast_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view->podcast_listview), TRUE);
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Podcast", renderer, "text", PODCAST_COL_TITLE, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view->podcast_listview), column);
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view->podcast_listview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    g_signal_connect(selection, "changed", G_CALLBACK(on_podcast_selection_changed), view);
    
    GtkWidget *podcast_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(podcast_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(podcast_scroll), view->podcast_listview);
    gtk_paned_pack1(GTK_PANED(view->paned), podcast_scroll, FALSE, TRUE);
    
    /* Episode list */
    view->episode_store = gtk_list_store_new(EPISODE_COL_COUNT,
                                             G_TYPE_INT,     /* ID */
                                             G_TYPE_STRING,  /* Title */
                                             G_TYPE_STRING,  /* Date */
                                             G_TYPE_STRING,  /* Duration */
                                             G_TYPE_BOOLEAN  /* Downloaded */
                                            );
    
    view->episode_listview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(view->episode_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view->episode_listview), TRUE);
    
    /* Enable tooltips for episodes */
    gtk_widget_set_has_tooltip(view->episode_listview, TRUE);
    g_signal_connect(view->episode_listview, "query-tooltip", 
                     G_CALLBACK(on_episode_query_tooltip), view);
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Episode", renderer, "text", EPISODE_COL_TITLE, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view->episode_listview), column);
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Date", renderer, "text", EPISODE_COL_DATE, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view->episode_listview), column);
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Duration", renderer, "text", EPISODE_COL_DURATION, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view->episode_listview), column);
    
    renderer = gtk_cell_renderer_toggle_new();
    column = gtk_tree_view_column_new_with_attributes("Downloaded", renderer, "active", EPISODE_COL_DOWNLOADED, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view->episode_listview), column);
    
    /* Connect row-activated signal for double-click playback */
    g_signal_connect(view->episode_listview, "row-activated", G_CALLBACK(on_episode_row_activated), view);
    
    GtkWidget *episode_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(episode_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(episode_scroll), view->episode_listview);
    gtk_paned_pack2(GTK_PANED(view->paned), episode_scroll, TRUE, TRUE);
    
    gtk_box_pack_start(GTK_BOX(view->container), view->paned, TRUE, TRUE, 0);
    
    return view;
}

void podcast_view_free(PodcastView *view) {
    if (!view) return;
    
    /* Clean up episode-specific data */
    if (view->current_chapters) {
        g_list_free_full(view->current_chapters, (GDestroyNotify)podcast_chapter_free);
    }
    g_free(view->current_transcript_url);
    g_free(view->current_transcript_type);
    if (view->current_funding) {
        g_list_free_full(view->current_funding, (GDestroyNotify)podcast_funding_free);
    }
    
    /* Clean up download progress tracking */
    if (view->download_progress) {
        g_hash_table_destroy(view->download_progress);
    }
    
    g_free(view);
}

void podcast_view_set_play_callback(PodcastView *view, EpisodePlayCallback callback, gpointer user_data) {
    if (!view) return;
    view->play_callback = callback;
    view->play_callback_data = user_data;
}

void podcast_view_set_seek_callback(PodcastView *view, SeekCallback callback, gpointer user_data) {
    if (!view) return;
    view->seek_callback = callback;
    view->seek_callback_data = user_data;
}

GtkWidget* podcast_view_get_widget(PodcastView *view) {
    return view ? view->container : NULL;
}

void podcast_view_add_subscription(PodcastView *view) {
    if (!view) return;
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Subscribe to Podcast",
        NULL,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Subscribe", GTK_RESPONSE_ACCEPT,
        NULL
    );
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    
    GtkWidget *label = gtk_label_new("Feed URL:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
    
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "https://example.com/feed.xml");
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 50);
    gtk_grid_attach(GTK_GRID(grid), entry, 1, 0, 1, 1);
    
    gtk_container_add(GTK_CONTAINER(content), grid);
    gtk_widget_show_all(dialog);
    
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (response == GTK_RESPONSE_ACCEPT) {
        const gchar *feed_url = gtk_entry_get_text(GTK_ENTRY(entry));
        
        if (feed_url && strlen(feed_url) > 0) {
            /* Show progress dialog */
            GtkWidget *progress_dialog = gtk_message_dialog_new(
                NULL,
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_INFO,
                GTK_BUTTONS_NONE,
                "Subscribing to podcast..."
            );
            gtk_widget_show_all(progress_dialog);
            
            while (gtk_events_pending()) {
                gtk_main_iteration();
            }
            
            /* Subscribe to podcast */
            gboolean success = podcast_manager_subscribe(view->podcast_manager, feed_url);
            
            gtk_widget_destroy(progress_dialog);
            
            if (success) {
                /* Refresh podcast list */
                podcast_view_refresh_podcasts(view);
                
                GtkWidget *msg = gtk_message_dialog_new(
                    NULL,
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_INFO,
                    GTK_BUTTONS_OK,
                    "Successfully subscribed to podcast!"
                );
                gtk_dialog_run(GTK_DIALOG(msg));
                gtk_widget_destroy(msg);
            } else {
                GtkWidget *msg = gtk_message_dialog_new(
                    NULL,
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Failed to subscribe to podcast. Please check the feed URL."
                );
                gtk_dialog_run(GTK_DIALOG(msg));
                gtk_widget_destroy(msg);
            }
        }
    }
    
    gtk_widget_destroy(dialog);
}

void podcast_view_refresh_podcasts(PodcastView *view) {
    if (!view) return;
    
    gtk_list_store_clear(view->podcast_store);
    
    GList *podcasts = podcast_manager_get_podcasts(view->podcast_manager);
    
    for (GList *l = podcasts; l != NULL; l = l->next) {
        Podcast *podcast = (Podcast *)l->data;
        
        GtkTreeIter iter;
        gtk_list_store_append(view->podcast_store, &iter);
        gtk_list_store_set(view->podcast_store, &iter,
                          PODCAST_COL_ID, podcast->id,
                          PODCAST_COL_TITLE, podcast->title ? podcast->title : "Unknown",
                          PODCAST_COL_AUTHOR, podcast->author ? podcast->author : "",
                          -1);
    }
}

void podcast_view_refresh_episodes(PodcastView *view, gint podcast_id) {
    if (!view) return;
    
    gtk_list_store_clear(view->episode_store);
    
    GList *episodes = podcast_manager_get_episodes(view->podcast_manager, podcast_id);
    
    for (GList *l = episodes; l != NULL; l = l->next) {
        PodcastEpisode *episode = (PodcastEpisode *)l->data;
        
        /* Format date */
        gchar date_str[64] = "";
        if (episode->published_date > 0) {
            GDateTime *dt = g_date_time_new_from_unix_local(episode->published_date);
            if (dt) {
                gchar *formatted = g_date_time_format(dt, "%Y-%m-%d");
                g_strlcpy(date_str, formatted, sizeof(date_str));
                g_free(formatted);
                g_date_time_unref(dt);
            }
        }
        
        /* Format duration */
        gchar duration_str[32] = "";
        if (episode->duration > 0) {
            gint hours = episode->duration / 3600;
            gint minutes = (episode->duration % 3600) / 60;
            gint seconds = episode->duration % 60;
            
            if (hours > 0) {
                g_snprintf(duration_str, sizeof(duration_str), "%d:%02d:%02d", hours, minutes, seconds);
            } else {
                g_snprintf(duration_str, sizeof(duration_str), "%d:%02d", minutes, seconds);
            }
        }
        
        GtkTreeIter iter;
        gtk_list_store_append(view->episode_store, &iter);
        gtk_list_store_set(view->episode_store, &iter,
                          EPISODE_COL_ID, episode->id,
                          EPISODE_COL_TITLE, episode->title ? episode->title : "Unknown",
                          EPISODE_COL_DATE, date_str,
                          EPISODE_COL_DURATION, duration_str,
                          EPISODE_COL_DOWNLOADED, episode->downloaded,
                          -1);
    }
}

void podcast_view_play_episode(PodcastView *view, gint episode_id) {
    if (!view) return;
    
    /* Get episode details from database */
    GList *episodes = database_get_podcast_episodes(view->database, view->selected_podcast_id);
    
    for (GList *l = episodes; l != NULL; l = l->next) {
        PodcastEpisode *episode = (PodcastEpisode *)l->data;
        if (episode->id == episode_id) {
            /* Play from local file if downloaded, otherwise stream */
            const gchar *uri = episode->downloaded && episode->local_file_path ? 
                              episode->local_file_path : episode->enclosure_url;
            
            g_print("Playing episode: %s from %s\n", episode->title, uri);
            
            /* Load chapters if available */
            GList *chapters = NULL;
            g_print("Checking for chapters: chapters_url='%s', enclosure_url='%s'\n",
                    episode->chapters_url ? episode->chapters_url : "(null)",
                    episode->enclosure_url ? episode->enclosure_url : "(null)");
            if (episode->chapters_url || episode->enclosure_url) {
                g_print("Calling podcast_episode_get_chapters for episode %d\n", episode_id);
                chapters = podcast_episode_get_chapters(view->podcast_manager, episode_id);
                if (chapters) {
                    g_print("Loaded %d chapters for episode\n", g_list_length(chapters));
                } else {
                    g_print("podcast_episode_get_chapters returned NULL\n");
                }
            } else {
                g_print("No chapters_url or enclosure_url found for episode\n");
            }
            
            /* Load funding from database */
            GList *funding = database_get_episode_funding(view->database, episode_id);
            if (funding) {
                g_print("Loaded %d funding options for episode\n", g_list_length(funding));
            }
            
            /* Call playback callback if set */
            if (view->play_callback) {
                view->play_callback(view->play_callback_data, uri, episode->title, chapters, 
                                  episode->transcript_url, episode->transcript_type, funding);
            }
            
            /* Update episode features in the podcast view toolbar */
            podcast_view_update_episode_features(view, chapters, episode->transcript_url, 
                                               episode->transcript_type, funding);
            
            /* Free chapters and funding (callback should have copied if needed) */
            if (chapters) {
                g_list_free_full(chapters, (GDestroyNotify)podcast_chapter_free);
            }
            if (funding) {
                g_list_free_full(funding, (GDestroyNotify)podcast_funding_free);
            }
            
            break;
        }
    }
    
    g_list_free_full(episodes, (GDestroyNotify)podcast_episode_free);
}

void podcast_view_download_episode(PodcastView *view, gint episode_id) {
    if (!view) return;
    
    /* Get episode details */
    GList *episodes = database_get_podcast_episodes(view->database, view->selected_podcast_id);
    
    for (GList *l = episodes; l != NULL; l = l->next) {
        PodcastEpisode *episode = (PodcastEpisode *)l->data;
        if (episode->id == episode_id) {
            if (!episode->downloaded) {
                g_print("Downloading episode: %s\n", episode->title);
                
                /* Store current download ID */
                view->current_download_id = episode_id;
                
                /* Show progress UI */
                gtk_widget_set_visible(view->progress_box, TRUE);
                gtk_label_set_text(GTK_LABEL(view->progress_label), episode->title);
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(view->progress_bar), 0.0);
                gtk_widget_set_sensitive(view->download_button, FALSE);
                gtk_widget_set_sensitive(view->cancel_button, TRUE);
                
                /* Make a copy of the episode for the async download */
                PodcastEpisode *episode_copy = g_new0(PodcastEpisode, 1);
                episode_copy->id = episode->id;
                episode_copy->podcast_id = episode->podcast_id;
                episode_copy->enclosure_url = g_strdup(episode->enclosure_url);
                episode_copy->title = g_strdup(episode->title);
                
                /* Start download with callbacks */
                podcast_episode_download(view->podcast_manager, episode_copy, 
                                       on_download_progress, on_download_complete, view);
            } else {
                g_print("Episode already downloaded: %s\n", episode->local_file_path);
            }
            break;
        }
    }
    
    g_list_free_full(episodes, (GDestroyNotify)podcast_episode_free);
}

/* Download progress callback - runs on main thread via g_idle_add */
typedef struct {
    PodcastView *view;
    gint episode_id;
    gdouble progress;
    gchar *status;
} ProgressUpdate;

static gboolean update_progress_ui(gpointer user_data) {
    ProgressUpdate *update = (ProgressUpdate *)user_data;
    
    if (update->view && update->view->current_download_id == update->episode_id) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(update->view->progress_bar), update->progress);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(update->view->progress_bar), 
                                 g_strdup_printf("%.0f%%", update->progress * 100));
        if (update->status) {
            gtk_label_set_text(GTK_LABEL(update->view->progress_label), update->status);
        }
    }
    
    g_free(update->status);
    g_free(update);
    return G_SOURCE_REMOVE;
}

static void on_download_progress(gpointer user_data, gint episode_id, gdouble progress, const gchar *status) {
    PodcastView *view = (PodcastView *)user_data;
    
    /* Schedule UI update on main thread */
    ProgressUpdate *update = g_new0(ProgressUpdate, 1);
    update->view = view;
    update->episode_id = episode_id;
    update->progress = progress;
    update->status = g_strdup(status);
    
    g_idle_add(update_progress_ui, update);
}

typedef struct {
    PodcastView *view;
    gint episode_id;
    gboolean success;
    gchar *error_msg;
} CompleteUpdate;

static gboolean update_complete_ui(gpointer user_data) {
    CompleteUpdate *update = (CompleteUpdate *)user_data;
    
    if (update->view && update->view->current_download_id == update->episode_id) {
        if (update->success) {
            gtk_label_set_text(GTK_LABEL(update->view->progress_label), "Download complete!");
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(update->view->progress_bar), 1.0);
            gtk_progress_bar_set_text(GTK_PROGRESS_BAR(update->view->progress_bar), "100%");
            
            /* Refresh the episode list to show download status */
            podcast_view_refresh_episodes(update->view, update->view->selected_podcast_id);
        } else {
            gchar *msg = g_strdup_printf("Download failed: %s", 
                                        update->error_msg ? update->error_msg : "Unknown error");
            gtk_label_set_text(GTK_LABEL(update->view->progress_label), msg);
            g_free(msg);
        }
        
        /* Hide progress UI after a delay or reset it */
        update->view->current_download_id = -1;
        gtk_widget_set_sensitive(update->view->download_button, TRUE);
        gtk_widget_set_sensitive(update->view->cancel_button, FALSE);
        
        /* Auto-hide progress after 3 seconds */
        g_timeout_add_seconds(3, (GSourceFunc)gtk_widget_hide, update->view->progress_box);
    }
    
    g_free(update->error_msg);
    g_free(update);
    return G_SOURCE_REMOVE;
}

static void on_download_complete(gpointer user_data, gint episode_id, gboolean success, const gchar *error_msg) {
    PodcastView *view = (PodcastView *)user_data;
    
    /* Schedule UI update on main thread */
    CompleteUpdate *update = g_new0(CompleteUpdate, 1);
    update->view = view;
    update->episode_id = episode_id;
    update->success = success;
    update->error_msg = g_strdup(error_msg);
    
    g_idle_add(update_complete_ui, update);
}
void podcast_view_update_episode_features(PodcastView *view, GList *chapters, const gchar *transcript_url, const gchar *transcript_type, GList *funding) {
    if (!view) return;
    
    /* Clean up existing data */
    if (view->current_chapters) {
        g_list_free_full(view->current_chapters, (GDestroyNotify)podcast_chapter_free);
        view->current_chapters = NULL;
    }
    g_free(view->current_transcript_url);
    view->current_transcript_url = NULL;
    g_free(view->current_transcript_type);
    view->current_transcript_type = NULL;
    if (view->current_funding) {
        g_list_free_full(view->current_funding, (GDestroyNotify)podcast_funding_free);
        view->current_funding = NULL;
    }
    
    /* Copy new data */
    if (chapters) {
        view->current_chapters = g_list_copy_deep(chapters, (GCopyFunc)podcast_chapter_copy, NULL);
        gtk_widget_set_sensitive(view->chapters_button, TRUE);
    } else {
        gtk_widget_set_sensitive(view->chapters_button, FALSE);
    }
    
    if (transcript_url) {
        view->current_transcript_url = g_strdup(transcript_url);
        view->current_transcript_type = g_strdup(transcript_type);
        gtk_widget_set_sensitive(view->transcript_button, TRUE);
    } else {
        gtk_widget_set_sensitive(view->transcript_button, FALSE);
    }
    
    if (funding) {
        view->current_funding = g_list_copy_deep(funding, (GCopyFunc)podcast_funding_copy, NULL);
        gtk_widget_set_sensitive(view->support_button, TRUE);
    } else {
        gtk_widget_set_sensitive(view->support_button, FALSE);
    }
}

void podcast_view_filter(PodcastView *view, const gchar *search_text) {
    if (!view) return;
    
    /* If no search text, show all podcasts and episodes */
    if (!search_text || strlen(search_text) == 0) {
        podcast_view_refresh_podcasts(view);
        if (view->selected_podcast_id > 0) {
            podcast_view_refresh_episodes(view, view->selected_podcast_id);
        }
        return;
    }
    
    GList *all_podcasts = database_get_podcasts(view->database);
    gtk_list_store_clear(view->podcast_store);
    
    gchar *search_lower = g_utf8_strdown(search_text, -1);
    
    /* Track which podcasts have matching episodes */
    GHashTable *podcasts_with_matches = g_hash_table_new(g_direct_hash, g_direct_equal);
    
    /* First, add podcasts that match by title or author */
    for (GList *l = all_podcasts; l != NULL; l = l->next) {
        Podcast *podcast = (Podcast *)l->data;
        gchar *title_lower = g_utf8_strdown(podcast->title ? podcast->title : "", -1);
        gchar *author_lower = g_utf8_strdown(podcast->author ? podcast->author : "", -1);
        
        gboolean match = (strstr(title_lower, search_lower) != NULL) || 
                        (strstr(author_lower, search_lower) != NULL);
        
        g_free(title_lower);
        g_free(author_lower);
        
        if (match) {
            g_hash_table_add(podcasts_with_matches, GINT_TO_POINTER(podcast->id));
        }
    }
    
    /* Search through episodes from ALL podcasts and track which podcasts have matches */
    gtk_list_store_clear(view->episode_store);
    
    for (GList *p = all_podcasts; p != NULL; p = p->next) {
        Podcast *podcast = (Podcast *)p->data;
        GList *all_episodes = database_get_podcast_episodes(view->database, podcast->id);
        gboolean podcast_has_episode_match = FALSE;
        
        for (GList *l = all_episodes; l != NULL; l = l->next) {
            PodcastEpisode *episode = (PodcastEpisode *)l->data;
            gchar *title_lower = g_utf8_strdown(episode->title ? episode->title : "", -1);
            gchar *desc_lower = g_utf8_strdown(episode->description ? episode->description : "", -1);
            
            gboolean match = (strstr(title_lower, search_lower) != NULL) || 
                            (strstr(desc_lower, search_lower) != NULL);
            
            g_free(title_lower);
            g_free(desc_lower);
            
            if (match) {
                podcast_has_episode_match = TRUE;
                
                GtkTreeIter iter;
                gtk_list_store_append(view->episode_store, &iter);
                
                gchar date_str[64] = "Unknown";
                if (episode->published_date > 0) {
                    GDateTime *dt = g_date_time_new_from_unix_local(episode->published_date);
                    if (dt) {
                        gchar *fmt = g_date_time_format(dt, "%Y-%m-%d");
                        g_snprintf(date_str, sizeof(date_str), "%s", fmt);
                        g_free(fmt);
                        g_date_time_unref(dt);
                    }
                }
                
                gchar duration_str[32] = "";
                if (episode->duration > 0) {
                    gint hours = episode->duration / 3600;
                    gint minutes = (episode->duration % 3600) / 60;
                    gint seconds = episode->duration % 60;
                    if (hours > 0) {
                        g_snprintf(duration_str, sizeof(duration_str), "%02d:%02d:%02d", hours, minutes, seconds);
                    } else {
                        g_snprintf(duration_str, sizeof(duration_str), "%02d:%02d", minutes, seconds);
                    }
                }
                
                gtk_list_store_set(view->episode_store, &iter,
                                 EPISODE_COL_ID, episode->id,
                                 EPISODE_COL_TITLE, episode->title,
                                 EPISODE_COL_DATE, date_str,
                                 EPISODE_COL_DURATION, duration_str,
                                 EPISODE_COL_DOWNLOADED, episode->local_file_path != NULL,
                                 -1);
            }
        }
        
        /* Add podcast to list if it has matching episodes */
        if (podcast_has_episode_match) {
            g_hash_table_add(podcasts_with_matches, GINT_TO_POINTER(podcast->id));
        }
        
        g_list_free_full(all_episodes, (GDestroyNotify)podcast_episode_free);
    }
    
    /* Now populate the podcast list with all podcasts that have matches */
    for (GList *l = all_podcasts; l != NULL; l = l->next) {
        Podcast *podcast = (Podcast *)l->data;
        
        if (g_hash_table_contains(podcasts_with_matches, GINT_TO_POINTER(podcast->id))) {
            GtkTreeIter iter;
            gtk_list_store_append(view->podcast_store, &iter);
            gtk_list_store_set(view->podcast_store, &iter,
                             PODCAST_COL_ID, podcast->id,
                             PODCAST_COL_TITLE, podcast->title,
                             PODCAST_COL_AUTHOR, podcast->author,
                             -1);
        }
    }
    
    g_free(search_lower);
    g_hash_table_destroy(podcasts_with_matches);
    g_list_free_full(all_podcasts, (GDestroyNotify)podcast_free);
}