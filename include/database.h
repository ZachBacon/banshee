#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <glib.h>

/* Forward declarations - these will be fully defined in podcast.h */
typedef struct PodcastEpisode PodcastEpisode;
typedef struct Podcast Podcast;
typedef struct PodcastFunding PodcastFunding;
typedef struct Database Database;

typedef struct {
    gint id;
    gchar *title;
    gchar *artist;
    gchar *album;
    gchar *genre;
    gint track_number;
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

struct Database {
    sqlite3 *db;
    gchar *db_path;
};

/* Database initialization */
Database* database_new(const gchar *db_path);
void database_free(Database *db);
gboolean database_init_tables(Database *db);

/* Track operations */
gint database_add_track(Database *db, Track *track);
Track* database_get_track(Database *db, gint track_id);
GList* database_get_all_tracks(Database *db);
gint database_get_audio_track_count(Database *db);
GList* database_get_tracks_by_artist(Database *db, const gchar *artist);
GList* database_get_tracks_by_album(Database *db, const gchar *artist, const gchar *album);
GList* database_get_albums_by_artist(Database *db, const gchar *artist);
gboolean database_update_track(Database *db, Track *track);
gboolean database_delete_track(Database *db, gint track_id);
GList* database_search_tracks(Database *db, const gchar *search_term);

/* Video operations */
GList* database_get_all_videos(Database *db);
GList* database_search_videos(Database *db, const gchar *search_term);

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
                                  const gchar *enclosure_type, gint64 published_date, gint duration,
                                  const gchar *chapters_url, const gchar *chapters_type,
                                  const gchar *transcript_url, const gchar *transcript_type);
GList* database_get_podcasts(Database *db);
Podcast* database_get_podcast_by_id(Database *db, gint podcast_id);
GList* database_get_podcast_episodes(Database *db, gint podcast_id);
PodcastEpisode* database_get_episode_by_id(Database *db, gint episode_id);
gboolean database_update_episode_progress(Database *db, gint episode_id, gint position, gboolean played);
gboolean database_update_episode_downloaded(Database *db, gint episode_id, const gchar *local_path);
gboolean database_delete_podcast(Database *db, gint podcast_id);
gboolean database_clear_episode_download(Database *db, gint episode_id);

/* Funding operations */
gboolean database_save_episode_funding(Database *db, gint episode_id, GList *funding_list);
GList* database_get_episode_funding(Database *db, gint episode_id);
gboolean database_save_podcast_funding(Database *db, gint podcast_id, GList *funding_list);
GList* database_load_podcast_funding(Database *db, gint podcast_id);

/* Value 4 Value operations */
gboolean database_save_podcast_value(Database *db, gint podcast_id, GList *value_list);
gboolean database_save_episode_value(Database *db, gint episode_id, GList *value_list);
GList* database_load_podcast_value(Database *db, gint podcast_id);
GList* database_load_episode_value(Database *db, gint episode_id);

/* Live item operations */
gboolean database_save_podcast_live_items(Database *db, gint podcast_id, GList *live_items);
GList* database_load_podcast_live_items(Database *db, gint podcast_id);
gboolean database_has_active_live_item(Database *db, gint podcast_id);

/* Preference operations */
gboolean database_set_preference(Database *db, const gchar *key, const gchar *value);
gchar* database_get_preference(Database *db, const gchar *key, const gchar *default_value);
gint database_get_preference_int(Database *db, const gchar *key, gint default_value);
gboolean database_get_preference_bool(Database *db, const gchar *key, gboolean default_value);
gdouble database_get_preference_double(Database *db, const gchar *key, gdouble default_value);

/* Cleanup helpers */
void database_free_playlist(Playlist *playlist);
void database_free_track(Track *track);

#endif /* DATABASE_H */
