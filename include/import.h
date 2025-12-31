#ifndef IMPORT_H
#define IMPORT_H

#include "database.h"
#include "coverart.h"

/* Import media files from a directory recursively */
void import_media_from_directory(const gchar *directory, Database *db);

/* Import media files and extract cover art */
void import_media_from_directory_with_covers(const gchar *directory, Database *db, CoverArtManager *cover_mgr);

#endif /* IMPORT_H */
