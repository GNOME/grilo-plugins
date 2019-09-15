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


function get_netopts_for_lang(lang)
  local netopts
  -- nil means use the system lang
  netopts = {
    user_agent = "Grilo Source SteamStore/0.3.0",
  }
  if lang then
    netopts.accept_language = lang
  end

  return netopts
end

------------------
-- Source utils --
------------------

function get_steam_store_url(appid)
  return "https://store.steampowered.com/api/appdetails?appids=" .. appid
end

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

  url = get_steam_store_url(req.id)
  grl.debug('Fetching URL ' .. url .. ' for appid ' .. req.id .. ' for system locale')
  grl.fetch(url, get_netopts_for_lang(nil), fetch_game_cb_system_lang, req.id)
end

---------------
-- Utilities --
---------------

function format_date(date)
  -- this will only work if the date comes from the en-US locale
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
    return nil
  end

  return string.format("%d-%d-%d", year, month_map[month], day)
end

function get_data_from_results(results, appid)
  local results_table

  results_table = grl.lua.json.string_to_table(results)

  if not results_table then
    grl.warning('Could not parse json from results: ' .. results)
    return nil
  end

  if not results_table[appid] then
    grl.warning('Results did not contain an appid toplevel member: ' .. results)
    return nil
  end

  if not results_table[appid].data then
    grl.warning('Results for the appid did not contain "data" member: ' .. results)
    return nil
  end

  return results_table[appid].data
end

function fetch_game_cb_system_lang(results, appid)
  local data, media

  data = get_data_from_results(results, appid)

  if not data then
    grl.callback()
    return
  end

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

  if media.publication_date then
    grl.callback(media, 0)
  else
    local url, user_data, netopts
    url = get_steam_store_url(appid)
    user_data = {
      appid = appid,
      media = media,
    }
    -- libsoup will automatically use the system language for requests to the
    -- api, but the publication date may be localized to a date format we don't
    -- know how to parse. When this is the case, do a second request explicitly
    -- for the en-US locale to get the publication date in a format we can
    -- parse.
    netopts = get_netopts_for_lang('en-US')
    grl.debug('Fetching URL ' .. url .. ' for appid ' .. appid .. ' for US locale to get dates')
    grl.fetch(url, netopts, fetch_game_cb_us, user_data)
  end
end

function fetch_game_cb_us(results, user_data)
  data = get_data_from_results(results, user_data.appid)

  if not data then
    grl.callback(user_data.media, 0)
    return
  end

  -- publication-date
  if type(data.release_date) == 'table' and data.release_date.date then
    local date = format_date(data.release_date.date)
    if date then
      user_data.media.publication_date = date
    end
  end

  grl.callback(user_data.media, 0)
end
