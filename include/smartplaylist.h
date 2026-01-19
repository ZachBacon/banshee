#ifndef SMARTPLAYLIST_H
#define SMARTPLAYLIST_H

#include <glib.h>
#include "database.h"

/* Query field types */
typedef enum {
    QUERY_FIELD_TITLE,
    QUERY_FIELD_ARTIST,
    QUERY_FIELD_ALBUM,
    QUERY_FIELD_GENRE,
    QUERY_FIELD_YEAR,
    QUERY_FIELD_RATING,
    QUERY_FIELD_PLAYCOUNT,
    QUERY_FIELD_DURATION,
    QUERY_FIELD_DATE_ADDED,
    QUERY_FIELD_LAST_PLAYED,
    QUERY_FIELD_IS_FAVORITE
} QueryFieldType;

/* Query operators */
typedef enum {
    QUERY_OP_EQUALS,
    QUERY_OP_NOT_EQUALS,
    QUERY_OP_CONTAINS,
    QUERY_OP_NOT_CONTAINS,
    QUERY_OP_STARTS_WITH,
    QUERY_OP_GREATER_THAN,
    QUERY_OP_LESS_THAN,
    QUERY_OP_GREATER_OR_EQUAL,
    QUERY_OP_LESS_OR_EQUAL
} QueryOperator;

/* Query condition */
typedef struct {
    QueryFieldType field;
    QueryOperator op;
    gchar *value;
} QueryCondition;

/* Smart playlist definition */
typedef struct {
    gint id;
    gchar *name;
    GList *conditions;  /* List of QueryCondition */
    gboolean match_all; /* TRUE = AND, FALSE = OR */
    gint limit;
    gchar *order_by;
    gboolean ascending;
    gint64 date_created;
    gint64 date_modified;
} SmartPlaylist;

/* Smart playlist functions */
SmartPlaylist* smartplaylist_new(const gchar *name);
void smartplaylist_free(SmartPlaylist *playlist);
void smartplaylist_add_condition(SmartPlaylist *playlist, QueryFieldType field, 
                                 QueryOperator op, const gchar *value);
gchar* smartplaylist_build_sql(SmartPlaylist *playlist);
GList* smartplaylist_get_tracks(SmartPlaylist *playlist, Database *db);

/* Predefined smart playlists */
SmartPlaylist* smartplaylist_create_favorites(void);
SmartPlaylist* smartplaylist_create_recently_added(void);
SmartPlaylist* smartplaylist_create_recently_played(void);
SmartPlaylist* smartplaylist_create_never_played(void);
SmartPlaylist* smartplaylist_create_most_played(void);

/* Database operations */
gint smartplaylist_save_to_db(SmartPlaylist *playlist, Database *db);
SmartPlaylist* smartplaylist_load_from_db(gint playlist_id, Database *db);
GList* smartplaylist_get_all_from_db(Database *db);
gboolean smartplaylist_delete_from_db(gint playlist_id, Database *db);

#endif /* SMARTPLAYLIST_H */
