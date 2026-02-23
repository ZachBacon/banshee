// Copyright (c) 2019 Ariadne Conill <ariadne@dereferenced.org>
// Copyright (c) 2025 Shriek contributors
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// This software is provided 'as is' and without any warranty, express or
// implied.  In no event shall the authors be liable for any damages arising
// from the use of this software.
//
// Adapted from the Audacious Streamtuner plugin (Shoutcast, Icecast, iHeartRadio).

#ifndef RADIO_H
#define RADIO_H

#include <glib.h>
#include "database.h"

/* ─── Local radio station (database-backed) ─── */

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

RadioStation* radio_station_new(const gchar *name, const gchar *url);
void radio_station_free(RadioStation *station);

/* Database operations */
gint radio_station_save(RadioStation *station, Database *db);
RadioStation* radio_station_load(gint station_id, Database *db);
GList* radio_station_get_all(Database *db);
GList* radio_station_search(Database *db, const gchar *search_term);
gboolean radio_station_delete(gint station_id, Database *db);
gboolean radio_station_update(RadioStation *station, Database *db);

/* ─── Stream tuner directory types ─── */

typedef enum {
    STREAM_TUNER_SHOUTCAST,
    STREAM_TUNER_ICECAST,
    STREAM_TUNER_IHEART
} StreamTunerType;

/* ─── Shoutcast directory ─── */

typedef struct {
    gchar *title;
    gchar *genre;
    gint   listeners;
    gint   bitrate;
    gint   station_id;
    gboolean is_aac;  /* FALSE = MP3, TRUE = AAC */
} ShoutcastEntry;

void shoutcast_entry_free(ShoutcastEntry *entry);

/* Async fetch: calls back on the main thread with GList* of ShoutcastEntry*.
   Pass NULL or "Top 500 Stations" for the top-500 list. */
typedef void (*ShoutcastCallback)(GList *entries, gpointer user_data);
void shoutcast_fetch_stations(const gchar *genre, ShoutcastCallback callback, gpointer user_data);

/* Build a playable URL from a Shoutcast station ID (caller g_free's result) */
gchar* shoutcast_get_play_url(gint station_id);

/* NULL-terminated static genre list; optionally writes count to *out_count */
const gchar* const* shoutcast_get_genres(gint *out_count);

/* ─── Icecast directory ─── */

typedef struct {
    gchar *title;
    gchar *genre;
    gchar *current_song;
    gchar *stream_uri;
    gchar *type_str;   /* "MP3", "AAC", "OGG", "Other" */
    gint   bitrate;
} IcecastEntry;

void icecast_entry_free(IcecastEntry *entry);

typedef void (*IcecastCallback)(GList *entries, gpointer user_data);
void icecast_fetch_stations(IcecastCallback callback, gpointer user_data);

/* ─── iHeartRadio directory ─── */

typedef struct {
    gint   market_id;
    gint   station_count;
    gchar *city;
    gchar *state;
    gchar *country_code;
} IHRMarket;

typedef struct {
    gchar *title;
    gchar *description;
    gchar *call_letters;
    gchar *stream_uri;
} IHRStation;

void ihr_market_free(IHRMarket *market);
void ihr_station_free(IHRStation *station);

typedef void (*IHRMarketCallback)(GList *markets, gpointer user_data);
typedef void (*IHRStationCallback)(GList *stations, gpointer user_data);

void ihr_fetch_markets(IHRMarketCallback callback, gpointer user_data);
void ihr_fetch_stations(gint market_id, IHRStationCallback callback, gpointer user_data);

/* ─── Playlist URL resolver ─── */

/* Resolve an M3U/PLS/XSPF playlist URL to a direct stream URL.
   If the URL does not appear to be a playlist, returns a copy of the input.
   Caller must g_free() the result. */
gchar* radio_resolve_stream_url(const gchar *url);

/* ─── Legacy convenience ─── */

typedef void (*StationDiscoveryCallback)(GList *stations, gpointer user_data);
void radio_discover_stations(const gchar *genre, StationDiscoveryCallback callback, gpointer user_data);

#endif /* RADIO_H */
