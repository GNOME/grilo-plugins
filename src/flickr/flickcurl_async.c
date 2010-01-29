#include "flickcurl_async.h"

typedef struct {
  flickcurl *fc;
  flickcurl_search_params *params;
  flickcurl_photos_list_params *list_params;
  PhotosSearchParamsCb callback;
  flickcurl_photos_list *photos_list;
  gpointer user_data;
} PhotosSearchParamsData;

static gboolean
photos_search_params_run_cb (gpointer data)
{
  PhotosSearchParamsData *wrap = (PhotosSearchParamsData *) data;

  wrap->callback (wrap->photos_list, wrap->user_data);
  g_free (wrap);

  return FALSE;
}

static gpointer
photos_search_params_run_main (gpointer data)
{
  PhotosSearchParamsData *wrap = (PhotosSearchParamsData *) data;

  wrap->photos_list = flickcurl_photos_search_params (wrap->fc,
                                                      wrap->params,
                                                      wrap->list_params);
  g_idle_add (photos_search_params_run_cb, wrap);

  return NULL;
}

void
photos_search_params_async (flickcurl *fc,
                            flickcurl_search_params *params,
                            flickcurl_photos_list_params *list_params,
                            PhotosSearchParamsCb callback,
                            gpointer user_data)
{
  PhotosSearchParamsData *wrap = g_new (PhotosSearchParamsData, 1);

  /* Wrap parameters */
  wrap->fc = fc;
  wrap->params = params;
  wrap->list_params = list_params;
  wrap->callback = callback;
  wrap->user_data = user_data;

  if (!g_thread_create (photos_search_params_run_main,
                        wrap,
                        FALSE,
                        NULL)) {
    g_critical ("Unable to create thread");
  }
}
