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
  title = req.title:gsub(METROLYRICS_INVALID_URL_CHARS, "")
  title = title:gsub("%s+", "-")
  url = string.format(METROLYRICS_DEFAULT_QUERY, title, artist)
  grl.fetch(url, "fetch_page_cb", netopts)
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
  local res = {}
  local lyrics_body = '<div class="lyrics%-body">(.-)</div>'
  local lyrics_verse = "<p class='verse'>(.-)</p>"

  -- from html, get lyrics line by line into table res
  feed = feed:match(lyrics_body)
  for verse in feed:gmatch(lyrics_verse) do
    local start = 1
    local e, s = verse:find("<br/>")
    while (e) do
      res[#res + 1] = verse:sub(start, e-1)
      start = s+1
      e, s = verse:find("<br/>", start)
    end
    res[#res + 1] = verse:sub(start, #verse) .. '\n\n'
  end

  -- switch table to string
  media.lyrics = table.concat(res)
  return media
end
