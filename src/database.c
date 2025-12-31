#include "database.h"
#include "podcast.h"
#include <stdio.h>
#include <string.h>

/* SQL statements for table creation */
static const char *CREATE_TRACKS_TABLE = 
    "CREATE TABLE IF NOT EXISTS tracks ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "title TEXT NOT NULL,"
    "artist TEXT,"
    "album TEXT,"
    "genre TEXT,"
    "year INTEGER,"
    "duration INTEGER,"
    "file_path TEXT NOT NULL UNIQUE,"
    "play_count INTEGER DEFAULT 0,"
    "rating INTEGER DEFAULT 0,"
    "last_played INTEGER,"
    "date_added INTEGER"
    ");";

static const char *CREATE_PLAYLISTS_TABLE = 
    "CREATE TABLE IF NOT EXISTS playlists ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "name TEXT NOT NULL,"
    "date_created INTEGER"
    ");";

static const char *CREATE_PLAYLIST_TRACKS_TABLE = 
    "CREATE TABLE IF NOT EXISTS playlist_tracks ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "playlist_id INTEGER,"
    "track_id INTEGER,"
    "position INTEGER,"
    "FOREIGN KEY(playlist_id) REFERENCES playlists(id),"
    "FOREIGN KEY(track_id) REFERENCES tracks(id)"
    ");";

static const char *CREATE_RADIO_STATIONS_TABLE = 
    "CREATE TABLE IF NOT EXISTS radio_stations ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "name TEXT NOT NULL,"
    "url TEXT NOT NULL,"
    "genre TEXT,"
    "description TEXT,"
    "bitrate INTEGER,"
    "homepage TEXT,"
    "date_added INTEGER,"
    "play_count INTEGER DEFAULT 0"
    ");";

static const char *CREATE_PODCASTS_TABLE =
    "CREATE TABLE IF NOT EXISTS podcasts ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "title TEXT NOT NULL,"
    "feed_url TEXT NOT NULL UNIQUE,"
    "link TEXT,"
    "description TEXT,"
    "author TEXT,"
    "image_url TEXT,"
    "language TEXT,"
    "last_updated INTEGER,"
    "last_fetched INTEGER,"
    "auto_download INTEGER DEFAULT 0"
    ");";

static const char *CREATE_PODCAST_EPISODES_TABLE =
    "CREATE TABLE IF NOT EXISTS podcast_episodes ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "podcast_id INTEGER,"
    "guid TEXT NOT NULL,"
    "title TEXT NOT NULL,"
    "description TEXT,"
    "enclosure_url TEXT,"
    "enclosure_length INTEGER,"
    "enclosure_type TEXT,"
    "published_date INTEGER,"
    "duration INTEGER,"
    "downloaded INTEGER DEFAULT 0,"
    "local_file_path TEXT,"
    "play_position INTEGER DEFAULT 0,"
    "played INTEGER DEFAULT 0,"
    "transcript_url TEXT,"
    "transcript_type TEXT,"
    "chapters_url TEXT,"
    "chapters_type TEXT,"
    "location_name TEXT,"
    "location_lat REAL,"
    "location_lon REAL,"
    "locked INTEGER DEFAULT 0,"
    "season TEXT,"
    "episode_num TEXT,"
    "FOREIGN KEY(podcast_id) REFERENCES podcasts(id),"
    "UNIQUE(podcast_id, guid)"
    ");";

static const char *CREATE_PREFERENCES_TABLE =
    "CREATE TABLE IF NOT EXISTS preferences ("
    "key TEXT PRIMARY KEY,"
    "value TEXT"
    ");";

Database* database_new(const gchar *db_path) {
    Database *db = g_new0(Database, 1);
    db->db_path = g_strdup(db_path);
    
    int rc = sqlite3_open(db_path, &db->db);
    if (rc != SQLITE_OK) {
        g_printerr("Cannot open database: %s\n", sqlite3_errmsg(db->db));
        g_free(db->db_path);
        g_free(db);
        return NULL;
    }
    
    return db;
}

void database_free(Database *db) {
    if (!db) return;
    
    if (db->db) {
        sqlite3_close(db->db);
    }
    
    g_free(db->db_path);
    g_free(db);
}

gboolean database_init_tables(Database *db) {
    if (!db || !db->db) return FALSE;
    
    char *err_msg = NULL;
    int rc;
    
    /* Create tracks table */
    rc = sqlite3_exec(db->db, CREATE_TRACKS_TABLE, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_printerr("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }
    
    /* Create playlists table */
    rc = sqlite3_exec(db->db, CREATE_PLAYLISTS_TABLE, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_printerr("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }
    
    /* Create playlist_tracks table */
    rc = sqlite3_exec(db->db, CREATE_PLAYLIST_TRACKS_TABLE, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_printerr("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }
    
    /* Create radio_stations table */
    rc = sqlite3_exec(db->db, CREATE_RADIO_STATIONS_TABLE, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_printerr("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }
    
    /* Create podcasts table */
    rc = sqlite3_exec(db->db, CREATE_PODCASTS_TABLE, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_printerr("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }
    
    /* Create podcast_episodes table */
    rc = sqlite3_exec(db->db, CREATE_PODCAST_EPISODES_TABLE, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_printerr("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }
    
    /* Create preferences table */
    rc = sqlite3_exec(db->db, CREATE_PREFERENCES_TABLE, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_printerr("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }
    
    return TRUE;
}

gint database_add_track(Database *db, Track *track) {
    if (!db || !db->db || !track) return -1;
    
    const char *sql = "INSERT INTO tracks (title, artist, album, genre, duration, file_path, date_added) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?);";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        g_printerr("Failed to prepare statement: %s\n", sqlite3_errmsg(db->db));
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, track->title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, track->artist, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, track->album, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, track->genre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, track->duration);
    sqlite3_bind_text(stmt, 6, track->file_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 7, g_get_real_time() / 1000000);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        g_printerr("Execution failed: %s\n", sqlite3_errmsg(db->db));
        return -1;
    }
    
    return sqlite3_last_insert_rowid(db->db);
}

Track* database_get_track(Database *db, gint track_id) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT id, title, artist, album, genre, duration, file_path, play_count, date_added "
                      "FROM tracks WHERE id = ?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, track_id);
    
    Track *track = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        track = g_new0(Track, 1);
        track->id = sqlite3_column_int(stmt, 0);
        track->title = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        track->artist = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        track->album = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
        track->genre = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
        track->duration = sqlite3_column_int(stmt, 5);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 6));
        track->play_count = sqlite3_column_int(stmt, 7);
        track->date_added = sqlite3_column_int64(stmt, 8);
    }
    
    sqlite3_finalize(stmt);
    return track;
}

GList* database_get_all_tracks(Database *db) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT id, title, artist, album, genre, duration, file_path, play_count, date_added "
                      "FROM tracks ORDER BY title;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    GList *tracks = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Track *track = g_new0(Track, 1);
        track->id = sqlite3_column_int(stmt, 0);
        track->title = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        track->artist = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        track->album = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
        track->genre = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
        track->duration = sqlite3_column_int(stmt, 5);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 6));
        track->play_count = sqlite3_column_int(stmt, 7);
        track->date_added = sqlite3_column_int64(stmt, 8);
        tracks = g_list_append(tracks, track);
    }
    
    sqlite3_finalize(stmt);
    return tracks;
}

GList* database_get_tracks_by_artist(Database *db, const gchar *artist) {
    if (!db || !db->db || !artist) return NULL;
    
    const char *sql = "SELECT id, title, artist, album, genre, duration, file_path, play_count, date_added "
                      "FROM tracks WHERE artist = ? ORDER BY album, title;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    sqlite3_bind_text(stmt, 1, artist, -1, SQLITE_STATIC);
    
    GList *tracks = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Track *track = g_new0(Track, 1);
        track->id = sqlite3_column_int(stmt, 0);
        track->title = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        track->artist = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        track->album = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
        track->genre = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
        track->duration = sqlite3_column_int(stmt, 5);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 6));
        track->play_count = sqlite3_column_int(stmt, 7);
        track->date_added = sqlite3_column_int64(stmt, 8);
        tracks = g_list_append(tracks, track);
    }
    
    sqlite3_finalize(stmt);
    return tracks;
}

GList* database_get_tracks_by_album(Database *db, const gchar *artist, const gchar *album) {
    if (!db || !db->db || !album) return NULL;
    
    const char *sql = artist ? 
        "SELECT id, title, artist, album, genre, duration, file_path, play_count, date_added "
        "FROM tracks WHERE artist = ? AND album = ? ORDER BY title;" :
        "SELECT id, title, artist, album, genre, duration, file_path, play_count, date_added "
        "FROM tracks WHERE album = ? ORDER BY title;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    if (artist) {
        sqlite3_bind_text(stmt, 1, artist, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, album, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_text(stmt, 1, album, -1, SQLITE_STATIC);
    }
    
    GList *tracks = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Track *track = g_new0(Track, 1);
        track->id = sqlite3_column_int(stmt, 0);
        track->title = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        track->artist = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        track->album = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
        track->genre = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
        track->duration = sqlite3_column_int(stmt, 5);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 6));
        track->play_count = sqlite3_column_int(stmt, 7);
        track->date_added = sqlite3_column_int64(stmt, 8);
        tracks = g_list_append(tracks, track);
    }
    
    sqlite3_finalize(stmt);
    return tracks;
}

typedef struct {
    gchar *artist;
    gchar *album;
} AlbumInfo;

GList* database_get_albums_by_artist(Database *db, const gchar *artist) {
    if (!db || !db->db) return NULL;
    
    const char *sql = artist ? 
        "SELECT DISTINCT artist, album FROM tracks WHERE artist = ? AND album IS NOT NULL AND album != '' ORDER BY album;" :
        "SELECT DISTINCT artist, album FROM tracks WHERE album IS NOT NULL AND album != '' ORDER BY artist, album;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    if (artist) {
        sqlite3_bind_text(stmt, 1, artist, -1, SQLITE_STATIC);
    }
    
    GList *albums = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AlbumInfo *info = g_new0(AlbumInfo, 1);
        info->artist = g_strdup((const gchar *)sqlite3_column_text(stmt, 0));
        info->album = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        albums = g_list_append(albums, info);
    }
    
    sqlite3_finalize(stmt);
    return albums;
}

gboolean database_update_track(Database *db, Track *track) {
    if (!db || !db->db || !track) return FALSE;
    
    const char *sql = "UPDATE tracks SET title=?, artist=?, album=?, genre=?, duration=?, "
                      "file_path=?, play_count=? WHERE id=?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return FALSE;
    }
    
    sqlite3_bind_text(stmt, 1, track->title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, track->artist, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, track->album, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, track->genre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, track->duration);
    sqlite3_bind_text(stmt, 6, track->file_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, track->play_count);
    sqlite3_bind_int(stmt, 8, track->id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE);
}

gboolean database_delete_track(Database *db, gint track_id) {
    if (!db || !db->db) return FALSE;
    
    const char *sql = "DELETE FROM tracks WHERE id=?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return FALSE;
    }
    
    sqlite3_bind_int(stmt, 1, track_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE);
}

GList* database_search_tracks(Database *db, const gchar *search_term) {
    if (!db || !db->db || !search_term) return NULL;
    
    const char *sql = "SELECT id, title, artist, album, genre, duration, file_path, play_count, date_added "
                      "FROM tracks WHERE title LIKE ? OR artist LIKE ? OR album LIKE ? ORDER BY title;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    gchar *pattern = g_strdup_printf("%%%s%%", search_term);
    sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, pattern, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, pattern, -1, SQLITE_TRANSIENT);
    g_free(pattern);
    
    GList *tracks = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Track *track = g_new0(Track, 1);
        track->id = sqlite3_column_int(stmt, 0);
        track->title = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        track->artist = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        track->album = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
        track->genre = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
        track->duration = sqlite3_column_int(stmt, 5);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 6));
        track->play_count = sqlite3_column_int(stmt, 7);
        track->date_added = sqlite3_column_int64(stmt, 8);
        tracks = g_list_append(tracks, track);
    }
    
    sqlite3_finalize(stmt);
    return tracks;
}

gint database_create_playlist(Database *db, const gchar *name) {
    if (!db || !db->db || !name) return -1;
    
    const char *sql = "INSERT INTO playlists (name, date_created) VALUES (?, ?);";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, g_get_real_time() / 1000000);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        return -1;
    }
    
    return sqlite3_last_insert_rowid(db->db);
}

GList* database_get_all_playlists(Database *db) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT id, name, date_created FROM playlists ORDER BY name;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    GList *playlists = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Playlist *playlist = g_new0(Playlist, 1);
        playlist->id = sqlite3_column_int(stmt, 0);
        playlist->name = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        playlist->date_created = sqlite3_column_int64(stmt, 2);
        playlists = g_list_append(playlists, playlist);
    }
    
    sqlite3_finalize(stmt);
    return playlists;
}

gboolean database_add_track_to_playlist(Database *db, gint playlist_id, gint track_id) {
    if (!db || !db->db) return FALSE;
    
    /* Get current max position */
    const char *max_sql = "SELECT MAX(position) FROM playlist_tracks WHERE playlist_id=?;";
    sqlite3_stmt *max_stmt;
    sqlite3_prepare_v2(db->db, max_sql, -1, &max_stmt, NULL);
    sqlite3_bind_int(max_stmt, 1, playlist_id);
    
    gint position = 0;
    if (sqlite3_step(max_stmt) == SQLITE_ROW) {
        position = sqlite3_column_int(max_stmt, 0) + 1;
    }
    sqlite3_finalize(max_stmt);
    
    /* Insert track */
    const char *sql = "INSERT INTO playlist_tracks (playlist_id, track_id, position) VALUES (?, ?, ?);";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return FALSE;
    }
    
    sqlite3_bind_int(stmt, 1, playlist_id);
    sqlite3_bind_int(stmt, 2, track_id);
    sqlite3_bind_int(stmt, 3, position);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE);
}

GList* database_get_playlist_tracks(Database *db, gint playlist_id) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT t.id, t.title, t.artist, t.album, t.genre, t.duration, "
                      "t.file_path, t.play_count, t.date_added "
                      "FROM tracks t "
                      "JOIN playlist_tracks pt ON t.id = pt.track_id "
                      "WHERE pt.playlist_id = ? "
                      "ORDER BY pt.position;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, playlist_id);
    
    GList *tracks = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Track *track = g_new0(Track, 1);
        track->id = sqlite3_column_int(stmt, 0);
        track->title = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        track->artist = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        track->album = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
        track->genre = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
        track->duration = sqlite3_column_int(stmt, 5);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 6));
        track->play_count = sqlite3_column_int(stmt, 7);
        track->date_added = sqlite3_column_int64(stmt, 8);
        tracks = g_list_append(tracks, track);
    }
    
    sqlite3_finalize(stmt);
    return tracks;
}

gboolean database_delete_playlist(Database *db, gint playlist_id) {
    if (!db || !db->db) return FALSE;
    
    /* Delete playlist tracks first */
    const char *sql1 = "DELETE FROM playlist_tracks WHERE playlist_id=?;";
    sqlite3_stmt *stmt1;
    sqlite3_prepare_v2(db->db, sql1, -1, &stmt1, NULL);
    sqlite3_bind_int(stmt1, 1, playlist_id);
    sqlite3_step(stmt1);
    sqlite3_finalize(stmt1);
    
    /* Delete playlist */
    const char *sql2 = "DELETE FROM playlists WHERE id=?;";
    sqlite3_stmt *stmt2;
    int rc = sqlite3_prepare_v2(db->db, sql2, -1, &stmt2, NULL);
    
    if (rc != SQLITE_OK) {
        return FALSE;
    }
    
    sqlite3_bind_int(stmt2, 1, playlist_id);
    rc = sqlite3_step(stmt2);
    sqlite3_finalize(stmt2);
    
    return (rc == SQLITE_DONE);
}

gboolean database_increment_play_count(Database *db, gint track_id) {
    if (!db || !db->db) return FALSE;
    
    const char *sql = "UPDATE tracks SET play_count = play_count + 1 WHERE id=?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return FALSE;
    }
    
    sqlite3_bind_int(stmt, 1, track_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE);
}

GList* database_get_most_played_tracks(Database *db, gint limit) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT id, title, artist, album, genre, duration, file_path, play_count, date_added "
                      "FROM tracks WHERE play_count > 0 ORDER BY play_count DESC LIMIT ?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, limit);
    
    GList *tracks = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Track *track = g_new0(Track, 1);
        track->id = sqlite3_column_int(stmt, 0);
        track->title = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        track->artist = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        track->album = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
        track->genre = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
        track->duration = sqlite3_column_int(stmt, 5);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 6));
        track->play_count = sqlite3_column_int(stmt, 7);
        track->date_added = sqlite3_column_int64(stmt, 8);
        tracks = g_list_append(tracks, track);
    }
    
    sqlite3_finalize(stmt);
    return tracks;
}

GList* database_get_recent_tracks(Database *db, gint limit) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT id, title, artist, album, genre, duration, file_path, play_count, date_added "
                      "FROM tracks ORDER BY date_added DESC LIMIT ?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, limit);
    
    GList *tracks = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Track *track = g_new0(Track, 1);
        track->id = sqlite3_column_int(stmt, 0);
        track->title = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        track->artist = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        track->album = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
        track->genre = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
        track->duration = sqlite3_column_int(stmt, 5);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 6));
        track->play_count = sqlite3_column_int(stmt, 7);
        track->date_added = sqlite3_column_int64(stmt, 8);
        tracks = g_list_append(tracks, track);
    }
    
    sqlite3_finalize(stmt);
    return tracks;
}

void track_free(Track *track) {
    if (!track) return;
    
    g_free(track->title);
    g_free(track->artist);
    g_free(track->album);
    g_free(track->genre);
    g_free(track->file_path);
    g_free(track);
}

void playlist_free(Playlist *playlist) {
    if (!playlist) return;
    
    g_free(playlist->name);
    g_free(playlist);
}

/* Aliases for consistency */
void database_free_track(Track *track) {
    track_free(track);
}

void database_free_playlist(Playlist *playlist) {
    playlist_free(playlist);
}

/* Podcast database functions */

gint database_add_podcast(Database *db, const gchar *title, const gchar *feed_url, const gchar *link,
                          const gchar *description, const gchar *author, const gchar *image_url, const gchar *language) {
    if (!db || !db->db) return -1;
    
    const char *sql = "INSERT INTO podcasts (title, feed_url, link, description, author, image_url, language, last_fetched) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;
    
    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, feed_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, link, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, author, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, image_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, language, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 8, g_get_real_time() / 1000000);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) return -1;
    return sqlite3_last_insert_rowid(db->db);
}

gint database_add_podcast_episode(Database *db, gint podcast_id, const gchar *guid, const gchar *title,
                                  const gchar *description, const gchar *enclosure_url, gint64 enclosure_length,
                                  const gchar *enclosure_type, gint64 published_date, gint duration) {
    if (!db || !db->db) return -1;
    
    const char *sql = "INSERT OR IGNORE INTO podcast_episodes "
                      "(podcast_id, guid, title, description, enclosure_url, enclosure_length, enclosure_type, published_date, duration) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;
    
    sqlite3_bind_int(stmt, 1, podcast_id);
    sqlite3_bind_text(stmt, 2, guid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, enclosure_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 6, enclosure_length);
    sqlite3_bind_text(stmt, 7, enclosure_type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 8, published_date);
    sqlite3_bind_int(stmt, 9, duration);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) return -1;
    return sqlite3_last_insert_rowid(db->db);
}

GList* database_get_podcast_episodes(Database *db, gint podcast_id) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT id, guid, title, description, enclosure_url, enclosure_length, enclosure_type, "
                      "published_date, duration, downloaded, local_file_path, play_position, played "
                      "FROM podcast_episodes WHERE podcast_id = ? ORDER BY published_date DESC;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;
    
    sqlite3_bind_int(stmt, 1, podcast_id);
    
    GList *episodes = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PodcastEpisode *episode = g_new0(PodcastEpisode, 1);
        episode->id = sqlite3_column_int(stmt, 0);
        episode->podcast_id = podcast_id;
        episode->guid = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        episode->title = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        episode->description = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
        episode->enclosure_url = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
        episode->enclosure_length = sqlite3_column_int64(stmt, 5);
        episode->enclosure_type = g_strdup((const gchar *)sqlite3_column_text(stmt, 6));
        episode->published_date = sqlite3_column_int64(stmt, 7);
        episode->duration = sqlite3_column_int(stmt, 8);
        episode->downloaded = sqlite3_column_int(stmt, 9);
        episode->local_file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 10));
        episode->play_position = sqlite3_column_int(stmt, 11);
        episode->played = sqlite3_column_int(stmt, 12);
        
        episodes = g_list_append(episodes, episode);
    }
    
    sqlite3_finalize(stmt);
    return episodes;
}

GList* database_get_podcasts(Database *db) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT id, title, feed_url, link, description, author, image_url, language, "
                      "last_updated, last_fetched, auto_download FROM podcasts ORDER BY title;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;
    
    GList *podcasts = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Podcast *podcast = g_new0(Podcast, 1);
        podcast->id = sqlite3_column_int(stmt, 0);
        podcast->title = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        podcast->feed_url = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        podcast->link = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
        podcast->description = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
        podcast->author = g_strdup((const gchar *)sqlite3_column_text(stmt, 5));
        podcast->image_url = g_strdup((const gchar *)sqlite3_column_text(stmt, 6));
        podcast->language = g_strdup((const gchar *)sqlite3_column_text(stmt, 7));
        podcast->last_updated = sqlite3_column_int64(stmt, 8);
        podcast->last_fetched = sqlite3_column_int64(stmt, 9);
        podcast->auto_download = sqlite3_column_int(stmt, 10);
        
        podcasts = g_list_append(podcasts, podcast);
    }
    
    sqlite3_finalize(stmt);
    return podcasts;
}

/* Preference operations */
gboolean database_set_preference(Database *db, const gchar *key, const gchar *value) {
    if (!db || !db->db || !key) {
        g_printerr("database_set_preference: Invalid parameters\n");
        return FALSE;
    }
    
    const char *sql = "INSERT OR REPLACE INTO preferences (key, value) VALUES (?, ?);";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        g_printerr("Failed to prepare statement for set_preference: %s\n", sqlite3_errmsg(db->db));
        return FALSE;
    }
    
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    
    if (value) {
        sqlite3_bind_text(stmt, 2, value, -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 2);
    }
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        g_printerr("Failed to execute set_preference: %s\n", sqlite3_errmsg(db->db));
        sqlite3_finalize(stmt);
        return FALSE;
    }
    
    sqlite3_finalize(stmt);
    return TRUE;
}

gchar* database_get_preference(Database *db, const gchar *key, const gchar *default_value) {
    if (!db) {
        g_printerr("database_get_preference: db is NULL\n");
        return g_strdup(default_value);
    }
    
    if (!db->db) {
        g_printerr("database_get_preference: db->db is NULL\n");
        return g_strdup(default_value);
    }
    
    if (!key) {
        g_printerr("database_get_preference: key is NULL\n");
        return g_strdup(default_value);
    }
    
    const char *sql = "SELECT value FROM preferences WHERE key = ?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        g_printerr("Failed to prepare statement for get_preference: %s\n", sqlite3_errmsg(db->db));
        return g_strdup(default_value);
    }
    
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    
    gchar *result = NULL;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const gchar *value = (const gchar *)sqlite3_column_text(stmt, 0);
        if (value) {
            result = g_strdup(value);
        } else {
            result = g_strdup(default_value);
        }
    } else if (rc == SQLITE_DONE) {
        /* No row found - use default */
        result = g_strdup(default_value);
    } else {
        /* Error occurred */
        g_printerr("Error reading preference '%s': %s\n", key, sqlite3_errmsg(db->db));
        result = g_strdup(default_value);
    }
    
    sqlite3_finalize(stmt);
    return result;
}

gint database_get_preference_int(Database *db, const gchar *key, gint default_value) {
    gchar *value_str = database_get_preference(db, key, NULL);
    if (!value_str) return default_value;
    
    gint result = atoi(value_str);
    g_free(value_str);
    return result;
}

gboolean database_get_preference_bool(Database *db, const gchar *key, gboolean default_value) {
    gchar *value_str = database_get_preference(db, key, NULL);
    if (!value_str) return default_value;
    
    gboolean result = (g_strcmp0(value_str, "true") == 0 || g_strcmp0(value_str, "1") == 0);
    g_free(value_str);
    return result;
}
