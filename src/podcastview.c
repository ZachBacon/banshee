#include "podcastview.h"
#include "transcriptview.h"
#include <string.h>

static void on_chapters_button_clicked(GtkButton *button, gpointer user_data);
static void on_transcript_button_clicked(GtkButton *button, gpointer user_data);
static void on_support_button_clicked(GtkButton *button, gpointer user_data);
static void on_value_button_clicked(GtkButton *button, gpointer user_data);
static void on_live_button_clicked(GtkButton *button, gpointer user_data);
static void on_chapter_seek(gpointer user_data, gdouble time);
static void on_funding_url_clicked(GtkWidget *button, gpointer user_data);
static void on_cancel_button_clicked(GtkButton *button, gpointer user_data);
static void on_episode_selection_changed(GtkSelectionModel *selection, guint position, guint n_items, gpointer user_data);
static void update_live_indicator(PodcastView *view, Podcast *podcast);

/* GTK4 dialog helper */
typedef struct {
    gboolean done;
    gint response;
} DialogResponseData;

static void on_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    (void)dialog;
    DialogResponseData *data = (DialogResponseData *)user_data;
    data->response = response_id;
    data->done = TRUE;
}

/* Helper function to update support button text based on current funding */
static void update_support_button_text(PodcastView *view) {
    if (!view->support_button || !view->current_funding) return;
    
    /* Use the first funding entry's message as the button text */
    PodcastFunding *funding = (PodcastFunding *)view->current_funding->data;
    const gchar *button_text = "Support";
    
    if (funding && funding->message && strlen(funding->message) > 0) {
        /* Truncate long messages for the button */
        if (strlen(funding->message) > 20) {
            gchar *truncated = g_strndup(funding->message, 17);
            gchar *button_label = g_strdup_printf("%s...", truncated);
            gtk_button_set_label(GTK_BUTTON(view->support_button), button_label);
            g_free(truncated);
            g_free(button_label);
        } else {
            gtk_button_set_label(GTK_BUTTON(view->support_button), funding->message);
        }
    } else {
        gtk_button_set_label(GTK_BUTTON(view->support_button), button_text);
    }
}

/* Helper function to update value button text based on current value tags */
static void update_value_button_text(PodcastView *view) {
    if (!view->value_button || !view->current_value) return;
    
    PodcastValue *value = (PodcastValue*)view->current_value->data;
    if (value) {
        gchar *button_text = g_strdup_printf("⚡ %s (%d recipients)", 
                                           value->type ? value->type : "Lightning",
                                           g_list_length(value->recipients));
        gtk_button_set_label(GTK_BUTTON(view->value_button), button_text);
        g_free(button_text);
    } else {
        gtk_button_set_label(GTK_BUTTON(view->value_button), "⚡ Value");
    }
}

/* Helper function to update live indicator based on podcast's live items */
static void update_live_indicator(PodcastView *view, Podcast *podcast) {
    if (!view->live_indicator || !view->live_button) return;
    
    /* Clear current live items */
    if (view->current_live_items) {
        g_list_free_full(view->current_live_items, (GDestroyNotify)podcast_live_item_free);
        view->current_live_items = NULL;
    }
    
    if (!podcast) {
        gtk_widget_set_visible(view->live_indicator, FALSE);
        gtk_widget_set_visible(view->live_button, FALSE);
        return;
    }
    
    /* Check if podcast has active live items */
    gboolean has_live = podcast->has_active_live;
    
    /* Copy live items for use */
    if (podcast->live_items) {
        view->current_live_items = g_list_copy_deep(podcast->live_items, (GCopyFunc)podcast_live_item_copy, NULL);
    }
    
    if (has_live) {
        /* Find the first live item to display */
        PodcastLiveItem *live_item = NULL;
        for (GList *l = view->current_live_items; l != NULL; l = l->next) {
            PodcastLiveItem *item = (PodcastLiveItem *)l->data;
            if (item->status == LIVE_STATUS_LIVE) {
                live_item = item;
                break;
            }
        }
        
        if (live_item && live_item->title) {
            gchar *label = g_strdup_printf("🔴 LIVE: %s", live_item->title);
            gtk_label_set_text(GTK_LABEL(view->live_indicator), label);
            g_free(label);
        } else {
            gtk_label_set_text(GTK_LABEL(view->live_indicator), "🔴 LIVE");
        }
        
        gtk_widget_set_visible(view->live_indicator, TRUE);
        gtk_widget_set_visible(view->live_button, TRUE);
        gtk_button_set_label(GTK_BUTTON(view->live_button), "Watch/Listen Live");
    } else if (view->current_live_items) {
        /* Has pending live items, show upcoming */
        PodcastLiveItem *next_live = NULL;
        gint64 now = g_get_real_time() / G_USEC_PER_SEC;
        
        for (GList *l = view->current_live_items; l != NULL; l = l->next) {
            PodcastLiveItem *item = (PodcastLiveItem *)l->data;
            if (item->status == LIVE_STATUS_PENDING && item->start_time > now) {
                if (!next_live || item->start_time < next_live->start_time) {
                    next_live = item;
                }
            }
        }
        
        if (next_live) {
            GDateTime *dt = g_date_time_new_from_unix_local(next_live->start_time);
            if (dt) {
                gchar *time_str = g_date_time_format(dt, "%b %d, %H:%M");
                gchar *label = g_strdup_printf("⏱ Upcoming: %s", time_str);
                gtk_label_set_text(GTK_LABEL(view->live_indicator), label);
                g_free(label);
                g_free(time_str);
                g_date_time_unref(dt);
            }
            gtk_widget_set_visible(view->live_indicator, TRUE);
            gtk_widget_set_visible(view->live_button, FALSE);
        } else {
            gtk_widget_set_visible(view->live_indicator, FALSE);
            gtk_widget_set_visible(view->live_button, FALSE);
        }
    } else {
        gtk_widget_set_visible(view->live_indicator, FALSE);
        gtk_widget_set_visible(view->live_button, FALSE);
    }
}

/* Download callbacks */
static void on_download_progress(gpointer user_data, gint episode_id, gdouble progress, const gchar *status);
static void on_download_complete(gpointer user_data, gint episode_id, gboolean success, const gchar *error_msg);

/* Column enums - kept for reference, but now use GObject properties */
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

/* ============================================================================
 * GTK4 Column Factory Functions for Podcast List
 * ============================================================================ */

static void setup_podcast_title_label(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_list_item_set_child(list_item, label);
}

static void bind_podcast_title_label(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    ShriekPodcastObject *podcast = gtk_list_item_get_item(list_item);
    if (podcast) {
        gtk_label_set_text(GTK_LABEL(label), shriek_podcast_object_get_title(podcast));
    }
}

/* ============================================================================
 * GTK4 Column Factory Functions for Episode List
 * ============================================================================ */

static void setup_episode_title_label(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_list_item_set_child(list_item, label);
}

static void bind_episode_title_label(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    ShriekEpisodeObject *episode = gtk_list_item_get_item(list_item);
    if (episode) {
        gtk_label_set_text(GTK_LABEL(label), shriek_episode_object_get_title(episode));
    }
}

static void setup_episode_date_label(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_list_item_set_child(list_item, label);
}

static void bind_episode_date_label(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    ShriekEpisodeObject *episode = gtk_list_item_get_item(list_item);
    if (episode) {
        gtk_label_set_text(GTK_LABEL(label), shriek_episode_object_get_date(episode));
    }
}

static void setup_episode_duration_label(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 1.0);
    gtk_list_item_set_child(list_item, label);
}

static void bind_episode_duration_label(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    ShriekEpisodeObject *episode = gtk_list_item_get_item(list_item);
    if (episode) {
        gtk_label_set_text(GTK_LABEL(label), shriek_episode_object_get_duration(episode));
    }
}

static void setup_episode_downloaded_check(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *check = gtk_check_button_new();
    gtk_widget_set_sensitive(check, FALSE);  /* Read-only indicator */
    gtk_list_item_set_child(list_item, check);
}

static void bind_episode_downloaded_check(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *check = gtk_list_item_get_child(list_item);
    ShriekEpisodeObject *episode = gtk_list_item_get_item(list_item);
    if (episode) {
        gtk_check_button_set_active(GTK_CHECK_BUTTON(check), 
            shriek_episode_object_get_downloaded(episode));
    }
}

static void on_podcast_selection_changed(GtkSelectionModel *selection, guint position, guint n_items, gpointer user_data) {
    (void)position;
    (void)n_items;
    PodcastView *view = (PodcastView *)user_data;
    
    /* Get selected item using GTK4 API */
    GtkSingleSelection *single_sel = GTK_SINGLE_SELECTION(selection);
    guint selected_pos = gtk_single_selection_get_selected(single_sel);
    
    if (selected_pos != GTK_INVALID_LIST_POSITION) {
        ShriekPodcastObject *podcast_obj = g_list_model_get_item(
            G_LIST_MODEL(view->podcast_store), selected_pos);
        
        if (!podcast_obj) return;
        
        gint podcast_id = shriek_podcast_object_get_id(podcast_obj);
        g_object_unref(podcast_obj);
        
        view->selected_podcast_id = podcast_id;
        
        /* Load podcast funding information and enable/disable support button */
        Podcast *podcast = database_get_podcast_by_id(view->database, podcast_id);
        
        /* Also get podcast from manager to get live items (which are parsed from feed) */
        Podcast *manager_podcast = NULL;
        GList *podcasts = podcast_manager_get_podcasts(view->podcast_manager);
        for (GList *l = podcasts; l != NULL; l = l->next) {
            Podcast *p = (Podcast *)l->data;
            if (p->id == podcast_id) {
                manager_podcast = p;
                break;
            }
        }
        
        /* Update live indicator */
        update_live_indicator(view, manager_podcast);
        
        if (podcast && podcast->funding) {
            /* Set current funding to podcast-level funding */
            if (view->current_funding) {
                g_list_free_full(view->current_funding, (GDestroyNotify)podcast_funding_free);
            }
            view->current_funding = g_list_copy_deep(podcast->funding, (GCopyFunc)podcast_funding_copy, NULL);
            gtk_widget_set_sensitive(view->support_button, TRUE);
            update_support_button_text(view);
        } else {
            /* No podcast-level funding, disable support button for now */
            if (view->current_funding) {
                g_list_free_full(view->current_funding, (GDestroyNotify)podcast_funding_free);
                view->current_funding = NULL;
            }
            gtk_widget_set_sensitive(view->support_button, FALSE);
            gtk_button_set_label(GTK_BUTTON(view->support_button), "Support");
        }
        
        /* Load podcast value information and enable/disable value button */
        if (podcast && podcast->value) {
            /* Set current value to podcast-level value */
            if (view->current_value) {
                g_list_free_full(view->current_value, (GDestroyNotify)podcast_value_free);
            }
            view->current_value = g_list_copy_deep(podcast->value, (GCopyFunc)podcast_value_copy, NULL);
            gtk_widget_set_sensitive(view->value_button, TRUE);
            update_value_button_text(view);
        } else {
            /* No podcast-level value, disable value button for now */
            if (view->current_value) {
                g_list_free_full(view->current_value, (GDestroyNotify)podcast_value_free);
                view->current_value = NULL;
            }
            gtk_widget_set_sensitive(view->value_button, FALSE);
            gtk_button_set_label(GTK_BUTTON(view->value_button), "⚡ Value");
        }
        
        if (podcast) {
            podcast_free(podcast);
        }
        
        podcast_view_refresh_episodes(view, podcast_id);
    }
}

static void on_episode_selection_changed(GtkSelectionModel *selection, guint position, guint n_items, gpointer user_data) {
    (void)position;
    (void)n_items;
    PodcastView *view = (PodcastView *)user_data;
    
    /* Get selected item using GTK4 API */
    GtkSingleSelection *single_sel = GTK_SINGLE_SELECTION(selection);
    guint selected_pos = gtk_single_selection_get_selected(single_sel);
    
    if (selected_pos != GTK_INVALID_LIST_POSITION) {
        ShriekEpisodeObject *episode_obj = g_list_model_get_item(
            G_LIST_MODEL(view->episode_store), selected_pos);
        
        if (!episode_obj) return;
        
        gint episode_id = shriek_episode_object_get_id(episode_obj);
        g_object_unref(episode_obj);
        
        /* Load episode and its funding information */
        PodcastEpisode *episode = database_get_episode_by_id(view->database, episode_id);
        if (episode && episode->funding) {
            /* Episode has its own funding, use that instead of podcast-level funding */
            if (view->current_funding) {
                g_list_free_full(view->current_funding, (GDestroyNotify)podcast_funding_free);
            }
            view->current_funding = g_list_copy_deep(episode->funding, (GCopyFunc)podcast_funding_copy, NULL);
            gtk_widget_set_sensitive(view->support_button, TRUE);
            update_support_button_text(view);
        } else if (!view->current_funding || g_list_length(view->current_funding) == 0) {
            /* No episode funding and no current podcast funding, check podcast funding */
            if (view->selected_podcast_id > 0) {
                Podcast *podcast = database_get_podcast_by_id(view->database, view->selected_podcast_id);
                if (podcast && podcast->funding) {
                    if (view->current_funding) {
                        g_list_free_full(view->current_funding, (GDestroyNotify)podcast_funding_free);
                    }
                    view->current_funding = g_list_copy_deep(podcast->funding, (GCopyFunc)podcast_funding_copy, NULL);
                    gtk_widget_set_sensitive(view->support_button, TRUE);
                    update_support_button_text(view);
                }
                if (podcast) {
                    podcast_free(podcast);
                }
            }
        }
        
        /* Handle Value 4 Value information */
        if (episode && episode->value) {
            /* Episode has its own value info, use that instead of podcast-level value */
            if (view->current_value) {
                g_list_free_full(view->current_value, (GDestroyNotify)podcast_value_free);
            }
            view->current_value = g_list_copy_deep(episode->value, (GCopyFunc)podcast_value_copy, NULL);
            gtk_widget_set_sensitive(view->value_button, TRUE);
            update_value_button_text(view);
        } else if (!view->current_value || g_list_length(view->current_value) == 0) {
            /* No episode value and no current podcast value, check podcast value */
            if (view->selected_podcast_id > 0) {
                Podcast *podcast = database_get_podcast_by_id(view->database, view->selected_podcast_id);
                if (podcast && podcast->value) {
                    if (view->current_value) {
                        g_list_free_full(view->current_value, (GDestroyNotify)podcast_value_free);
                    }
                    view->current_value = g_list_copy_deep(podcast->value, (GCopyFunc)podcast_value_copy, NULL);
                    gtk_widget_set_sensitive(view->value_button, TRUE);
                    update_value_button_text(view);
                }
                if (podcast) {
                    podcast_free(podcast);
                }
            }
        }
        
        if (episode) {
            podcast_episode_free(episode);
        }
        
        /* Clear funding popover to force recreation with new funding data */
        if (view->funding_popover) {
            gtk_widget_unparent(view->funding_popover);
            view->funding_popover = NULL;
        }
        
        /* Clear value popover to force recreation with new value data */
        if (view->value_popover) {
            gtk_widget_unparent(view->value_popover);
            view->value_popover = NULL;
        }
    } else {
        /* No episode selected, revert to podcast-level funding if available */
        if (view->selected_podcast_id > 0) {
            Podcast *podcast = database_get_podcast_by_id(view->database, view->selected_podcast_id);
            if (podcast && podcast->funding) {
                if (view->current_funding) {
                    g_list_free_full(view->current_funding, (GDestroyNotify)podcast_funding_free);
                }
                view->current_funding = g_list_copy_deep(podcast->funding, (GCopyFunc)podcast_funding_copy, NULL);
                gtk_widget_set_sensitive(view->support_button, TRUE);
                update_support_button_text(view);
            } else {
                if (view->current_funding) {
                    g_list_free_full(view->current_funding, (GDestroyNotify)podcast_funding_free);
                    view->current_funding = NULL;
                }
                gtk_widget_set_sensitive(view->support_button, FALSE);
                gtk_button_set_label(GTK_BUTTON(view->support_button), "Support");
            }
            
            /* Handle podcast-level value info */
            if (podcast && podcast->value) {
                if (view->current_value) {
                    g_list_free_full(view->current_value, (GDestroyNotify)podcast_value_free);
                }
                view->current_value = g_list_copy_deep(podcast->value, (GCopyFunc)podcast_value_copy, NULL);
                gtk_widget_set_sensitive(view->value_button, TRUE);
                update_value_button_text(view);
            } else {
                if (view->current_value) {
                    g_list_free_full(view->current_value, (GDestroyNotify)podcast_value_free);
                    view->current_value = NULL;
                }
                gtk_widget_set_sensitive(view->value_button, FALSE);
                gtk_button_set_label(GTK_BUTTON(view->value_button), "⚡ Value");
            }
            
            if (podcast) {
                podcast_free(podcast);
            }
        }
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
    podcast_manager_update_all_feeds(view->podcast_manager);
    
    /* Refresh the UI */
    podcast_view_refresh_podcasts(view);
    
    /* If a podcast is selected, refresh its episodes too */
    guint selected_pos = gtk_single_selection_get_selected(view->podcast_selection);
    if (selected_pos != GTK_INVALID_LIST_POSITION) {
        ShriekPodcastObject *podcast_obj = g_list_model_get_item(
            G_LIST_MODEL(view->podcast_store), selected_pos);
        if (podcast_obj) {
            gint podcast_id = shriek_podcast_object_get_id(podcast_obj);
            g_object_unref(podcast_obj);
            podcast_view_refresh_episodes(view, podcast_id);
        }
    }
}

static void on_download_button_clicked(GtkButton *button, gpointer user_data) {
    PodcastView *view = (PodcastView *)user_data;
    (void)button;
    
    guint selected_pos = gtk_single_selection_get_selected(view->episode_selection);
    if (selected_pos != GTK_INVALID_LIST_POSITION) {
        ShriekEpisodeObject *episode_obj = g_list_model_get_item(
            G_LIST_MODEL(view->episode_store), selected_pos);
        if (episode_obj) {
            gint episode_id = shriek_episode_object_get_id(episode_obj);
            g_object_unref(episode_obj);
            podcast_view_download_episode(view, episode_id);
        }
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

/* GTK4 activate handler for episode double-click */
static void on_episode_activated(GtkColumnView *column_view, guint position, gpointer user_data) {
    (void)column_view;
    PodcastView *view = (PodcastView *)user_data;
    
    ShriekEpisodeObject *episode_obj = g_list_model_get_item(
        G_LIST_MODEL(view->episode_store), position);
    
    if (episode_obj) {
        gint episode_id = shriek_episode_object_get_id(episode_obj);
        g_object_unref(episode_obj);
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

/* Tooltip functionality removed - requires GTK4 reimplementation with GtkPopover or per-item tooltips */

static void on_chapter_seek(gpointer user_data, gdouble time) {
    PodcastView *view = (PodcastView *)user_data;
    
    /* Call the seek callback if registered */
    if (view->seek_callback) {
        view->seek_callback(view->seek_callback_data, time);
    } else {
        /* Warning: No seek callback registered */
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
        
        view->chapter_popover = gtk_popover_new();
        gtk_widget_set_parent(view->chapter_popover, view->chapters_button);
        gtk_popover_set_child(GTK_POPOVER(view->chapter_popover), chapter_view_get_widget(view->chapter_view));
        gtk_widget_set_size_request(chapter_view_get_widget(view->chapter_view), 300, 400);
    }
    
    /* Update chapters and show popover */
    chapter_view_set_chapters(view->chapter_view, view->current_chapters);
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
        
        view->transcript_popover = gtk_popover_new();
        gtk_widget_set_parent(view->transcript_popover, view->transcript_button);
        gtk_popover_set_child(GTK_POPOVER(view->transcript_popover), transcript_view_get_widget(transcript_view));
        gtk_widget_set_size_request(transcript_view_get_widget(transcript_view), 500, 600);
        
        /* Store transcript view in popover data for later access */
        g_object_set_data(G_OBJECT(view->transcript_popover), "transcript_view", transcript_view);
    }
    
    /* Load transcript */
    TranscriptView *transcript_view = (TranscriptView *)g_object_get_data(G_OBJECT(view->transcript_popover), "transcript_view");
    transcript_view_load_from_url(transcript_view, view->current_transcript_url, view->current_transcript_type);
    
    gtk_popover_popup(GTK_POPOVER(view->transcript_popover));
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
        gtk_box_append(GTK_BOX(funding_box), title_label);
        
        /* Add funding links */
        for (GList *l = view->current_funding; l != NULL; l = l->next) {
            PodcastFunding *funding = (PodcastFunding *)l->data;
            
            GtkWidget *funding_button = gtk_button_new();
            gtk_button_set_has_frame(GTK_BUTTON(funding_button), FALSE);
            
            GtkWidget *funding_label_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            
            /* Add icon if we can determine the platform */
            const gchar *icon_name = "web-browser-symbolic";  /* Default icon */
            if (g_str_has_prefix(funding->url, "https://www.patreon.com")) {
                icon_name = "applications-internet-symbolic";
            } else if (g_str_has_prefix(funding->url, "https://ko-fi.com")) {
                icon_name = "face-smile-symbolic";
            }
            
            GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
            gtk_box_append(GTK_BOX(funding_label_box), icon);
            
            GtkWidget *label = gtk_label_new(funding->message ? funding->message : funding->url);
            gtk_widget_set_halign(label, GTK_ALIGN_START);
            gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
            gtk_label_set_max_width_chars(GTK_LABEL(label), 40);
            gtk_widget_set_hexpand(label, TRUE);
            gtk_box_append(GTK_BOX(funding_label_box), label);
            
            gtk_button_set_child(GTK_BUTTON(funding_button), funding_label_box);
            
            /* Store URL in button data */
            g_object_set_data_full(G_OBJECT(funding_button), "funding_url", 
                                   g_strdup(funding->url), g_free);
            
            g_signal_connect(funding_button, "clicked", G_CALLBACK(on_funding_url_clicked), NULL);
            
            gtk_box_append(GTK_BOX(funding_box), funding_button);
        }
        
        view->funding_popover = gtk_popover_new();
        gtk_widget_set_parent(view->funding_popover, view->support_button);
        gtk_popover_set_child(GTK_POPOVER(view->funding_popover), funding_box);
        gtk_widget_set_size_request(funding_box, 350, -1);
    }
    
    gtk_popover_popup(GTK_POPOVER(view->funding_popover));
}

static void on_value_button_clicked(GtkButton *button, gpointer user_data) {
    PodcastView *view = (PodcastView *)user_data;
    (void)button;
    
    if (!view->current_value) {
        return;
    }
    
    /* Create popover if it doesn't exist */
    if (!view->value_popover) {
        GtkWidget *value_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_start(value_box, 15);
        gtk_widget_set_margin_end(value_box, 15);
        gtk_widget_set_margin_top(value_box, 15);
        gtk_widget_set_margin_bottom(value_box, 15);
        
        GtkWidget *title_label = gtk_label_new("⚡ Lightning Network - Value 4 Value");
        gtk_widget_set_halign(title_label, GTK_ALIGN_START);
        PangoAttrList *attrs = pango_attr_list_new();
        pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
        gtk_label_set_attributes(GTK_LABEL(title_label), attrs);
        pango_attr_list_unref(attrs);
        gtk_box_append(GTK_BOX(value_box), title_label);
        
        for (GList *l = view->current_value; l != NULL; l = l->next) {
            PodcastValue *value = (PodcastValue *)l->data;
            
            /* Value info header */
            GtkWidget *value_info = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            
            GtkWidget *method_label = gtk_label_new(NULL);
            gchar *method_text = g_strdup_printf("<b>Method:</b> %s", value->method ? value->method : "Unknown");
            gtk_label_set_markup(GTK_LABEL(method_label), method_text);
            g_free(method_text);
            gtk_box_append(GTK_BOX(value_info), method_label);
            
            if (value->suggested) {
                GtkWidget *suggested_label = gtk_label_new(NULL);
                gchar *suggested_text = g_strdup_printf("<b>Suggested:</b> %s sats", value->suggested);
                gtk_label_set_markup(GTK_LABEL(suggested_label), suggested_text);
                g_free(suggested_text);
                gtk_widget_set_hexpand(suggested_label, TRUE);
                gtk_widget_set_halign(suggested_label, GTK_ALIGN_END);
                gtk_box_append(GTK_BOX(value_info), suggested_label);
            }
            
            gtk_widget_set_margin_top(value_info, 5);
            gtk_widget_set_margin_bottom(value_info, 5);
            gtk_box_append(GTK_BOX(value_box), value_info);
            
            /* Recipients */
            if (value->recipients) {
                GtkWidget *recipients_label = gtk_label_new("<b>Recipients:</b>");
                gtk_label_set_markup(GTK_LABEL(recipients_label), "<b>Recipients:</b>");
                gtk_widget_set_halign(recipients_label, GTK_ALIGN_START);
                gtk_widget_set_margin_top(recipients_label, 5);
                gtk_widget_set_margin_bottom(recipients_label, 5);
                gtk_box_append(GTK_BOX(value_box), recipients_label);
                
                gint total_split = 0;
                for (GList *r = value->recipients; r != NULL; r = r->next) {
                    ValueRecipient *recipient = (ValueRecipient *)r->data;
                    total_split += recipient->split;
                    
                    GtkWidget *recipient_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
                    gtk_widget_set_margin_start(recipient_box, 20);
                    
                    /* Recipient name and split */
                    GtkWidget *name_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
                    
                    GtkWidget *name_label = gtk_label_new(recipient->name ? recipient->name : "Unknown");
                    PangoAttrList *name_attrs = pango_attr_list_new();
                    pango_attr_list_insert(name_attrs, pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD));
                    gtk_label_set_attributes(GTK_LABEL(name_label), name_attrs);
                    pango_attr_list_unref(name_attrs);
                    gtk_widget_set_halign(name_label, GTK_ALIGN_START);
                    gtk_box_append(GTK_BOX(name_box), name_label);
                    
                    GtkWidget *split_label = gtk_label_new(NULL);
                    gchar *split_text = g_strdup_printf("%d%%", recipient->split);
                    gtk_label_set_markup(GTK_LABEL(split_label), split_text);
                    g_free(split_text);
                    gtk_widget_set_halign(split_label, GTK_ALIGN_END);
                    gtk_widget_set_hexpand(split_label, TRUE);
                    gtk_box_append(GTK_BOX(name_box), split_label);
                    
                    gtk_box_append(GTK_BOX(recipient_box), name_box);
                    
                    /* Lightning address */
                    if (recipient->address) {
                        GtkWidget *address_label = gtk_label_new(recipient->address);
                        gtk_label_set_ellipsize(GTK_LABEL(address_label), PANGO_ELLIPSIZE_MIDDLE);
                        gtk_label_set_max_width_chars(GTK_LABEL(address_label), 50);
                        gtk_widget_set_halign(address_label, GTK_ALIGN_START);
                        
                        /* Make address selectable and monospace */
                        gtk_label_set_selectable(GTK_LABEL(address_label), TRUE);
                        PangoAttrList *addr_attrs = pango_attr_list_new();
                        pango_attr_list_insert(addr_attrs, pango_attr_family_new("monospace"));
                        pango_attr_list_insert(addr_attrs, pango_attr_scale_new(0.85));
                        gtk_label_set_attributes(GTK_LABEL(address_label), addr_attrs);
                        pango_attr_list_unref(addr_attrs);
                        
                        gtk_box_append(GTK_BOX(recipient_box), address_label);
                    }
                    
                    /* Custom key/value if present */
                    if (recipient->custom_key || recipient->custom_value) {
                        GtkWidget *custom_label = gtk_label_new(NULL);
                        gchar *custom_text = g_strdup_printf("<i>Custom: %s = %s</i>", 
                                                           recipient->custom_key ? recipient->custom_key : "?",
                                                           recipient->custom_value ? recipient->custom_value : "?");
                        gtk_label_set_markup(GTK_LABEL(custom_label), custom_text);
                        g_free(custom_text);
                        gtk_widget_set_halign(custom_label, GTK_ALIGN_START);
                        gtk_box_append(GTK_BOX(recipient_box), custom_label);
                    }
                    
                    gtk_widget_set_margin_top(recipient_box, 2);
                    gtk_widget_set_margin_bottom(recipient_box, 2);
                    gtk_box_append(GTK_BOX(value_box), recipient_box);
                }
                
                /* Show total split validation */
                if (total_split != 100) {
                    GtkWidget *warning_label = gtk_label_new(NULL);
                    gchar *warning_text = g_strdup_printf("<span color='orange'>⚠ Total split: %d%% (should be 100%%)</span>", total_split);
                    gtk_label_set_markup(GTK_LABEL(warning_label), warning_text);
                    g_free(warning_text);
                    gtk_widget_set_halign(warning_label, GTK_ALIGN_START);
                    gtk_widget_set_margin_top(warning_label, 5);
                    gtk_box_append(GTK_BOX(value_box), warning_label);
                }
            }
        }
        
        /* Add info text */
        GtkWidget *info_label = gtk_label_new("<i>Use a Value 4 Value enabled podcast app\nto send Lightning Network micropayments</i>");
        gtk_label_set_markup(GTK_LABEL(info_label), "<i>Use a Value 4 Value enabled podcast app\nto send Lightning Network micropayments</i>");
        gtk_widget_set_halign(info_label, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top(info_label, 10);
        gtk_box_append(GTK_BOX(value_box), info_label);
        
        view->value_popover = gtk_popover_new();
        gtk_widget_set_parent(view->value_popover, view->value_button);
        gtk_popover_set_child(GTK_POPOVER(view->value_popover), value_box);
        gtk_widget_set_size_request(value_box, 450, -1);
    }
    
    gtk_popover_popup(GTK_POPOVER(view->value_popover));
}

/* Handle click on live button - opens the live stream */
static void on_live_button_clicked(GtkButton *button, gpointer user_data) {
    PodcastView *view = (PodcastView *)user_data;
    (void)button;
    
    if (!view->current_live_items) {
        return;
    }
    
    /* Find the first active live item */
    PodcastLiveItem *live_item = NULL;
    for (GList *l = view->current_live_items; l != NULL; l = l->next) {
        PodcastLiveItem *item = (PodcastLiveItem *)l->data;
        if (item->status == LIVE_STATUS_LIVE) {
            live_item = item;
            break;
        }
    }
    
    if (!live_item) {
        return;
    }
    
    /* Try to play the enclosure URL directly if available */
    if (live_item->enclosure_url && view->play_callback) {
        view->play_callback(view->play_callback_data, 
                           live_item->enclosure_url, 
                           live_item->title ? live_item->title : "Live Stream",
                           NULL, NULL, NULL, NULL);
        return;
    }
    
    /* Otherwise, try to open a content link in the browser */
    if (live_item->content_links) {
        PodcastContentLink *link = (PodcastContentLink *)live_item->content_links->data;
        if (link && link->href) {
            /* Create a popover to show content link options */
            GtkWidget *popover = gtk_popover_new();
            gtk_widget_set_parent(popover, view->live_button);
            GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
            gtk_widget_set_margin_start(box, 10);
            gtk_widget_set_margin_end(box, 10);
            gtk_widget_set_margin_top(box, 10);
            gtk_widget_set_margin_bottom(box, 10);
            
            GtkWidget *title = gtk_label_new("Open Live Stream:");
            PangoAttrList *attrs = pango_attr_list_new();
            pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
            gtk_label_set_attributes(GTK_LABEL(title), attrs);
            pango_attr_list_unref(attrs);
            gtk_widget_set_margin_top(title, 5);
            gtk_widget_set_margin_bottom(title, 5);
            gtk_box_append(GTK_BOX(box), title);
            
            for (GList *cl = live_item->content_links; cl != NULL; cl = cl->next) {
                PodcastContentLink *content_link = (PodcastContentLink *)cl->data;
                if (content_link && content_link->href) {
                    GtkWidget *link_button = gtk_link_button_new_with_label(
                        content_link->href, 
                        content_link->text ? content_link->text : "Open Stream");
                    gtk_box_append(GTK_BOX(box), link_button);
                }
            }
            
            gtk_popover_set_child(GTK_POPOVER(popover), box);
            gtk_popover_popup(GTK_POPOVER(popover));
        }
    }
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
    view->value_popover = NULL;
    view->current_chapters = NULL;
    view->current_transcript_url = NULL;
    view->current_transcript_type = NULL;
    view->current_funding = NULL;
    view->current_value = NULL;
    
    /* Initialize callbacks */
    view->play_callback = NULL;
    view->play_callback_data = NULL;
    view->seek_callback = NULL;
    view->seek_callback_data = NULL;
    
    /* Main container */
    view->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    /* Toolbar - using GtkBox with buttons in GTK4 */
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(toolbar, "toolbar");
    gtk_widget_set_margin_start(toolbar, 4);
    gtk_widget_set_margin_end(toolbar, 4);
    gtk_widget_set_margin_top(toolbar, 4);
    gtk_widget_set_margin_bottom(toolbar, 4);
    
    view->add_button = gtk_button_new_from_icon_name("list-add");
    gtk_button_set_label(GTK_BUTTON(view->add_button), "Subscribe");
    gtk_box_append(GTK_BOX(toolbar), view->add_button);
    g_signal_connect(view->add_button, "clicked", G_CALLBACK(on_add_button_clicked), view);
    
    view->remove_button = gtk_button_new_from_icon_name("list-remove");
    gtk_button_set_label(GTK_BUTTON(view->remove_button), "Unsubscribe");
    gtk_box_append(GTK_BOX(toolbar), view->remove_button);
    
    view->refresh_button = gtk_button_new_from_icon_name("view-refresh");
    gtk_button_set_label(GTK_BUTTON(view->refresh_button), "Refresh");
    gtk_box_append(GTK_BOX(toolbar), view->refresh_button);
    g_signal_connect(view->refresh_button, "clicked", G_CALLBACK(on_refresh_button_clicked), view);
    
    /* Separator */
    GtkWidget *separator1 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start(separator1, 4);
    gtk_widget_set_margin_end(separator1, 4);
    gtk_box_append(GTK_BOX(toolbar), separator1);
    
    view->download_button = gtk_button_new_from_icon_name("document-save");
    gtk_button_set_label(GTK_BUTTON(view->download_button), "Download");
    gtk_box_append(GTK_BOX(toolbar), view->download_button);
    g_signal_connect(view->download_button, "clicked", G_CALLBACK(on_download_button_clicked), view);
    
    view->cancel_button = gtk_button_new_from_icon_name("process-stop");
    gtk_button_set_label(GTK_BUTTON(view->cancel_button), "Cancel");
    gtk_box_append(GTK_BOX(toolbar), view->cancel_button);
    g_signal_connect(view->cancel_button, "clicked", G_CALLBACK(on_cancel_button_clicked), view);
    gtk_widget_set_sensitive(view->cancel_button, FALSE);
    
    /* Separator before episode-specific buttons */
    GtkWidget *separator2 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start(separator2, 4);
    gtk_widget_set_margin_end(separator2, 4);
    gtk_box_append(GTK_BOX(toolbar), separator2);
    
    /* Episode-specific buttons */
    view->chapters_button = gtk_button_new_from_icon_name("view-list-symbolic");
    gtk_button_set_label(GTK_BUTTON(view->chapters_button), "Chapters");
    gtk_widget_set_sensitive(view->chapters_button, FALSE);  /* Disabled until chapters available */
    gtk_box_append(GTK_BOX(toolbar), view->chapters_button);
    g_signal_connect(view->chapters_button, "clicked", G_CALLBACK(on_chapters_button_clicked), view);
    
    view->transcript_button = gtk_button_new_from_icon_name("text-x-generic-symbolic");
    gtk_button_set_label(GTK_BUTTON(view->transcript_button), "Transcript");
    gtk_widget_set_sensitive(view->transcript_button, FALSE);  /* Disabled until transcript available */
    gtk_box_append(GTK_BOX(toolbar), view->transcript_button);
    g_signal_connect(view->transcript_button, "clicked", G_CALLBACK(on_transcript_button_clicked), view);
    
    view->support_button = gtk_button_new_from_icon_name("emblem-favorite-symbolic");
    gtk_button_set_label(GTK_BUTTON(view->support_button), "Support");
    gtk_widget_set_sensitive(view->support_button, FALSE);  /* Disabled until funding available */
    gtk_box_append(GTK_BOX(toolbar), view->support_button);
    g_signal_connect(view->support_button, "clicked", G_CALLBACK(on_support_button_clicked), view);
    
    view->value_button = gtk_button_new_from_icon_name("weather-storm-symbolic");
    gtk_button_set_label(GTK_BUTTON(view->value_button), "⚡ Value");
    gtk_widget_set_sensitive(view->value_button, FALSE);  /* Disabled until value tags available */
    gtk_box_append(GTK_BOX(toolbar), view->value_button);
    g_signal_connect(view->value_button, "clicked", G_CALLBACK(on_value_button_clicked), view);
    
    /* Separator before live indicator */
    GtkWidget *separator3 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start(separator3, 4);
    gtk_widget_set_margin_end(separator3, 4);
    gtk_box_append(GTK_BOX(toolbar), separator3);
    
    /* Live indicator - a label that shows when podcast is live */
    view->live_indicator = gtk_label_new("");
    /* Style the live indicator with red background and white text */
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css_provider,
        "label.live-indicator { "
        "  background-color: #ff0000; "
        "  color: white; "
        "  padding: 2px 8px; "
        "  border-radius: 4px; "
        "  font-weight: bold; "
        "}");
    gtk_widget_add_css_class(view->live_indicator, "live-indicator");
    gtk_style_context_add_provider_for_display(
        gtk_widget_get_display(view->live_indicator),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css_provider);
    gtk_box_append(GTK_BOX(toolbar), view->live_indicator);
    gtk_widget_set_visible(view->live_indicator, FALSE);  /* Hidden by default */
    
    /* Live button - to watch/listen to live stream */
    view->live_button = gtk_button_new_from_icon_name("media-playback-start-symbolic");
    gtk_button_set_label(GTK_BUTTON(view->live_button), "Watch/Listen Live");
    gtk_box_append(GTK_BOX(toolbar), view->live_button);
    g_signal_connect(view->live_button, "clicked", G_CALLBACK(on_live_button_clicked), view);
    gtk_widget_set_visible(view->live_button, FALSE);  /* Hidden by default */
    
    view->current_live_items = NULL;
    
    gtk_box_append(GTK_BOX(view->container), toolbar);
    
    /* Progress bar for downloads */
    view->progress_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(view->progress_box, 5);
    gtk_widget_set_margin_end(view->progress_box, 5);
    gtk_widget_set_margin_top(view->progress_box, 5);
    gtk_widget_set_margin_bottom(view->progress_box, 5);
    
    view->progress_label = gtk_label_new("");
    gtk_widget_set_halign(view->progress_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(view->progress_box), view->progress_label);
    
    view->progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(view->progress_bar), TRUE);
    gtk_box_append(GTK_BOX(view->progress_box), view->progress_bar);
    
    gtk_box_append(GTK_BOX(view->container), view->progress_box);
    /* Initially hide the progress box */
    gtk_widget_set_visible(view->progress_box, FALSE);
    
    /* Paned container for podcast list and episode list */
    view->paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(view->paned), 250);
    
    /* Podcast list - GTK4 GListStore/GtkColumnView */
    view->podcast_store = g_list_store_new(SHRIEK_TYPE_PODCAST_OBJECT);
    view->podcast_selection = gtk_single_selection_new(G_LIST_MODEL(g_object_ref(view->podcast_store)));
    gtk_single_selection_set_autoselect(view->podcast_selection, FALSE);
    
    /* Create podcast column view */
    view->podcast_listview = gtk_column_view_new(GTK_SELECTION_MODEL(g_object_ref(view->podcast_selection)));
    gtk_column_view_set_show_column_separators(GTK_COLUMN_VIEW(view->podcast_listview), FALSE);
    gtk_column_view_set_show_row_separators(GTK_COLUMN_VIEW(view->podcast_listview), FALSE);
    
    /* Podcast title column */
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_podcast_title_label), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_podcast_title_label), NULL);
    GtkColumnViewColumn *column = gtk_column_view_column_new("Podcast", factory);
    gtk_column_view_column_set_resizable(column, TRUE);
    gtk_column_view_column_set_expand(column, TRUE);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(view->podcast_listview), column);
    
    /* Connect selection signal */
    view->podcast_selection_handler_id = g_signal_connect(
        view->podcast_selection, "selection-changed", 
        G_CALLBACK(on_podcast_selection_changed), view);
    
    GtkWidget *podcast_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(podcast_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(podcast_scroll), view->podcast_listview);
    gtk_paned_set_start_child(GTK_PANED(view->paned), podcast_scroll);
    gtk_paned_set_shrink_start_child(GTK_PANED(view->paned), TRUE);
    gtk_paned_set_resize_start_child(GTK_PANED(view->paned), FALSE);
    
    /* Episode list - GTK4 GListStore/GtkColumnView */
    view->episode_store = g_list_store_new(SHRIEK_TYPE_EPISODE_OBJECT);
    view->episode_selection = gtk_single_selection_new(G_LIST_MODEL(g_object_ref(view->episode_store)));
    gtk_single_selection_set_autoselect(view->episode_selection, FALSE);
    
    /* Create episode column view */
    view->episode_listview = gtk_column_view_new(GTK_SELECTION_MODEL(g_object_ref(view->episode_selection)));
    gtk_column_view_set_show_column_separators(GTK_COLUMN_VIEW(view->episode_listview), TRUE);
    gtk_column_view_set_show_row_separators(GTK_COLUMN_VIEW(view->episode_listview), FALSE);
    
    /* Episode title column */
    factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_episode_title_label), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_episode_title_label), NULL);
    column = gtk_column_view_column_new("Episode", factory);
    gtk_column_view_column_set_resizable(column, TRUE);
    gtk_column_view_column_set_expand(column, TRUE);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(view->episode_listview), column);
    
    /* Episode date column */
    factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_episode_date_label), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_episode_date_label), NULL);
    column = gtk_column_view_column_new("Date", factory);
    gtk_column_view_column_set_resizable(column, TRUE);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(view->episode_listview), column);
    
    /* Episode duration column */
    factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_episode_duration_label), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_episode_duration_label), NULL);
    column = gtk_column_view_column_new("Duration", factory);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(view->episode_listview), column);
    
    /* Episode downloaded column */
    factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_episode_downloaded_check), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_episode_downloaded_check), NULL);
    column = gtk_column_view_column_new("Downloaded", factory);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(view->episode_listview), column);
    
    /* Connect episode selection signal */
    view->episode_selection_handler_id = g_signal_connect(
        view->episode_selection, "selection-changed", 
        G_CALLBACK(on_episode_selection_changed), view);
    
    /* Connect activate signal for double-click playback using GtkColumnView */
    g_signal_connect(view->episode_listview, "activate", G_CALLBACK(on_episode_activated), view);
    
    GtkWidget *episode_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(episode_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(episode_scroll), view->episode_listview);
    gtk_paned_set_end_child(GTK_PANED(view->paned), episode_scroll);
    gtk_paned_set_shrink_end_child(GTK_PANED(view->paned), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(view->paned), TRUE);
    
    gtk_widget_set_vexpand(view->paned, TRUE);
    gtk_box_append(GTK_BOX(view->container), view->paned);
    
    return view;
}

void podcast_view_free(PodcastView *view) {
    if (!view) return;
    
    /* Mark as destroyed so pending async callbacks know not to touch the view */
    view->destroyed = TRUE;
    
    /* Cancel any active download */
    view->current_download_id = -1;
    
    /* Clean up episode-specific data */
    if (view->current_chapters) {
        g_list_free_full(view->current_chapters, (GDestroyNotify)podcast_chapter_free);
    }
    g_free(view->current_transcript_url);
    g_free(view->current_transcript_type);
    if (view->current_funding) {
        g_list_free_full(view->current_funding, (GDestroyNotify)podcast_funding_free);
    }
    if (view->current_value) {
        g_list_free_full(view->current_value, (GDestroyNotify)podcast_value_free);
    }
    if (view->current_live_items) {
        g_list_free_full(view->current_live_items, (GDestroyNotify)podcast_live_item_free);
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
    gtk_widget_set_margin_start(grid, 12);
    gtk_widget_set_margin_end(grid, 12);
    gtk_widget_set_margin_top(grid, 12);
    gtk_widget_set_margin_bottom(grid, 12);
    
    GtkWidget *label = gtk_label_new("Feed URL:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
    
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "https://example.com/feed.xml");
    gtk_editable_set_width_chars(GTK_EDITABLE(entry), 50);
    gtk_grid_attach(GTK_GRID(grid), entry, 1, 0, 1, 1);
    
    gtk_box_append(GTK_BOX(content), grid);
    
    /* Store data for async dialog handling in GTK4 */
    DialogResponseData data = { FALSE, GTK_RESPONSE_NONE };
    
    g_signal_connect(dialog, "response", G_CALLBACK(on_dialog_response), &data);
    
    gtk_window_present(GTK_WINDOW(dialog));
    
    /* Block until dialog responds */
    while (!data.done) {
        g_main_context_iteration(NULL, TRUE);
    }
    
    gint response = data.response;
    
    if (response == GTK_RESPONSE_ACCEPT) {
        const gchar *feed_url = gtk_editable_get_text(GTK_EDITABLE(entry));
        
        if (feed_url && strlen(feed_url) > 0) {
            /* Show progress dialog using GtkAlertDialog in GTK4 style */
            GtkAlertDialog *progress_alert = gtk_alert_dialog_new("Subscribing to podcast...");
            /* Note: In GTK4, we can't easily show a progress dialog without response buttons.
               For now, we'll just do the subscription synchronously */
            
            /* Subscribe to podcast */
            gboolean success = podcast_manager_subscribe(view->podcast_manager, feed_url);
            
            if (success) {
                /* Refresh podcast list */
                podcast_view_refresh_podcasts(view);
                
                GtkAlertDialog *msg = gtk_alert_dialog_new("Successfully subscribed to podcast!");
                gtk_alert_dialog_show(msg, NULL);
                g_object_unref(msg);
            } else {
                GtkAlertDialog *msg = gtk_alert_dialog_new("Failed to subscribe to podcast. Please check the feed URL.");
                gtk_alert_dialog_show(msg, NULL);
                g_object_unref(msg);
            }
            g_object_unref(progress_alert);
        }
    }
    
    gtk_window_destroy(GTK_WINDOW(dialog));
}

void podcast_view_refresh_podcasts(PodcastView *view) {
    if (!view) return;
    
    g_list_store_remove_all(view->podcast_store);
    
    GList *podcasts = podcast_manager_get_podcasts(view->podcast_manager);
    
    for (GList *l = podcasts; l != NULL; l = l->next) {
        Podcast *podcast = (Podcast *)l->data;
        
        ShriekPodcastObject *obj = shriek_podcast_object_new(
            podcast->id,
            podcast->title ? podcast->title : "Unknown",
            podcast->author ? podcast->author : ""
        );
        g_list_store_append(view->podcast_store, obj);
        g_object_unref(obj);
    }
}

void podcast_view_refresh_episodes(PodcastView *view, gint podcast_id) {
    if (!view) return;
    
    g_list_store_remove_all(view->episode_store);
    
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
        
        ShriekEpisodeObject *obj = shriek_episode_object_new(
            episode->id,
            episode->title ? episode->title : "Unknown",
            date_str,
            duration_str,
            episode->downloaded
        );
        g_list_store_append(view->episode_store, obj);
        g_object_unref(obj);
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
            
            /* Load chapters if available */
            GList *chapters = NULL;
            if (episode->chapters_url || episode->enclosure_url) {
                chapters = podcast_episode_get_chapters(view->podcast_manager, episode_id);
            }
            
            /* Load funding from database */
            GList *funding = database_get_episode_funding(view->database, episode_id);
            
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
                /* Episode already downloaded */
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
    
    if (update->view && !update->view->destroyed && update->view->current_download_id == update->episode_id) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(update->view->progress_bar), update->progress);
        gchar *progress_text = g_strdup_printf("%.0f%%", update->progress * 100);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(update->view->progress_bar), progress_text);
        g_free(progress_text);
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

/* GTK4: Helper to hide widget after timeout (replaces deprecated gtk_widget_hide callback) */
static gboolean hide_progress_box_cb(gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(user_data);
    if (widget && GTK_IS_WIDGET(widget)) {
        gtk_widget_set_visible(widget, FALSE);
    }
    return G_SOURCE_REMOVE;
}

typedef struct {
    PodcastView *view;
    gint episode_id;
    gboolean success;
    gchar *error_msg;
} CompleteUpdate;

static gboolean update_complete_ui(gpointer user_data) {
    CompleteUpdate *update = (CompleteUpdate *)user_data;
    
    if (update->view && !update->view->destroyed && update->view->current_download_id == update->episode_id) {
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
        g_timeout_add_seconds(3, hide_progress_box_cb, update->view->progress_box);
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
        update_support_button_text(view);
    } else {
        gtk_widget_set_sensitive(view->support_button, FALSE);
        gtk_button_set_label(GTK_BUTTON(view->support_button), "Support");
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
    g_list_store_remove_all(view->podcast_store);
    
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
    g_list_store_remove_all(view->episode_store);
    
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
                
                ShriekEpisodeObject *obj = shriek_episode_object_new(
                    episode->id,
                    episode->title ? episode->title : "Unknown",
                    date_str,
                    duration_str,
                    episode->local_file_path != NULL
                );
                g_list_store_append(view->episode_store, obj);
                g_object_unref(obj);
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
            ShriekPodcastObject *obj = shriek_podcast_object_new(
                podcast->id,
                podcast->title ? podcast->title : "Unknown",
                podcast->author ? podcast->author : ""
            );
            g_list_store_append(view->podcast_store, obj);
            g_object_unref(obj);
        }
    }
    
    g_free(search_lower);
    g_hash_table_destroy(podcasts_with_matches);
    g_list_free_full(all_podcasts, (GDestroyNotify)podcast_free);
}

Podcast* podcast_view_get_selected_podcast(PodcastView *view) {
    if (!view || view->selected_podcast_id <= 0) {
        return NULL;
    }
    
    return database_get_podcast_by_id(view->database, view->selected_podcast_id);
}