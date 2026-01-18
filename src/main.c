#include <gtk/gtk.h>
#include <gst/gst.h>
#include <glib.h>
#include "player.h"
#include "database.h"
#include "ui.h"
#include "playlist.h"

#define APP_NAME "Banshee Media Player"
#define VERSION "1.0.0"

typedef struct {
    MediaPlayer *player;
    Database *database;
    MediaPlayerUI *ui;
    PlaylistManager *playlist_manager;
    guint update_timer_id;
    gboolean video_playing;  /* Flag to disable timer during video playback */
} Application;

/* Forward declaration */
static gboolean update_position(gpointer user_data);
static void on_video_position_update(MediaPlayer *player, gint64 position, gint64 duration, gpointer user_data);

typedef struct {
    MediaPlayerUI *ui;
    gint64 position;
    gint64 duration;
} PositionUpdateData;

static gboolean update_position_main_thread(gpointer user_data) {
    PositionUpdateData *data = (PositionUpdateData *)user_data;
    ui_update_position(data->ui, data->position, data->duration);
    g_free(data);
    return G_SOURCE_REMOVE;
}

/* Global app pointer for video view to access */
static Application *g_app = NULL;

void app_set_video_playing(gboolean playing) {
    if (!g_app) return;
    g_app->video_playing = playing;
    /* Position updates now run in GStreamer thread, no GTK timer to manage */
}

void app_set_video_now_playing(const gchar *title) {
    if (!g_app || !g_app->ui) return;
    ui_update_now_playing_video(g_app->ui, title);
}

static void on_video_position_update(MediaPlayer *player, gint64 position, gint64 duration, gpointer user_data) {
    (void)player;  /* Unused */
    Application *app = (Application *)user_data;
    if (app && app->ui) {
        /* Use g_idle_add to ensure UI updates happen on main thread */
        PositionUpdateData *data = g_new0(PositionUpdateData, 1);
        data->ui = app->ui;
        data->position = position;
        data->duration = duration;
        g_idle_add(update_position_main_thread, data);
    }
}

static gboolean update_position(gpointer user_data) {
    Application *app = (Application *)user_data;
    
    if (player_get_state(app->player) == PLAYER_STATE_PLAYING) {
        gint64 position = player_get_position(app->player);
        gint64 duration = player_get_duration(app->player);
        ui_update_position(app->ui, position, duration);
    }
    
    return G_SOURCE_CONTINUE;  /* Continue calling */
}

static void cleanup_application(Application *app) {
    if (app->update_timer_id > 0) {
        g_source_remove(app->update_timer_id);
    }
    
    if (app->ui) {
        ui_free(app->ui);
    }
    
    if (app->playlist_manager) {
        playlist_manager_free(app->playlist_manager);
    }
    
    if (app->player) {
        player_free(app->player);
    }
    
    if (app->database) {
        database_free(app->database);
    }
    
    g_free(app);
}

static Application* init_application(void) {
    Application *app = g_new0(Application, 1);
    
    /* Initialize GStreamer */
    gst_init(NULL, NULL);
    
    /* Create player */
    app->player = player_new();
    if (!app->player) {
        g_printerr("Failed to create media player\n");
        g_free(app);
        return NULL;
    }
    
    /* Initialize database */
    gchar *db_path = g_build_filename(g_get_user_data_dir(), "banshee", "library.db", NULL);
    gchar *db_dir = g_path_get_dirname(db_path);
    g_mkdir_with_parents(db_dir, 0755);
    g_free(db_dir);
    
    app->database = database_new(db_path);
    g_free(db_path);
    
    if (!app->database) {
        g_printerr("Failed to open database\n");
        player_free(app->player);
        g_free(app);
        return NULL;
    }
    
    if (!database_init_tables(app->database)) {
        g_printerr("Failed to initialize database tables\n");
        database_free(app->database);
        player_free(app->player);
        g_free(app);
        return NULL;
    }
    
    /* Create playlist manager */
    app->playlist_manager = playlist_manager_new();
    
    /* Create UI */
    app->ui = ui_new(app->player, app->database);
    if (!app->ui) {
        g_printerr("Failed to create UI\n");
        playlist_manager_free(app->playlist_manager);
        database_free(app->database);
        player_free(app->player);
        g_free(app);
        return NULL;
    }
    
    /* Set up position callback for seek bar updates */
    player_set_position_callback(app->player, (PlayerPositionCallback)on_video_position_update, app);
    
    /* Load tracks and update UI */
    GList *tracks = database_get_all_tracks(app->database);
    ui_update_track_list(app->ui, tracks);
    
    /* Position updates now run in GStreamer's own thread - no GTK timer needed */
    app->video_playing = FALSE;  /* Initialize video flag */
    
    return app;
}

int main(int argc, char *argv[]) {
    /* Initialize GTK */
    gtk_init(&argc, &argv);
    
    /* Create and initialize application */
    Application *app = init_application();
    if (!app) {
        return 1;
    }
    
    g_app = app;  /* Set global pointer for video view access */
    
    g_print("%s v%s\n", APP_NAME, VERSION);
    
    /* UI is shown in ui_new() - no need to call ui_show */
    
    /* Run main loop */
    gtk_main();
    
    /* Cleanup */
    cleanup_application(app);
    
    return 0;
}
