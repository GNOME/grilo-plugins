--[[
 * Copyright (C) 2018 Thiago Mendes
 *
 * Contact: Thiago Mendes <thiago@posteo.de>
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
  id = "grl-radio-browser-lua",
  name = "Radio Browser",
  description = "A community with the aim of collecting as many internet radio stations as possible",
  supported_keys = { "country", "codecs", "states", "languages", "tags" },
  icon = 'resource:///org/gnome/grilo/plugins/radiofrance/radiofrance.png',
  supported_media = 'audio',
  tags = { 'radio', 'net:internet', 'net:plaintext' }
}

-------------------
-- API RESOURCES --
-------------------
-- Max items
MAX_ITEMS = 100

-- api top X
API_TOP = 10

-- do not count broken stations
api_hidebroken = false

-- api urls
api_main_url = "http://www.radio-browser.info/webservice/json"
api_search_url = "http://www.radio-browser.info/webservice/json/stations"

api_main_endpoints = {
  {
    id = string.format("%s/%s", api_main_url, "countries"),
    title = "List of countries"
  },
  {
    id = string.format("%s/%s", api_main_url, "codecs"),
    title = "List of codecs"
  },
  {
    id = string.format("%s/%s", api_main_url, "states"),
    title = "List of states"
  },
  {
    id = string.format("%s/%s", api_main_url, "languages"),
    title = "List of languages"
  },
  {
    id = string.format("%s/%s", api_main_url, "tags"),
    title = "List of tags"
  },
  {
    id = string.format("%s/%s", api_main_url, "stations"),
    title = "List of all radio stations"
  },
  {
    id = string.format("%s/%s/%d", api_main_url, "stations/topclick", API_TOP),
    title = "Stations by clicks"
  },
  {
    id = string.format("%s/%s/%d", api_main_url, "stations/topvote", API_TOP),
    title = "Stations by votes"
  },
  {
    id = string.format("%s/%s/%d", api_main_url, "stations/lastclick" , API_TOP),
    title = "Stations by recent click"
  },
  {
    id = string.format("%s/%s/%d", api_main_url, "stations/lastchange", API_TOP),
    title = "Stations by recently changed/added"
  }
}

------------------
-- Source utils --
------------------

function grl_source_browse(media_id)
  if media_id == nil then
    for _, endpoint in ipairs(api_main_endpoints) do
      local usr_opt = create_item(endpoint)
      grl.callback(usr_opt, -1)
    end
    grl.callback()
    return
  end

  local count = grl.get_options("count")
  local skip = grl.get_options("skip")

  if skip + count > MAX_ITEMS then
    grl.callback()
    return
  end

  local url = string.format("%s?offset=%d&limit=%d", media_id, skip, MAX_ITEMS)
  if (api_hidebroken) then
    url = url.."&hidebroken=true"
  end
  grl.fetch(url, grl_radio_browser_now_fetch_cb, media_id)
end

------------------------
-- Callback functions --
------------------------

-- return all the media found
function grl_radio_browser_now_fetch_cb(results, media_id)
  local count = grl.get_options("count")
  local skip = grl.get_options("skip")

  local json = {}
  json = grl.lua.json.string_to_table(results)

  for _, item in pairs(json) do
      if skip > 0 then
          skip = skip - 1
      elseif count > 0 then
          local media = create_media(item, media_id)
          if media then
              grl.callback(media, count)
              count = count - 1
          end
      end
  end
  grl.callback()
end

-------------
-- Helpers --
-------------

-- User first options --
function create_item(item)
  local option = {}
  option.type = "container"
  option.id = item.id
  option.title = item.title
  return option
end

-- List of countries --
function generate_list_countries(json)
  local media = {}
  media.type = "container"
  if json.value and string.len(json.value) > 0 then
    media.title = json.value
    media.id = api_search_url .. "/bycountry/" .. json.value
  end
  -- Plugin Specific info
  if json.stationcount and string.len(json.stationcount) > 0 then
    media.grl_radio_browser_station_counter = json.stationcount
  end
  return media
end

-- List of codecs --
function generate_list_codecs(json)
  local media = {}
  media.type = "container"
  if json.value and string.len(json.value) > 0 then
    media.title = json.value
    media.id = api_search_url .. "/bycodec/" .. json.value
  end
  -- Plugin Specific info
  if json.stationcount and string.len(json.stationcount) > 0 then
    media.grl_radio_browser_station_counter = json.stationcount
  end
  return media
end

-- List of states --
function generate_list_states(json)
  local media = {}
  media.type = "container"
  if json.value and string.len(json.value) > 0 then
    media.title = json.value
    media.id = api_search_url .. "/bystate/" .. json.value
  end
  -- Plugin Specific info
  if json.country and string.len(json.country) > 0 then
    media.grl_radio_browser_country = json.country
  end
  if json.stationcount and string.len(json.stationcount) > 0 then
    media.grl_radio_browser_station_counter = json.stationcount
  end
  return media
end

-- List of languages
function generate_list_languages(json)
  local media = {}
  media.type = "container"
  if json.value and string.len(json.value) > 0 then
    media.title = json.value
    media.id = api_search_url .. "/bylanguage/" .. json.value
  end
  -- Plugin Specific info
  if json.stationcount and string.len(json.stationcount) > 0 then
    media.grl_radio_browser_station_counter = json.stationcount
  end
  return media
end

-- List of tags
function generate_list_tags(json)
  local media = {}
  media.type = "container"
  if json.value and string.len(json.value) > 0 then
    media.title = json.value
    media.id = api_search_url .. "/bytag/" .. json.value
  end
  -- Plugin Specific info
  if json.stationcount and string.len(json.stationcount) > 0 then
    media.grl_radio_browser_station_counter = json.stationcount
  end
  return media
end

-- Media list
function generate_list_media(json)
  local media = {}
  media.type = "audio"
  media.mime_type = "audio/mpeg"
  if json.url and string.len(json.url) > 0 then
    media.url = json.url
  else
      return nil
  end
  if json.name and string.len(json.name) > 0 then
    media.title = json.name
  end
  if json.id and string.len(json.id) > 0 then
    media.id = json.stationuuid
  end
  if json.url and string.len(json.url) > 0 then
    media.url = json.url
  end
  if json.bitrate and string.len(json.bitrate) > 0 then
    media.bitrate = json.bitrate
  end
  if json.tags and string.len(json.tags) > 0 then
    media.genres = json.tags
  end
  if json.favicon and string.len(json.favicon) > 0 then
    media.thumbnail = json.favicon
  end
  -- Plugin Specific info
  if json.codec and string.len(json.codec) > 0 then
    media.grl_radio_browser_audio_codec = json.codec
  end
    if json.country and string.len(json.country) > 0 then
    media.grl_radio_browser_country = json.country
  end
  if json.state and string.len(json.state) > 0 then
    media.grl_radio_browser_state = json.state
  end
  if json.language and string.len(json.language) > 0 then
    media.grl_radio_browser_language = json.language
  end
  if json.votes and string.len(json.votes) > 0 then
    media.grl_radio_browser_upvotes = json.votes
  end
  if json.negativevotes and string.len(json.negativevotes) > 0 then
    media.grl_radio_browser_downvotes = json.negativevotes
  end
  if json.clickcount and string.len(json.clickcount) > 0 then
    media.grl_radio_browser_clickcounter = json.clickcount
  end
  return media
end

function create_media(json, id)
  if id == api_main_endpoints[1].id then
    return generate_list_countries(json)
  elseif id == api_main_endpoints[2].id then
    return generate_list_codecs(json)
  elseif id == api_main_endpoints[3].id then
    return generate_list_states(json)
  elseif id == api_main_endpoints[4].id then
    return generate_list_languages(json)
  elseif id == api_main_endpoints[5].id then
    return generate_list_tags(json)
  else
    return generate_list_media(json)
  end
  return nil
end
