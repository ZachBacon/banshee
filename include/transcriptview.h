#ifndef TRANSCRIPTVIEW_H
#define TRANSCRIPTVIEW_H

#include <gtk/gtk.h>
#include "podcast.h"

/* Transcript segment for time-synced transcripts */
typedef struct {
    gdouble start_time;
    gdouble end_time;
    gchar *text;
} TranscriptSegment;

/* Callback for transcript seeking */
typedef void (*TranscriptSeekCallback)(gpointer user_data, gdouble time);

typedef struct {
    GtkWidget *container;
    GtkWidget *textview;
    GtkTextBuffer *buffer;
    GtkWidget *search_entry;
    GtkWidget *search_button;
    
    /* Transcript data */
    GList *segments;  /* List of TranscriptSegment */
    gchar *full_text; /* Full transcript text */
    
    /* Search functionality */
    GtkTextMark *search_mark;
    
    /* Seeking callback */
    TranscriptSeekCallback seek_callback;
    gpointer seek_callback_data;
} TranscriptView;

/* Transcript view lifecycle */
TranscriptView* transcript_view_new(void);
void transcript_view_free(TranscriptView *view);
GtkWidget* transcript_view_get_widget(TranscriptView *view);

/* Transcript operations */
gboolean transcript_view_load_from_url(TranscriptView *view, const gchar *transcript_url, const gchar *transcript_type);
void transcript_view_set_text(TranscriptView *view, const gchar *text);
void transcript_view_highlight_time(TranscriptView *view, gdouble current_time);
void transcript_view_clear(TranscriptView *view);
void transcript_view_set_seek_callback(TranscriptView *view, TranscriptSeekCallback callback, gpointer user_data);

/* Memory management */
void transcript_segment_free(TranscriptSegment *segment);

#endif /* TRANSCRIPTVIEW_H */