--[[
* Copyright (C) 2018 Grilo Project
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
  id = "grl-steam-store",
  name = "Steam Store",
  description = "a source for game information from the Steam store api",
  supported_keys = { "title", "description", "thumbnail", "external-url", "rating", "publication-date", "genre", "developer", "publisher"},
  resolve_keys = {
    ["type"] = "none",
    required = { "id" },
  },
  tags = { 'games', 'net:internet' },
}

netopts = {
  user_agent = "Grilo Source SteamStore/0.3.0",
}

------------------
-- Source utils --
------------------

BASE_API_URL = "https://store.steampowered.com/api/appdetails?appids="

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

function grl_source_resolve()
  local url, req

  req = grl.get_media_keys()
  if not req or not req.id
    or #req.id == 0 then
    grl.callback()
    return
  end

  url = BASE_API_URL .. req.id
  grl.debug('Fetching URL ' .. url .. ' for appid ' .. req.id)
  grl.fetch(url, netopts, fetch_game_cb, req.id)
end

---------------
-- Utilities --
---------------

function format_date(date)
  local month_map = {
    Jan = 1,
    Feb = 2,
    Mar = 3,
    Apr = 4,
    May = 5,
    Jun = 6,
    Jul = 7,
    Aug = 8,
    Sep = 9,
    Oct = 10,
    Nov = 11,
    Dec = 12,
  }
  month, day, year = string.match(date, '^(%w+) (%d+), (%d+)')
  day = tonumber(day)
  year = tonumber(year)

  if not month or not month_map[month] or not day or not year then
    grl.warning('could not parse date: ' .. date)
    return nil
  end

  return string.format("%d-%02d-%02d", year, month_map[month], day)
end

function fetch_game_cb(results, appid)
  local results_table, data, media

  results_table = grl.lua.json.string_to_table(results)

  if not results_table[appid] or not results_table[appid].data then
    grl.warning('Got a result without data')
    grl.callback()
    return
  end

  data = results_table[appid].data

  media = {}

  -- simple properties
  local propmap = {
    title = 'name',
    description = 'about_the_game',
    thumbnail = 'header_image',
    external_url = 'website',
    developer = 'developers',
    publisher = 'publishers',
  }

  for media_key, data_key in pairs(propmap) do
    if data[data_key] then
      media[media_key] = data[data_key]
    end
  end

  -- genre
  if data.genres and #data.genres > 0 then
    media.genre = {}
    for i, genre_info in ipairs(data.genres) do
      if genre_info.description then
        table.insert(media.genre, genre_info.description)
      end
    end
  end

  -- rating
  if type(data.metacritic) == 'table' and data.metacritic.score then
    media.rating = data.metacritic.score
  end

  -- publication-date
  if type(data.release_date) == 'table' and data.release_date.date then
    local date = format_date(data.release_date.date)
    if date then
      media.publication_date = date
    end
  end

  grl.callback(media, 0)
end
