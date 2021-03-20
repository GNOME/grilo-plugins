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

-- Test the API at:
-- http://explorer.content.guardianapis.com/search?api-key=rppwmmu3mfqj6gkbs8kcjg23&show-fields=all&page-size=50&tag=type/video
GUARDIANVIDEOS_URL               = 'http://content.guardianapis.com/search?tag=type/video&page=%d&page-size=%d&show-fields=all&api-key=%s'

---------------------------
-- Source initialization --
---------------------------

source = {
  id = "grl-guardianvideos-lua",
  name = "The Guardian Videos",
  description = "A source for browsing videos from the Guardian",
  supported_keys = { "id", "thumbnail", "title", "url" },
  supported_media = 'video',
  config_keys = {
    required = { "api-key" },
  },
  auto_split_threshold = 50,
  icon = 'resource:///org/gnome/grilo/plugins/guardianvideos/guardianvideos.svg',
  tags = { 'news', 'net:internet', 'net:plaintext' }
}

------------------
-- Source utils --
------------------
self = {}

function grl_source_init (configs)
  self.api_key = configs.api_key
  return true
end

function grl_source_browse(media_id)
  local count = grl.get_options("count")
  local skip = grl.get_options("skip")
  local urls = {}

  local page = skip / count + 1
  if page > math.floor(page) then
    local url = string.format(GUARDIANVIDEOS_URL, math.floor(page), count, self.api_key)
    grl.debug ("Fetching URL #1: " .. url .. " (count: " .. count .. " skip: " .. skip .. ")")
    table.insert(urls, url)

    url = string.format(GUARDIANVIDEOS_URL, math.floor(page) + 1, count, self.api_key)
    grl.debug ("Fetching URL #2: " .. url .. " (count: " .. count .. " skip: " .. skip .. ")")
    table.insert(urls, url)
  else
    local url = string.format(GUARDIANVIDEOS_URL, page, count, self.api_key)
    grl.debug ("Fetching URL: " .. url .. " (count: " .. count .. " skip: " .. skip .. ")")
    table.insert(urls, url)
  end

  grl.fetch(urls, guardianvideos_fetch_cb)
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
  media.title = grl.unescape(item.webTitle)

  main = item.fields.main
  -- Some entries rarely don't have a 'main' field. So, we bail out.
  if not main then
    grl.warning ("No media URL metadata found for: " .. media.title)
    return media
  end

  media.url = main:match('src="(.-%.mp4)"')
  media.thumbnail = main:match('poster="(.-%.jpg)"')

  if not media.url then
    grl.warning ("No media streaming URL found for: " .. media.title)
    grl.debug ("Data from: " .. media.title .. "\n" .. grl.lua.inspect(item));
  end

  if not media.thumbnail then
    -- Try to see if we have a 'thumbnail' field. This field seems
    -- to exist for some entries, which don't have a 'poster' field
    if item.fields.thumbnail then
      media.thumbnail = item.fields.thumbnail
    else
      grl.debug ("No media thumbnail URL found for: " .. media.title)
    end
  end

  return media
end
