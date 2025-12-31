#ifndef CHAPTERVIEW_H
#define CHAPTERVIEW_H

#include <gtk/gtk.h>
#include "podcast.h"

/* Callback for chapter seeking */
typedef void (*ChapterSeekCallback)(gpointer user_data, gdouble time);

typedef struct {
    GtkWidget *container;
    GtkWidget *listview;
    GtkListStore *store;
    
    GList *chapters;
    
    /* Callback for seeking */
    ChapterSeekCallback seek_callback;
    gpointer seek_callback_data;
} ChapterView;

/* Chapter view lifecycle */
ChapterView* chapter_view_new(void);
void chapter_view_free(ChapterView *view);
GtkWidget* chapter_view_get_widget(ChapterView *view);

/* Chapter operations */
void chapter_view_set_chapters(ChapterView *view, GList *chapters);
void chapter_view_clear(ChapterView *view);
void chapter_view_highlight_current(ChapterView *view, gdouble current_time);
void chapter_view_set_seek_callback(ChapterView *view, ChapterSeekCallback callback, gpointer user_data);

#endif /* CHAPTERVIEW_H */
