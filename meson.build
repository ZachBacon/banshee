project('banshee', 'c',
  version: '1.0.0',
  default_options: ['warning_level=2', 'c_std=gnu11'])

# Dependencies
gtk_dep = dependency('gtk+-3.0', version: '>=3.22')
gstreamer_dep = dependency('gstreamer-1.0', version: '>=1.14')
gstreamer_video_dep = dependency('gstreamer-video-1.0', version: '>=1.14')
gstreamer_pbutils_dep = dependency('gstreamer-pbutils-1.0', version: '>=1.14')
gstreamer_tag_dep = dependency('gstreamer-tag-1.0', version: '>=1.14')
glib_dep = dependency('glib-2.0', version: '>=2.56')
sqlite_dep = dependency('sqlite3', version: '>=3.22')
libxml_dep = dependency('libxml-2.0', version: '>=2.9')
libcurl_dep = dependency('libcurl', version: '>=7.50')
json_glib_dep = dependency('json-glib-1.0', version: '>=1.2')

# Include directories
inc_dirs = include_directories('include')

# Source files
src_files = [
  'src/main.c',
  'src/player.c',
  'src/database.c',
  'src/ui.c',
  'src/playlist.c',
  'src/browser.c',
  'src/coverart.c',
  'src/smartplaylist.c',
  'src/radio.c',
  'src/source.c',
  'src/import.c',
  'src/albumview.c',
  'src/podcast.c',
  'src/podcastview.c',
  'src/chapterview.c',
  'src/transcriptview.c',
  'src/videoview.c',
]

# Build executable
executable('banshee',
  src_files,
  include_directories: inc_dirs,
  dependencies: [gtk_dep, gstreamer_dep, gstreamer_video_dep, gstreamer_pbutils_dep, gstreamer_tag_dep, glib_dep, sqlite_dep, libxml_dep, libcurl_dep, json_glib_dep],
  install: true
)

# Desktop file (optional - uncomment to install)
# install_data('banshee.desktop',
#   install_dir: join_paths(get_option('datadir'), 'applications')
# )
