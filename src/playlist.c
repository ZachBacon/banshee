#include "playlist.h"
#include <stdlib.h>
#include <time.h>

PlaylistManager* playlist_manager_new(void) {
    PlaylistManager *manager = g_new0(PlaylistManager, 1);
    manager->tracks = NULL;
    manager->current_index = -1;
    manager->shuffle = FALSE;
    manager->repeat = FALSE;
    return manager;
}

void playlist_manager_free(PlaylistManager *manager) {
    if (!manager) return;
    
    if (manager->tracks) {
        g_list_free(manager->tracks);
    }
    
    g_free(manager);
}

void playlist_manager_set_tracks(PlaylistManager *manager, GList *tracks) {
    if (!manager) return;
    
    if (manager->tracks) {
        g_list_free(manager->tracks);
    }
    
    manager->tracks = g_list_copy(tracks);
    manager->current_index = (tracks != NULL) ? 0 : -1;
}

Track* playlist_manager_get_current(PlaylistManager *manager) {
    if (!manager || !manager->tracks || manager->current_index < 0) {
        return NULL;
    }
    
    GList *item = g_list_nth(manager->tracks, manager->current_index);
    return (item != NULL) ? (Track *)item->data : NULL;
}

Track* playlist_manager_next(PlaylistManager *manager) {
    if (!manager || !manager->tracks) return NULL;
    
    gint count = g_list_length(manager->tracks);
    if (count == 0) return NULL;
    
    manager->current_index++;
    
    if (manager->current_index >= count) {
        if (manager->repeat) {
            manager->current_index = 0;
        } else {
            manager->current_index = count - 1;
            return NULL;
        }
    }
    
    return playlist_manager_get_current(manager);
}

Track* playlist_manager_previous(PlaylistManager *manager) {
    if (!manager || !manager->tracks) return NULL;
    
    gint count = g_list_length(manager->tracks);
    if (count == 0) return NULL;
    
    manager->current_index--;
    
    if (manager->current_index < 0) {
        if (manager->repeat) {
            manager->current_index = count - 1;
        } else {
            manager->current_index = 0;
            return NULL;
        }
    }
    
    return playlist_manager_get_current(manager);
}

gboolean playlist_manager_has_next(PlaylistManager *manager) {
    if (!manager || !manager->tracks) return FALSE;
    
    gint count = g_list_length(manager->tracks);
    return (manager->current_index < count - 1) || manager->repeat;
}

gboolean playlist_manager_has_previous(PlaylistManager *manager) {
    if (!manager || !manager->tracks) return FALSE;
    
    return (manager->current_index > 0) || manager->repeat;
}

void playlist_manager_set_shuffle(PlaylistManager *manager, gboolean shuffle) {
    if (!manager) return;
    
    manager->shuffle = shuffle;
    
    if (shuffle && manager->tracks) {
        playlist_manager_shuffle_tracks(manager);
    }
}

void playlist_manager_set_repeat(PlaylistManager *manager, gboolean repeat) {
    if (!manager) return;
    manager->repeat = repeat;
}

/* Fisher-Yates shuffle algorithm */
void playlist_manager_shuffle_tracks(PlaylistManager *manager) {
    if (!manager || !manager->tracks) return;
    
    gint count = g_list_length(manager->tracks);
    if (count <= 1) return;
    
    /* Convert list to array for efficient shuffling */
    Track **array = g_new0(Track*, count);
    GList *current = manager->tracks;
    for (gint i = 0; i < count; i++) {
        array[i] = (Track *)current->data;
        current = current->next;
    }
    
    /* Shuffle */
    srand(time(NULL));
    for (gint i = count - 1; i > 0; i--) {
        gint j = rand() % (i + 1);
        Track *temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
    
    /* Rebuild list */
    g_list_free(manager->tracks);
    manager->tracks = NULL;
    for (gint i = 0; i < count; i++) {
        manager->tracks = g_list_append(manager->tracks, array[i]);
    }
    
    g_free(array);
    manager->current_index = 0;
}

void playlist_manager_set_position(PlaylistManager *manager, gint index) {
    if (!manager || !manager->tracks) return;
    
    gint count = g_list_length(manager->tracks);
    if (index >= 0 && index < count) {
        manager->current_index = index;
    }
}

gint playlist_manager_get_position(PlaylistManager *manager) {
    if (!manager) return -1;
    return manager->current_index;
}

gint playlist_manager_get_count(PlaylistManager *manager) {
    if (!manager || !manager->tracks) return 0;
    return g_list_length(manager->tracks);
}
