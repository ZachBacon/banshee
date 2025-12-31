#ifndef RADIO_H
#define RADIO_H

#include <glib.h>
#include "database.h"

/* Radio station structure */
typedef struct {
    gint id;
    gchar *name;
    gchar *url;
    gchar *genre;
    gchar *description;
    gint bitrate;
    gchar *homepage;
    gint64 date_added;
    gint play_count;
} RadioStation;

/* Radio station functions */
RadioStation* radio_station_new(const gchar *name, const gchar *url);
void radio_station_free(RadioStation *station);

/* Database operations */
gint radio_station_save(RadioStation *station, Database *db);
RadioStation* radio_station_load(gint station_id, Database *db);
GList* radio_station_get_all(Database *db);
GList* radio_station_search(Database *db, const gchar *search_term);
gboolean radio_station_delete(gint station_id, Database *db);
gboolean radio_station_update(RadioStation *station, Database *db);

/* Predefined stations */
GList* radio_station_get_defaults(void);

/* Station discovery */
typedef void (*StationDiscoveryCallback)(GList *stations, gpointer user_data);
void radio_discover_stations(const gchar *genre, StationDiscoveryCallback callback, gpointer user_data);

#endif /* RADIO_H */
