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

#include "radio.h"
#include <string.h>
#include <curl/curl.h>
#include <json-glib/json-glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

/* ═══════════════════════════════════════════════════════════════════════════
   CURL helpers
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    gchar *data;
    gsize  len;
} CurlBuffer;

static gsize curl_write_cb(char *ptr, gsize size, gsize nmemb, void *userdata) {
    CurlBuffer *buf = userdata;
    gsize bytes = size * nmemb;
    buf->data = g_realloc(buf->data, buf->len + bytes + 1);
    memcpy(buf->data + buf->len, ptr, bytes);
    buf->len += bytes;
    buf->data[buf->len] = '\0';
    return bytes;
}

/* Perform a GET or POST and return the body (caller g_free's buf->data).
   Returns TRUE on HTTP 200. */
static gboolean curl_fetch(const gchar *url, const gchar *post_data, CurlBuffer *buf) {
    CURL *curl = curl_easy_init();
    if (!curl) return FALSE;

    buf->data = NULL;
    buf->len  = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Shriek/1.0");

    if (post_data) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER,
            curl_slist_append(NULL, "Content-Type: application/x-www-form-urlencoded"));
    }

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code != 200) {
        g_free(buf->data);
        buf->data = NULL;
        buf->len  = 0;
        return FALSE;
    }
    return TRUE;
}

/* ═══════════════════════════════════════════════════════════════════════════
   Playlist URL resolver – handles M3U, PLS, and XSPF playlists
   ═══════════════════════════════════════════════════════════════════════════ */

/* Extract the first http(s) URL from M3U content. */
static gchar* extract_url_from_m3u(const gchar *data) {
    const gchar *p = data;
    while (p && *p) {
        /* Skip leading whitespace */
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p == '\0') break;

        /* Find end of line */
        const gchar *eol = strpbrk(p, "\r\n");
        gsize line_len = eol ? (gsize)(eol - p) : strlen(p);

        /* Skip comments (#EXTM3U, #EXTINF, etc.) and blank lines */
        if (line_len > 0 && *p != '#') {
            gchar *line = g_strndup(p, line_len);
            g_strstrip(line);
            if (g_str_has_prefix(line, "http://") || g_str_has_prefix(line, "https://")) {
                return line;
            }
            g_free(line);
        }
        p = eol ? eol + 1 : NULL;
    }
    return NULL;
}

/* Extract the first File= URL from PLS content. */
static gchar* extract_url_from_pls(const gchar *data) {
    const gchar *p = data;
    while (p && *p) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p == '\0') break;

        const gchar *eol = strpbrk(p, "\r\n");
        gsize line_len = eol ? (gsize)(eol - p) : strlen(p);

        if (line_len > 5 && g_ascii_strncasecmp(p, "File", 4) == 0) {
            /* Find the '=' after FileN */
            const gchar *eq = memchr(p, '=', line_len);
            if (eq) {
                eq++; /* skip '=' */
                gsize val_len = line_len - (gsize)(eq - p);
                gchar *val = g_strndup(eq, val_len);
                g_strstrip(val);
                if (g_str_has_prefix(val, "http://") || g_str_has_prefix(val, "https://")) {
                    return val;
                }
                g_free(val);
            }
        }
        p = eol ? eol + 1 : NULL;
    }
    return NULL;
}

gchar* radio_resolve_stream_url(const gchar *url) {
    if (!url || !*url) return g_strdup("");

    /* Quick check: does the URL look like a playlist? */
    gboolean might_be_playlist = FALSE;
    if (g_str_has_suffix(url, ".m3u") || g_str_has_suffix(url, ".m3u8") ||
        g_str_has_suffix(url, ".pls") || g_str_has_suffix(url, ".xspf") ||
        strstr(url, "tunein-station") != NULL) {
        might_be_playlist = TRUE;
    }
    /* .m3u8 is HLS – GStreamer handles it natively, don't resolve */
    if (g_str_has_suffix(url, ".m3u8")) {
        return g_strdup(url);
    }

    if (!might_be_playlist) return g_strdup(url);

    /* Fetch the playlist content */
    CurlBuffer buf;
    if (!curl_fetch(url, NULL, &buf) || !buf.data) {
        g_warning("radio: failed to resolve playlist URL: %s", url);
        return g_strdup(url); /* fall back to original */
    }

    gchar *resolved = NULL;

    /* Detect format and extract */
    if (g_str_has_prefix(buf.data, "[playlist]") ||
        g_str_has_prefix(buf.data, "[Playlist]") ||
        g_str_has_prefix(buf.data, "[PLAYLIST]")) {
        resolved = extract_url_from_pls(buf.data);
    } else {
        /* Treat as M3U (also works for plain text with URLs) */
        resolved = extract_url_from_m3u(buf.data);
    }

    g_free(buf.data);

    if (resolved) {
        g_debug("radio: resolved %s → %s", url, resolved);
        return resolved;
    }

    g_warning("radio: no stream URL found in playlist: %s", url);
    return g_strdup(url);
}

/* ═══════════════════════════════════════════════════════════════════════════
   Async-callback plumbing (runs callback on main thread via g_idle_add)
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    GList    *results;
    GCallback callback;
    gpointer  user_data;
} IdleCallbackData;

static gboolean idle_deliver_results(gpointer data) {
    IdleCallbackData *icd = data;
    /* All callbacks share the same signature: (GList*, gpointer) */
    void (*fn)(GList*, gpointer) = (void (*)(GList*, gpointer))icd->callback;
    fn(icd->results, icd->user_data);
    g_free(icd);
    return G_SOURCE_REMOVE;
}

static void deliver_on_main_thread(GList *results, GCallback callback, gpointer user_data) {
    IdleCallbackData *icd = g_new0(IdleCallbackData, 1);
    icd->results   = results;
    icd->callback  = callback;
    icd->user_data = user_data;
    g_idle_add(idle_deliver_results, icd);
}

/* ═══════════════════════════════════════════════════════════════════════════
   Local RadioStation (database-backed) – unchanged from the original
   ═══════════════════════════════════════════════════════════════════════════ */

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
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return -1;

    sqlite3_bind_text(stmt, 1, station->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, station->url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, station->genre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, station->description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, station->bitrate);
    sqlite3_bind_text(stmt, 6, station->homepage, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 7, station->date_added);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return -1;
    return (gint)sqlite3_last_insert_rowid(db->db);
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
            station->id          = sqlite3_column_int(stmt, 0);
            station->name        = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
            station->url         = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
            station->genre       = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
            station->description = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
            station->bitrate     = sqlite3_column_int(stmt, 5);
            station->homepage    = g_strdup((const gchar *)sqlite3_column_text(stmt, 6));
            station->date_added  = sqlite3_column_int64(stmt, 7);
            station->play_count  = sqlite3_column_int(stmt, 8);
            stations = g_list_prepend(stations, station);
        }
        sqlite3_finalize(stmt);
    }
    return g_list_reverse(stations);
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
            station->id          = sqlite3_column_int(stmt, 0);
            station->name        = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
            station->url         = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
            station->genre       = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
            station->description = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
            station->bitrate     = sqlite3_column_int(stmt, 5);
            station->homepage    = g_strdup((const gchar *)sqlite3_column_text(stmt, 6));
            station->date_added  = sqlite3_column_int64(stmt, 7);
            station->play_count  = sqlite3_column_int(stmt, 8);
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
            station->id          = sqlite3_column_int(stmt, 0);
            station->name        = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
            station->url         = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
            station->genre       = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
            station->description = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
            station->bitrate     = sqlite3_column_int(stmt, 5);
            station->homepage    = g_strdup((const gchar *)sqlite3_column_text(stmt, 6));
            station->date_added  = sqlite3_column_int64(stmt, 7);
            station->play_count  = sqlite3_column_int(stmt, 8);
            stations = g_list_prepend(stations, station);
        }
        sqlite3_finalize(stmt);
    }
    return g_list_reverse(stations);
}

gboolean radio_station_delete(gint station_id, Database *db) {
    if (!db || !db->db) return FALSE;

    const char *sql = "DELETE FROM radio_stations WHERE id=?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return FALSE;

    sqlite3_bind_int(stmt, 1, station_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

gboolean radio_station_update(RadioStation *station, Database *db) {
    if (!db || !db->db || !station) return FALSE;

    const char *sql = "UPDATE radio_stations SET name=?, url=?, genre=?, description=?, bitrate=?, homepage=? WHERE id=?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return FALSE;

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

/* ═══════════════════════════════════════════════════════════════════════════
   Shoutcast directory  (https://directory.shoutcast.com)
   ═══════════════════════════════════════════════════════════════════════════ */

static const gchar *shoutcast_genres[] = {
    "Top 500 Stations",
    "Alternative", "Blues", "Classical", "Country",
    "Decades", "Easy Listening", "Electronic", "Folk",
    "Inspirational", "International", "Jazz", "Latin",
    "Metal", "Misc", "New Age", "Pop", "Public Radio",
    "R&B and Urban", "Rap", "Reggae", "Rock",
    "Seasonal and Holiday", "Soundtracks", "Talk", "Themes",
    NULL
};

const gchar* const* shoutcast_get_genres(gint *out_count) {
    if (out_count) {
        gint n = 0;
        while (shoutcast_genres[n]) n++;
        *out_count = n;
    }
    return shoutcast_genres;
}

gchar* shoutcast_get_play_url(gint station_id) {
    return g_strdup_printf("https://yp.shoutcast.com/sbin/tunein-station.m3u?id=%d", station_id);
}

void shoutcast_entry_free(ShoutcastEntry *entry) {
    if (!entry) return;
    g_free(entry->title);
    g_free(entry->genre);
    g_free(entry);
}

typedef struct {
    gchar *genre;
    ShoutcastCallback callback;
    gpointer user_data;
} ShoutcastFetchCtx;

static void shoutcast_fetch_thread(GTask *task, gpointer src, gpointer data, GCancellable *cancel) {
    (void)src; (void)cancel;
    ShoutcastFetchCtx *ctx = data;

    gchar *url = NULL;
    gchar *post_data = NULL;

    if (!ctx->genre || g_strcmp0(ctx->genre, "Top 500 Stations") == 0) {
        url = g_strdup("https://directory.shoutcast.com/Home/Top");
        post_data = g_strdup("");  /* endpoint requires POST */
    } else {
        url = g_strdup("https://directory.shoutcast.com/Home/BrowseByGenre");
        post_data = g_strdup_printf("genrename=%s", ctx->genre);
    }

    CurlBuffer buf;
    GList *results = NULL;

    if (curl_fetch(url, post_data, &buf) && buf.data) {
        /* Parse JSON array of station objects */
        JsonParser *parser = json_parser_new();
        if (json_parser_load_from_data(parser, buf.data, (gssize)buf.len, NULL)) {
            JsonNode *root = json_parser_get_root(parser);
            if (root && JSON_NODE_HOLDS_ARRAY(root)) {
                JsonArray *arr = json_node_get_array(root);
                guint len = json_array_get_length(arr);
                g_debug("shoutcast: retrieved %u stations", len);

                for (guint i = 0; i < len; i++) {
                    JsonObject *obj = json_array_get_object_element(arr, i);
                    if (!obj) continue;

                    ShoutcastEntry *e = g_new0(ShoutcastEntry, 1);
                    e->title      = g_strdup(json_object_get_string_member_with_default(obj, "Name", ""));
                    e->genre      = g_strdup(json_object_get_string_member_with_default(obj, "Genre", ""));
                    e->listeners  = (gint)json_object_get_int_member_with_default(obj, "Listeners", 0);
                    e->bitrate    = (gint)json_object_get_int_member_with_default(obj, "Bitrate", 0);
                    e->station_id = (gint)json_object_get_int_member_with_default(obj, "ID", 0);

                    const gchar *fmt = json_object_get_string_member_with_default(obj, "Format", "audio/mpeg");
                    e->is_aac = (g_strcmp0(fmt, "audio/mpeg") != 0);

                    results = g_list_prepend(results, e);
                }
                results = g_list_reverse(results);
            }
        }
        g_object_unref(parser);
        g_free(buf.data);
    }

    g_free(url);
    g_free(post_data);

    deliver_on_main_thread(results, G_CALLBACK(ctx->callback), ctx->user_data);

    g_free(ctx->genre);
    g_free(ctx);
    g_task_return_boolean(task, TRUE);
}

void shoutcast_fetch_stations(const gchar *genre, ShoutcastCallback callback, gpointer user_data) {
    if (!callback) return;

    ShoutcastFetchCtx *ctx = g_new0(ShoutcastFetchCtx, 1);
    ctx->genre     = g_strdup(genre);
    ctx->callback  = callback;
    ctx->user_data = user_data;

    GTask *task = g_task_new(NULL, NULL, NULL, NULL);
    g_task_set_task_data(task, ctx, NULL);  /* freed inside thread */
    g_task_run_in_thread(task, shoutcast_fetch_thread);
    g_object_unref(task);
}

/* ═══════════════════════════════════════════════════════════════════════════
   Icecast directory  (http://dir.xiph.org/yp.xml)
   ═══════════════════════════════════════════════════════════════════════════ */

void icecast_entry_free(IcecastEntry *entry) {
    if (!entry) return;
    g_free(entry->title);
    g_free(entry->genre);
    g_free(entry->current_song);
    g_free(entry->stream_uri);
    g_free(entry->type_str);
    g_free(entry);
}

typedef struct {
    IcecastCallback callback;
    gpointer user_data;
} IcecastFetchCtx;

static void icecast_fetch_thread(GTask *task, gpointer src, gpointer data, GCancellable *cancel) {
    (void)src; (void)cancel;
    IcecastFetchCtx *ctx = data;

    CurlBuffer buf;
    GList *results = NULL;

    if (curl_fetch("http://dir.xiph.org/yp.xml", NULL, &buf) && buf.data) {
        xmlDocPtr doc = xmlReadMemory(buf.data, (int)buf.len, "yp.xml", NULL,
                                      XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
        if (doc) {
            xmlNodePtr root = xmlDocGetRootElement(doc);
            for (xmlNodePtr node = root ? root->children : NULL; node; node = node->next) {
                if (node->type != XML_ELEMENT_NODE) continue;
                if (xmlStrcmp(node->name, (const xmlChar *)"entry") != 0) continue;

                IcecastEntry *e = g_new0(IcecastEntry, 1);
                e->type_str = g_strdup("Other");

                for (xmlNodePtr child = node->children; child; child = child->next) {
                    if (child->type != XML_ELEMENT_NODE) continue;
                    xmlChar *text = xmlNodeGetContent(child);
                    if (!text) continue;

                    if (xmlStrcmp(child->name, (const xmlChar *)"server_name") == 0) {
                        e->title = g_strdup((const gchar *)text);
                    } else if (xmlStrcmp(child->name, (const xmlChar *)"listen_url") == 0) {
                        e->stream_uri = g_strdup((const gchar *)text);
                    } else if (xmlStrcmp(child->name, (const xmlChar *)"genre") == 0) {
                        e->genre = g_strdup((const gchar *)text);
                    } else if (xmlStrcmp(child->name, (const xmlChar *)"current_song") == 0) {
                        e->current_song = g_strdup((const gchar *)text);
                    } else if (xmlStrcmp(child->name, (const xmlChar *)"bitrate") == 0) {
                        e->bitrate = atoi((const char *)text);
                    } else if (xmlStrcmp(child->name, (const xmlChar *)"server_type") == 0) {
                        g_free(e->type_str);
                        if (xmlStrcmp(text, (const xmlChar *)"audio/mpeg") == 0)
                            e->type_str = g_strdup("MP3");
                        else if (xmlStrcmp(text, (const xmlChar *)"audio/aacp") == 0)
                            e->type_str = g_strdup("AAC");
                        else if (xmlStrcmp(text, (const xmlChar *)"application/ogg") == 0)
                            e->type_str = g_strdup("OGG");
                        else
                            e->type_str = g_strdup("Other");
                    }

                    xmlFree(text);
                }

                results = g_list_prepend(results, e);
            }
            xmlFreeDoc(doc);
        }
        g_free(buf.data);
    }
    results = g_list_reverse(results);

    g_debug("icecast: retrieved %u stations", g_list_length(results));

    deliver_on_main_thread(results, G_CALLBACK(ctx->callback), ctx->user_data);

    g_free(ctx);
    g_task_return_boolean(task, TRUE);
}

void icecast_fetch_stations(IcecastCallback callback, gpointer user_data) {
    if (!callback) return;

    IcecastFetchCtx *ctx = g_new0(IcecastFetchCtx, 1);
    ctx->callback  = callback;
    ctx->user_data = user_data;

    GTask *task = g_task_new(NULL, NULL, NULL, NULL);
    g_task_set_task_data(task, ctx, NULL);
    g_task_run_in_thread(task, icecast_fetch_thread);
    g_object_unref(task);
}

/* ═══════════════════════════════════════════════════════════════════════════
   iHeartRadio directory  (https://api.iheart.com)
   ═══════════════════════════════════════════════════════════════════════════ */

void ihr_market_free(IHRMarket *m) {
    if (!m) return;
    g_free(m->city);
    g_free(m->state);
    g_free(m->country_code);
    g_free(m);
}

void ihr_station_free(IHRStation *s) {
    if (!s) return;
    g_free(s->title);
    g_free(s->description);
    g_free(s->call_letters);
    g_free(s->stream_uri);
    g_free(s);
}

/* ── Markets ── */

typedef struct {
    IHRMarketCallback callback;
    gpointer user_data;
} IHRMarketFetchCtx;

static void ihr_markets_thread(GTask *task, gpointer src, gpointer data, GCancellable *cancel) {
    (void)src; (void)cancel;
    IHRMarketFetchCtx *ctx = data;

    CurlBuffer buf;
    GList *results = NULL;

    if (curl_fetch("https://api.iheart.com/api/v2/content/markets?limit=10000&cache=true", NULL, &buf) && buf.data) {
        JsonParser *parser = json_parser_new();
        if (json_parser_load_from_data(parser, buf.data, (gssize)buf.len, NULL)) {
            JsonNode *root_node = json_parser_get_root(parser);
            if (root_node && JSON_NODE_HOLDS_OBJECT(root_node)) {
                JsonObject *root = json_node_get_object(root_node);
                JsonArray *hits = json_object_get_array_member(root, "hits");
                if (hits) {
                    guint len = json_array_get_length(hits);
                    g_debug("iheart: fetched %u markets", len);

                    for (guint i = 0; i < len; i++) {
                        JsonObject *mkt = json_array_get_object_element(hits, i);
                        if (!mkt) continue;

                        IHRMarket *m = g_new0(IHRMarket, 1);
                        m->market_id     = (gint)json_object_get_int_member_with_default(mkt, "marketId", 0);
                        m->station_count = (gint)json_object_get_int_member_with_default(mkt, "stationCount", 0);
                        m->city          = g_strdup(json_object_get_string_member_with_default(mkt, "city", ""));
                        m->state         = g_strdup(json_object_get_string_member_with_default(mkt, "stateAbbreviation", ""));
                        m->country_code  = g_strdup(json_object_get_string_member_with_default(mkt, "countryAbbreviation", ""));

                        results = g_list_prepend(results, m);
                    }
                    results = g_list_reverse(results);
                }
            }
        }
        g_object_unref(parser);
        g_free(buf.data);
    }

    deliver_on_main_thread(results, G_CALLBACK(ctx->callback), ctx->user_data);
    g_free(ctx);
    g_task_return_boolean(task, TRUE);
}

void ihr_fetch_markets(IHRMarketCallback callback, gpointer user_data) {
    if (!callback) return;

    IHRMarketFetchCtx *ctx = g_new0(IHRMarketFetchCtx, 1);
    ctx->callback  = callback;
    ctx->user_data = user_data;

    GTask *task = g_task_new(NULL, NULL, NULL, NULL);
    g_task_set_task_data(task, ctx, NULL);
    g_task_run_in_thread(task, ihr_markets_thread);
    g_object_unref(task);
}

/* ── Stations by market ── */

typedef struct {
    gint market_id;
    IHRStationCallback callback;
    gpointer user_data;
} IHRStationFetchCtx;

static void ihr_stations_thread(GTask *task, gpointer src, gpointer data, GCancellable *cancel) {
    (void)src; (void)cancel;
    IHRStationFetchCtx *ctx = data;

    gchar *url = g_strdup_printf(
        "https://api.iheart.com/api/v2/content/liveStations?limit=100&marketId=%d",
        ctx->market_id);

    CurlBuffer buf;
    GList *results = NULL;

    if (curl_fetch(url, NULL, &buf) && buf.data) {
        JsonParser *parser = json_parser_new();
        if (json_parser_load_from_data(parser, buf.data, (gssize)buf.len, NULL)) {
            JsonNode *root_node = json_parser_get_root(parser);
            if (root_node && JSON_NODE_HOLDS_OBJECT(root_node)) {
                JsonObject *root = json_node_get_object(root_node);
                JsonArray *hits = json_object_get_array_member(root, "hits");
                if (hits) {
                    guint len = json_array_get_length(hits);
                    g_debug("iheart: fetched %u stations for market %d", len, ctx->market_id);

                    for (guint i = 0; i < len; i++) {
                        JsonObject *st = json_array_get_object_element(hits, i);
                        if (!st) continue;

                        IHRStation *s = g_new0(IHRStation, 1);
                        s->title        = g_strdup(json_object_get_string_member_with_default(st, "name", ""));
                        s->description  = g_strdup(json_object_get_string_member_with_default(st, "description", ""));
                        s->call_letters = g_strdup(json_object_get_string_member_with_default(st, "callLetters", ""));

                        if (json_object_has_member(st, "streams")) {
                            JsonObject *streams = json_object_get_object_member(st, "streams");
                            if (streams) {
                                /* Prefer HLS (GStreamer handles it natively via hlsdemux),
                                   then fall back to Shoutcast streams */
                                static const char *stream_keys[] = {
                                    "secure_hls_stream",
                                    "hls_stream",
                                    "secure_shoutcast_stream",
                                    "shoutcast_stream",
                                    NULL
                                };
                                for (int k = 0; stream_keys[k]; k++) {
                                    const gchar *v = json_object_get_string_member_with_default(
                                        streams, stream_keys[k], "");
                                    if (v && *v) {
                                        s->stream_uri = g_strdup(v);
                                        g_debug("iheart: using %s = %s", stream_keys[k], v);
                                        break;
                                    }
                                }
                            }
                        }
                        if (!s->stream_uri) s->stream_uri = g_strdup("");

                        results = g_list_prepend(results, s);
                    }
                    results = g_list_reverse(results);
                }
            }
        }
        g_object_unref(parser);
        g_free(buf.data);
    }

    g_free(url);
    deliver_on_main_thread(results, G_CALLBACK(ctx->callback), ctx->user_data);
    g_free(ctx);
    g_task_return_boolean(task, TRUE);
}

void ihr_fetch_stations(gint market_id, IHRStationCallback callback, gpointer user_data) {
    if (!callback) return;

    IHRStationFetchCtx *ctx = g_new0(IHRStationFetchCtx, 1);
    ctx->market_id = market_id;
    ctx->callback  = callback;
    ctx->user_data = user_data;

    GTask *task = g_task_new(NULL, NULL, NULL, NULL);
    g_task_set_task_data(task, ctx, NULL);
    g_task_run_in_thread(task, ihr_stations_thread);
    g_object_unref(task);
}

/* ═══════════════════════════════════════════════════════════════════════════
   Legacy convenience – fetches Shoutcast top-500 and adapts to RadioStation*
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    StationDiscoveryCallback callback;
    gpointer user_data;
} DiscoverCtx;

static void discover_shoutcast_done(GList *entries, gpointer user_data) {
    DiscoverCtx *ctx = user_data;
    GList *stations = NULL;

    for (GList *l = entries; l; l = l->next) {
        ShoutcastEntry *e = l->data;
        RadioStation *rs = radio_station_new(e->title, shoutcast_get_play_url(e->station_id));
        /* shoutcast_get_play_url returned a freshly allocated string,
           but radio_station_new g_strdup's it, so free the intermediate */
        g_free(rs->url);
        rs->url = shoutcast_get_play_url(e->station_id);
        rs->genre   = g_strdup(e->genre);
        rs->bitrate = e->bitrate;
        stations = g_list_prepend(stations, rs);
    }
    stations = g_list_reverse(stations);

    g_list_free_full(entries, (GDestroyNotify)shoutcast_entry_free);

    if (ctx->callback) ctx->callback(stations, ctx->user_data);
    g_free(ctx);
}

void radio_discover_stations(const gchar *genre, StationDiscoveryCallback callback, gpointer user_data) {
    DiscoverCtx *ctx = g_new0(DiscoverCtx, 1);
    ctx->callback  = callback;
    ctx->user_data = user_data;
    shoutcast_fetch_stations(genre, discover_shoutcast_done, ctx);
}
