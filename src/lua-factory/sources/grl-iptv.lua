--[[
 * Copyright (C) 2024 Krifa75
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

IPTV_BASE_API_URL            = 'https://iptv-org.github.io/api/'

---------------------------
-- Source initialization --
---------------------------

source = {
  id = "grl-iptv",
  name = "IPTV",
  description = "A source for watching a collection of publicly available channels",
  supported_keys = { "id", "childcount", "title", "region", "url", "site", "thumbnail", "http-referrer", "user-agent" },
  supported_media = 'video',
  tags = { 'tv', 'net:internet' }
}

-- Global table to store channels
ldata = {}

-- Number of items in ldata --
num_channels = 0

------------------
-- Source utils --
------------------

function grl_source_browse(media_id)
  if next(ldata) == nil then
    -- empty data, fetch all channels and send media(s) --
    grl.debug('Channels table is empty, start fetching all...')

    local url = IPTV_BASE_API_URL .. 'streams.json'
    grl.fetch(url, on_get_streams_done_cb, media_id)
  else
    browse_media(media_id)
  end
end

------------------------
-- Callback functions --
------------------------

function on_get_streams_done_cb(results, media_id)
  if not results then
    grl.callback()
    return
  end
  
  local json = grl.lua.json.string_to_table(results)
  if not json then
    grl.callback()
    return
  end

  for _, result in pairs(json) do
    if result.channel ~= nil and result.channel ~= '' then
      if ldata[result.channel] == nil then
        ldata[result.channel] = {
          urls = { result.url },
          num_url = 1,

          -- new keys --
          http_referrer = result.http_referrer,
          user_agent = result.user_agent,
        }
      else
        -- Same channel can have multiple urls --
        table.insert(ldata[result.channel].urls, result.url)
        ldata[result.channel].num_url = ldata[result.channel].num_url + 1
      end
    end
  end

  local url = IPTV_BASE_API_URL .. 'channels.json'
  grl.fetch(url, on_get_channels_done_cb, media_id)
end

function on_get_channels_done_cb(results, media_id)
  if not results then
    grl.callback()
    return
  end
  
  local json = grl.lua.json.string_to_table(results)
  if not json then
    grl.callback()
    return
  end

  for _, result in pairs(json) do
    local is_valid = (result.closed == nil)
    local is_nsfw = result.is_nsfw

    if not is_nsfw and is_valid then
      if ldata[result.id] ~= nill then
        ldata[result.id].name = result.name
        ldata[result.id].country = result.country
        ldata[result.id].website = result.website
        ldata[result.id].logo = result.logo

        num_channels = num_channels + 1
      end
    end
  end

  grl.debug('fetching channels done, start sending media...')

  browse_media(media_id)
end

-------------
-- Helpers --
-------------

function fill_media (id, media, channel, custom_title)
  media.id = id
  media.title = custom_title or channel.name
  media.region = channel.country
  media.site = channel.website
  media.thumbnail = channel.logo
  media.http_referrer = channel.http_referrer
  media.user_agent = channel.user_agent
end

function create_media(id, channel)
  local media = {}

  if channel.num_url > 1 then
    media.type = "container"
    media.childcount = channel.num_url
  else
    media.type = "video"
    media.url = channel.urls[1]
  end

  fill_media (id, media, channel)

  grl.debug('Create media for ID : ' .. id)

  return media
end

function send_child_media(id, channel)
  local count = ldata[id].num_url

  for idx=1,channel.num_url do
    local media = {}
    media.type = "video"

    title = channel.name .. ' alt ' .. idx

    fill_media(id, media, channel, title)

    grl.debug('Send child media for ID : ' .. id)

    media.url = channel.urls[idx]
    count = count - 1
    grl.callback(media, count)
  end
end

function send_all_media()
  local count = grl.get_options("count")
  local skip = grl.get_options("skip")

  if count <= 0 then
    count = num_channels
  end

  for id, channel in pairs(ldata) do
    if skip > 0 then
      skip = skip - 1
    elseif count > 0 then
      if channel.name ~= nil then
        local media = create_media(id, channel)

        count = count - 1

        grl.debug('Send media for ID : ' .. id)

        grl.callback(media, count)
      end
    end
  end

  if count > 0 then
    grl.callback()
  end
end

function browse_media(media_id)
  if media_id ~= nil then
    if ldata[media_id] ~= nil then
      if ldata[media_id].num_url > 0 then
        send_child_media(media_id, ldata[media_id])
      else
        media = create_media(media_id, ldata[media_id])

        grl.debug('Send media for ID : ' .. media_id)

        grl.callback(media)        
      end
    else
      grl.callback()
    end
  else
    send_all_media()
  end
end
