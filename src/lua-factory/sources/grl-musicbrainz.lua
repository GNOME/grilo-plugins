--[[
 * Copyright (C) 2014 Victor Toso.
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
  id = "grl-musicbrainz-coverart",
  name = "Musicbrainz Cover Art",
  description = "a source for coverart",
  supported_keys = { "thumbnail" },
  supported_media = { 'audio', 'video' },
  resolve_keys = {
    ["type"] = "audio",
    required = { "mb-release-id" },
  },
  tags = { 'music', 'net:internet' },
}

netopts = {
  user_agent = "Grilo Source Musicbrainz/0.3.8",
}

MUSICBRAINZ_DEFAULT_QUERY = "https://coverartarchive.org/%s/%s"
MUSICBRAINZ_RELEASES = {
  {name = "release", id = "mb_release_id"},
  {name = "release-group", id = "mb_release_group_id"}
}

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

function grl_source_resolve()
  req = grl.get_media_keys()
  if not req then
    grl.callback()
    return
  end

  -- try to get the cover art associated with the mb_album_id
  -- if it does not exist, try the mb_release_group_id one
  -- if none of them exist, return nothing.
  local urls = {}
  for _, release in ipairs(MUSICBRAINZ_RELEASES) do
    id = req[release.id]
    if id and #id > 0 then
      urls[#urls + 1] = string.format(MUSICBRAINZ_DEFAULT_QUERY, release.name, id)
    end
  end

  grl.fetch(urls, netopts, fetch_results_cb)
end

---------------
-- Utilities --
---------------

function fetch_results_cb(results)
  local json_results = nil

  for index, feed in ipairs(results) do
    local json = grl.lua.json.string_to_table (feed)
    if json and json.images then
      json_results = json.images
      break
    end
  end

  if not json_results then
    grl.callback()
    return
  end

  media = build_media(json_results)
  grl.callback(media)
end

function build_media(results)
  local media = {}
  local res = {}

  if results and #results > 0 then
    local result = results[1]
    -- force urls to https
    res[1] = result.image and result.image:gsub("http://", "https://") or nil

    if result.thumbnails then
      for _, url in pairs(result.thumbnails) do
        res[#res + 1] = url and url:gsub("http://", "https://") or nil
      end
    end
  end

  media.thumbnail = res
  return media
end
