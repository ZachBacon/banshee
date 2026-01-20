#ifndef MODELS_H
#define MODELS_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* ============================================================================
 * ShriekTrackObject - GObject wrapper for Track data (for use with GListStore)
 * ============================================================================ */

#define SHRIEK_TYPE_TRACK_OBJECT (shriek_track_object_get_type())
G_DECLARE_FINAL_TYPE(ShriekTrackObject, shriek_track_object, SHRIEK, TRACK_OBJECT, GObject)

ShriekTrackObject* shriek_track_object_new(gint id, gint track_number, 
                                              const gchar *title, const gchar *artist,
                                              const gchar *album, const gchar *duration_str,
                                              gint duration_seconds, const gchar *file_path,
                                              gint play_count);

/* Property accessors */
gint shriek_track_object_get_id(ShriekTrackObject *self);
gint shriek_track_object_get_track_number(ShriekTrackObject *self);
const gchar* shriek_track_object_get_title(ShriekTrackObject *self);
const gchar* shriek_track_object_get_artist(ShriekTrackObject *self);
const gchar* shriek_track_object_get_album(ShriekTrackObject *self);
const gchar* shriek_track_object_get_duration_str(ShriekTrackObject *self);
gint shriek_track_object_get_duration_seconds(ShriekTrackObject *self);
const gchar* shriek_track_object_get_file_path(ShriekTrackObject *self);
gint shriek_track_object_get_play_count(ShriekTrackObject *self);

/* ============================================================================
 * ShriekBrowserItem - GObject wrapper for browser list items
 * ============================================================================ */

#define SHRIEK_TYPE_BROWSER_ITEM (shriek_browser_item_get_type())
G_DECLARE_FINAL_TYPE(ShriekBrowserItem, shriek_browser_item, SHRIEK, BROWSER_ITEM, GObject)

ShriekBrowserItem* shriek_browser_item_new(gint id, const gchar *name, gint count);

gint shriek_browser_item_get_id(ShriekBrowserItem *self);
const gchar* shriek_browser_item_get_name(ShriekBrowserItem *self);
gint shriek_browser_item_get_count(ShriekBrowserItem *self);

/* ============================================================================
 * ShriekSourceObject - GObject wrapper for Source sidebar items
 * ============================================================================ */

#define SHRIEK_TYPE_SOURCE_OBJECT (shriek_source_object_get_type())
G_DECLARE_FINAL_TYPE(ShriekSourceObject, shriek_source_object, SHRIEK, SOURCE_OBJECT, GObject)

/* Forward declare Source type from source.h */
typedef struct _Source Source;

ShriekSourceObject* shriek_source_object_new(const gchar *name, const gchar *icon_name, 
                                                gpointer source_ptr);

const gchar* shriek_source_object_get_name(ShriekSourceObject *self);
const gchar* shriek_source_object_get_icon_name(ShriekSourceObject *self);
gpointer shriek_source_object_get_source(ShriekSourceObject *self);
GListModel* shriek_source_object_get_children(ShriekSourceObject *self);
void shriek_source_object_add_child(ShriekSourceObject *self, ShriekSourceObject *child);

/* ============================================================================
 * ShriekPodcastObject - GObject wrapper for podcast list items
 * ============================================================================ */

#define SHRIEK_TYPE_PODCAST_OBJECT (shriek_podcast_object_get_type())
G_DECLARE_FINAL_TYPE(ShriekPodcastObject, shriek_podcast_object, SHRIEK, PODCAST_OBJECT, GObject)

ShriekPodcastObject* shriek_podcast_object_new(gint id, const gchar *title, const gchar *author);

gint shriek_podcast_object_get_id(ShriekPodcastObject *self);
const gchar* shriek_podcast_object_get_title(ShriekPodcastObject *self);
const gchar* shriek_podcast_object_get_author(ShriekPodcastObject *self);

/* ============================================================================
 * ShriekEpisodeObject - GObject wrapper for episode list items
 * ============================================================================ */

#define SHRIEK_TYPE_EPISODE_OBJECT (shriek_episode_object_get_type())
G_DECLARE_FINAL_TYPE(ShriekEpisodeObject, shriek_episode_object, SHRIEK, EPISODE_OBJECT, GObject)

ShriekEpisodeObject* shriek_episode_object_new(gint id, const gchar *title, 
                                                  const gchar *date, const gchar *duration,
                                                  gboolean downloaded);

gint shriek_episode_object_get_id(ShriekEpisodeObject *self);
const gchar* shriek_episode_object_get_title(ShriekEpisodeObject *self);
const gchar* shriek_episode_object_get_date(ShriekEpisodeObject *self);
const gchar* shriek_episode_object_get_duration(ShriekEpisodeObject *self);
gboolean shriek_episode_object_get_downloaded(ShriekEpisodeObject *self);

/* ============================================================================
 * ShriekVideoObject - GObject wrapper for video list items
 * ============================================================================ */

#define SHRIEK_TYPE_VIDEO_OBJECT (shriek_video_object_get_type())
G_DECLARE_FINAL_TYPE(ShriekVideoObject, shriek_video_object, SHRIEK, VIDEO_OBJECT, GObject)

ShriekVideoObject* shriek_video_object_new(gint id, const gchar *title, 
                                              const gchar *artist, const gchar *duration,
                                              const gchar *file_path);

gint shriek_video_object_get_id(ShriekVideoObject *self);
const gchar* shriek_video_object_get_title(ShriekVideoObject *self);
const gchar* shriek_video_object_get_artist(ShriekVideoObject *self);
const gchar* shriek_video_object_get_duration(ShriekVideoObject *self);
const gchar* shriek_video_object_get_file_path(ShriekVideoObject *self);

/* ============================================================================
 * ShriekChapterObject - GObject wrapper for chapter list items
 * ============================================================================ */

#define SHRIEK_TYPE_CHAPTER_OBJECT (shriek_chapter_object_get_type())
G_DECLARE_FINAL_TYPE(ShriekChapterObject, shriek_chapter_object, SHRIEK, CHAPTER_OBJECT, GObject)

ShriekChapterObject* shriek_chapter_object_new(gdouble start_time, const gchar *title,
                                                  const gchar *img, const gchar *url);

gdouble shriek_chapter_object_get_start_time(ShriekChapterObject *self);
const gchar* shriek_chapter_object_get_title(ShriekChapterObject *self);
const gchar* shriek_chapter_object_get_img(ShriekChapterObject *self);
const gchar* shriek_chapter_object_get_url(ShriekChapterObject *self);

G_END_DECLS

#endif /* MODELS_H */
