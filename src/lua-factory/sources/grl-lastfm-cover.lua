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
  id = "grl-lastfm-cover",
  name = "Last.fm Cover",
  description = "a source for music covers",
  goa_account_provider = 'lastfm',
  goa_account_feature = 'music',
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

LASTFM_SEARCH_ALBUM = 'https://ws.audioscrobbler.com/2.0/?method=album.getInfo&api_key=%s&artist=%s&album=%s'

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

function grl_source_resolve()
  local url, req
  local artist, title

  req = grl.get_media_keys()
  if not req or not req.artist or not req.album
    or #req.artist == 0 or #req.album == 0 then
    grl.callback()
    return
  end

  -- Prepare artist and title strings to the url
  artist = grl.encode(req.artist)
  album = grl.encode(req.album)
  url = string.format(LASTFM_SEARCH_ALBUM, grl.goa_consumer_key(), artist, album)
  grl.fetch(url, fetch_page_cb)
end

---------------
-- Utilities --
---------------

function fetch_page_cb(result)
  if not result then
    grl.callback()
    return
  end

  local media = {}
  media.thumbnail = {}
  local image_sizes = { "mega", "extralarge", "large", "medium", "small" }

  for _, size in pairs(image_sizes) do
    local url

    url = string.match(result, '<image size="' .. size .. '">(.-)</image>')
    if url ~= nil and url ~= '' then
      grl.debug ('Image size ' .. size .. ' = ' .. url)
      table.insert(media.thumbnail, url)
    end
  end

  if #media.thumbnail == 0 then
    grl.callback()
  else
    grl.callback(media, 0)
  end
end
