--[[
 * Copyright (C) 2015 Bastien Nocera.
 *
 * Contact: Bastien Nocera <hadess@hadess.net>
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
  id = "grl-spotify-cover",
  name = "Spotify Cover",
  description = "a source for music covers",
  supported_keys = { 'thumbnail' },
  supported_media = { 'audio' },
  resolve_keys = {
    ["type"] = "audio",
    required = { "artist", "album" },
  },
  tags = { 'music', 'net:internet' },
}

------------------
-- Source utils --
------------------

SPOTIFY_SEARCH_ALBUM = 'https://api.spotify.com/v1/search?q=album:%s+artist:%s&type=album&limit=1'

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

function grl_source_resolve(media, options, callback)
  local url
  local artist, title

  if not media or not media.artist or not media.album
    or #media.artist == 0 or #media.album == 0 then
    callback()
    return
  end

  -- Prepare artist and title strings to the url
  artist = grl.encode(media.artist)
  album = grl.encode(media.album)
  url = string.format(SPOTIFY_SEARCH_ALBUM, album, artist)

  local userdata = {callback = callback, media = media}
  grl.fetch(url, fetch_page_cb, userdata)
end

---------------
-- Utilities --
---------------

function fetch_page_cb(result, userdata)
  local json = {}

  if not result then
    userdata.callback()
    return
  end

  json = grl.lua.json.string_to_table(result)
  if not json or
     not json.albums or
     json.albums.total == 0 or
     not json.albums.items or
     not #json.albums.items or
     not json.albums.items[1].images then
    userdata.callback()
    return
  end

  userdata.media.thumbnail = {}
  for i, item in ipairs(json.albums.items[1].images) do
    table.insert(userdata.media.thumbnail, item.url)
  end

  userdata.callback(userdata.media, 0)
end
