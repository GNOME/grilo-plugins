/*
 * Copyright (C) 2014 Victor Toso.
 *
 * Contact: Victor Toso <me@victortoso.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef _GRL_THETVDB_RESOURCEs_H_
#define _GRL_THETVDB_RESOURCEs_H_

#include <gom/gom.h>

/*----- Series ----- */
#define SERIES_TYPE_RESOURCE   \
  (series_resource_get_type())

#define SERIES_TYPE_TYPE  \
  (series_type_get_type())

#define SERIES_RESOURCE(obj)                         \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                \
                               SERIES_TYPE_RESOURCE, \
                               SeriesResource))

#define SERIES_RESOURCE_CLASS(klass)              \
  (G_TYPE_CHECK_CLASS_CAST ((klass),              \
                            SERIES_TYPE_RESOURCE, \
                            SeriesResourceClass))

#define SERIES_IS_RESOURCE(obj)                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                 \
                               SERIES_TYPE_RESOURCE))

#define SERIES_IS_RESOURCE_CLASS(klass)             \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                \
                            SERIES_TYPE_RESOURCE))

#define SERIES_RESOURCE_GET_CLASS(obj)              \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                \
                              SERIES_TYPE_RESOURCE, \
                              SeriesResourceClass))

#define SERIES_TABLE_NAME           "series"
#define SERIES_COLUMN_ID            "id"
#define SERIES_COLUMN_LANGUAGE      "language"
#define SERIES_COLUMN_SERIES_NAME   "series-name"
#define SERIES_COLUMN_SERIES_ID     "series-id"
#define SERIES_COLUMN_STATUS        "status"
#define SERIES_COLUMN_OVERVIEW      "overview"
#define SERIES_COLUMN_IMDB_ID       "imdb-id"
#define SERIES_COLUMN_ZAP2IT_ID     "zap2it-id"
#define SERIES_COLUMN_FIRST_AIRED   "first-aired"
#define SERIES_COLUMN_RATING        "rating"
#define SERIES_COLUMN_ACTOR_NAMES   "actor-names"
#define SERIES_COLUMN_GENRES        "genres"
#define SERIES_COLUMN_URL_BANNER    "url-banner"
#define SERIES_COLUMN_URL_FANART    "url-fanart"
#define SERIES_COLUMN_URL_POSTER    "url-poster"

typedef struct _SeriesResource        SeriesResource;
typedef struct _SeriesResourceClass   SeriesResourceClass;
typedef struct _SeriesResourcePrivate SeriesResourcePrivate;

struct _SeriesResource
{
   GomResource parent;
   SeriesResourcePrivate *priv;
};

struct _SeriesResourceClass
{
   GomResourceClass parent_class;
};

GType series_resource_get_type (void);

/*----- Episodes ----- */
#define EPISODE_TYPE_RESOURCE   \
  (episode_resource_get_type())

#define EPISODE_TYPE_TYPE  \
  (episode_type_get_type())

#define EPISODE_RESOURCE(obj)                         \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                 \
                               EPISODE_TYPE_RESOURCE, \
                               EpisodeResource))

#define EPISODE_RESOURCE_CLASS(klass)              \
  (G_TYPE_CHECK_CLASS_CAST ((klass),               \
                            EPISODE_TYPE_RESOURCE, \
                            EpisodeResourceClass))

#define EPISODE_IS_RESOURCE(obj)                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                  \
                               EPISODE_TYPE_RESOURCE))

#define EPISODE_IS_RESOURCE_CLASS(klass)             \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                 \
                            EPISODE_TYPE_RESOURCE))

#define EPISODE_RESOURCE_GET_CLASS(obj)              \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                 \
                              EPISODE_TYPE_RESOURCE, \
                              EpisodeResourceClass))

#define EPISODE_TABLE_NAME                "episodes"
#define EPISODE_COLUMN_ID                 "id"
#define EPISODE_COLUMN_LANGUAGE           "language"
#define EPISODE_COLUMN_SERIES_ID          "series-id"
#define EPISODE_COLUMN_OVERVIEW           "overview"
#define EPISODE_COLUMN_IMDB_ID            "imdb-id"
#define EPISODE_COLUMN_FIRST_AIRED        "first-aired"
#define EPISODE_COLUMN_RATING             "rating"
#define EPISODE_COLUMN_SEASON_NUMBER      "season-number"
#define EPISODE_COLUMN_EPISODE_NUMBER     "episode-number"
#define EPISODE_COLUMN_ABSOLUTE_NUMBER    "absolute-number"
#define EPISODE_COLUMN_SEASON_ID          "season-id"
#define EPISODE_COLUMN_EPISODE_ID         "episode-id"
#define EPISODE_COLUMN_EPISODE_NAME       "episode-name"
#define EPISODE_COLUMN_URL_EPISODE_SCREEN "url-episode-screen"
#define EPISODE_COLUMN_DIRECTOR_NAMES     "director-names"
#define EPISODE_COLUMN_GUEST_STARS_NAMES  "guest-stars-names"

typedef struct _EpisodeResource        EpisodeResource;
typedef struct _EpisodeResourceClass   EpisodeResourceClass;
typedef struct _EpisodeResourcePrivate EpisodeResourcePrivate;

struct _EpisodeResource
{
   GomResource parent;
   EpisodeResourcePrivate *priv;
};

struct _EpisodeResourceClass
{
   GomResourceClass parent_class;
};

GType episode_resource_get_type (void);

/*----- Series Fuzzy Names ----- */
#define FUZZY_SERIES_NAMES_TYPE_RESOURCE   \
  (fuzzy_series_names_resource_get_type())

#define FUZZY_SERIES_NAMES_TYPE_TYPE  \
  (fuzzy_series_names_type_get_type())

#define FUZZY_SERIES_NAMES_RESOURCE(obj)                         \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                            \
                               FUZZY_SERIES_NAMES_TYPE_RESOURCE, \
                               FuzzySeriesNamesResource))

#define FUZZY_SERIES_NAMES_RESOURCE_CLASS(klass)              \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                          \
                            FUZZY_SERIES_NAMES_TYPE_RESOURCE, \
                            FuzzySeriesNamesResourceClass))

#define FUZZY_SERIES_NAMES_IS_RESOURCE(obj)                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                             \
                               FUZZY_SERIES_NAMES_TYPE_RESOURCE))

#define FUZZY_SERIES_NAMES_IS_RESOURCE_CLASS(klass)             \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                            \
                            FUZZY_SERIES_NAMES_TYPE_RESOURCE))

#define FUZZY_SERIES_NAMES_RESOURCE_GET_CLASS(obj)              \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                            \
                              FUZZY_SERIES_NAMES_TYPE_RESOURCE, \
                              FuzzySeriesNamesResourceClass))

#define FUZZY_SERIES_NAMES_TABLE_NAME           "fuzzy_series_names"
#define FUZZY_SERIES_NAMES_COLUMN_ID            "id"
#define FUZZY_SERIES_NAMES_COLUMN_SERIES_ID     "tvdb-series-id"
#define FUZZY_SERIES_NAMES_COLUMN_FUZZY_NAME    "fuzzy-name"

typedef struct _FuzzySeriesNamesResource        FuzzySeriesNamesResource;
typedef struct _FuzzySeriesNamesResourceClass   FuzzySeriesNamesResourceClass;
typedef struct _FuzzySeriesNamesResourcePrivate FuzzySeriesNamesResourcePrivate;

struct _FuzzySeriesNamesResource
{
   GomResource parent;
   FuzzySeriesNamesResourcePrivate *priv;
};

struct _FuzzySeriesNamesResourceClass
{
   GomResourceClass parent_class;
};

GType fuzzy_series_names_resource_get_type (void);

#endif /* _GRL_THETVDB_RESOURCES_H_ */
