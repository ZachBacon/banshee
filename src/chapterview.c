#include "chapterview.h"
#include <string.h>

/* Helper function to format time */
static gchar* format_time(gdouble start_time) {
    gint hours = (gint)(start_time / 3600);
    gint minutes = (gint)((start_time - hours * 3600) / 60);
    gint seconds = (gint)(start_time - hours * 3600 - minutes * 60);
    
    if (hours > 0) {
        return g_strdup_printf("%d:%02d:%02d", hours, minutes, seconds);
    } else {
        return g_strdup_printf("%d:%02d", minutes, seconds);
    }
}

/* GTK4: Chapter row activation handler */
static void on_chapter_activated(GtkColumnView *column_view, guint position, gpointer user_data) {
    ChapterView *view = (ChapterView *)user_data;
    (void)column_view;
    
    ShriekChapterObject *obj = g_list_model_get_item(G_LIST_MODEL(view->store), position);
    if (!obj) return;
    
    gdouble start_time = shriek_chapter_object_get_start_time(obj);
    
    /* Call seek callback if set */
    if (view->seek_callback) {
        view->seek_callback(view->seek_callback_data, start_time);
    }
    
    g_object_unref(obj);
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

/* GTK4 Factory functions for chapter columns */
static void setup_time_label(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_list_item_set_child(list_item, label);
}

static void bind_time_label(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    ShriekChapterObject *obj = gtk_list_item_get_item(list_item);
    if (obj) {
        gchar *time_str = format_time(shriek_chapter_object_get_start_time(obj));
        gtk_label_set_text(GTK_LABEL(label), time_str);
        g_free(time_str);
    }
}

static void setup_title_label(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_list_item_set_child(list_item, label);
}

static void bind_title_label(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    ShriekChapterObject *obj = gtk_list_item_get_item(list_item);
    if (obj) {
        const gchar *title = shriek_chapter_object_get_title(obj);
        gtk_label_set_text(GTK_LABEL(label), title ? title : "Untitled");
    }
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
    
    /* GTK4: Create GListStore for chapters */
    view->store = g_list_store_new(SHRIEK_TYPE_CHAPTER_OBJECT);
    
    /* Create selection model */
    view->selection = gtk_single_selection_new(G_LIST_MODEL(view->store));
    gtk_single_selection_set_autoselect(view->selection, FALSE);
    
    /* Create GtkColumnView */
    view->columnview = gtk_column_view_new(GTK_SELECTION_MODEL(view->selection));
    gtk_column_view_set_show_column_separators(GTK_COLUMN_VIEW(view->columnview), FALSE);
    gtk_column_view_set_show_row_separators(GTK_COLUMN_VIEW(view->columnview), FALSE);
    
    /* Time column */
    GtkListItemFactory *time_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(time_factory, "setup", G_CALLBACK(setup_time_label), NULL);
    g_signal_connect(time_factory, "bind", G_CALLBACK(bind_time_label), NULL);
    GtkColumnViewColumn *time_column = gtk_column_view_column_new("Time", time_factory);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(view->columnview), time_column);
    
    /* Title column */
    GtkListItemFactory *title_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(title_factory, "setup", G_CALLBACK(setup_title_label), NULL);
    g_signal_connect(title_factory, "bind", G_CALLBACK(bind_title_label), NULL);
    GtkColumnViewColumn *title_column = gtk_column_view_column_new("Chapter", title_factory);
    gtk_column_view_column_set_resizable(title_column, TRUE);
    gtk_column_view_column_set_expand(title_column, TRUE);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(view->columnview), title_column);
    
    /* Connect row activation signal */
    g_signal_connect(view->columnview, "activate",
                    G_CALLBACK(on_chapter_activated), view);
    
    /* Add to scrolled window */
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), view->columnview);
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
    
    if (view->store) {
        g_object_unref(view->store);
    }
    
    if (view->selection) {
        g_object_unref(view->selection);
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
        
        ShriekChapterObject *obj = shriek_chapter_object_new(
            chapter->start_time,
            chapter->title ? chapter->title : "Untitled",
            chapter->img,
            chapter->url
        );
        g_list_store_append(view->store, obj);
        g_object_unref(obj);
    }
}

void chapter_view_clear(ChapterView *view) {
    if (!view) return;
    
    g_list_store_remove_all(view->store);
    
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
    guint n_items = g_list_model_get_n_items(G_LIST_MODEL(view->store));
    
    for (guint i = 0; i < n_items; i++) {
        ShriekChapterObject *obj = g_list_model_get_item(G_LIST_MODEL(view->store), i);
        if (obj) {
            gdouble start_time = shriek_chapter_object_get_start_time(obj);
            
            if (start_time == current_chapter->start_time) {
                gtk_single_selection_set_selected(view->selection, i);
                
                /* Scroll to the selected row - GTK4 does this automatically with selection changes */
                g_object_unref(obj);
                break;
            }
            g_object_unref(obj);
        }
    }
}

void chapter_view_set_seek_callback(ChapterView *view, ChapterSeekCallback callback, gpointer user_data) {
    if (!view) return;
    view->seek_callback = callback;
    view->seek_callback_data = user_data;
}
