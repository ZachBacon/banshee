#include "smartplaylist.h"
#include <string.h>
#include <stdio.h>

SmartPlaylist* smartplaylist_new(const gchar *name) {
    SmartPlaylist *playlist = g_new0(SmartPlaylist, 1);
    playlist->name = g_strdup(name);
    playlist->conditions = NULL;
    playlist->match_all = TRUE;
    playlist->limit = 0;
    playlist->order_by = g_strdup("date_added");
    playlist->ascending = FALSE;
    playlist->date_created = g_get_real_time() / 1000000;
    playlist->date_modified = playlist->date_created;
    return playlist;
}

void smartplaylist_free(SmartPlaylist *playlist) {
    if (!playlist) return;
    
    g_free(playlist->name);
    g_free(playlist->order_by);
    
    for (GList *l = playlist->conditions; l != NULL; l = l->next) {
        QueryCondition *cond = (QueryCondition *)l->data;
        g_free(cond->value);
        g_free(cond);
    }
    g_list_free(playlist->conditions);
    
    g_free(playlist);
}

void smartplaylist_add_condition(SmartPlaylist *playlist, QueryFieldType field,
                                 QueryOperator op, const gchar *value) {
    if (!playlist) return;
    
    QueryCondition *cond = g_new0(QueryCondition, 1);
    cond->field = field;
    cond->op = op;
    cond->value = g_strdup(value);
    
    playlist->conditions = g_list_append(playlist->conditions, cond);
    playlist->date_modified = g_get_real_time() / 1000000;
}

static const gchar* field_to_column(QueryFieldType field) {
    switch (field) {
        case QUERY_FIELD_TITLE: return "title";
        case QUERY_FIELD_ARTIST: return "artist";
        case QUERY_FIELD_ALBUM: return "album";
        case QUERY_FIELD_GENRE: return "genre";
        case QUERY_FIELD_YEAR: return "year";
        case QUERY_FIELD_RATING: return "rating";
        case QUERY_FIELD_PLAYCOUNT: return "play_count";
        case QUERY_FIELD_DURATION: return "duration";
        case QUERY_FIELD_DATE_ADDED: return "date_added";
        case QUERY_FIELD_LAST_PLAYED: return "last_played";
        case QUERY_FIELD_IS_FAVORITE: return "is_favorite";
        default: return "title";
    }
}

static const gchar* op_to_sql(QueryOperator op) {
    switch (op) {
        case QUERY_OP_EQUALS: return "=";
        case QUERY_OP_NOT_EQUALS: return "!=";
        case QUERY_OP_CONTAINS: return "LIKE";
        case QUERY_OP_NOT_CONTAINS: return "NOT LIKE";
        case QUERY_OP_STARTS_WITH: return "LIKE";
        case QUERY_OP_GREATER_THAN: return ">";
        case QUERY_OP_LESS_THAN: return "<";
        case QUERY_OP_GREATER_OR_EQUAL: return ">=";
        case QUERY_OP_LESS_OR_EQUAL: return "<=";
        default: return "=";
    }
}

gchar* smartplaylist_build_sql(SmartPlaylist *playlist) {
    if (!playlist) return NULL;
    
    GString *sql = g_string_new("SELECT id, title, artist, album, genre, track_number, duration, "
                                "file_path, play_count, date_added, last_played FROM tracks WHERE ");
    
    if (playlist->conditions == NULL) {
        g_string_append(sql, "1=1");
    } else {
        gboolean first = TRUE;
        for (GList *l = playlist->conditions; l != NULL; l = l->next) {
            QueryCondition *cond = (QueryCondition *)l->data;
            
            if (!first) {
                g_string_append(sql, playlist->match_all ? " AND " : " OR ");
            }
            first = FALSE;
            
            const gchar *column = field_to_column(cond->field);
            const gchar *op = op_to_sql(cond->op);
            
            /* Use ? placeholders to prevent SQL injection */
            if (cond->op == QUERY_OP_CONTAINS || cond->op == QUERY_OP_NOT_CONTAINS) {
                g_string_append_printf(sql, "%s %s ?", column, op);
            } else if (cond->op == QUERY_OP_STARTS_WITH) {
                g_string_append_printf(sql, "%s LIKE ?", column);
            } else {
                g_string_append_printf(sql, "%s %s ?", column, op);
            }
        }
    }
    
    /* order_by is from a controlled set (field_to_column), not user input */
    if (playlist->order_by) {
        g_string_append_printf(sql, " ORDER BY %s %s",
                              playlist->order_by,
                              playlist->ascending ? "ASC" : "DESC");
    }
    
    if (playlist->limit > 0) {
        g_string_append_printf(sql, " LIMIT %d", playlist->limit);
    }
    
    return g_string_free(sql, FALSE);
}

GList* smartplaylist_get_tracks(SmartPlaylist *playlist, Database *db) {
    if (!playlist || !db) return NULL;
    
    gchar *sql = smartplaylist_build_sql(playlist);
    if (!sql) return NULL;
    
    GList *tracks = NULL;
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        /* Bind condition values as parameters */
        gint param_index = 1;
        for (GList *l = playlist->conditions; l != NULL; l = l->next) {
            QueryCondition *cond = (QueryCondition *)l->data;
            
            if (cond->op == QUERY_OP_CONTAINS || cond->op == QUERY_OP_NOT_CONTAINS) {
                gchar *like_val = g_strdup_printf("%%%s%%", cond->value);
                sqlite3_bind_text(stmt, param_index, like_val, -1, g_free);
            } else if (cond->op == QUERY_OP_STARTS_WITH) {
                gchar *like_val = g_strdup_printf("%s%%", cond->value);
                sqlite3_bind_text(stmt, param_index, like_val, -1, g_free);
            } else {
                sqlite3_bind_text(stmt, param_index, cond->value, -1, SQLITE_TRANSIENT);
            }
            param_index++;
        }
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            /* Read columns matching the explicit SELECT statement */
            Track *track = g_new0(Track, 1);
            track->id = sqlite3_column_int(stmt, 0);
            track->title = g_strdup((const char*)sqlite3_column_text(stmt, 1));
            track->artist = g_strdup((const char*)sqlite3_column_text(stmt, 2));
            track->album = g_strdup((const char*)sqlite3_column_text(stmt, 3));
            track->genre = sqlite3_column_text(stmt, 4) ? 
                g_strdup((const char*)sqlite3_column_text(stmt, 4)) : NULL;
            track->track_number = sqlite3_column_int(stmt, 5);
            track->duration = sqlite3_column_int(stmt, 6);
            track->file_path = g_strdup((const char*)sqlite3_column_text(stmt, 7));
            track->play_count = sqlite3_column_int(stmt, 8);
            track->date_added = sqlite3_column_int64(stmt, 9);
            track->last_played = sqlite3_column_int64(stmt, 10);
            
            tracks = g_list_prepend(tracks, track);
        }
        sqlite3_finalize(stmt);
    }
    
    g_free(sql);
    return g_list_reverse(tracks);
}

SmartPlaylist* smartplaylist_create_favorites(void) {
    SmartPlaylist *playlist = smartplaylist_new("Favorites");
    smartplaylist_add_condition(playlist, QUERY_FIELD_IS_FAVORITE, QUERY_OP_EQUALS, "1");
    playlist->limit = 100;
    return playlist;
}

SmartPlaylist* smartplaylist_create_recently_added(void) {
    SmartPlaylist *playlist = smartplaylist_new("Recently Added");
    playlist->order_by = g_strdup("date_added");
    playlist->ascending = FALSE;
    playlist->limit = 50;
    return playlist;
}

SmartPlaylist* smartplaylist_create_recently_played(void) {
    SmartPlaylist *playlist = smartplaylist_new("Recently Played");
    smartplaylist_add_condition(playlist, QUERY_FIELD_PLAYCOUNT, QUERY_OP_GREATER_THAN, "0");
    playlist->order_by = g_strdup("last_played");
    playlist->ascending = FALSE;
    playlist->limit = 50;
    return playlist;
}

SmartPlaylist* smartplaylist_create_never_played(void) {
    SmartPlaylist *playlist = smartplaylist_new("Never Played");
    smartplaylist_add_condition(playlist, QUERY_FIELD_PLAYCOUNT, QUERY_OP_EQUALS, "0");
    playlist->limit = 100;
    return playlist;
}

SmartPlaylist* smartplaylist_create_most_played(void) {
    SmartPlaylist *playlist = smartplaylist_new("Most Played");
    smartplaylist_add_condition(playlist, QUERY_FIELD_PLAYCOUNT, QUERY_OP_GREATER_THAN, "0");
    playlist->order_by = g_strdup("play_count");
    playlist->ascending = FALSE;
    playlist->limit = 50;
    return playlist;
}

gint smartplaylist_save_to_db(SmartPlaylist *playlist, Database *db) {
    /* Simplified - would need proper smart_playlists table */
    return database_create_playlist(db, playlist->name);
}

SmartPlaylist* smartplaylist_load_from_db(gint playlist_id, Database *db) {
    /* Simplified - would need proper deserialization */
    return NULL;
}

GList* smartplaylist_get_all_from_db(Database *db) {
    /* Simplified - would return list of all smart playlists */
    GList *playlists = NULL;
    playlists = g_list_prepend(playlists, smartplaylist_create_most_played());
    playlists = g_list_prepend(playlists, smartplaylist_create_never_played());
    playlists = g_list_prepend(playlists, smartplaylist_create_recently_played());
    playlists = g_list_prepend(playlists, smartplaylist_create_recently_added());
    playlists = g_list_prepend(playlists, smartplaylist_create_favorites());
    return playlists;
}

gboolean smartplaylist_delete_from_db(gint playlist_id, Database *db) {
    return database_delete_playlist(db, playlist_id);
}
