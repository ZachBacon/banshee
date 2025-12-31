#include "database.h"
#include "coverart.h"
#include <gst/gst.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

static gboolean is_audio_file(const gchar *filename) {
    const gchar *extensions[] = {
        ".mp3", ".ogg", ".flac", ".wav", ".m4a", ".aac", ".opus", ".wma", NULL
    };
    
    gchar *lower = g_ascii_strdown(filename, -1);
    gboolean is_audio = FALSE;
    
    for (int i = 0; extensions[i] != NULL; i++) {
        if (g_str_has_suffix(lower, extensions[i])) {
            is_audio = TRUE;
            break;
        }
    }
    
    g_free(lower);
    return is_audio;
}

static void extract_tags_from_file(const gchar *filepath, Track *track) {
    GstElement *pipeline = gst_parse_launch("filesrc name=src ! decodebin ! fakesink", NULL);
    if (!pipeline) return;
    
    GstElement *filesrc = gst_bin_get_by_name(GST_BIN(pipeline), "src");
    g_object_set(filesrc, "location", filepath, NULL);
    gst_object_unref(filesrc);
    
    gst_element_set_state(pipeline, GST_STATE_PAUSED);
    
    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, 5 * GST_SECOND,
        GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_TAG | GST_MESSAGE_ERROR);
    
    if (msg) {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_TAG) {
            GstTagList *tags = NULL;
            gst_message_parse_tag(msg, &tags);
            
            gchar *title = NULL, *artist = NULL, *album = NULL, *genre = NULL;
            guint year = 0;
            
            gst_tag_list_get_string(tags, GST_TAG_TITLE, &title);
            gst_tag_list_get_string(tags, GST_TAG_ARTIST, &artist);
            gst_tag_list_get_string(tags, GST_TAG_ALBUM, &album);
            gst_tag_list_get_string(tags, GST_TAG_GENRE, &genre);
            gst_tag_list_get_uint(tags, GST_TAG_DATE_TIME, &year);
            
            if (title) {
                g_free(track->title);
                track->title = title;
            }
            if (artist) {
                g_free(track->artist);
                track->artist = artist;
            }
            if (album) {
                g_free(track->album);
                track->album = album;
            }
            if (genre) {
                g_free(track->genre);
                track->genre = genre;
            }
            
            gst_tag_list_unref(tags);
        }
        gst_message_unref(msg);
    }
    
    /* Get duration */
    gint64 duration;
    if (gst_element_query_duration(pipeline, GST_FORMAT_TIME, &duration)) {
        track->duration = (gint)(duration / GST_SECOND);
    }
    
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

static void scan_directory_recursive(const gchar *path, Database *db, CoverArtManager *cover_mgr, gint *count) {
    GDir *dir = g_dir_open(path, 0, NULL);
    if (!dir) return;
    
    const gchar *name;
    while ((name = g_dir_read_name(dir)) != NULL) {
        gchar *fullpath = g_build_filename(path, name, NULL);
        
        if (g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
            scan_directory_recursive(fullpath, db, cover_mgr, count);
        } else if (is_audio_file(name)) {
            Track *track = g_new0(Track, 1);
            track->file_path = g_strdup(fullpath);
            
            /* Extract title from filename as fallback */
            gchar *basename = g_path_get_basename(fullpath);
            gchar *title = g_strndup(basename, strlen(basename) - 4); /* Remove extension */
            track->title = title;
            g_free(basename);
            
            track->artist = g_strdup("Unknown Artist");
            track->album = g_strdup("Unknown Album");
            track->date_added = g_get_real_time() / 1000000;
            
            /* Try to extract metadata */
            extract_tags_from_file(fullpath, track);
            
            /* Extract and cache album art if we have a cover art manager */
            if (cover_mgr && track->artist && track->album) {
                coverart_extract_and_cache(cover_mgr, fullpath, track->artist, track->album);
            }
            
            /* Add to database */
            if (database_add_track(db, track) > 0) {
                (*count)++;
                if (*count % 10 == 0) {
                    g_print("Imported %d tracks...\r", *count);
                    fflush(stdout);
                    
                    /* Process pending GTK events to keep UI responsive */
                    while (gtk_events_pending()) {
                        gtk_main_iteration();
                    }
                }
            }
            
            g_free(track->title);
            g_free(track->artist);
            g_free(track->album);
            if (track->genre) g_free(track->genre);
            g_free(track->file_path);
            g_free(track);
        }
        
        g_free(fullpath);
    }
    
    g_dir_close(dir);
}

void import_media_from_directory_with_covers(const gchar *directory, Database *db, CoverArtManager *cover_mgr) {
    gint count = 0;
    
    g_print("Scanning %s for media files", cover_mgr ? " and extracting cover art" : "");
    g_print("...\n");
    scan_directory_recursive(directory, db, cover_mgr, &count);
    g_print("\nImported %d tracks total.\n", count);
}

void import_media_from_directory(const gchar *directory, Database *db) {
    import_media_from_directory_with_covers(directory, db, NULL);
}
