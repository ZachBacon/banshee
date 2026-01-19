#include "chapterview.h"
#include <string.h>

enum {
    CHAPTER_COL_START_TIME,
    CHAPTER_COL_TITLE,
    CHAPTER_COL_IMG,
    CHAPTER_COL_URL,
    CHAPTER_COL_COUNT
};

static void on_chapter_row_activated(GtkTreeView *treeview, GtkTreePath *path,
                                     GtkTreeViewColumn *column, gpointer user_data) {
    ChapterView *view = (ChapterView *)user_data;
    (void)column;
    
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    GtkTreeIter iter;
    
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gdouble start_time;
        gtk_tree_model_get(model, &iter, CHAPTER_COL_START_TIME, &start_time, -1);
        
        /* Call seek callback if set */
        if (view->seek_callback) {
            view->seek_callback(view->seek_callback_data, start_time);
        }
    }
}

static void format_time_cell(GtkTreeViewColumn *col, GtkCellRenderer *cell,
                             GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    (void)col; (void)data;
    gdouble start_time;
    gtk_tree_model_get(model, iter, CHAPTER_COL_START_TIME, &start_time, -1);
    
    gint hours = (gint)(start_time / 3600);
    gint minutes = (gint)((start_time - hours * 3600) / 60);
    gint seconds = (gint)(start_time - hours * 3600 - minutes * 60);
    
    gchar time_str[32];
    if (hours > 0) {
        g_snprintf(time_str, sizeof(time_str), "%d:%02d:%02d", hours, minutes, seconds);
    } else {
        g_snprintf(time_str, sizeof(time_str), "%d:%02d", minutes, seconds);
    }
    
    g_object_set(cell, "text", time_str, NULL);
}

static gpointer copy_chapter(gconstpointer src, gpointer data) {
    (void)data;
    const PodcastChapter *orig = (const PodcastChapter *)src;
    PodcastChapter *copy = g_new0(PodcastChapter, 1);
    copy->start_time = orig->start_time;
    copy->title = g_strdup(orig->title);
    copy->img = g_strdup(orig->img);
    copy->url = g_strdup(orig->url);
    return copy;
}

ChapterView* chapter_view_new(void) {
    ChapterView *view = g_new0(ChapterView, 1);
    
    view->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    /* Add label */
    GtkWidget *label = gtk_label_new("Chapters");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(label, 6);
    gtk_widget_set_margin_top(label, 6);
    gtk_widget_set_margin_bottom(label, 6);
    gtk_box_append(GTK_BOX(view->container), label);
    
    /* Create list store */
    view->store = gtk_list_store_new(CHAPTER_COL_COUNT,
                                     G_TYPE_DOUBLE,   /* Start time */
                                     G_TYPE_STRING,   /* Title */
                                     G_TYPE_STRING,   /* Image URL */
                                     G_TYPE_STRING);  /* URL */
    
    /* Create tree view */
    view->listview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(view->store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view->listview), TRUE);
    
    /* Time column */
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        "Time", renderer, "text", CHAPTER_COL_START_TIME, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
        format_time_cell, NULL, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view->listview), column);
    
    /* Title column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "Chapter", renderer, "text", CHAPTER_COL_TITLE, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view->listview), column);
    
    /* Connect row activation signal */
    g_signal_connect(view->listview, "row-activated",
                    G_CALLBACK(on_chapter_row_activated), view);
    
    /* Add to scrolled window */
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), view->listview);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_append(GTK_BOX(view->container), scrolled);

    /* GTK4: widgets are visible by default */
    return view;
}

void chapter_view_free(ChapterView *view) {
    if (!view) return;
    
    if (view->chapters) {
        g_list_free_full(view->chapters, (GDestroyNotify)podcast_chapter_free);
    }
    
    g_free(view);
}

GtkWidget* chapter_view_get_widget(ChapterView *view) {
    return view ? view->container : NULL;
}

void chapter_view_set_chapters(ChapterView *view, GList *chapters) {
    if (!view) return;
    
    /* Clear existing chapters */
    chapter_view_clear(view);
    
    /* Store new chapters */
    view->chapters = g_list_copy_deep(chapters, copy_chapter, NULL);
    
    /* Populate list store */
    for (GList *l = chapters; l != NULL; l = l->next) {
        PodcastChapter *chapter = (PodcastChapter *)l->data;
        
        GtkTreeIter iter;
        gtk_list_store_append(view->store, &iter);
        gtk_list_store_set(view->store, &iter,
                          CHAPTER_COL_START_TIME, chapter->start_time,
                          CHAPTER_COL_TITLE, chapter->title ? chapter->title : "Untitled",
                          CHAPTER_COL_IMG, chapter->img,
                          CHAPTER_COL_URL, chapter->url,
                          -1);
    }
}

void chapter_view_clear(ChapterView *view) {
    if (!view) return;
    
    gtk_list_store_clear(view->store);
    
    if (view->chapters) {
        g_list_free_full(view->chapters, (GDestroyNotify)podcast_chapter_free);
        view->chapters = NULL;
    }
}

void chapter_view_highlight_current(ChapterView *view, gdouble current_time) {
    if (!view || !view->chapters) return;
    
    /* Find the current chapter based on time */
    PodcastChapter *current_chapter = podcast_chapter_at_time(view->chapters, current_time);
    if (!current_chapter) return;
    
    /* Find and select the corresponding row */
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(view->store), &iter);
    
    while (valid) {
        gdouble start_time;
        gtk_tree_model_get(GTK_TREE_MODEL(view->store), &iter,
                          CHAPTER_COL_START_TIME, &start_time, -1);
        
        if (start_time == current_chapter->start_time) {
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view->listview));
            gtk_tree_selection_select_iter(selection, &iter);
            
            /* Scroll to the selected row */
            GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(view->store), &iter);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(view->listview), path, NULL, FALSE, 0, 0);
            gtk_tree_path_free(path);
            break;
        }
        
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(view->store), &iter);
    }
}

void chapter_view_set_seek_callback(ChapterView *view, ChapterSeekCallback callback, gpointer user_data) {
    if (!view) return;
    view->seek_callback = callback;
    view->seek_callback_data = user_data;
}
