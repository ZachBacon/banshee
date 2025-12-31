#include "radio.h"
#include <string.h>

RadioStation* radio_station_new(const gchar *name, const gchar *url) {
    RadioStation *station = g_new0(RadioStation, 1);
    station->name = g_strdup(name);
    station->url = g_strdup(url);
    station->date_added = g_get_real_time() / 1000000;
    station->play_count = 0;
    return station;
}

void radio_station_free(RadioStation *station) {
    if (!station) return;
    g_free(station->name);
    g_free(station->url);
    g_free(station->genre);
    g_free(station->description);
    g_free(station->homepage);
    g_free(station);
}

gint radio_station_save(RadioStation *station, Database *db) {
    if (!db || !db->db || !station) return -1;
    
    const char *sql = "INSERT INTO radio_stations (name, url, genre, description, bitrate, homepage, date_added) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?);";
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, station->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, station->url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, station->genre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, station->description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, station->bitrate);
    sqlite3_bind_text(stmt, 6, station->homepage, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 7, station->date_added);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        return -1;
    }
    
    return sqlite3_last_insert_rowid(db->db);
}

GList* radio_station_get_all(Database *db) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT id, name, url, genre, description, bitrate, homepage, date_added, play_count "
                      "FROM radio_stations ORDER BY name";
    
    sqlite3_stmt *stmt;
    GList *stations = NULL;
    
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            RadioStation *station = g_new0(RadioStation, 1);
            station->id = sqlite3_column_int(stmt, 0);
            station->name = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
            station->url = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
            station->genre = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
            station->description = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
            station->bitrate = sqlite3_column_int(stmt, 5);
            station->homepage = g_strdup((const gchar *)sqlite3_column_text(stmt, 6));
            station->date_added = sqlite3_column_int64(stmt, 7);
            station->play_count = sqlite3_column_int(stmt, 8);
            
            stations = g_list_append(stations, station);
        }
        sqlite3_finalize(stmt);
    }
    
    return stations;
}

GList* radio_station_get_defaults(void) {
    GList *stations = NULL;
    
    RadioStation *station;
    
    station = radio_station_new("SomaFM - Groove Salad", "http://ice1.somafm.com/groovesalad-128-mp3");
    station->genre = g_strdup("Ambient/Downtempo");
    station->bitrate = 128;
    stations = g_list_append(stations, station);
    
    station = radio_station_new("SomaFM - Def Con Radio", "http://ice1.somafm.com/defcon-128-mp3");
    station->genre = g_strdup("Electronic");
    station->bitrate = 128;
    stations = g_list_append(stations, station);
    
    station = radio_station_new("Digitally Imported - Trance", "http://prem2.di.fm:80/trance");
    station->genre = g_strdup("Trance");
    station->bitrate = 128;
    stations = g_list_append(stations, station);
    
    station = radio_station_new("SKY.FM - Smooth Jazz", "http://prem1.sky.fm:80/smoothjazz");
    station->genre = g_strdup("Jazz");
    station->bitrate = 128;
    stations = g_list_append(stations, station);
    
    return stations;
}

RadioStation* radio_station_load(gint station_id, Database *db) {
    if (!db || !db->db) return NULL;
    
    const char *sql = "SELECT id, name, url, genre, description, bitrate, homepage, date_added, play_count "
                      "FROM radio_stations WHERE id = ?";
    
    sqlite3_stmt *stmt;
    RadioStation *station = NULL;
    
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, station_id);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            station = g_new0(RadioStation, 1);
            station->id = sqlite3_column_int(stmt, 0);
            station->name = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
            station->url = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
            station->genre = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
            station->description = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
            station->bitrate = sqlite3_column_int(stmt, 5);
            station->homepage = g_strdup((const gchar *)sqlite3_column_text(stmt, 6));
            station->date_added = sqlite3_column_int64(stmt, 7);
            station->play_count = sqlite3_column_int(stmt, 8);
        }
        sqlite3_finalize(stmt);
    }
    
    return station;
}

GList* radio_station_search(Database *db, const gchar *search_term) {
    if (!db || !db->db || !search_term) return NULL;
    
    const char *sql = "SELECT id, name, url, genre, description, bitrate, homepage, date_added, play_count "
                      "FROM radio_stations WHERE name LIKE ? OR genre LIKE ? ORDER BY name";
    
    sqlite3_stmt *stmt;
    GList *stations = NULL;
    
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        gchar *pattern = g_strdup_printf("%%%s%%", search_term);
        sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, pattern, -1, SQLITE_TRANSIENT);
        g_free(pattern);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            RadioStation *station = g_new0(RadioStation, 1);
            station->id = sqlite3_column_int(stmt, 0);
            station->name = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
            station->url = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
            station->genre = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
            station->description = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
            station->bitrate = sqlite3_column_int(stmt, 5);
            station->homepage = g_strdup((const gchar *)sqlite3_column_text(stmt, 6));
            station->date_added = sqlite3_column_int64(stmt, 7);
            station->play_count = sqlite3_column_int(stmt, 8);
            
            stations = g_list_append(stations, station);
        }
        sqlite3_finalize(stmt);
    }
    
    return stations;
}

gboolean radio_station_delete(gint station_id, Database *db) {
    if (!db || !db->db) return FALSE;
    
    const char *sql = "DELETE FROM radio_stations WHERE id=?;";
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }
    
    sqlite3_bind_int(stmt, 1, station_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE);
}

gboolean radio_station_update(RadioStation *station, Database *db) {
    if (!db || !db->db || !station) return FALSE;
    
    const char *sql = "UPDATE radio_stations SET name=?, url=?, genre=?, description=?, bitrate=?, homepage=? WHERE id=?;";
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }
    
    sqlite3_bind_text(stmt, 1, station->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, station->url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, station->genre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, station->description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, station->bitrate);
    sqlite3_bind_text(stmt, 6, station->homepage, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, station->id);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE);
}

void radio_discover_stations(const gchar *genre, StationDiscoveryCallback callback, gpointer user_data) {
    /* Simplified - would integrate with online radio directories */
    GList *stations = radio_station_get_defaults();
    if (callback) {
        callback(stations, user_data);
    }
}
