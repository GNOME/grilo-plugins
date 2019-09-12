--[[
 * Copyright © 2018 The Grilo Developers.
 *
 * Contact: Jean Felder <jfelder@gnome.org>
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
--]]

---------------------------
-- Source initialization --
---------------------------

source = {
  id = "grl-theaudiodb-cover",
  name = "TheAudioDB Cover",
  description = "a source for album covers and artist art",
  supported_keys = { 'thumbnail' },
  supported_media = { 'audio' },
  config_keys = {
    required = { "api-key" },
  },
  resolve_keys = {
    ["type"] = "audio",
    required = { "artist" },
    optional = { "album" },
  },
  tags = { 'music', 'net:internet' },
}

netopts = {
  user_agent = "Grilo Source TheAudioDB/0.3.8",
}

------------------
-- Source utils --
------------------
theaudiodb = {}
covers_fields = {
  album = {"strAlbumThumb", "strAlbumThumbBack", "strAlbumCDart", "strAlbumSpine"},
  artists = {"strArtistThumb", "strArtistClearart", "strArtistFanart", "strArtistFanart2", "strArtistFanart3"}
}

THEAUDIODB_ROOT_URL = "https://theaudiodb.com/api/v1/json/%s/"
THEAUDIODB_SEARCH_ALBUM = THEAUDIODB_ROOT_URL .. "searchalbum.php?s=%s&a=%s"
THEAUDIODB_SEARCH_ARTIST = THEAUDIODB_ROOT_URL .. "search.php?s=%s"

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

function grl_source_init (configs)
  theaudiodb.api_key = configs.api_key
  return true
end

-- Resolve operation is able to download artist arts or album cover arts.
-- To download an album cover art, both artist and album keys have to be set.
-- To download an artist art, only the artist key has to be set.
function grl_source_resolve()
  local url, keys
  local artist, album
  local search_type

  keys = grl.get_media_keys()

  if not keys.artist or #keys.artist == 0 then
    grl.callback()
    return
  end

  -- Prepare artist and optional album strings to the url
  artist = grl.encode(keys.artist)
  if keys.album and  #keys.album > 0 then
    search_type = "album"
    album = grl.encode(keys.album)
    url = string.format(THEAUDIODB_SEARCH_ALBUM, theaudiodb.api_key, artist, album)
  else
    search_type = "artists"
    url = string.format(THEAUDIODB_SEARCH_ARTIST, theaudiodb.api_key, artist)
  end

  grl.fetch(url, netopts, fetch_cb, search_type)
end

---------------
-- Utilities --
---------------

function fetch_cb(result, search_type)
  local json = {}

  if not result then
    grl.callback()
    return
  end

  json = grl.lua.json.string_to_table(result)
  if not json or not json[search_type] or #json[search_type] == 0 then
    grl.callback()
    return
  end

  local media = {}
  local thumb = {}
  for _, val in ipairs(covers_fields[search_type]) do
    thumb[#thumb + 1] = json[search_type][1][val] or nil
  end

  media.thumbnail = thumb

  grl.callback(media)
end
