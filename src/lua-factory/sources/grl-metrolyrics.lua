--[[
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
--]]

---------------------------
-- Source initialization --
---------------------------

source = {
  id = "grl-metrolyrics",
  name = "Metrolyrics",
  description = "a source for lyrics",
  supported_keys = { "lyrics" },
  supported_media = { 'audio', 'video' },
  resolve_keys = {
    ["type"] = "audio",
    required = { "artist", "title" },
  },
  tags = { 'music', 'net:internet', 'net:plaintext' },
}

netopts = {
  user_agent = "Grilo Source Metrolyrics/0.2.8",
}

------------------
-- Source utils --
------------------

METROLYRICS_INVALID_URL_CHARS = "[" .. "%(%)%[%]%$%&" .. "]"
METROLYRICS_DEFAULT_QUERY = "http://www.metrolyrics.com/%s-lyrics-%s.html"

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

function grl_source_resolve()
  local url, req
  local artist, title

  req = grl.get_media_keys()
  if not req or not req.artist or not req.title
    or #req.artist == 0 or #req.title == 0 then
    grl.callback()
    return
  end

  -- Prepare artist and title strings to the url
  artist = req.artist:gsub(METROLYRICS_INVALID_URL_CHARS, "")
  artist = artist:gsub("%s+", "-")
  artist = grl.encode(artist)
  title = req.title:gsub(METROLYRICS_INVALID_URL_CHARS, "")
  title = title:gsub("%s+", "-")
  title = grl.encode(title)
  url = string.format(METROLYRICS_DEFAULT_QUERY, title, artist)
  grl.fetch(url, fetch_page_cb, netopts)
end

---------------
-- Utilities --
---------------

function fetch_page_cb(feed)
  local media = nil
  if feed and not feed:find("notfound") then
    media = metrolyrics_get_lyrics(feed)
  end
  grl.callback(media, 0)
end

function metrolyrics_get_lyrics(feed)
  local media = {}
  local lyrics_body = '<div id="lyrics%-body%-text".->(.-)</div>'
  local noise_array = {
    { noise = "</p>",  sub = "\n\n" },
    { noise = "<p class='verse'><p class='verse'>",  sub = "\n\n" },
    { noise = "<p class='verse'>",  sub = "" },
    { noise = "<br/>",  sub = "" },
    { noise = "<br>",  sub = "" },
  }

  -- remove html noise
  feed = feed:match(lyrics_body)
  if not feed then
    grl.warning ("This Lyrics do not match our parser! Please file a bug!")
    return nil
  end

  for _, it in ipairs (noise_array) do
    feed = feed:gsub(it.noise, it.sub)
  end

  -- strip the lyrics
  feed = feed:gsub("^[%s%W]*(.-)[%s%W]*$", "%1")

  -- switch table to string
  media.lyrics = feed
  return media
end
