#include "database.h"
#include "coverart.h"
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

/* Shared extension arrays — single source of truth */
static const gchar *audio_extensions[] = {
    ".mp3", ".ogg", ".flac", ".wav", ".m4a", ".aac", ".opus", ".wma", ".ape", ".mpc",
    NULL
};

static const gchar *video_extensions[] = {
    ".mp4", ".mkv", ".avi", ".mov", ".wmv", ".flv", ".webm", ".m4v", ".mpg", ".mpeg",
    ".3gp", ".ogv", ".ts", ".m2ts", ".vob", ".divx", ".xvid", ".asf", ".rm", ".rmvb",
    NULL
};

static gboolean check_extensions(const gchar *filename, const gchar **extensions) {
    gchar *lower = g_ascii_strdown(filename, -1);
    gboolean match = FALSE;
    for (int i = 0; extensions[i] != NULL; i++) {
        if (g_str_has_suffix(lower, extensions[i])) {
            match = TRUE;
            break;
        }
    }
    g_free(lower);
    return match;
}

static gboolean is_audio_file(const gchar *filename) {
    return check_extensions(filename, audio_extensions);
}

static gboolean is_video_file(const gchar *filename) {
    return check_extensions(filename, video_extensions);
}

static gboolean is_media_file(const gchar *filename) {
    return is_audio_file(filename) || is_video_file(filename);
}

/* Helper: strip file extension by finding the last '.' */
static gchar* strip_extension(const gchar *basename) {
    const gchar *dot = strrchr(basename, '.');
    if (dot && dot != basename) {
        return g_strndup(basename, dot - basename);
    }
    return g_strdup(basename);
}

static void extract_tags_from_file(const gchar *filepath, Track *track) {
    GstElement *pipeline = gst_parse_launch("filesrc name=src ! decodebin ! fakesink", NULL);
    if (!pipeline) return;
    
    GstElement *filesrc = gst_bin_get_by_name(GST_BIN(pipeline), "src");
    g_object_set(filesrc, "location", filepath, NULL);
    gst_object_unref(filesrc);
    
    gst_element_set_state(pipeline, GST_STATE_PAUSED);
    
    GstBus *bus = gst_element_get_bus(pipeline);
    
    /* Loop collecting TAG messages until ASYNC_DONE or error/timeout.
     * GStreamer often sends multiple TAG messages as different elements
     * discover metadata. */
    gboolean done = FALSE;
    while (!done) {
        GstMessage *msg = gst_bus_timed_pop_filtered(bus, 5 * GST_SECOND,
            GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_TAG | GST_MESSAGE_ERROR);
        
        if (!msg) break;  /* Timeout */
        
        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_TAG: {
                GstTagList *tags = NULL;
                gst_message_parse_tag(msg, &tags);
                
                gchar *val = NULL;
                guint uval = 0;
                
                if (gst_tag_list_get_string(tags, GST_TAG_TITLE, &val) && val) {
                    g_free(track->title);
                    track->title = val;
                }
                if (gst_tag_list_get_string(tags, GST_TAG_ARTIST, &val) && val) {
                    g_free(track->artist);
                    track->artist = val;
                }
                if (gst_tag_list_get_string(tags, GST_TAG_ALBUM, &val) && val) {
                    g_free(track->album);
                    track->album = val;
                }
                if (gst_tag_list_get_string(tags, GST_TAG_GENRE, &val) && val) {
                    g_free(track->genre);
                    track->genre = val;
                }
                if (gst_tag_list_get_uint(tags, GST_TAG_TRACK_NUMBER, &uval) && uval > 0) {
                    track->track_number = (gint)uval;
                }
                
                gst_tag_list_unref(tags);
                break;
            }
            case GST_MESSAGE_ASYNC_DONE:
                done = TRUE;
                break;
            case GST_MESSAGE_ERROR:
                done = TRUE;
                break;
            default:
                break;
        }
        gst_message_unref(msg);
    }
    
    /* Get duration (more reliable after ASYNC_DONE) */
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
        } else if (is_media_file(name)) {
            Track *track = g_new0(Track, 1);
            track->file_path = g_strdup(fullpath);
            
            /* Extract title from filename as fallback */
            gchar *basename = g_path_get_basename(fullpath);
            track->title = strip_extension(basename);
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
            
            /* Add to database - all media files go to tracks table for consistency */
            if (database_add_track(db, track) > 0) {
                (*count)++;
                if (*count % 10 == 0) {
                    g_print("Imported %d tracks...\r", *count);
                    fflush(stdout);
                    
                    /* Process pending events to keep UI responsive (GTK4) */
                    while (g_main_context_pending(NULL)) {
                        g_main_context_iteration(NULL, FALSE);
                    }
                }
            }
            
            database_free_track(track);
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

static void scan_audio_files_recursive(const gchar *path, Database *db, CoverArtManager *cover_mgr, gint *count) {
    GDir *dir = g_dir_open(path, 0, NULL);
    if (!dir) return;
    
    const gchar *name;
    while ((name = g_dir_read_name(dir)) != NULL) {
        gchar *fullpath = g_build_filename(path, name, NULL);
        
        if (g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
            scan_audio_files_recursive(fullpath, db, cover_mgr, count);
        } else if (is_audio_file(name)) {
            Track *track = g_new0(Track, 1);
            track->file_path = g_strdup(fullpath);
            
            /* Extract title from filename as fallback */
            gchar *basename = g_path_get_basename(fullpath);
            track->title = strip_extension(basename);
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
            
            /* Add to database - only audio files */
            if (database_add_track(db, track) > 0) {
                (*count)++;
                if (*count % 10 == 0) {
                    g_print("Imported %d audio tracks...\r", *count);
                    fflush(stdout);
                    
                    /* Process pending events to keep UI responsive (GTK4) */
                    while (g_main_context_pending(NULL)) {
                        g_main_context_iteration(NULL, FALSE);
                    }
                }
            }
            
            database_free_track(track);
        }
        g_free(fullpath);
    }
    
    g_dir_close(dir);
}

static void scan_video_files_recursive(const gchar *path, Database *db, CoverArtManager *cover_mgr, gint *count) {
    GDir *dir = g_dir_open(path, 0, NULL);
    if (!dir) return;
    
    const gchar *name;
    while ((name = g_dir_read_name(dir)) != NULL) {
        gchar *fullpath = g_build_filename(path, name, NULL);
        
        if (g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
            scan_video_files_recursive(fullpath, db, cover_mgr, count);
        } else if (is_video_file(name)) {
            Track *track = g_new0(Track, 1);
            track->file_path = g_strdup(fullpath);
            
            /* Extract title from filename as fallback */
            gchar *basename = g_path_get_basename(fullpath);
            track->title = strip_extension(basename);
            g_free(basename);
            
            track->artist = g_strdup("Unknown Artist");
            track->album = g_strdup("Unknown Album");
            track->date_added = g_get_real_time() / 1000000;
            
            /* Try to extract metadata */
            extract_tags_from_file(fullpath, track);
            
            /* Add to database - only video files */
            if (database_add_track(db, track) > 0) {
                (*count)++;
                if (*count % 10 == 0) {
                    g_print("Imported %d video files...\r", *count);
                    fflush(stdout);
                    
                    /* Process pending events to keep UI responsive (GTK4) */
                    while (g_main_context_pending(NULL)) {
                        g_main_context_iteration(NULL, FALSE);
                    }
                }
            }
            
            database_free_track(track);
        }
        g_free(fullpath);
    }
    
    g_dir_close(dir);
}

void import_audio_from_directory_with_covers(const gchar *directory, Database *db, CoverArtManager *cover_mgr) {
    gint count = 0;
    
    g_print("Scanning %s for audio files", cover_mgr ? " and extracting cover art" : "");
    g_print("...\n");
    scan_audio_files_recursive(directory, db, cover_mgr, &count);
    g_print("\nImported %d audio tracks total.\n", count);
}

void import_video_from_directory_with_covers(const gchar *directory, Database *db, CoverArtManager *cover_mgr) {
    gint count = 0;
    
    g_print("Scanning for video files...\n");
    scan_video_files_recursive(directory, db, cover_mgr, &count);
    g_print("\nImported %d video files total.\n", count);
}
