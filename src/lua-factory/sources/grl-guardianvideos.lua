--[[
 * Copyright (C) 2014 Bastien Nocera
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

GUARDIANVIDEOS_URL               = 'http://content.guardianapis.com/search?tag=video&page=%s&page-size=%s&show-fields=all'

---------------------------
-- Source initialization --
---------------------------

source = {
  id = "grl-guardianvideos-lua",
  name = "The Guardian Videos",
  description = "A source for browsing videos from the Guardian",
  supported_keys = { "id", "thumbnail", "title", "url" },
  supported_media = 'video',
  auto_split_threshold = 50,
  tags = { 'news' }
}

------------------
-- Source utils --
------------------

function grl_source_browse(media_id)
  local count = grl.get_options("count")
  local skip = grl.get_options("skip")
  local urls = {}

  local page = skip / count + 1
  if page > math.floor(page) then
    local url = string.format(GUARDIANVIDEOS_URL, math.floor(page), count)
    grl.debug ("Fetching URL #1: " .. url .. " (count: " .. count .. " skip: " .. skip .. ")")
    table.insert(urls, url)

    url = string.format(GUARDIANVIDEOS_URL, math.floor(page) + 1, count)
    grl.debug ("Fetching URL #2: " .. url .. " (count: " .. count .. " skip: " .. skip .. ")")
    table.insert(urls, url)
  else
    local url = string.format(GUARDIANVIDEOS_URL, page, count)
    grl.debug ("Fetching URL: " .. url .. " (count: " .. count .. " skip: " .. skip .. ")")
    table.insert(urls, url)
  end

  grl.fetch(urls, "guardianvideos_fetch_cb")
end

------------------------
-- Callback functions --
------------------------

-- return all the media found
function guardianvideos_fetch_cb(results)
  local count = grl.get_options("count")

  for i, result in ipairs(results) do
    local json = {}
    json = grl.lua.json.string_to_table(result)
    if not json or json.stat == "fail" or not json.response or not json.response.results then
      grl.callback()
      return
    end

    for index, item in pairs(json.response.results) do
      local media = create_media(item)
      count = count - 1
      grl.callback(media, count)
    end

    -- Bail out if we've given enough items
    if count == 0 then
      return
    end
  end
end

-------------
-- Helpers --
-------------

function create_media(item)
  local media = {}

  media.type = "video"
  media.id = item.id
  media.url = item.webUrl
  media.title = item.webTitle
  media.thumbnail = item.fields.thumbnail

  return media
end
