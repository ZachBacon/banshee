#include "transcriptview.h"
#include "podcast.h"
#include <string.h>

static gchar* fetch_transcript_url(const gchar *url) {
    /* Use the fetch_url function from podcast.c */
    return fetch_url(url);
}

static void on_search_clicked(GtkWidget *button, gpointer user_data) {
    TranscriptView *view = (TranscriptView *)user_data;
    (void)button;
    
    const gchar *search_text = gtk_entry_get_text(GTK_ENTRY(view->search_entry));
    if (!search_text || strlen(search_text) == 0) return;
    
    GtkTextIter start, match_start, match_end;
    
    /* Start search from current position or beginning */
    if (view->search_mark) {
        gtk_text_buffer_get_iter_at_mark(view->buffer, &start, view->search_mark);
    } else {
        gtk_text_buffer_get_start_iter(view->buffer, &start);
    }
    
    if (gtk_text_iter_forward_search(&start, search_text, GTK_TEXT_SEARCH_CASE_INSENSITIVE,
                                     &match_start, &match_end, NULL)) {
        /* Highlight the match */
        gtk_text_buffer_select_range(view->buffer, &match_start, &match_end);
        
        /* Scroll to the match */
        gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(view->textview), &match_start, 0.0, FALSE, 0.0, 0.0);
        
        /* Update search mark for next search */
        if (view->search_mark) {
            gtk_text_buffer_move_mark(view->buffer, view->search_mark, &match_end);
        } else {
            view->search_mark = gtk_text_buffer_create_mark(view->buffer, "search_mark", &match_end, FALSE);
        }
    } else {
        /* No more matches, start from beginning */
        gtk_text_buffer_get_start_iter(view->buffer, &start);
        if (gtk_text_iter_forward_search(&start, search_text, GTK_TEXT_SEARCH_CASE_INSENSITIVE,
                                         &match_start, &match_end, NULL)) {
            gtk_text_buffer_select_range(view->buffer, &match_start, &match_end);
            gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(view->textview), &match_start, 0.0, FALSE, 0.0, 0.0);
            
            if (view->search_mark) {
                gtk_text_buffer_move_mark(view->buffer, view->search_mark, &match_end);
            } else {
                view->search_mark = gtk_text_buffer_create_mark(view->buffer, "search_mark", &match_end, FALSE);
            }
        }
    }
}

static void on_search_entry_activate(GtkWidget *entry, gpointer user_data) {
    (void)entry;
    on_search_clicked(NULL, user_data);
}

TranscriptView* transcript_view_new(void) {
    TranscriptView *view = g_new0(TranscriptView, 1);
    
    view->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(view->container, 5);
    gtk_widget_set_margin_end(view->container, 5);
    gtk_widget_set_margin_top(view->container, 5);
    gtk_widget_set_margin_bottom(view->container, 5);
    
    /* Title label */
    GtkWidget *label = gtk_label_new("Transcript");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(label), attrs);
    pango_attr_list_unref(attrs);
    gtk_box_pack_start(GTK_BOX(view->container), label, FALSE, FALSE, 0);
    
    /* Search box */
    GtkWidget *search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    view->search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(view->search_entry), "Search transcript...");
    g_signal_connect(view->search_entry, "activate", G_CALLBACK(on_search_entry_activate), view);
    gtk_box_pack_start(GTK_BOX(search_box), view->search_entry, TRUE, TRUE, 0);
    
    view->search_button = gtk_button_new_with_label("Search");
    g_signal_connect(view->search_button, "clicked", G_CALLBACK(on_search_clicked), view);
    gtk_box_pack_start(GTK_BOX(search_box), view->search_button, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(view->container), search_box, FALSE, FALSE, 0);
    
    /* Text view for transcript */
    view->textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view->textview), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view->textview), GTK_WRAP_WORD);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view->textview), FALSE);
    
    view->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view->textview));
    
    /* Scrolled window for text view */
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), view->textview);
    gtk_box_pack_start(GTK_BOX(view->container), scrolled, TRUE, TRUE, 0);
    
    view->segments = NULL;
    view->full_text = NULL;
    view->search_mark = NULL;
    
    gtk_widget_show_all(view->container);
    
    return view;
}

void transcript_view_free(TranscriptView *view) {
    if (!view) return;
    
    if (view->segments) {
        g_list_free_full(view->segments, (GDestroyNotify)transcript_segment_free);
    }
    
    g_free(view->full_text);
    g_free(view);
}

GtkWidget* transcript_view_get_widget(TranscriptView *view) {
    return view ? view->container : NULL;
}

static GList* parse_simple_json_transcript(const gchar *json_data) {
    GList *segments = NULL;
    
    /* Simple JSON parsing without external library */
    /* Look for "text" fields in the JSON */
    gchar **lines = g_strsplit(json_data, "\n", -1);
    
    for (gint i = 0; lines[i] != NULL; i++) {
        gchar *line = g_strstrip(lines[i]);
        
        /* Look for text fields in JSON */
        gchar *text_start = strstr(line, "\"text\"");
        if (text_start) {
            gchar *colon = strchr(text_start, ':');
            if (colon) {
                gchar *quote_start = strchr(colon, '"');
                if (quote_start) {
                    quote_start++; /* Skip opening quote */
                    gchar *quote_end = strchr(quote_start, '"');
                    if (quote_end) {
                        gchar *text = g_strndup(quote_start, quote_end - quote_start);
                        
                        /* Create a simple segment without timing for now */
                        TranscriptSegment *segment = g_new0(TranscriptSegment, 1);
                        segment->start_time = 0.0;
                        segment->end_time = 0.0;
                        segment->text = text;
                        
                        segments = g_list_append(segments, segment);
                    }
                }
            }
        }
    }
    
    g_strfreev(lines);
    return segments;
}

static gchar* parse_webvtt_transcript(const gchar *vtt_data) {
    /* Simple WebVTT parsing - extract text and format speaker changes */
    GString *text = g_string_new("");
    gchar **lines = g_strsplit(vtt_data, "\n", -1);
    gchar *current_speaker = NULL;
    
    gboolean in_cue = FALSE;
    for (gint i = 0; lines[i] != NULL; i++) {
        gchar *line = g_strstrip(lines[i]);
        
        if (strlen(line) == 0) {
            in_cue = FALSE;
            continue;
        }
        
        /* Skip WEBVTT header and NOTE lines */
        if (g_str_has_prefix(line, "WEBVTT") || g_str_has_prefix(line, "NOTE")) {
            continue;
        }
        
        /* Check if line contains timestamp (format: 00:00:00.000 --> 00:00:00.000) */
        if (strstr(line, "-->")) {
            in_cue = TRUE;
            continue;
        }
        
        /* If we're in a cue and this isn't a timestamp, it's text */
        if (in_cue) {
            /* Check for speaker voice tag: <v Name> */
            if (g_str_has_prefix(line, "<v ")) {
                gchar *end = strchr(line + 3, '>');
                if (end) {
                    gchar *speaker = g_strndup(line + 3, end - (line + 3));
                    
                    /* If speaker changed, start a new line */
                    if (!current_speaker || g_strcmp0(current_speaker, speaker) != 0) {
                        g_free(current_speaker);
                        current_speaker = speaker;
                        
                        /* Add newline before new speaker (if not first) */
                        if (text->len > 0) {
                            g_string_append(text, "\n\n");
                        }
                        g_string_append_printf(text, "<%s> ", current_speaker);
                    } else {
                        g_free(speaker);
                        g_string_append(text, " ");
                    }
                    
                    /* Append the text after the voice tag */
                    gchar *content = end + 1;
                    if (*content) {
                        g_string_append(text, content);
                    }
                    continue;
                }
            }
            
            /* Regular text line (no voice tag) */
            if (text->len > 0 && !g_str_has_suffix(text->str, "\n") && !g_str_has_suffix(text->str, " ")) {
                g_string_append(text, " ");
            }
            g_string_append(text, line);
        }
    }
    
    g_free(current_speaker);
    g_strfreev(lines);
    return g_string_free(text, FALSE);
}

gboolean transcript_view_load_from_url(TranscriptView *view, const gchar *transcript_url, const gchar *transcript_type) {
    if (!view || !transcript_url) return FALSE;
    
    g_print("Loading transcript from: %s (type: %s)\n", transcript_url, transcript_type ? transcript_type : "unknown");
    
    gchar *transcript_data = fetch_transcript_url(transcript_url);
    if (!transcript_data) {
        gtk_text_buffer_set_text(view->buffer, "Failed to load transcript.", -1);
        return FALSE;
    }
    
    transcript_view_clear(view);
    
    /* Determine format and parse accordingly */
    if (transcript_type && (strstr(transcript_type, "json") || g_str_has_suffix(transcript_url, ".json"))) {
        /* JSON format with timestamps */
        view->segments = parse_simple_json_transcript(transcript_data);
        
        if (view->segments) {
            /* Build full text from segments */
            GString *full_text = g_string_new("");
            for (GList *l = view->segments; l != NULL; l = l->next) {
                TranscriptSegment *segment = (TranscriptSegment *)l->data;
                if (full_text->len > 0) {
                    g_string_append(full_text, " ");
                }
                g_string_append(full_text, segment->text);
            }
            view->full_text = g_string_free(full_text, FALSE);
            gtk_text_buffer_set_text(view->buffer, view->full_text, -1);
        } else {
            gtk_text_buffer_set_text(view->buffer, "Failed to parse JSON transcript.", -1);
        }
    } else if (transcript_type && (strstr(transcript_type, "vtt") || g_str_has_suffix(transcript_url, ".vtt"))) {
        /* WebVTT format */
        view->full_text = parse_webvtt_transcript(transcript_data);
        gtk_text_buffer_set_text(view->buffer, view->full_text, -1);
    } else {
        /* Plain text or unknown format */
        view->full_text = g_strdup(transcript_data);
        gtk_text_buffer_set_text(view->buffer, view->full_text, -1);
    }
    
    g_free(transcript_data);
    return TRUE;
}

void transcript_view_set_text(TranscriptView *view, const gchar *text) {
    if (!view) return;
    
    transcript_view_clear(view);
    view->full_text = g_strdup(text);
    gtk_text_buffer_set_text(view->buffer, text ? text : "", -1);
}

void transcript_view_highlight_time(TranscriptView *view, gdouble current_time) {
    if (!view || !view->segments) return;
    
    /* Find the segment that contains the current time */
    for (GList *l = view->segments; l != NULL; l = l->next) {
        TranscriptSegment *segment = (TranscriptSegment *)l->data;
        
        if (current_time >= segment->start_time && current_time <= segment->end_time) {
            /* TODO: Highlight this segment in the text view */
            g_print("Current segment: %.1f-%.1f: %s\n", segment->start_time, segment->end_time, segment->text);
            break;
        }
    }
}

void transcript_view_clear(TranscriptView *view) {
    if (!view) return;
    
    if (view->segments) {
        g_list_free_full(view->segments, (GDestroyNotify)transcript_segment_free);
        view->segments = NULL;
    }
    
    g_free(view->full_text);
    view->full_text = NULL;
    
    gtk_text_buffer_set_text(view->buffer, "", -1);
    
    if (view->search_mark) {
        gtk_text_buffer_delete_mark(view->buffer, view->search_mark);
        view->search_mark = NULL;
    }
}

void transcript_view_set_seek_callback(TranscriptView *view, TranscriptSeekCallback callback, gpointer user_data) {
    if (!view) return;
    view->seek_callback = callback;
    view->seek_callback_data = user_data;
}

void transcript_segment_free(TranscriptSegment *segment) {
    if (!segment) return;
    g_free(segment->text);
    g_free(segment);
}