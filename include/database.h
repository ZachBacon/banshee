#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <glib.h>

typedef struct {
    gint id;
    gchar *title;
    gchar *artist;
    gchar *album;
    gchar *genre;
    gint duration;
    gchar *file_path;
    gint play_count;
    gint64 date_added;
} Track;

typedef struct {
    gint id;
    gchar *name;
    gint64 date_created;
    gint track_count;
} Playlist;

typedef struct {
    sqlite3 *db;
    gchar *db_path;
} Database;

/* Database initialization */
Database* database_new(const gchar *db_path);
void database_free(Database *db);
gboolean database_init_tables(Database *db);

/* Track operations */
gint database_add_track(Database *db, Track *track);
Track* database_get_track(Database *db, gint track_id);
GList* database_get_all_tracks(Database *db);
GList* database_get_tracks_by_artist(Database *db, const gchar *artist);
GList* database_get_tracks_by_album(Database *db, const gchar *artist, const gchar *album);
GList* database_get_albums_by_artist(Database *db, const gchar *artist);
gboolean database_update_track(Database *db, Track *track);
gboolean database_delete_track(Database *db, gint track_id);
GList* database_search_tracks(Database *db, const gchar *search_term);

/* Playlist operations */
gint database_create_playlist(Database *db, const gchar *name);
GList* database_get_all_playlists(Database *db);
gboolean database_add_track_to_playlist(Database *db, gint playlist_id, gint track_id);
GList* database_get_playlist_tracks(Database *db, gint playlist_id);
gboolean database_delete_playlist(Database *db, gint playlist_id);

/* Statistics */
gboolean database_increment_play_count(Database *db, gint track_id);
GList* database_get_most_played_tracks(Database *db, gint limit);
GList* database_get_recent_tracks(Database *db, gint limit);

/* Podcast operations */
gint database_add_podcast(Database *db, const gchar *title, const gchar *feed_url, const gchar *link,
                          const gchar *description, const gchar *author, const gchar *image_url, const gchar *language);
gint database_add_podcast_episode(Database *db, gint podcast_id, const gchar *guid, const gchar *title,
                                  const gchar *description, const gchar *enclosure_url, gint64 enclosure_length,
                                  const gchar *enclosure_type, gint64 published_date, gint duration);
GList* database_get_podcasts(Database *db);
GList* database_get_podcast_episodes(Database *db, gint podcast_id);
gboolean database_update_episode_progress(Database *db, gint episode_id, gint position, gboolean played);

/* Cleanup helpers */
void track_free(Track *track);
void playlist_free(Playlist *playlist);
void database_free_track(Track *track);
void database_free_playlist(Playlist *playlist);

#endif /* DATABASE_H */
