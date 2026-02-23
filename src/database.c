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
    "track_number INTEGER DEFAULT 0,"
    "year INTEGER,"
    "duration INTEGER,"
    "file_path TEXT NOT NULL UNIQUE,"
    "play_count INTEGER DEFAULT 0,"
    "rating INTEGER DEFAULT 0,"
    "last_played INTEGER,"
    "date_added INTEGER,"
    "is_favorite INTEGER DEFAULT 0"
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

static const char *CREATE_FUNDING_TABLE =
    "CREATE TABLE IF NOT EXISTS episode_funding ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "episode_id INTEGER,"
    "url TEXT NOT NULL,"
    "message TEXT,"
    "platform TEXT,"
    "FOREIGN KEY(episode_id) REFERENCES podcast_episodes(id) ON DELETE CASCADE"
    ");";

static const char *CREATE_PODCAST_FUNDING_TABLE =
    "CREATE TABLE IF NOT EXISTS podcast_funding ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "podcast_id INTEGER,"
    "url TEXT NOT NULL,"
    "message TEXT,"
    "platform TEXT,"
    "FOREIGN KEY(podcast_id) REFERENCES podcasts(id) ON DELETE CASCADE"
    ");";

static const char *CREATE_PREFERENCES_TABLE =
    "CREATE TABLE IF NOT EXISTS preferences ("
    "key TEXT PRIMARY KEY,"
    "value TEXT"
    ");";

static const char *CREATE_PODCAST_VALUE_TABLE =
    "CREATE TABLE IF NOT EXISTS podcast_value ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "podcast_id INTEGER,"
    "type TEXT NOT NULL,"
    "method TEXT NOT NULL,"
    "suggested TEXT,"
    "FOREIGN KEY(podcast_id) REFERENCES podcasts(id) ON DELETE CASCADE"
    ");";

static const char *CREATE_EPISODE_VALUE_TABLE =
    "CREATE TABLE IF NOT EXISTS episode_value ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "episode_id INTEGER,"
    "type TEXT NOT NULL,"
    "method TEXT NOT NULL,"
    "suggested TEXT,"
    "FOREIGN KEY(episode_id) REFERENCES podcast_episodes(id) ON DELETE CASCADE"
    ");";

static const char *CREATE_VALUE_RECIPIENTS_TABLE =
    "CREATE TABLE IF NOT EXISTS value_recipients ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "value_id INTEGER,"
    "value_type TEXT NOT NULL,"
    "name TEXT,"
    "recipient_type TEXT,"
    "address TEXT,"
    "split INTEGER,"
    "fee INTEGER DEFAULT 0,"
    "custom_key TEXT,"
    "custom_value TEXT,"
    "FOREIGN KEY(value_id) REFERENCES podcast_value(id) ON DELETE CASCADE,"
    "FOREIGN KEY(value_id) REFERENCES episode_value(id) ON DELETE CASCADE"
    ");";

static const char *CREATE_LIVE_ITEMS_TABLE =
    "CREATE TABLE IF NOT EXISTS podcast_live_items ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "podcast_id INTEGER NOT NULL,"
    "guid TEXT,"
    "title TEXT,"
    "description TEXT,"
    "enclosure_url TEXT,"
    "enclosure_type TEXT,"
    "enclosure_length INTEGER,"
    "start_time INTEGER,"
    "end_time INTEGER,"
    "status TEXT NOT NULL DEFAULT 'pending',"
    "image_url TEXT,"
    "FOREIGN KEY(podcast_id) REFERENCES podcasts(id) ON DELETE CASCADE,"
    "UNIQUE(podcast_id, guid)"
    ");";

static const char *CREATE_LIVE_ITEM_CONTENT_LINKS_TABLE =
    "CREATE TABLE IF NOT EXISTS live_item_content_links ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "live_item_id INTEGER NOT NULL,"
    "href TEXT NOT NULL,"
    "text TEXT,"
    "FOREIGN KEY(live_item_id) REFERENCES podcast_live_items(id) ON DELETE CASCADE"
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
    
    /* Enable foreign key enforcement */
    sqlite3_exec(db->db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
    
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

gboolean database_begin_transaction(Database *db) {
    if (!db || !db->db) return FALSE;
    char *err_msg = NULL;
    int rc = sqlite3_exec(db->db, "BEGIN TRANSACTION;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_warning("Failed to begin transaction: %s", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }
    return TRUE;
}

gboolean database_commit_transaction(Database *db) {
    if (!db || !db->db) return FALSE;
    char *err_msg = NULL;
    int rc = sqlite3_exec(db->db, "COMMIT;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_warning("Failed to commit transaction: %s", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }
    return TRUE;
}

gboolean database_rollback_transaction(Database *db) {
    if (!db || !db->db) return FALSE;
    char *err_msg = NULL;
    int rc = sqlite3_exec(db->db, "ROLLBACK;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_warning("Failed to rollback transaction: %s", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }
    return TRUE;
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
    
    /* Create episode_funding table */
    rc = sqlite3_exec(db->db, CREATE_FUNDING_TABLE, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_printerr("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }
    
    /* Create podcast_funding table */
    rc = sqlite3_exec(db->db, CREATE_PODCAST_FUNDING_TABLE, NULL, NULL, &err_msg);
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
    
    /* Create podcast_value table */
    rc = sqlite3_exec(db->db, CREATE_PODCAST_VALUE_TABLE, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_printerr("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }
    
    /* Create episode_value table */
    rc = sqlite3_exec(db->db, CREATE_EPISODE_VALUE_TABLE, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_printerr("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }
    
    /* Create value_recipients table */
    rc = sqlite3_exec(db->db, CREATE_VALUE_RECIPIENTS_TABLE, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_printerr("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }
    
    /* Create podcast_live_items table */
    rc = sqlite3_exec(db->db, CREATE_LIVE_ITEMS_TABLE, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_printerr("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }
    
    /* Create live_item_content_links table */
    rc = sqlite3_exec(db->db, CREATE_LIVE_ITEM_CONTENT_LINKS_TABLE, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        g_printerr("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return FALSE;
    }
    
    /* Migration: Add track_number column to existing tracks table if it doesn't exist */
    rc = sqlite3_exec(db->db, "ALTER TABLE tracks ADD COLUMN track_number INTEGER DEFAULT 0;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        /* Column may already exist, that's fine */
        sqlite3_free(err_msg);
    }
    
    /* Migration: Add is_favorite column to existing tracks table if it doesn't exist */
    rc = sqlite3_exec(db->db, "ALTER TABLE tracks ADD COLUMN is_favorite INTEGER DEFAULT 0;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        /* Column may already exist, that's fine */
        sqlite3_free(err_msg);
    }
    
    return TRUE;
}

gint database_add_track(Database *db, Track *track) {
    if (!db || !db->db || !track) return -1;
    
    const char *sql = "INSERT INTO tracks (title, artist, album, genre, track_number, duration, file_path, date_added) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    
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
    sqlite3_bind_int(stmt, 5, track->track_number);
    sqlite3_bind_int(stmt, 6, track->duration);
    sqlite3_bind_text(stmt, 7, track->file_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 8, g_get_real_time() / 1000000);
    
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
    
    const char *sql = "SELECT id, title, artist, album, genre, track_number, duration, file_path, play_count, date_added "
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
        track->track_number = sqlite3_column_int(stmt, 5);
        track->duration = sqlite3_column_int(stmt, 6);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 7));
        track->play_count = sqlite3_column_int(stmt, 8);
        track->date_added = sqlite3_column_int64(stmt, 9);
    }
    
    sqlite3_finalize(stmt);
    return track;
}

GList* database_get_all_tracks(Database *db) {
    if (!db || !db->db) return NULL;
    
    /* Get only audio files from tracks table (positive filter for audio extensions) */
    const char *sql = "SELECT id, title, artist, album, genre, track_number, duration, file_path, play_count, date_added "
                      "FROM tracks WHERE "
                      AUDIO_EXT_FILTER
                      " ORDER BY artist, album, track_number, title;";
    
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
        track->track_number = sqlite3_column_int(stmt, 5);
        track->duration = sqlite3_column_int(stmt, 6);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 7));
        track->play_count = sqlite3_column_int(stmt, 8);
        track->date_added = sqlite3_column_int64(stmt, 9);
        
        tracks = g_list_prepend(tracks, track);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(tracks);
}

gint database_get_audio_track_count(Database *db) {
    if (!db || !db->db) return 0;
    
    const char *sql = "SELECT COUNT(*) FROM tracks WHERE "
                      AUDIO_EXT_FILTER ";";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return 0;
    }
    
    gint count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

GList* database_get_tracks_by_artist(Database *db, const gchar *artist) {
    if (!db || !db->db || !artist) return NULL;
    
    const char *sql = "SELECT id, title, artist, album, genre, track_number, duration, file_path, play_count, date_added "
                      "FROM tracks WHERE artist = ? AND "
                      AUDIO_EXT_FILTER " ORDER BY album, track_number, title;";
    
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
        track->track_number = sqlite3_column_int(stmt, 5);
        track->duration = sqlite3_column_int(stmt, 6);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 7));
        track->play_count = sqlite3_column_int(stmt, 8);
        track->date_added = sqlite3_column_int64(stmt, 9);
        tracks = g_list_prepend(tracks, track);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(tracks);
}

GList* database_get_tracks_by_album(Database *db, const gchar *artist, const gchar *album) {
    if (!db || !db->db || !album) return NULL;
    
    const char *sql = artist ? 
        "SELECT id, title, artist, album, genre, track_number, duration, file_path, play_count, date_added "
        "FROM tracks WHERE artist = ? AND album = ? AND "
        AUDIO_EXT_FILTER " ORDER BY track_number, title;" :
        "SELECT id, title, artist, album, genre, track_number, duration, file_path, play_count, date_added "
        "FROM tracks WHERE album = ? AND "
        AUDIO_EXT_FILTER " ORDER BY track_number, title;";
    
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
        track->track_number = sqlite3_column_int(stmt, 5);
        track->duration = sqlite3_column_int(stmt, 6);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 7));
        track->play_count = sqlite3_column_int(stmt, 8);
        track->date_added = sqlite3_column_int64(stmt, 9);
        tracks = g_list_prepend(tracks, track);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(tracks);
}

typedef struct {
    gchar *artist;
    gchar *album;
} AlbumInfo;

GList* database_get_albums_by_artist(Database *db, const gchar *artist) {
    if (!db || !db->db) return NULL;
    
    const char *sql = artist ? 
        "SELECT DISTINCT artist, album FROM tracks WHERE artist = ? AND album IS NOT NULL AND album != '' AND "
        AUDIO_EXT_FILTER " ORDER BY album;" :
        "SELECT DISTINCT artist, album FROM tracks WHERE album IS NOT NULL AND album != '' AND "
        AUDIO_EXT_FILTER " ORDER BY artist, album;";
    
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
        albums = g_list_prepend(albums, info);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(albums);
}

gboolean database_update_track(Database *db, Track *track) {
    if (!db || !db->db || !track) return FALSE;
    
    const char *sql = "UPDATE tracks SET title=?, artist=?, album=?, genre=?, duration=?, "
                      "file_path=?, play_count=? WHERE id=?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        g_warning("database_update_track: prepare failed: %s", sqlite3_errmsg(db->db));
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
        g_warning("database_delete_track: prepare failed: %s", sqlite3_errmsg(db->db));
        return FALSE;
    }
    
    sqlite3_bind_int(stmt, 1, track_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE);
}

GList* database_search_tracks(Database *db, const gchar *search_term) {
    if (!db || !db->db || !search_term) return NULL;
    
    const char *sql = "SELECT id, title, artist, album, genre, track_number, duration, file_path, play_count, date_added "
                      "FROM tracks WHERE (title LIKE ? OR artist LIKE ? OR album LIKE ?) AND "
                      AUDIO_EXT_FILTER " ORDER BY artist, album, track_number, title;";
    
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
        track->track_number = sqlite3_column_int(stmt, 5);
        track->duration = sqlite3_column_int(stmt, 6);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 7));
        track->play_count = sqlite3_column_int(stmt, 8);
        track->date_added = sqlite3_column_int64(stmt, 9);
        tracks = g_list_prepend(tracks, track);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(tracks);
}

/* Video operations */
GList* database_get_all_videos(Database *db) {
    if (!db || !db->db) return NULL;
    
    /* Get all files and filter by video extensions (case-insensitive) */
    const char *sql = "SELECT id, title, artist, album, genre, track_number, duration, file_path, play_count, date_added "
                      "FROM tracks WHERE "
                      VIDEO_EXT_FILTER
                      " ORDER BY title;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return NULL;
    }
    
    GList *videos = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Track *video = g_new0(Track, 1);
        video->id = sqlite3_column_int(stmt, 0);
        video->title = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        video->artist = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        video->album = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
        video->genre = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
        video->track_number = sqlite3_column_int(stmt, 5);
        video->duration = sqlite3_column_int(stmt, 6);
        video->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 7));
        video->play_count = sqlite3_column_int(stmt, 8);
        video->date_added = sqlite3_column_int64(stmt, 9);
        videos = g_list_prepend(videos, video);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(videos);
}

GList* database_search_videos(Database *db, const gchar *search_term) {
    if (!db || !db->db || !search_term) return NULL;
    
    const char *sql = "SELECT id, title, artist, album, genre, track_number, duration, file_path, play_count, date_added "
                      "FROM tracks WHERE (title LIKE ? OR artist LIKE ? OR album LIKE ?) AND "
                      VIDEO_EXT_FILTER " ORDER BY title;";
    
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
    
    GList *videos = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Track *video = g_new0(Track, 1);
        video->id = sqlite3_column_int(stmt, 0);
        video->title = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        video->artist = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        video->album = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
        video->genre = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
        video->track_number = sqlite3_column_int(stmt, 5);
        video->duration = sqlite3_column_int(stmt, 6);
        video->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 7));
        video->play_count = sqlite3_column_int(stmt, 8);
        video->date_added = sqlite3_column_int64(stmt, 9);
        videos = g_list_prepend(videos, video);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(videos);
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
        playlists = g_list_prepend(playlists, playlist);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(playlists);
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
        g_warning("database_add_track_to_playlist: prepare failed: %s", sqlite3_errmsg(db->db));
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
                      "WHERE pt.playlist_id = ? AND "
                      "(LOWER(t.file_path) LIKE '%.mp3' OR LOWER(t.file_path) LIKE '%.ogg' OR LOWER(t.file_path) LIKE '%.flac' OR "
                      "LOWER(t.file_path) LIKE '%.wav' OR LOWER(t.file_path) LIKE '%.m4a' OR LOWER(t.file_path) LIKE '%.aac' OR "
                      "LOWER(t.file_path) LIKE '%.opus' OR LOWER(t.file_path) LIKE '%.wma' OR LOWER(t.file_path) LIKE '%.ape' OR "
                      "LOWER(t.file_path) LIKE '%.mpc') "
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
        tracks = g_list_prepend(tracks, track);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(tracks);
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
        g_warning("database_delete_playlist: prepare failed: %s", sqlite3_errmsg(db->db));
        return FALSE;
    }
    
    sqlite3_bind_int(stmt2, 1, playlist_id);
    rc = sqlite3_step(stmt2);
    sqlite3_finalize(stmt2);
    
    return (rc == SQLITE_DONE);
}

gboolean database_increment_play_count(Database *db, gint track_id) {
    if (!db || !db->db) return FALSE;
    
    gint64 now = g_get_real_time() / 1000000;  /* Convert to seconds */
    const char *sql = "UPDATE tracks SET play_count = play_count + 1, last_played = ? WHERE id=?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return FALSE;
    }
    
    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_int(stmt, 2, track_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE);
}

gboolean database_toggle_favorite(Database *db, gint track_id) {
    if (!db || !db->db) return FALSE;
    
    const char *sql = "UPDATE tracks SET is_favorite = NOT is_favorite WHERE id=?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        g_warning("database_toggle_favorite: prepare failed: %s", sqlite3_errmsg(db->db));
        return FALSE;
    }
    
    sqlite3_bind_int(stmt, 1, track_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE);
}

gboolean database_set_favorite(Database *db, gint track_id, gboolean is_favorite) {
    if (!db || !db->db) return FALSE;
    
    const char *sql = "UPDATE tracks SET is_favorite = ? WHERE id=?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        g_warning("database_set_favorite: prepare failed: %s", sqlite3_errmsg(db->db));
        return FALSE;
    }
    
    sqlite3_bind_int(stmt, 1, is_favorite ? 1 : 0);
    sqlite3_bind_int(stmt, 2, track_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE);
}

gboolean database_is_favorite(Database *db, gint track_id) {
    if (!db || !db->db) return FALSE;
    
    const char *sql = "SELECT is_favorite FROM tracks WHERE id=?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return FALSE;
    }
    
    sqlite3_bind_int(stmt, 1, track_id);
    
    gboolean is_fav = FALSE;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        is_fav = sqlite3_column_int(stmt, 0) != 0;
    }
    
    sqlite3_finalize(stmt);
    return is_fav;
}

GList* database_get_favorite_tracks(Database *db, gint limit) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT id, title, artist, album, genre, track_number, duration, file_path, play_count, date_added, last_played, is_favorite "
                      "FROM tracks WHERE is_favorite = 1 AND "
                      AUDIO_EXT_FILTER " ORDER BY title ASC LIMIT ?;";
    
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
        track->track_number = sqlite3_column_int(stmt, 5);
        track->duration = sqlite3_column_int(stmt, 6);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 7));
        track->play_count = sqlite3_column_int(stmt, 8);
        track->date_added = sqlite3_column_int64(stmt, 9);
        track->last_played = sqlite3_column_int64(stmt, 10);
        track->is_favorite = sqlite3_column_int(stmt, 11) != 0;
        tracks = g_list_prepend(tracks, track);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(tracks);
}

GList* database_get_most_played_tracks(Database *db, gint limit) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT id, title, artist, album, genre, track_number, duration, file_path, play_count, date_added "
                      "FROM tracks WHERE play_count > 0 AND "
                      AUDIO_EXT_FILTER " ORDER BY play_count DESC LIMIT ?;";
    
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
        track->track_number = sqlite3_column_int(stmt, 5);
        track->duration = sqlite3_column_int(stmt, 6);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 7));
        track->play_count = sqlite3_column_int(stmt, 8);
        track->date_added = sqlite3_column_int64(stmt, 9);
        tracks = g_list_prepend(tracks, track);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(tracks);
}

GList* database_get_recent_tracks(Database *db, gint limit) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT id, title, artist, album, genre, track_number, duration, file_path, play_count, date_added "
                      "FROM tracks WHERE "
                      AUDIO_EXT_FILTER " ORDER BY date_added DESC LIMIT ?;";
    
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
        track->track_number = sqlite3_column_int(stmt, 5);
        track->duration = sqlite3_column_int(stmt, 6);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 7));
        track->play_count = sqlite3_column_int(stmt, 8);
        track->date_added = sqlite3_column_int64(stmt, 9);
        tracks = g_list_prepend(tracks, track);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(tracks);
}

GList* database_get_recently_played_tracks(Database *db, gint limit) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT id, title, artist, album, genre, track_number, duration, file_path, play_count, date_added, last_played "
                      "FROM tracks WHERE last_played IS NOT NULL AND last_played > 0 AND "
                      AUDIO_EXT_FILTER " ORDER BY last_played DESC LIMIT ?;";
    
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
        track->track_number = sqlite3_column_int(stmt, 5);
        track->duration = sqlite3_column_int(stmt, 6);
        track->file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 7));
        track->play_count = sqlite3_column_int(stmt, 8);
        track->date_added = sqlite3_column_int64(stmt, 9);
        track->last_played = sqlite3_column_int64(stmt, 10);
        tracks = g_list_prepend(tracks, track);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(tracks);
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
                                  const gchar *enclosure_type, gint64 published_date, gint duration,
                                  const gchar *chapters_url, const gchar *chapters_type,
                                  const gchar *transcript_url, const gchar *transcript_type) {
    if (!db || !db->db) return -1;
    
    const char *sql = "INSERT INTO podcast_episodes "
                      "(podcast_id, guid, title, description, enclosure_url, enclosure_length, enclosure_type, published_date, duration, "
                      "chapters_url, chapters_type, transcript_url, transcript_type) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(podcast_id, guid) DO UPDATE SET "
                      "title=excluded.title, description=excluded.description, "
                      "chapters_url=excluded.chapters_url, chapters_type=excluded.chapters_type, "
                      "transcript_url=excluded.transcript_url, transcript_type=excluded.transcript_type;";
    
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
    sqlite3_bind_text(stmt, 10, chapters_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, chapters_type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, transcript_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, transcript_type, -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) return -1;
    return sqlite3_last_insert_rowid(db->db);
}

GList* database_get_podcast_episodes(Database *db, gint podcast_id) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT id, guid, title, description, enclosure_url, enclosure_length, enclosure_type, "
                      "published_date, duration, downloaded, local_file_path, play_position, played, "
                      "chapters_url, chapters_type, transcript_url, transcript_type "
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
        episode->chapters_url = g_strdup((const gchar *)sqlite3_column_text(stmt, 13));
        episode->chapters_type = g_strdup((const gchar *)sqlite3_column_text(stmt, 14));
        episode->transcript_url = g_strdup((const gchar *)sqlite3_column_text(stmt, 15));
        episode->transcript_type = g_strdup((const gchar *)sqlite3_column_text(stmt, 16));
        
        /* Load value information */
        episode->value = database_load_episode_value(db, episode->id);
        
        episodes = g_list_prepend(episodes, episode);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(episodes);
}

PodcastEpisode* database_get_episode_by_id(Database *db, gint episode_id) {
    if (!db || !db->db || episode_id <= 0) return NULL;
    
    const char *sql = "SELECT id, podcast_id, guid, title, description, enclosure_url, enclosure_length, enclosure_type, "
                      "published_date, duration, downloaded, local_file_path, play_position, played, "
                      "chapters_url, chapters_type, transcript_url, transcript_type "
                      "FROM podcast_episodes WHERE id = ?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;
    
    sqlite3_bind_int(stmt, 1, episode_id);
    
    PodcastEpisode *episode = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        episode = g_new0(PodcastEpisode, 1);
        episode->id = sqlite3_column_int(stmt, 0);
        episode->podcast_id = sqlite3_column_int(stmt, 1);
        episode->guid = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        episode->title = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
        episode->description = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
        episode->enclosure_url = g_strdup((const gchar *)sqlite3_column_text(stmt, 5));
        episode->enclosure_length = sqlite3_column_int64(stmt, 6);
        episode->enclosure_type = g_strdup((const gchar *)sqlite3_column_text(stmt, 7));
        episode->published_date = sqlite3_column_int64(stmt, 8);
        episode->duration = sqlite3_column_int(stmt, 9);
        episode->downloaded = sqlite3_column_int(stmt, 10);
        episode->local_file_path = g_strdup((const gchar *)sqlite3_column_text(stmt, 11));
        episode->play_position = sqlite3_column_int(stmt, 12);
        episode->played = sqlite3_column_int(stmt, 13);
        episode->chapters_url = g_strdup((const gchar *)sqlite3_column_text(stmt, 14));
        episode->chapters_type = g_strdup((const gchar *)sqlite3_column_text(stmt, 15));
        episode->transcript_url = g_strdup((const gchar *)sqlite3_column_text(stmt, 16));
        episode->transcript_type = g_strdup((const gchar *)sqlite3_column_text(stmt, 17));
        
        /* Load funding information */
        episode->funding = database_get_episode_funding(db, episode_id);
        
        /* Load value information */
        episode->value = database_load_episode_value(db, episode_id);
    }
    
    sqlite3_finalize(stmt);
    return episode;
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
        
        /* Load funding information */
        podcast->funding = database_load_podcast_funding(db, podcast->id);
        
        /* Initialize fields not stored in database */
        podcast->images = NULL;
        podcast->value = database_load_podcast_value(db, podcast->id);
        
        podcasts = g_list_prepend(podcasts, podcast);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(podcasts);
}

Podcast* database_get_podcast_by_id(Database *db, gint podcast_id) {
    if (!db || !db->db || podcast_id <= 0) return NULL;
    
    const char *sql = "SELECT id, title, feed_url, link, description, author, image_url, language, "
                      "last_updated, last_fetched, auto_download FROM podcasts WHERE id = ?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;
    
    sqlite3_bind_int(stmt, 1, podcast_id);
    
    Podcast *podcast = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        podcast = g_new0(Podcast, 1);
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
        
        /* Load funding information */
        podcast->funding = database_load_podcast_funding(db, podcast_id);
        
        /* Initialize fields not stored in database */
        podcast->images = NULL;
        podcast->value = database_load_podcast_value(db, podcast_id);
    }
    
    sqlite3_finalize(stmt);
    return podcast;
}

/* Funding operations */
gboolean database_save_episode_funding(Database *db, gint episode_id, GList *funding_list) {
    if (!db || !db->db || episode_id <= 0) return FALSE;
    
    database_begin_transaction(db);
    
    /* First, delete existing funding for this episode */
    const char *delete_sql = "DELETE FROM episode_funding WHERE episode_id = ?;";
    sqlite3_stmt *delete_stmt;
    int rc = sqlite3_prepare_v2(db->db, delete_sql, -1, &delete_stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(delete_stmt, 1, episode_id);
        sqlite3_step(delete_stmt);
        sqlite3_finalize(delete_stmt);
    }
    
    /* Insert new funding entries */
    if (!funding_list) {
        database_commit_transaction(db);
        return TRUE;
    }
    
    const char *insert_sql = "INSERT INTO episode_funding (episode_id, url, message, platform) VALUES (?, ?, ?, ?);";
    
    for (GList *l = funding_list; l != NULL; l = l->next) {
        PodcastFunding *funding = (PodcastFunding *)l->data;
        
        sqlite3_stmt *stmt;
        rc = sqlite3_prepare_v2(db->db, insert_sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) continue;
        
        sqlite3_bind_int(stmt, 1, episode_id);
        sqlite3_bind_text(stmt, 2, funding->url, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, funding->message, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, funding->platform, -1, SQLITE_TRANSIENT);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    database_commit_transaction(db);
    return TRUE;
}

GList* database_get_episode_funding(Database *db, gint episode_id) {
    if (!db || !db->db || episode_id <= 0) return NULL;
    
    const char *sql = "SELECT url, message, platform FROM episode_funding WHERE episode_id = ?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;
    
    sqlite3_bind_int(stmt, 1, episode_id);
    
    GList *funding_list = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PodcastFunding *funding = g_new0(PodcastFunding, 1);
        funding->url = g_strdup((const gchar *)sqlite3_column_text(stmt, 0));
        funding->message = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        funding->platform = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        
        funding_list = g_list_prepend(funding_list, funding);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(funding_list);
}

gboolean database_save_podcast_funding(Database *db, gint podcast_id, GList *funding_list) {
    if (!db || !db->db || podcast_id <= 0) return FALSE;
    
    database_begin_transaction(db);
    
    /* First, delete existing funding for this podcast */
    const char *delete_sql = "DELETE FROM podcast_funding WHERE podcast_id = ?;";
    sqlite3_stmt *delete_stmt;
    int rc = sqlite3_prepare_v2(db->db, delete_sql, -1, &delete_stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(delete_stmt, 1, podcast_id);
        sqlite3_step(delete_stmt);
        sqlite3_finalize(delete_stmt);
    }
    
    if (!funding_list) {
        database_commit_transaction(db);
        return TRUE; /* No funding to save */
    }
    
    /* Insert new funding entries */
    const char *insert_sql = "INSERT INTO podcast_funding (podcast_id, url, message, platform) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *insert_stmt;
    rc = sqlite3_prepare_v2(db->db, insert_sql, -1, &insert_stmt, NULL);
    if (rc != SQLITE_OK) {
        database_rollback_transaction(db);
        return FALSE;
    }
    
    for (GList *l = funding_list; l != NULL; l = l->next) {
        PodcastFunding *funding = (PodcastFunding *)l->data;
        if (!funding->url) continue; /* Skip invalid entries */
        
        sqlite3_bind_int(insert_stmt, 1, podcast_id);
        sqlite3_bind_text(insert_stmt, 2, funding->url, -1, SQLITE_STATIC);
        sqlite3_bind_text(insert_stmt, 3, funding->message, -1, SQLITE_STATIC);
        sqlite3_bind_text(insert_stmt, 4, funding->platform, -1, SQLITE_STATIC);
        
        sqlite3_step(insert_stmt);
        sqlite3_reset(insert_stmt);
    }
    
    sqlite3_finalize(insert_stmt);
    database_commit_transaction(db);
    return TRUE;
}

GList* database_load_podcast_funding(Database *db, gint podcast_id) {
    if (!db || !db->db || podcast_id <= 0) return NULL;
    
    const char *sql = "SELECT url, message, platform FROM podcast_funding WHERE podcast_id = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;
    
    sqlite3_bind_int(stmt, 1, podcast_id);
    
    GList *funding_list = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PodcastFunding *funding = g_new0(PodcastFunding, 1);
        funding->url = g_strdup((const gchar *)sqlite3_column_text(stmt, 0));
        funding->message = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        funding->platform = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        
        funding_list = g_list_prepend(funding_list, funding);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(funding_list);
}

gboolean database_save_podcast_value(Database *db, gint podcast_id, GList *value_list) {
    if (!db || !db->db || podcast_id <= 0) return FALSE;
    
    database_begin_transaction(db);
    
    /* First, delete existing values for this podcast */
    const char *delete_sql = "DELETE FROM podcast_value WHERE podcast_id = ?;";
    sqlite3_stmt *delete_stmt;
    int rc = sqlite3_prepare_v2(db->db, delete_sql, -1, &delete_stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(delete_stmt, 1, podcast_id);
        sqlite3_step(delete_stmt);
        sqlite3_finalize(delete_stmt);
    }
    
    if (!value_list) {
        database_commit_transaction(db);
        return TRUE; /* No values to save */
    }
    
    /* Save each value in the list */
    for (GList *l = value_list; l != NULL; l = l->next) {
        PodcastValue *value = (PodcastValue *)l->data;
        
        /* Insert new value entry */
        const char *insert_sql = "INSERT INTO podcast_value (podcast_id, type, method, suggested) VALUES (?, ?, ?, ?);";
        sqlite3_stmt *insert_stmt;
        rc = sqlite3_prepare_v2(db->db, insert_sql, -1, &insert_stmt, NULL);
        if (rc != SQLITE_OK) {
            database_rollback_transaction(db);
            return FALSE;
        }
        
        sqlite3_bind_int(insert_stmt, 1, podcast_id);
        sqlite3_bind_text(insert_stmt, 2, value->type, -1, SQLITE_STATIC);
        sqlite3_bind_text(insert_stmt, 3, value->method, -1, SQLITE_STATIC);
        sqlite3_bind_text(insert_stmt, 4, value->suggested, -1, SQLITE_STATIC);
        
        rc = sqlite3_step(insert_stmt);
        sqlite3_finalize(insert_stmt);
        
        if (rc != SQLITE_DONE) {
            database_rollback_transaction(db);
            return FALSE;
        }
        
        gint64 value_id = sqlite3_last_insert_rowid(db->db);
        
        /* Save recipients */
        if (value->recipients) {
            const char *recipient_sql = "INSERT INTO value_recipients (value_id, value_type, name, recipient_type, address, split, fee, custom_key, custom_value) VALUES (?, 'podcast', ?, ?, ?, ?, ?, ?, ?);";
            sqlite3_stmt *recipient_stmt;
            rc = sqlite3_prepare_v2(db->db, recipient_sql, -1, &recipient_stmt, NULL);
            if (rc != SQLITE_OK) {
                database_rollback_transaction(db);
                return FALSE;
            }
            
            for (GList *rl = value->recipients; rl != NULL; rl = rl->next) {
                ValueRecipient *recipient = (ValueRecipient *)rl->data;
                
                sqlite3_bind_int64(recipient_stmt, 1, value_id);
                sqlite3_bind_text(recipient_stmt, 2, recipient->name, -1, SQLITE_STATIC);
                sqlite3_bind_text(recipient_stmt, 3, recipient->type, -1, SQLITE_STATIC);
                sqlite3_bind_text(recipient_stmt, 4, recipient->address, -1, SQLITE_STATIC);
                sqlite3_bind_int(recipient_stmt, 5, recipient->split);
                sqlite3_bind_int(recipient_stmt, 6, recipient->fee ? 1 : 0);
                sqlite3_bind_text(recipient_stmt, 7, recipient->custom_key, -1, SQLITE_STATIC);
                sqlite3_bind_text(recipient_stmt, 8, recipient->custom_value, -1, SQLITE_STATIC);
                
                sqlite3_step(recipient_stmt);
                sqlite3_reset(recipient_stmt);
            }
            
            sqlite3_finalize(recipient_stmt);
        }
    }
    
    database_commit_transaction(db);
    return TRUE;
}

gboolean database_save_episode_value(Database *db, gint episode_id, GList *value_list) {
    if (!db || !db->db || episode_id <= 0) return FALSE;
    
    database_begin_transaction(db);
    
    /* First, delete existing values for this episode */
    const char *delete_sql = "DELETE FROM episode_value WHERE episode_id = ?;";
    sqlite3_stmt *delete_stmt;
    int rc = sqlite3_prepare_v2(db->db, delete_sql, -1, &delete_stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(delete_stmt, 1, episode_id);
        sqlite3_step(delete_stmt);
        sqlite3_finalize(delete_stmt);
    }
    
    if (!value_list) {
        database_commit_transaction(db);
        return TRUE; /* No values to save */
    }
    
    /* Save each value in the list */
    for (GList *l = value_list; l != NULL; l = l->next) {
        PodcastValue *value = (PodcastValue *)l->data;
        
        /* Insert new value entry */
        const char *insert_sql = "INSERT INTO episode_value (episode_id, type, method, suggested) VALUES (?, ?, ?, ?);";
        sqlite3_stmt *insert_stmt;
        rc = sqlite3_prepare_v2(db->db, insert_sql, -1, &insert_stmt, NULL);
        if (rc != SQLITE_OK) {
            database_rollback_transaction(db);
            return FALSE;
        }
        
        sqlite3_bind_int(insert_stmt, 1, episode_id);
        sqlite3_bind_text(insert_stmt, 2, value->type, -1, SQLITE_STATIC);
        sqlite3_bind_text(insert_stmt, 3, value->method, -1, SQLITE_STATIC);
        sqlite3_bind_text(insert_stmt, 4, value->suggested, -1, SQLITE_STATIC);
        
        rc = sqlite3_step(insert_stmt);
        sqlite3_finalize(insert_stmt);
        
        if (rc != SQLITE_DONE) {
            database_rollback_transaction(db);
            return FALSE;
        }
        
        gint64 value_id = sqlite3_last_insert_rowid(db->db);
        
        /* Save recipients */
        if (value->recipients) {
            const char *recipient_sql = "INSERT INTO value_recipients (value_id, value_type, name, recipient_type, address, split, fee, custom_key, custom_value) VALUES (?, 'episode', ?, ?, ?, ?, ?, ?, ?);";
            sqlite3_stmt *recipient_stmt;
            rc = sqlite3_prepare_v2(db->db, recipient_sql, -1, &recipient_stmt, NULL);
            if (rc != SQLITE_OK) {
                database_rollback_transaction(db);
                return FALSE;
            }
            
            for (GList *rl = value->recipients; rl != NULL; rl = rl->next) {
                ValueRecipient *recipient = (ValueRecipient *)rl->data;
                
                sqlite3_bind_int64(recipient_stmt, 1, value_id);
                sqlite3_bind_text(recipient_stmt, 2, recipient->name, -1, SQLITE_STATIC);
                sqlite3_bind_text(recipient_stmt, 3, recipient->type, -1, SQLITE_STATIC);
                sqlite3_bind_text(recipient_stmt, 4, recipient->address, -1, SQLITE_STATIC);
                sqlite3_bind_int(recipient_stmt, 5, recipient->split);
                sqlite3_bind_int(recipient_stmt, 6, recipient->fee ? 1 : 0);
                sqlite3_bind_text(recipient_stmt, 7, recipient->custom_key, -1, SQLITE_STATIC);
                sqlite3_bind_text(recipient_stmt, 8, recipient->custom_value, -1, SQLITE_STATIC);
                
                sqlite3_step(recipient_stmt);
                sqlite3_reset(recipient_stmt);
            }
            
            sqlite3_finalize(recipient_stmt);
        }
    }
    
    database_commit_transaction(db);
    return TRUE;
}

GList* database_load_podcast_value(Database *db, gint podcast_id) {
    if (!db || !db->db || podcast_id <= 0) return NULL;
    
    const char *sql = "SELECT id, type, method, suggested FROM podcast_value WHERE podcast_id = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;
    
    sqlite3_bind_int(stmt, 1, podcast_id);
    
    GList *value_list = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        gint64 value_id = sqlite3_column_int64(stmt, 0);
        PodcastValue *value = g_new0(PodcastValue, 1);
        value->type = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        value->method = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        value->suggested = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
        value->recipients = NULL;
        
        /* Load recipients */
        const char *recipient_sql = "SELECT name, recipient_type, address, split, fee, custom_key, custom_value FROM value_recipients WHERE value_id = ? AND value_type = 'podcast';";
        sqlite3_stmt *recipient_stmt;
        rc = sqlite3_prepare_v2(db->db, recipient_sql, -1, &recipient_stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int64(recipient_stmt, 1, value_id);
            
            while (sqlite3_step(recipient_stmt) == SQLITE_ROW) {
                ValueRecipient *recipient = g_new0(ValueRecipient, 1);
                recipient->name = g_strdup((const gchar *)sqlite3_column_text(recipient_stmt, 0));
                recipient->type = g_strdup((const gchar *)sqlite3_column_text(recipient_stmt, 1));
                recipient->address = g_strdup((const gchar *)sqlite3_column_text(recipient_stmt, 2));
                recipient->split = sqlite3_column_int(recipient_stmt, 3);
                recipient->fee = sqlite3_column_int(recipient_stmt, 4) != 0;
                recipient->custom_key = g_strdup((const gchar *)sqlite3_column_text(recipient_stmt, 5));
                recipient->custom_value = g_strdup((const gchar *)sqlite3_column_text(recipient_stmt, 6));
                
                value->recipients = g_list_prepend(value->recipients, recipient);
            }
            
            sqlite3_finalize(recipient_stmt);
        }
        value->recipients = g_list_reverse(value->recipients);
        
        value_list = g_list_prepend(value_list, value);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(value_list);
}

GList* database_load_episode_value(Database *db, gint episode_id) {
    if (!db || !db->db || episode_id <= 0) return NULL;
    
    const char *sql = "SELECT id, type, method, suggested FROM episode_value WHERE episode_id = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;
    
    sqlite3_bind_int(stmt, 1, episode_id);
    
    GList *value_list = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        gint64 value_id = sqlite3_column_int64(stmt, 0);
        PodcastValue *value = g_new0(PodcastValue, 1);
        value->type = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        value->method = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        value->suggested = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
        value->recipients = NULL;
        
        /* Load recipients */
        const char *recipient_sql = "SELECT name, recipient_type, address, split, fee, custom_key, custom_value FROM value_recipients WHERE value_id = ? AND value_type = 'episode';";
        sqlite3_stmt *recipient_stmt;
        rc = sqlite3_prepare_v2(db->db, recipient_sql, -1, &recipient_stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int64(recipient_stmt, 1, value_id);
            
            while (sqlite3_step(recipient_stmt) == SQLITE_ROW) {
                ValueRecipient *recipient = g_new0(ValueRecipient, 1);
                recipient->name = g_strdup((const gchar *)sqlite3_column_text(recipient_stmt, 0));
                recipient->type = g_strdup((const gchar *)sqlite3_column_text(recipient_stmt, 1));
                recipient->address = g_strdup((const gchar *)sqlite3_column_text(recipient_stmt, 2));
                recipient->split = sqlite3_column_int(recipient_stmt, 3);
                recipient->fee = sqlite3_column_int(recipient_stmt, 4) != 0;
                recipient->custom_key = g_strdup((const gchar *)sqlite3_column_text(recipient_stmt, 5));
                recipient->custom_value = g_strdup((const gchar *)sqlite3_column_text(recipient_stmt, 6));
                
                value->recipients = g_list_prepend(value->recipients, recipient);
            }
            
            sqlite3_finalize(recipient_stmt);
        }
        value->recipients = g_list_reverse(value->recipients);
        
        value_list = g_list_prepend(value_list, value);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(value_list);
}

gboolean database_update_episode_downloaded(Database *db, gint episode_id, const gchar *local_path) {
    if (!db || !db->db || episode_id <= 0) return FALSE;
    
    const char *sql = "UPDATE podcast_episodes SET downloaded=1, local_file_path=? WHERE id=?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        g_warning("database_update_episode_downloaded: prepare failed: %s", sqlite3_errmsg(db->db));
        return FALSE;
    }
    
    sqlite3_bind_text(stmt, 1, local_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, episode_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE);
}

gboolean database_update_episode_progress(Database *db, gint episode_id, gint position, gboolean played) {
    if (!db || !db->db || episode_id <= 0) return FALSE;
    
    const char *sql = "UPDATE podcast_episodes SET play_position=?, played=? WHERE id=?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        g_warning("database_update_episode_progress: prepare failed: %s", sqlite3_errmsg(db->db));
        return FALSE;
    }
    
    sqlite3_bind_int(stmt, 1, position);
    sqlite3_bind_int(stmt, 2, played ? 1 : 0);
    sqlite3_bind_int(stmt, 3, episode_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE);
}

gboolean database_delete_podcast(Database *db, gint podcast_id) {
    if (!db || !db->db || podcast_id <= 0) return FALSE;
    
    database_begin_transaction(db);
    
    /* Delete all related data explicitly (handles databases where
     * ON DELETE CASCADE may not be active due to missing PRAGMA).
     * Order: deepest children first, then parents. */
    
    /* Delete value_recipients for both podcast and episode values */
    const char *sql_delete_podcast_recipients =
        "DELETE FROM value_recipients WHERE value_type='podcast' AND value_id IN "
        "(SELECT id FROM podcast_value WHERE podcast_id=?);";
    const char *sql_delete_episode_recipients =
        "DELETE FROM value_recipients WHERE value_type='episode' AND value_id IN "
        "(SELECT id FROM episode_value WHERE episode_id IN "
        "(SELECT id FROM podcast_episodes WHERE podcast_id=?));";
    
    /* Delete episode-level child tables */
    const char *sql_delete_episode_funding =
        "DELETE FROM episode_funding WHERE episode_id IN "
        "(SELECT id FROM podcast_episodes WHERE podcast_id=?);";
    const char *sql_delete_episode_value =
        "DELETE FROM episode_value WHERE episode_id IN "
        "(SELECT id FROM podcast_episodes WHERE podcast_id=?);";
    
    /* Delete live item content links, then live items */
    const char *sql_delete_content_links =
        "DELETE FROM live_item_content_links WHERE live_item_id IN "
        "(SELECT id FROM podcast_live_items WHERE podcast_id=?);";
    const char *sql_delete_live_items =
        "DELETE FROM podcast_live_items WHERE podcast_id=?;";
    
    /* Delete podcast-level tables */
    const char *sql_delete_podcast_value = "DELETE FROM podcast_value WHERE podcast_id=?;";
    const char *sql_delete_podcast_funding = "DELETE FROM podcast_funding WHERE podcast_id=?;";
    const char *sql_delete_episodes = "DELETE FROM podcast_episodes WHERE podcast_id=?;";
    const char *sql_delete_podcast = "DELETE FROM podcasts WHERE id=?;";
    
    const char *queries[] = {
        sql_delete_podcast_recipients,
        sql_delete_episode_recipients,
        sql_delete_episode_funding,
        sql_delete_episode_value,
        sql_delete_content_links,
        sql_delete_live_items,
        sql_delete_podcast_value,
        sql_delete_podcast_funding,
        sql_delete_episodes,
        sql_delete_podcast
    };
    
    sqlite3_stmt *stmt;
    int rc;
    gboolean success = TRUE;
    
    for (int i = 0; i < (int)G_N_ELEMENTS(queries); i++) {
        rc = sqlite3_prepare_v2(db->db, queries[i], -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            g_warning("database_delete_podcast: Failed to prepare query %d: %s", i, sqlite3_errmsg(db->db));
            continue;
        }
        sqlite3_bind_int(stmt, 1, podcast_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        /* Only the final DELETE (podcast itself) must succeed */
        if (i == (int)G_N_ELEMENTS(queries) - 1 && rc != SQLITE_DONE) {
            success = FALSE;
        }
    }
    
    if (success) {
        database_commit_transaction(db);
    } else {
        database_rollback_transaction(db);
    }
    
    return success;
}

gboolean database_clear_episode_download(Database *db, gint episode_id) {
    if (!db || !db->db || episode_id <= 0) return FALSE;
    
    const char *sql = "UPDATE podcast_episodes SET downloaded=0, local_file_path=NULL WHERE id=?;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return FALSE;
    
    sqlite3_bind_int(stmt, 1, episode_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE);
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

gdouble database_get_preference_double(Database *db, const gchar *key, gdouble default_value) {
    gchar *value_str = database_get_preference(db, key, NULL);
    if (!value_str) return default_value;
    
    gdouble result = g_ascii_strtod(value_str, NULL);
    g_free(value_str);
    return result;
}

/* Live item database operations */
gboolean database_save_podcast_live_items(Database *db, gint podcast_id, GList *live_items) {
    if (!db || !db->db || podcast_id <= 0) return FALSE;
    
    database_begin_transaction(db);
    
    /* First, delete existing live items for this podcast */
    const char *delete_sql = "DELETE FROM podcast_live_items WHERE podcast_id = ?;";
    sqlite3_stmt *delete_stmt;
    int rc = sqlite3_prepare_v2(db->db, delete_sql, -1, &delete_stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(delete_stmt, 1, podcast_id);
        sqlite3_step(delete_stmt);
        sqlite3_finalize(delete_stmt);
    }
    
    if (!live_items) {
        database_commit_transaction(db);
        return TRUE;  /* Nothing to save */
    }
    
    const char *sql = "INSERT INTO podcast_live_items (podcast_id, guid, title, description, "
                      "enclosure_url, enclosure_type, enclosure_length, start_time, end_time, "
                      "status, image_url) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        database_rollback_transaction(db);
        return FALSE;
    }
    
    for (GList *l = live_items; l != NULL; l = l->next) {
        PodcastLiveItem *item = (PodcastLiveItem *)l->data;
        
        sqlite3_reset(stmt);
        sqlite3_bind_int(stmt, 1, podcast_id);
        sqlite3_bind_text(stmt, 2, item->guid, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, item->title, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, item->description, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, item->enclosure_url, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, item->enclosure_type, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 7, item->enclosure_length);
        sqlite3_bind_int64(stmt, 8, item->start_time);
        sqlite3_bind_int64(stmt, 9, item->end_time);
        sqlite3_bind_text(stmt, 10, podcast_live_status_to_string(item->status), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 11, item->image_url, -1, SQLITE_TRANSIENT);
        
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            g_warning("Failed to save live item: %s", sqlite3_errmsg(db->db));
            continue;
        }
        
        gint64 live_item_id = sqlite3_last_insert_rowid(db->db);
        
        /* Save content links */
        if (item->content_links) {
            const char *link_sql = "INSERT INTO live_item_content_links (live_item_id, href, text) VALUES (?, ?, ?);";
            sqlite3_stmt *link_stmt;
            if (sqlite3_prepare_v2(db->db, link_sql, -1, &link_stmt, NULL) == SQLITE_OK) {
                for (GList *cl = item->content_links; cl != NULL; cl = cl->next) {
                    PodcastContentLink *link = (PodcastContentLink *)cl->data;
                    sqlite3_reset(link_stmt);
                    sqlite3_bind_int64(link_stmt, 1, live_item_id);
                    sqlite3_bind_text(link_stmt, 2, link->href, -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(link_stmt, 3, link->text, -1, SQLITE_TRANSIENT);
                    sqlite3_step(link_stmt);
                }
                sqlite3_finalize(link_stmt);
            }
        }
    }
    
    sqlite3_finalize(stmt);
    database_commit_transaction(db);
    return TRUE;
}

GList* database_load_podcast_live_items(Database *db, gint podcast_id) {
    if (!db || !db->db || podcast_id <= 0) return NULL;
    
    const char *sql = "SELECT id, guid, title, description, enclosure_url, enclosure_type, "
                      "enclosure_length, start_time, end_time, status, image_url "
                      "FROM podcast_live_items WHERE podcast_id = ? ORDER BY start_time DESC;";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;
    
    sqlite3_bind_int(stmt, 1, podcast_id);
    
    GList *live_items = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PodcastLiveItem *item = g_new0(PodcastLiveItem, 1);
        
        item->id = sqlite3_column_int(stmt, 0);
        item->podcast_id = podcast_id;
        item->guid = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
        item->title = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
        item->description = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
        item->enclosure_url = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
        item->enclosure_type = g_strdup((const gchar *)sqlite3_column_text(stmt, 5));
        item->enclosure_length = sqlite3_column_int64(stmt, 6);
        item->start_time = sqlite3_column_int64(stmt, 7);
        item->end_time = sqlite3_column_int64(stmt, 8);
        item->status = podcast_live_status_from_string((const gchar *)sqlite3_column_text(stmt, 9));
        item->image_url = g_strdup((const gchar *)sqlite3_column_text(stmt, 10));
        
        /* Load content links */
        const char *link_sql = "SELECT href, text FROM live_item_content_links WHERE live_item_id = ?;";
        sqlite3_stmt *link_stmt;
        if (sqlite3_prepare_v2(db->db, link_sql, -1, &link_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(link_stmt, 1, item->id);
            while (sqlite3_step(link_stmt) == SQLITE_ROW) {
                PodcastContentLink *link = g_new0(PodcastContentLink, 1);
                link->href = g_strdup((const gchar *)sqlite3_column_text(link_stmt, 0));
                link->text = g_strdup((const gchar *)sqlite3_column_text(link_stmt, 1));
                item->content_links = g_list_prepend(item->content_links, link);
            }
            sqlite3_finalize(link_stmt);
        }
        item->content_links = g_list_reverse(item->content_links);
        
        live_items = g_list_prepend(live_items, item);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(live_items);
}

gboolean database_has_active_live_item(Database *db, gint podcast_id) {
    if (!db || !db->db || podcast_id <= 0) return FALSE;
    
    const char *sql = "SELECT COUNT(*) FROM podcast_live_items WHERE podcast_id = ? AND status = 'live';";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return FALSE;
    
    sqlite3_bind_int(stmt, 1, podcast_id);
    
    gboolean has_live = FALSE;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        has_live = sqlite3_column_int(stmt, 0) > 0;
    }
    
    sqlite3_finalize(stmt);
    return has_live;
}

/* --- Browse query functions (used by browser panel) --- */

void database_browse_result_free(DatabaseBrowseResult *result) {
    if (!result) return;
    g_free(result->name);
    g_free(result);
}

static GList* database_browse_query(Database *db, const char *sql, const gchar *bind_text) {
    if (!db || !db->db) return NULL;
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;
    
    if (bind_text) {
        sqlite3_bind_text(stmt, 1, bind_text, -1, SQLITE_TRANSIENT);
    }
    
    GList *results = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DatabaseBrowseResult *result = g_new0(DatabaseBrowseResult, 1);
        result->name = g_strdup((const gchar *)sqlite3_column_text(stmt, 0));
        result->count = sqlite3_column_int(stmt, 1);
        results = g_list_prepend(results, result);
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(results);
}

GList* database_browse_artists(Database *db) {
    return database_browse_query(db,
        "SELECT DISTINCT Artist, COUNT(*) FROM tracks "
        "WHERE Artist IS NOT NULL AND Artist != '' AND " AUDIO_EXT_FILTER
        " GROUP BY Artist ORDER BY Artist",
        NULL);
}

GList* database_browse_albums(Database *db, const gchar *artist_filter) {
    if (artist_filter) {
        return database_browse_query(db,
            "SELECT DISTINCT Album, COUNT(*) FROM tracks "
            "WHERE Album IS NOT NULL AND Album != '' AND Artist = ? AND " AUDIO_EXT_FILTER
            " GROUP BY Album ORDER BY Album",
            artist_filter);
    }
    return database_browse_query(db,
        "SELECT DISTINCT Album, COUNT(*) FROM tracks "
        "WHERE Album IS NOT NULL AND Album != '' AND " AUDIO_EXT_FILTER
        " GROUP BY Album ORDER BY Album",
        NULL);
}

GList* database_browse_genres(Database *db) {
    return database_browse_query(db,
        "SELECT DISTINCT Genre, COUNT(*) FROM tracks "
        "WHERE Genre IS NOT NULL AND Genre != '' AND " AUDIO_EXT_FILTER
        " GROUP BY Genre ORDER BY Genre",
        NULL);
}

GList* database_browse_years(Database *db) {
    return database_browse_query(db,
        "SELECT DISTINCT CAST(Year AS TEXT), COUNT(*) FROM tracks "
        "WHERE Year > 0 AND " AUDIO_EXT_FILTER
        " GROUP BY Year ORDER BY Year DESC",
        NULL);
}

static GList* database_distinct_query(Database *db, const char *sql, const gchar *bind_text) {
    if (!db || !db->db) return NULL;
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;
    
    if (bind_text) {
        sqlite3_bind_text(stmt, 1, bind_text, -1, SQLITE_TRANSIENT);
    }
    
    GList *list = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        list = g_list_prepend(list, g_strdup((const gchar *)sqlite3_column_text(stmt, 0)));
    }
    
    sqlite3_finalize(stmt);
    return g_list_reverse(list);
}

GList* database_get_distinct_artists(Database *db) {
    return database_distinct_query(db,
        "SELECT DISTINCT Artist FROM tracks "
        "WHERE Artist IS NOT NULL AND Artist != '' AND " AUDIO_EXT_FILTER
        " ORDER BY Artist",
        NULL);
}

GList* database_get_distinct_albums(Database *db, const gchar *artist_filter) {
    if (artist_filter) {
        return database_distinct_query(db,
            "SELECT DISTINCT Album FROM tracks "
            "WHERE Album IS NOT NULL AND Album != '' AND Artist = ? AND " AUDIO_EXT_FILTER
            " ORDER BY Album",
            artist_filter);
    }
    return database_distinct_query(db,
        "SELECT DISTINCT Album FROM tracks "
        "WHERE Album IS NOT NULL AND Album != '' AND " AUDIO_EXT_FILTER
        " ORDER BY Album",
        NULL);
}

GList* database_get_distinct_genres(Database *db) {
    return database_distinct_query(db,
        "SELECT DISTINCT Genre FROM tracks "
        "WHERE Genre IS NOT NULL AND Genre != '' AND " AUDIO_EXT_FILTER
        " ORDER BY Genre",
        NULL);
}
