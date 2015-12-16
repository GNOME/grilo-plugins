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
  tags = { 'music', 'net:internet', 'net:plaintext' },
}

------------------
-- Source utils --
------------------

LASTFM_SEARCH_ALBUM = 'http://ws.audioscrobbler.com/2.0/?method=album.getInfo&api_key=%s&artist=%s&album=%s'

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
  url = string.format(LASTFM_SEARCH_ALBUM, grl.goa_consumer_key(), artist, album)
  local userdata = {callback = callback, media = media}
  grl.fetch(url, fetch_page_cb, userdata)
end

---------------
-- Utilities --
---------------

function fetch_page_cb(result, userdata)
  if not result then
    userdata.callback()
    return
  end

  userdata.media.thumbnail = {}
  for k, v in string.gmatch(result, '<image size="(.-)">(.-)</image>') do
    grl.debug ('Image size ' .. k .. ' = ' .. v)
    table.insert(userdata.media.thumbnail, v)
  end

  userdata.callback(userdata.media, 0)
end
