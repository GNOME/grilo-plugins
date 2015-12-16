--[[
 * Copyright (C) 2015 Bastien Nocera
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

-- Documented at:
-- http://getpocket.com/developer/docs/v3/retrieve
--
-- We only get videos here because if we didn't filter ahead of time
-- we'd need to check whether each URL was supported through
-- totem-pl-parser/quvi, which would be too slow
POCKET_GET_URL   = 'https://getpocket.com/v3/get?consumer_key=%s&access_token=%s&sort=newest&contentType=video&detailType=complete&count=%d&offset=%d'

HAS_VIDEO = '1'
IS_VIDEO = '2'

---------------------------
-- Source initialization --
---------------------------

source = {
  id = "grl-pocket-lua",
  name = 'Pocket',
  description = 'A source for browsing Pocket videos',
  goa_account_provider = 'pocket',
  goa_account_feature = 'read-later',
  supported_keys = { 'id', 'thumbnail', 'title', 'url', 'favourite', 'creation-date' },
  supported_media = 'video',
  icon = 'resource:///org/gnome/grilo/plugins/pocket/pocket.svg',
  tags = { 'net:internet' }
}

------------------
-- Source utils --
------------------

function grl_source_browse(media, options, callback)
  local count = options.count
  local skip = options.skip

  local url = string.format(POCKET_GET_URL, grl.goa_consumer_key(), grl.goa_access_token(), count, skip)
  grl.debug ("Fetching URL: " .. url .. " (count: " .. count .. " skip: " .. skip .. ")")

  local userdata = {callback = callback, count = count}
  grl.fetch(url, pocket_fetch_cb, userdata)
end

------------------------
-- Callback functions --
------------------------

-- Newest first
function sort_added_func(itema, itemb)
  return itema.time_added > itemb.time_added
end

-- From http://lua-users.org/wiki/StringRecipes
function string.starts(String,Start)
  return string.sub(String,1,string.len(Start))==Start
end

-- return all the media found
function pocket_fetch_cb(results, userdata)
  local count = userdata.count

  if not results then
    userdata.callback()
    return
  end

  json = grl.lua.json.string_to_table(results)

  -- Put the table in an array so we can sort it
  local array = {}
  for n, item in pairs(json.list) do table.insert(array, item) end
  table.sort(array, sort_added_func)

  for i, item in ipairs(array) do
    local media = create_media(item)
    if media then
      count = count - 1
      userdata.callback(media, count)
    end

    -- Bail out if we've given enough items
    if count == 0 then
      return
    end
  end

  userdata.callback()
end

-------------
-- Helpers --
-------------

function create_media(item)
  local media = {}

  if not item.has_video or
    (item.has_video ~= HAS_VIDEO and item.has_video ~= IS_VIDEO) then
    grl.debug("We filtered for videos, but this isn't one: " .. grl.lua.inspect(item))
    return nil
  end

  if item.has_video == HAS_VIDEO then
    if not item.videos then
      grl.debug('Item has no video, skipping: ' .. grl.lua.inspect(item))
      return nil
    end

    if #item.videos > 1 then
      grl.debug('Item has than one video, skipping: ' .. grl.lua.inspect(item))
      return nil
    end
  end

  media.type = "video"

  media.id = item.resolved_id
  if media.id == '' then
    media.id = item.item_id
  end

  local url = item.resolved_url
  if url == '' then
    url = item.given_url
  end

  if item.has_video == HAS_VIDEO then
    url = item.videos['1'].src

    -- BUG: Pocket puts garbage like:
    -- src = "//player.vimeo.com/video/75911370"
    -- FIXME: this should be https instead but then
    -- quvi doesn't detect it
    if string.starts(url, '//') then url = 'http:' .. url end
  end

  if grl.is_video_site(url) then
    media.external_url = url
  else
    media.url = url
  end

  media.title = item.resolved_title
  if media.title == '' then
    media.title = item.given_title
  end
  if media.title == '' then
    media.title = media.url
  end

  media.favourite = (item.favorite and item.favorite == '1')

  if item.image then
    media.thumbnail = item.image.src
  end

  media.creation_date = item.time_added
  media.modification_date = item.time_updated

  return media
end
