--[[
 * Copyright (C) 2016 Grilo Project
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

-- API Documentation available at:
-- https://affiliate.itunes.apple.com/resources/documentation/itunes-store-web-service-search-api/

-- Browse URL, will return a URL to be parsed by totem-pl-parser
ITUNES_PODCAST_URL = 'https://itunes.apple.com/%s/rss/toppodcasts/limit=%d/json'
-- Search URL, will return the URL to an RSS feed
ITUNES_SEARCH_PODCAST_URL = 'https://itunes.apple.com/search?term=%s&entity=podcast&country=%s&limit=%d'
-- The web service will not return more than 200 items
MAX_ITEMS = 200

---------------------------
-- Source initialization --
---------------------------

source = {
  id = "grl-itunes-podcast",
  name = "iTunes Podcast",
  description = "iTunes Podcast",
  supported_keys = { 'artist', 'thumbnail', 'id', 'title', 'region',
                     'external-url', 'genre', 'modification-date',
                     'mime-type', 'description' },
  supported_media = 'audio',
  config_keys = {
    optional = { 'country' },
  },
  -- From http://www.powerpresspodcast.com/wp-content/uploads/2016/02/itunes-podcast-app-logo.png
  icon = 'resource:///org/gnome/grilo/plugins/itunes-podcast/itunes-podcast.png',
  tags = { 'podcast', 'net:internet' },
}

-- Global table to store config data
ldata = {}

------------------
-- Source utils --
------------------

-- Valid countries are listed at
-- https://developer.apple.com/library/ios/documentation/LanguagesUtilities/Conceptual/iTunesConnect_Guide/Appendices/AppStoreTerritories.html
--
-- io.input("AppStoreTerritories.html")
-- t = io.read("*all")
-- table = t:match('TableHeading_TableRow_TableCell.-<td  scope="row">(.-)</table>')
-- io.write ('countries = { ')
-- for i in table:gmatch('<p>([A-Z][A-Z])</p>') do
--     io.write ('"' .. i .. '"')
--     io.write (', ')
-- end
-- io.write ('}\n')

countries = { "AE", "AG", "AI", "AL", "AM", "AO", "AR", "AT", "AU", "AZ", "BB", "BE", "BF", "BG", "BH", "BJ", "BM", "BN", "BO", "BR", "BS", "BT", "BW", "BY", "BZ", "CA", "CG", "CH", "CL", "CN", "CO", "CR", "CV", "CY", "CZ", "DE", "DK", "DM", "DO", "DZ", "EC", "EE", "EG", "ES", "FI", "FJ", "FM", "FR", "GB", "GD", "GH", "GM", "GR", "GT", "GW", "GY", "HK", "HN", "HR", "HU", "ID", "IE", "IL", "IN", "IS", "IT", "JM", "JO", "JP", "KE", "KG", "KH", "KN", "KR", "KW", "KY", "KZ", "LA", "LB", "LC", "LK", "LR", "LT", "LU", "LV", "MD", "MG", "MK", "ML", "MN", "MO", "MR", "MS", "MT", "MU", "MW", "MX", "MY", "MZ", "NA", "NE", "NG", "NI", "NL", "NO", "NP", "NZ", "OM", "PA", "PE", "PG", "PH", "PK", "PL", "PT", "PW", "PY", "QA", "RO", "RU", "SA", "SB", "SC", "SE", "SG", "SI", "SK", "SL", "SN", "SR", "ST", "SV", "SZ", "TC", "TD", "TH", "TJ", "TM", "TN", "TR", "TT", "TW", "TZ", "UA", "UG", "US", "UY", "UZ", "VC", "VE", "VG", "VN", "YE", "ZA", "ZW", }

function get_count()
   -- itunes api does not handle limit=-1
   local count = grl.get_options("count")

   if count == -1 then
      return MAX_ITEMS
   end

   return count
end

function grl_source_init(configs)
  ldata.country = 'US'

  if not configs.country then
    return true
  end

  for i in countries do
    if configs.country ~= i then
      ldata.country = i
      return true
    end
  end

  grl.warning ('Invalid Podcast territory "' .. configs.country .. '"')

  return false
end

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

function grl_source_browse(media_id)
  local count = get_count()
  local skip = grl.get_options("skip")

  if skip + count > MAX_ITEMS then
    grl.callback()
    return
  end

  local url = string.format(ITUNES_PODCAST_URL, ldata.country, skip + count)
  grl.debug ("Fetching URL: " .. url .. " (count: " .. count .. " skip: " .. skip .. ")")
  grl.fetch(url, fetch_browse_results_cb)
end

function grl_source_search(text)
  local count = get_count()
  local skip = grl.get_options("skip")

  if skip + count > MAX_ITEMS then
    grl.callback()
    return
  end

  text = string.gsub(text, " ", "+")

  local url = string.format(ITUNES_SEARCH_PODCAST_URL, text, ldata.country, skip + count)
  grl.debug ("Fetching URL: " .. url .. " (count: " .. count .. " skip: " .. skip .. " text: " .. text .. ")")
  grl.fetch(url, fetch_search_results_cb)
end

---------------
-- Utilities --
---------------

function fetch_browse_results_cb(results)
  local count = get_count()
  local skip = grl.get_options("skip")

  if not results then
    grl.callback()
    return
  end

  local json = grl.lua.json.string_to_table(results)
  if not json or not json.feed or not json.feed.entry then
    grl.callback()
    return
  end

  for i, result in ipairs(json.feed.entry) do
    if skip > 0 then
      skip = skip - 1
    elseif count > 0 then
      local media = {}

      media.genre = result.category.attributes.label
      media.id = result.id.attributes["im:id"]
      media.artist = result["im:artist"].label

      media.thumbnail = {}
      local last_thumb_size = 0
      for j, image in ipairs(result["im:image"]) do
        if tonumber(image.attributes.height) > last_thumb_size then
          table.insert(media.thumbnail, 1, image.label)
        else
          table.insert(media.thumbnail, image.label)
        end
      end
      if result["im:releaseDate"] then
        media.modification_date = result["im:releaseDate"].label
      else
        print (grl.lua.inspect (result))
      end
      if result.summary then media.description = result.summary.label end
      media.title = result.title.label
      media.external_url = result.link.attributes.href

      count = count - 1
      grl.debug ('Sending out media ' .. media.id .. ' (external url: ' .. media.external_url .. ' left: ' .. count .. ')')
      grl.callback(media, count)
    end
  end

  if count > 0 then
    grl.callback()
  end
end

function fetch_search_results_cb(results)
  local count = get_count()
  local skip = grl.get_options("skip")

  if not results then
    grl.callback()
    return
  end

  local json = grl.lua.json.string_to_table(results)
  if not json or json.resultCount < 1 then
    grl.callback()
    return
  end

  for i, result in ipairs(json.results) do
    if skip > 0 then
      skip = skip - 1
    elseif count > 0 then
      local media = {}

      media.artist = result.artistName
      media.thumbnail = {}
      if result.artworkUrl600 then table.insert(media.thumbnail, result.artworkUrl600) end
      if result.artworkUrl100 then table.insert(media.thumbnail, result.artworkUrl100) end
      if result.artworkUrl60 then table.insert(media.thumbnail, result.artworkUrl60) end
      if result.artworkUrl30 then table.insert(media.thumbnail, result.artworkUrl30) end
      media.id = result.collectionId
      media.title = result.collectionName
      media.region = result.country
      media.url = result.feedUrl
      media.genre = result.genres
      media.modification_date = result.releaseDate
      media.mime_type = 'application/rss+xml'

      count = count - 1
      grl.debug ('Sending out media ' .. media.id .. ' (feed url: ' .. media.url .. ' left: ' .. count .. ')')
      grl.callback(media, count)
    end
  end

  if count > 0 then
    grl.callback()
  end
end
