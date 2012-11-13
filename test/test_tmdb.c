#include <grilo.h>
#include <math.h>
#include <float.h>

#define TMDB_PLUGIN_ID "grl-tmdb"
#define TEST_PATH "test_data/tmdb/"

GrlSource *source;
GrlMedia *media;
GrlOperationOptions *options;

static void
setup_tmdb (const char *ini)
{
  GrlConfig *config;
  GrlRegistry *registry;
  GError *error = NULL;

  config = grl_config_new (TMDB_PLUGIN_ID, NULL);
  /* Does not matter what we set it to, just needs to be non-empty as we're
   * going to fake the network responses. */
  grl_config_set_api_key (config, "TMDB_TEST_API_KEY");

  registry = grl_registry_get_default ();
  grl_registry_add_config (registry, config, &error);
  g_assert (error == NULL);

  g_setenv ("GRL_NET_MOCKED", ini, TRUE);

  grl_registry_load_plugin_by_id (registry, TMDB_PLUGIN_ID, &error);
  g_assert (error == NULL);

  source = GRL_SOURCE (grl_registry_lookup_source (registry, TMDB_PLUGIN_ID));
  g_assert (source != NULL);

  g_assert (grl_source_supported_operations (source) & GRL_OP_RESOLVE);

  media = grl_media_video_new ();
  g_assert (media != NULL);

  options = grl_operation_options_new (NULL);
  g_assert (options != NULL);
}

static void
shutdown_tmdb (void)
{
  GrlRegistry *registry;
  GError *error = NULL;

  registry = grl_registry_get_default ();
  grl_registry_unload_plugin (registry, TMDB_PLUGIN_ID, &error);
  g_assert (error == NULL);

  g_object_unref (media);
  media = NULL;

  g_object_unref (options);
  options = NULL;
}

static void
test_preconditions (void)
{
  GrlMedia *local_media;
  GError *error = NULL;

  setup_tmdb ("test_data/tmdb/empty-data.ini");

  local_media = grl_media_audio_new ();

  grl_source_resolve_sync (source,
                           local_media,
                           grl_source_supported_keys (source),
                           options,
                           &error);

  /* Check that the plugin didn't even try to resolve data, otherwise the mock
   * file would have resulted in an error */
  g_assert (error == NULL);

  g_object_unref (local_media);

  local_media = grl_media_image_new ();

  grl_source_resolve_sync (source,
                           local_media,
                           grl_source_supported_keys (source),
                           options,
                           &error);

  /* Check that the plugin didn't even try to resolve data, otherwise the
   * empty mock file would have resulted in an error */
  g_assert (error == NULL);
  g_object_unref (local_media);

  /* Check the same for title-less video */
  grl_source_resolve_sync (source,
                           media,
                           grl_source_supported_keys (source),
                           options,
                           &error);
  g_assert (error == NULL);

  shutdown_tmdb ();
}

static void
test_missing_configuration (void)
{
  GError *error = NULL;

  setup_tmdb (TEST_PATH "empty-data.ini");

  /* Doesn't matter, we just need to get it to resolve */
  grl_media_set_title (media, "Non-Empty");

  grl_source_resolve_sync (source,
                           media,
                           grl_source_supported_keys (source),
                           options,
                           &error);

  /* Check that the plugin didn't even try to resolve data, otherwise the mock
   * file would have resulted in an error */
  g_assert (error != NULL);

  shutdown_tmdb ();
}

/** Compare the floats.
 * A simple == will fail on values that are effectively the same,
 * due to rounding issues.
 */
static gboolean compare_floats(gfloat a, gfloat b)
{
   return fabs(a - b) < DBL_EPSILON;
}

static void
test_fast_resolution (void)
{
  GError *error = NULL;
  GrlKeyID backdrop, original_title, posters;
  GrlRegistry *registry;
  GDateTime *date, *orig;

  setup_tmdb (TEST_PATH "no-details.ini");

  registry = grl_registry_get_default ();
  backdrop = grl_registry_lookup_metadata_key (registry, "tmdb-backdrops");
  original_title = grl_registry_lookup_metadata_key (registry, "tmdb-original-title");
  posters = grl_registry_lookup_metadata_key (registry, "tmdb-poster");

  grl_operation_options_set_flags (options, GRL_RESOLVE_FAST_ONLY);
  grl_media_set_title (media, "TMDBTestTitle");

  grl_source_resolve_sync (source,
                           media,
                           grl_source_supported_keys (source),
                           options,
                           &error);

  /* Fast resolution must not result in an error if the details are missing */
  g_assert (error == NULL);

  /* Check if we have everything we need */
  g_assert (compare_floats (grl_media_get_rating (media), 3.8f));
  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), original_title), ==,
                   "Sherlock Holmes");
  /* There's only one poster/backdrop in the search result */
  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), backdrop), ==,
                   "http://cf2.imgobject.com/t/p/original/uM414ugc1B910bTvGEIzsucfMMC.jpg");

  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), posters), ==,
                   "http://cf2.imgobject.com/t/p/original/22ngurXbLqab7Sko6aTSdwOCe5W.jpg");
  orig = g_date_time_new_utc (2009, 12, 25, 0, 0, 0.0);
  date = grl_media_get_publication_date (media);
  g_assert_cmpint (g_date_time_compare (orig, date), ==, 0);
  g_date_time_unref (orig);

  shutdown_tmdb ();
}

#define DESCRIPTION \
"In a dynamic new portrayal of Arthur Conan Doyle’s most famous characters, “Sherlock Holmes” sends Holmes and his stalwart partner Watson on their latest challenge. Revealing fighting skills as lethal as his legendary intellect, Holmes will battle as never before to bring down a new nemesis and unravel a deadly plot that could destroy England."

static void
test_full_resolution (void)
{
  GError *error = NULL;
  GrlRegistry *registry;
  GrlKeyID backdrop, posters, imdb_id, keywords, performer, producer;
  GrlKeyID director, age_certs, original_title;
  GDateTime *date, *orig;

  setup_tmdb (TEST_PATH "sherlock.ini");

  registry = grl_registry_get_default ();
  backdrop = grl_registry_lookup_metadata_key (registry, "tmdb-backdrops");
  g_assert_cmpint (backdrop, !=, GRL_METADATA_KEY_INVALID);
  posters = grl_registry_lookup_metadata_key (registry, "tmdb-poster");
  g_assert_cmpint (posters, !=, GRL_METADATA_KEY_INVALID);
  imdb_id = grl_registry_lookup_metadata_key (registry, "tmdb-imdb-id");
  g_assert_cmpint (imdb_id, !=, GRL_METADATA_KEY_INVALID);
  keywords = GRL_METADATA_KEY_KEYWORD;
  performer = GRL_METADATA_KEY_PERFORMER;
  producer = GRL_METADATA_KEY_PRODUCER;
  director = GRL_METADATA_KEY_DIRECTOR;
  age_certs = grl_registry_lookup_metadata_key (registry, "tmdb-age-certificates");
  g_assert_cmpint (age_certs, !=, GRL_METADATA_KEY_INVALID);
  original_title = GRL_METADATA_KEY_ORIGINAL_TITLE;

  grl_media_set_title (media, "Sherlock Holmes");

  grl_source_resolve_sync (source,
                           media,
                           grl_source_supported_keys (source),
                           options,
                           &error);
  g_assert (error == NULL);

  /* Check if we got everything we need for the fast resolution */
  g_assert (compare_floats (grl_media_get_rating (media), 3.8f));
  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), original_title), ==,
                   "Sherlock Holmes");
  /* There's only one poster/backdrop in the search result */
  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), backdrop), ==,
                   "http://cf2.imgobject.com/t/p/original/uM414ugc1B910bTvGEIzsucfMMC.jpg");

  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), posters), ==,
                   "http://cf2.imgobject.com/t/p/original/22ngurXbLqab7Sko6aTSdwOCe5W.jpg");
  orig = g_date_time_new_utc (2009, 12, 25, 0, 0, 0.0);
  date = grl_media_get_publication_date (media);
  g_assert_cmpint (g_date_time_compare (orig, date), ==, 0);
  g_date_time_unref (orig);

  /* And now the slow properties */
  g_assert_cmpstr (grl_media_get_site (media), ==,
                   "http://sherlock-holmes-movie.warnerbros.com/");
  g_assert_cmpint (grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_GENRE), ==, 6);
  g_assert_cmpint (grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_STUDIO), ==, 3);

  g_assert_cmpstr (grl_media_get_description (media), ==, DESCRIPTION);
  g_assert_cmpstr (grl_media_get_certificate (media), ==, "PG-13");

  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), imdb_id), ==, "tt0988045");
  g_assert_cmpint (grl_data_length (GRL_DATA (media), keywords), ==, 15);

  g_assert_cmpint (grl_data_length (GRL_DATA (media), performer), ==, 10);

  g_assert_cmpint (grl_data_length (GRL_DATA (media), producer), ==, 9);

  g_assert_cmpint (grl_data_length (GRL_DATA (media), director), ==, 1);
  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), director), ==, "Guy Ritchie");

  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), age_certs), ==,
                   "GB:12A;NL:12;BG:C;HU:16;DE:12;DK:15;US:PG-13");

  shutdown_tmdb ();
}

int
main(int argc, char **argv)
{
  g_setenv ("GRL_PLUGIN_LIST", TMDB_PLUGIN_ID, TRUE);

  grl_init (&argc, &argv);
#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init (NULL);
#endif

  test_preconditions ();
  test_missing_configuration ();
  test_fast_resolution ();
  test_full_resolution ();
}
