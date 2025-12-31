#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <glib.h>
#include "database.h"

typedef struct {
    GList *tracks;
    gint current_index;
    gboolean shuffle;
    gboolean repeat;
} PlaylistManager;

/* Playlist manager initialization */
PlaylistManager* playlist_manager_new(void);
void playlist_manager_free(PlaylistManager *manager);

/* Playlist operations */
void playlist_manager_set_tracks(PlaylistManager *manager, GList *tracks);
Track* playlist_manager_get_current(PlaylistManager *manager);
Track* playlist_manager_next(PlaylistManager *manager);
Track* playlist_manager_previous(PlaylistManager *manager);
gboolean playlist_manager_has_next(PlaylistManager *manager);
gboolean playlist_manager_has_previous(PlaylistManager *manager);

/* Shuffle and repeat */
void playlist_manager_set_shuffle(PlaylistManager *manager, gboolean shuffle);
void playlist_manager_set_repeat(PlaylistManager *manager, gboolean repeat);
void playlist_manager_shuffle_tracks(PlaylistManager *manager);

/* Position management */
void playlist_manager_set_position(PlaylistManager *manager, gint index);
gint playlist_manager_get_position(PlaylistManager *manager);
gint playlist_manager_get_count(PlaylistManager *manager);

#endif /* PLAYLIST_H */
