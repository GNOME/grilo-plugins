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

BASE_API_URL            = 'https://api.opensubtitles.com/api/v1/'

headers = {
  ['Accept'] = 'application/json',
  ['Content-Type'] = 'application/json',
  ['User-Agent'] = "Grilo Source OpenSubtitles/0.3.16",
}

---------------------------
-- Source initialization --
---------------------------

source = {
  id = 'grl-opensubtitles',
  name = 'OpenSubtitles',
  description = 'A source providing a list of subtitles for a video',
  supported_keys = { 'title', 'episode-title', 'episode', 'season', 'show', 'subtitles-lang', 'subtitles-url' },
  supported_media = 'video',
  config_keys = {
    required = { 'api-key' },
  },
  resolve_keys = {
    ["type"] = "video",
    required = { 'title' } ,
    optional = {
      'gibest-hash',
    }
  },
  tags = { 'subtitles', 'net:internet', 'net:plaintext' },
}

-- Table containing subtitles data --
local ldata = nil

-- the length of ldata --
local len_data = nil

-- Media containing all subtitles --
local media_subtitles = nil

------------------
-- Source utils --
------------------

function grl_source_init (configs)
  headers['Api-Key'] = configs.api_key  
  return true
end

function grl_source_resolve(media_id)
  local keys = grl.get_media_keys()

  if not keys then
    grl.warning("resolve was called without metadata-key")
    grl.callback()
    return
  end

  ldata = {}
  len_data = 0

  media_subtitles = {}

  -- start the search at the first page --
  search_subtitles(1)
end

------------------------
-- Callback functions --
------------------------

function on_search_request_done_cb(results, current_page)
  local json = grl.lua.json.string_to_table (results)

  if not json then
    grl.callback()
  end

  total_pages = json.total_pages

  if total_pages == 0 then
    grl.callback()
  end

  if current_page <= total_pages then
    parse_results (json.data)

    current_page = current_page + 1
    if current_page > total_pages then
      get_url_subtitles(ldata)
    else
      search_subtitles(current_page)
    end
  end
end

function on_request_download_subtitle_cb(results, lang)
  local json = grl.lua.json.string_to_table (results)

  if not json then
    grl.callback()
  end

  media_subtitles[#media_subtitles + 1] = { subtitles_lang = lang, subtitles_url = json.link }

  len_data = len_data - 1

  if len_data == 0 then
    grl.callback(media_subtitles, 0)
  end
end

-------------
-- Helpers --
-------------

function search_subtitles(num_page)
  local keys = grl.get_media_keys()
  local params = {}

  if keys.title then
    params.query = keys.title
  end

  if keys.gibest_hash then
    params.moviehash = keys.gibest_hash
  end

  params.page = num_page

  grl.request(BASE_API_URL .. "subtitles", "GET", headers, params, on_search_request_done_cb, num_page)
end

function get_url_subtitles(subtitles)
  for lang, subtitle in pairs(subtitles) do

    local params = {
      -- special key to use our own RestProxyCall so that we can send json data  --
      ['grl-json'] = "{ \"file_id\": " ..tostring(subtitle.files[1]).. " }"
    }

    grl.request(BASE_API_URL .. "download", "POST", headers, params, on_request_download_subtitle_cb, lang)
  end
end

function maybe_add_sub(subtitle_data, new_score, total_count)
  if (new_score > subtitle_data.score) or
     (new_score == subtitle_data.score and total_count > subtitle_data.nb_downloads) then
    subtitle_data.score = new_score
    subtitle_data.nb_downloads = total_count
    return true
  end

  return false
end

function parse_results(data)
  local keys = grl.get_media_keys()

  for _, result in pairs(data) do
    local new_score = 0
    local total_count = 0
    local get_files_id = false
    local lang = result.attributes.language

    if not lang then
      goto continue
    end
  
    -- Verify that the season/episode matches the media before using that data --
    if keys.show then
      season = result.attributes.feature_details.season_number
      episode = result.attributes.feature_details.epsiode_number

      if season ~= keys.season or episode ~= keys.episode then
        goto continue
      end
    end

    -- From https://forum.opensubtitles.org/viewtopic.php?f=8&t=17146&p=48063&hilit=total+downloads#p48063 --
    -- download_count is from the old API and new_download_count from the new API --
    total_count = result.attributes.download_count + result.attributes.new_download_count

    -- Scoring system from popcorn-opensubtitles --
    if result.attributes.moviehash_match then
      new_score = new_score + 100
    end
    if result.attributes.from_trusted then
      new_score = new_score + 100
    end

    if not ldata[lang] then
      ldata[lang] = {
        nb_downloads = total_count,
        score = new_score,
        files = {}
      }

      get_files_id = true
      len_data = len_data + 1
    else
      -- replace the current lang subtitle if it has a better score or more download
      get_files_id = maybe_add_sub (ldata[lang], new_score, total_count)
    end

    if get_files_id then
      files_id = {}

      for _, file in pairs(result.attributes.files) do
        table.insert(files_id, file.file_id)
      end

      ldata[lang].files = files_id
    end

    ::continue::
  end
end
