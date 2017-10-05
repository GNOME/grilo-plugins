--[[
 * Copyright (C) 2015 Grilo Project
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
  id = "grl-appletrailers-lua",
  name = "Apple Movie Trailers",
  description = "Apple Trailers",
  supported_media = 'video',
  supported_keys = { 'author', 'publication-date', 'description', 'duration', 'genre', 'id', 'thumbnail', 'title', 'url', 'certificate', 'studio', 'license', 'performer', 'size' },
  config_keys = {
    optional = { 'definition' },
  },
  icon = 'resource:///org/gnome/grilo/plugins/appletrailers/trailers.svg',
  tags = { 'country:us', 'cinema', 'net:internet', 'net:plaintext' },
}

-- Global table to store config data
ldata = {}

-- Global table to store parse results
cached_xml = nil

function grl_source_init(configs)
  ldata.hd = (configs.definition and configs.definition == 'hd')
  return true
end

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

APPLE_TRAILERS_CURRENT_SD = "http://trailers.apple.com/trailers/home/xml/current_480p.xml"
APPLE_TRAILERS_CURRENT_HD = "http://trailers.apple.com/trailers/home/xml/current_720p.xml"

function grl_source_browse()
  local skip = grl.get_options("skip")
  local count = grl.get_options("count")

  -- Make sure to reset the cache when browsing again
  if skip == 0 then
    cached_xml = nil
  end

  if cached_xml then
    parse_results(cached_xml)
  else
    local url = APPLE_TRAILERS_CURRENT_SD
    if ldata.hd then
      url = APPLE_TRAILERS_CURRENT_HD
    end

    grl.debug('Fetching URL: ' .. url .. ' (count: ' .. count .. ' skip: ' .. skip .. ')')
    grl.fetch(url, fetch_results_cb)
  end
end

---------------
-- Utilities --
---------------

function fetch_results_cb(results)
  if not results then
    grl.warning('Failed to fetch XML file')
    grl.callback()
    return
  end

  cached_xml = grl.lua.xml.string_to_table(results)
  parse_results(cached_xml)
end

function parse_results(results)
  local count = grl.get_options("count")
  local skip = grl.get_options("skip")

  for i, item in pairs(results.records.movieinfo) do
    local media = {}

    media.type = 'video'
    media.id = item.id
    if item.cast then
      media.performer = {}
      for j, cast in pairs(item.cast.name) do
        table.insert(media.performer, cast.xml)
      end
    end
    if item.genre then
      media.genre = {}
      for j, genre in pairs(item.genre.name) do
        table.insert(media.genre, genre.xml)
      end
    end
    media.license = item.info.copyright.xml
    media.description = item.info.description.xml
    media.director = item.info.director.xml
    media.publication_date = item.info.releasedate.xml
    media.certificate = item.info.rating.xml
    media.studio = item.info.studio.xml
    media.title = item.info.title.xml
    media.thumbnail = item.poster.xlarge.xml
    media.url = item.preview.large.xml
    media.size = tonumber(item.preview.large.filesize)
    local mins, secs = item.info.runtime.xml:match('(%d):(%d)')
    media.duration = tonumber(mins) * 60 + tonumber(secs)

    if skip > 0 then
      skip = skip - 1
    else
      count = count - 1
      grl.callback(media, count)
      if count == 0 then
        return
      end
    end
  end

  if count ~= 0 then
    grl.callback()
  end
end
