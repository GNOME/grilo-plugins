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
  supported_keys = { "title", "id", "grl_radio_browser_station_counter",
      "grl_radio_browser_country", "url", "bitrate", "genres", "thumbnail",
      "grl_radio_browser_state", "grl_radio_browser_upvotes",
      "grl_radio_browser_downvotes", "grl_radio_browser_clickcounter" },
  supported_media = 'audio',
  tags = { 'radio', 'net:internet', 'net:plaintext' }
}

-------------------
-- API RESOURCES --
-------------------
-- api max itens
MAX_ITENS = 100

-- api top X
API_TOP = 10

-- do not count broken stations
api_hidebroken = true

-- api urls
api_main_url = "http://www.radio-browser.info/webservice/json"
api_search_url = "http://www.radio-browser.info/webservice/json/stations"

api_main_endpoints = {
    countries = {
      id = string.format("%s/%s", api_main_url, "countries"),
      title = "List of countries",
      search_key = "bycountry"
    },
    codecs = {
      id = string.format("%s/%s", api_main_url, "codecs"),
      title = "List of codecs",
      search_key = "bycodec"
    },
    states = {
      id = string.format("%s/%s", api_main_url, "states"),
      title = "List of states",
      search_key = "bystate"
    },
    languages = {
      id = string.format("%s/%s", api_main_url, "languages"),
      title = "List of languages",
      search_key = "bylanguage"
    },
    tags = {
      id = string.format("%s/%s", api_main_url, "tags"),
      title = "List of tags",
      search_key = "bytag"
    },
    stations = {
      id = string.format("%s/%s", api_main_url, "stations"),
      title = "List of all radio stations"
    },
    clicks = {
      id = string.format("%s/%s/%d", api_main_url, "stations/topclick", API_TOP),
      title = "Stations by clicks"
    },
    votes = {
      id = string.format("%s/%s/%d", api_main_url, "stations/topvote", API_TOP),
      title = "Stations by votes"
    },
    lastclick = {
      id = string.format("%s/%s/%d", api_main_url, "stations/lastclick" , API_TOP),
      title = "Stations by recent click"
    },
    lastchange = {
      id = string.format("%s/%s/%d", api_main_url, "stations/lastchange", API_TOP),
      title = "Stations by recently changed/added"
    }
}

------------------
-- Source utils --
------------------

function grl_source_browse(media_id)
  local skip = grl.get_options("skip")

  -- This is a workaround to make this plugin to work with rythmbox
  if skip > 0 then
      grl.callback()
      return
  end

  if not media_id then
    for _, endpoint in pairs(api_main_endpoints) do
      local item = create_item(endpoint)
      grl.callback(item, -1)
    end
    grl.callback()
    return
  end

  local count = grl.get_options("count")

  if count <= 0 then
    count = MAX_ITENS
  end

  local url = media_id

  if (api_hidebroken) then
    url = url.."&hidebroken=true"
  end

  grl.fetch(url, grl_radio_browser_now_fetch_cb, media_id)
end

------------------------
-- Callback functions --
------------------------

-- return all the media found
function grl_radio_browser_now_fetch_cb(result, media_id)
  local count = grl.get_options("count")
  local skip = grl.get_options("skip")

  local json = {}
  jsons = grl.lua.json.string_to_table(result)

  for _, json in pairs(jsons) do
    local media = create_media(json, media_id)
    if media then
      grl.callback(media, count)
      count = count - 1
    else
      grl.warning("Can't parse media data")
    end

    if count == 0 then
      return
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

-- Container list
function generate_container(json, search_key)
  local media = {}
  media.type = "container"
  if json.value and string.len(json.value) > 0 then
    media.title = json.value
    media.id = string.format("%s/%s/%s", api_search_url, search_key, json.value)
  end
  -- Plugin Specific Keys
  if json.stationcount and string.len(json.stationcount) > 0 then
    media.grl_radio_browser_station_counter = json.stationcount
  end
  if json.country and string.len(json.country) > 0 then
    media.grl_radio_browser_country = json.country
  end
  return media
end

-- Media list
function generate_list_media(json)
  local media = {}
  media.type = "audio"
  media.mime_type = "audio/mpeg"
  -- Grilo Metadata keys
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
  if json.bitrate and string.len(json.bitrate) > 0 then
    media.bitrate = json.bitrate
  end
  if json.tags and string.len(json.tags) > 0 then
    media.genres = json.tags
  end
  if json.favicon and string.len(json.favicon) > 0 then
    media.thumbnail = json.favicon
  end
  -- Plugin Specific keys
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
  if id == api_main_endpoints.countries.id then
    return generate_container(json, api_main_endpoints.countries.search_key)
  elseif id == api_main_endpoints.codecs.id then
    return generate_container(json, api_main_endpoints.codecs.search_key)
  elseif id == api_main_endpoints.states.id then
    return generate_container(json, api_main_endpoints.states.search_key)
  elseif id == api_main_endpoints.languages.id then
    return generate_container(json, api_main_endpoints.languages.search_key)
  elseif id == api_main_endpoints.tags.id then
    return generate_container(json, api_main_endpoints.tags.search_key)
  else
    return generate_list_media(json)
  end
  grl.warning("Unknown type of media")
  return nil
end
