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
  description = "a source for music covers",
  supported_keys = { 'thumbnail' },
  supported_media = { 'audio' },
  config_keys = {
    required = { "api-key" },
  },
  resolve_keys = {
    ["type"] = "audio",
    required = { "artist", "album" },
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
covers_fields = {"strAlbumThumb", "strAlbumThumbBack", "strAlbumCDart", "strAlbumSpine"}

THEAUDIODB_SEARCH_ALBUM = 'https://theaudiodb.com/api/v1/json/%s/searchalbum.php?s=%s&a=%s'

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

function grl_source_init (configs)
  theaudiodb.api_key = configs.api_key
  return true
end

function grl_source_resolve()
  local url, keys
  local artist, title

  keys = grl.get_media_keys()
  if not keys or not keys.artist or not keys.album
    or #keys.artist == 0 or #keys.album == 0 then
    grl.callback()
    return
  end

  -- Prepare artist and title strings to the url
  artist = grl.encode(keys.artist)
  album = grl.encode(keys.album)
  url = string.format(THEAUDIODB_SEARCH_ALBUM, theaudiodb.api_key, artist, album)
  grl.fetch(url, fetch_cb, netopts)
end

---------------
-- Utilities --
---------------

function fetch_cb(result)
  local json = {}

  if not result then
    grl.callback()
    return
  end

  json = grl.lua.json.string_to_table(result)
  if not json or not json.album or #json.album == 0 then
    grl.callback()
    return
  end

  local media = {}
  local thumb = {}
  for _, val in ipairs(covers_fields) do
    thumb[#thumb + 1] = json.album[val] or nil
  end

  media.thumbnail = thumb

  grl.callback(media)
end
