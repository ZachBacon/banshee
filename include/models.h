#ifndef MODELS_H
#define MODELS_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* ============================================================================
 * BansheeTrackObject - GObject wrapper for Track data (for use with GListStore)
 * ============================================================================ */

#define BANSHEE_TYPE_TRACK_OBJECT (banshee_track_object_get_type())
G_DECLARE_FINAL_TYPE(BansheeTrackObject, banshee_track_object, BANSHEE, TRACK_OBJECT, GObject)

BansheeTrackObject* banshee_track_object_new(gint id, gint track_number, 
                                              const gchar *title, const gchar *artist,
                                              const gchar *album, const gchar *duration_str,
                                              gint duration_seconds, const gchar *file_path,
                                              gint play_count);

/* Property accessors */
gint banshee_track_object_get_id(BansheeTrackObject *self);
gint banshee_track_object_get_track_number(BansheeTrackObject *self);
const gchar* banshee_track_object_get_title(BansheeTrackObject *self);
const gchar* banshee_track_object_get_artist(BansheeTrackObject *self);
const gchar* banshee_track_object_get_album(BansheeTrackObject *self);
const gchar* banshee_track_object_get_duration_str(BansheeTrackObject *self);
gint banshee_track_object_get_duration_seconds(BansheeTrackObject *self);
const gchar* banshee_track_object_get_file_path(BansheeTrackObject *self);
gint banshee_track_object_get_play_count(BansheeTrackObject *self);

/* ============================================================================
 * BansheeBrowserItem - GObject wrapper for browser list items
 * ============================================================================ */

#define BANSHEE_TYPE_BROWSER_ITEM (banshee_browser_item_get_type())
G_DECLARE_FINAL_TYPE(BansheeBrowserItem, banshee_browser_item, BANSHEE, BROWSER_ITEM, GObject)

BansheeBrowserItem* banshee_browser_item_new(gint id, const gchar *name, gint count);

gint banshee_browser_item_get_id(BansheeBrowserItem *self);
const gchar* banshee_browser_item_get_name(BansheeBrowserItem *self);
gint banshee_browser_item_get_count(BansheeBrowserItem *self);

/* ============================================================================
 * BansheeSourceObject - GObject wrapper for Source sidebar items
 * ============================================================================ */

#define BANSHEE_TYPE_SOURCE_OBJECT (banshee_source_object_get_type())
G_DECLARE_FINAL_TYPE(BansheeSourceObject, banshee_source_object, BANSHEE, SOURCE_OBJECT, GObject)

/* Forward declare Source type from source.h */
typedef struct _Source Source;

BansheeSourceObject* banshee_source_object_new(const gchar *name, const gchar *icon_name, 
                                                gpointer source_ptr);

const gchar* banshee_source_object_get_name(BansheeSourceObject *self);
const gchar* banshee_source_object_get_icon_name(BansheeSourceObject *self);
gpointer banshee_source_object_get_source(BansheeSourceObject *self);
GListModel* banshee_source_object_get_children(BansheeSourceObject *self);
void banshee_source_object_add_child(BansheeSourceObject *self, BansheeSourceObject *child);

/* ============================================================================
 * BansheePodcastObject - GObject wrapper for podcast list items
 * ============================================================================ */

#define BANSHEE_TYPE_PODCAST_OBJECT (banshee_podcast_object_get_type())
G_DECLARE_FINAL_TYPE(BansheePodcastObject, banshee_podcast_object, BANSHEE, PODCAST_OBJECT, GObject)

BansheePodcastObject* banshee_podcast_object_new(gint id, const gchar *title, const gchar *author);

gint banshee_podcast_object_get_id(BansheePodcastObject *self);
const gchar* banshee_podcast_object_get_title(BansheePodcastObject *self);
const gchar* banshee_podcast_object_get_author(BansheePodcastObject *self);

/* ============================================================================
 * BansheeEpisodeObject - GObject wrapper for episode list items
 * ============================================================================ */

#define BANSHEE_TYPE_EPISODE_OBJECT (banshee_episode_object_get_type())
G_DECLARE_FINAL_TYPE(BansheeEpisodeObject, banshee_episode_object, BANSHEE, EPISODE_OBJECT, GObject)

BansheeEpisodeObject* banshee_episode_object_new(gint id, const gchar *title, 
                                                  const gchar *date, const gchar *duration,
                                                  gboolean downloaded);

gint banshee_episode_object_get_id(BansheeEpisodeObject *self);
const gchar* banshee_episode_object_get_title(BansheeEpisodeObject *self);
const gchar* banshee_episode_object_get_date(BansheeEpisodeObject *self);
const gchar* banshee_episode_object_get_duration(BansheeEpisodeObject *self);
gboolean banshee_episode_object_get_downloaded(BansheeEpisodeObject *self);

/* ============================================================================
 * BansheeVideoObject - GObject wrapper for video list items
 * ============================================================================ */

#define BANSHEE_TYPE_VIDEO_OBJECT (banshee_video_object_get_type())
G_DECLARE_FINAL_TYPE(BansheeVideoObject, banshee_video_object, BANSHEE, VIDEO_OBJECT, GObject)

BansheeVideoObject* banshee_video_object_new(gint id, const gchar *title, 
                                              const gchar *artist, const gchar *duration,
                                              const gchar *file_path);

gint banshee_video_object_get_id(BansheeVideoObject *self);
const gchar* banshee_video_object_get_title(BansheeVideoObject *self);
const gchar* banshee_video_object_get_artist(BansheeVideoObject *self);
const gchar* banshee_video_object_get_duration(BansheeVideoObject *self);
const gchar* banshee_video_object_get_file_path(BansheeVideoObject *self);

/* ============================================================================
 * BansheeChapterObject - GObject wrapper for chapter list items
 * ============================================================================ */

#define BANSHEE_TYPE_CHAPTER_OBJECT (banshee_chapter_object_get_type())
G_DECLARE_FINAL_TYPE(BansheeChapterObject, banshee_chapter_object, BANSHEE, CHAPTER_OBJECT, GObject)

BansheeChapterObject* banshee_chapter_object_new(gdouble start_time, const gchar *title,
                                                  const gchar *img, const gchar *url);

gdouble banshee_chapter_object_get_start_time(BansheeChapterObject *self);
const gchar* banshee_chapter_object_get_title(BansheeChapterObject *self);
const gchar* banshee_chapter_object_get_img(BansheeChapterObject *self);
const gchar* banshee_chapter_object_get_url(BansheeChapterObject *self);

G_END_DECLS

#endif /* MODELS_H */
