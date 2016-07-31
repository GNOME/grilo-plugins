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

---------------------------
-- Source initialization --
---------------------------

source = {
  id = "grl-thegamesdb",
  name = "TheGamesDB.net",
  description = "TheGamesDB.net",
  supported_keys = { "description", "thumbnail", "external-url", "rating", "publication-date", "genre" },
  resolve_keys = {
    ["type"] = "none",
    required = { "title" },
  },
  tags = { 'games', 'net:internet', 'net:plaintext' }
}

netopts = {
  user_agent = "Grilo Source TheGamesDB/0.3.0",
}

------------------
-- Source utils --
------------------

THEGAMESDB_BASE_API_URL = "http://thegamesdb.net/api/"

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

function grl_source_resolve()
  local url, req
  local title

  req = grl.get_media_keys()
  if not req or not req.title
    or #req.title == 0 then
    grl.callback()
    return
  end

  title = get_title (req.title)
  url = THEGAMESDB_BASE_API_URL .. 'GetGamesList.php?name=' .. title
  local suffix = get_suffix (req.url)
  if req.mime_type then
    local platform_name = get_platform_name(req.mime_type, suffix)
    if platform_name then
     url = url .. "&platform=" .. grl.encode(platform_name)
    end
  end
  grl.debug('Fetching URL ' .. url .. ' for game ' .. req.title .. ' (' .. tostring(req.mime_type) ..')')
  grl.fetch(url, fetch_results_cb, netopts)
end

---------------
-- Utilities --
---------------

function get_id(results, title)
  if not results then return nil end
  local results_table = grl.lua.xml.string_to_table(results)
  if not results_table or
     not results_table.Data or
     not results_table.Data.Game then
    return nil
  end
  for index, game in pairs(results_table.Data.Game) do
    if game.GameTitle and
       string.lower(game.GameTitle.xml) == string.lower(title) then
      return game.id.xml
    end
  end
  -- If we didn't find an "exact" match, return the first item
  return results_table.Data.Game[1].id.xml
end

function fetch_results_cb(results)
  local req = grl.get_media_keys()
  local id = get_id(results, req.title)

  if id then
    local url = THEGAMESDB_BASE_API_URL ..  'GetGame.php?id=' .. id
    grl.fetch(url, fetch_game_cb)
    grl.debug('Fetching URL ' .. url ..' (ID: ' .. id .. ' title: ' .. req.title .. ')')
  else
    grl.callback()
  end
end

-- Returns the base_url followed by the Game table
function get_game(results)
  if not results then return nil end
  local results_table = grl.lua.xml.string_to_table(results)
  if not results_table then return nil end

  local base_url = nil
  if results_table.Data and
     results_table.Data.baseImgUrl then
    base_url = results_table.Data.baseImgUrl.xml
  end

  if results_table and
     results_table.Data and
     results_table.Data.Game then
    return base_url, results_table.Data.Game
  end

  return nil, nil
end

function fetch_game_cb(results)
  local base_url, game = get_game(results)

  if not game then
    grl.callback()
  else
    local media = {}

    if game.Images and
       game.Images.boxart then
      -- Handle having a single boxart image
      if game.Images.boxart.side and
         game.Images.boxart.side == 'front' then
        media.thumbnail = base_url .. game.Images.boxart.xml
      else
        for index, boxart in pairs(game.Images.boxart) do
          if boxart.side == 'front' then
            media.thumbnail = base_url .. boxart.xml
          end
        end
      end
    end

    if game.Overview then media.description = game.Overview.xml end
    if game.id then media.external_url = 'http://thegamesdb.net/game/' .. game.id.xml .. '/' end

    if game.ReleaseDate then
      local month, day, year = game.ReleaseDate.xml:match('(%d+)/(%d+)/(%d+)')
      media.publication_date = string.format('%04d-%02d-%02d', year, month, day)
    end

    if game.Genres then
      media.genre = {}
      for index, genre in pairs(game.Genres) do
        table.insert(media.genre, genre.xml)
      end
    end

    if game.Rating then
      -- from /10 to /5
      media.rating = tonumber(game.Rating.xml) / 2
    end

    if game.Developer then
      -- FIXME media.developer = game.Developer.xml
    end

    if game.Publisher then
      -- FIXME media.publisher = game.Publisher.xml
    end

    if game.Players then
      -- FIXME media.players = tonumber(game.Players.xml)
    end

    if game['Co-op'] then
      if game['Co-op'].xml == 'Yes' then
        -- FIXME media.coop = true
      end
    end

    grl.callback(media, 0)
  end
end

function get_platform_name(mime_type, suffix)
    local platform_names = {}

    -- mime-types are upstream in shared-mime-info
    -- names are from:
    -- http://thegamesdb.net/api/GetPlatformsList.php
    -- http://wiki.thegamesdb.net/index.php/GetPlatformsList
    platform_names['application/vnd.nintendo.snes.rom'] = 'Super Nintendo (SNES)'
    platform_names['application/x-amiga-disk-format'] = 'Amiga'
    platform_names['application/x-atari-2600-rom'] = 'Atari 2600'
    platform_names['application/x-atari-5200-rom'] = 'Atari 5200'
    platform_names['application/x-atari-7800-rom'] = 'Atari 7800'
    platform_names['application/x-dc-rom'] = 'Sega Dreamcast'
    -- Also represents 'Nintendo Game Boy Color'
    platform_names['application/x-gameboy-rom'] = 'Nintendo Game Boy'
    platform_names['application/x-gamecube-rom'] = 'Nintendo GameCube'
    platform_names['application/x-gba-rom'] = 'Nintendo Game Boy Advance'
    platform_names['application/x-genesis-rom'] = 'Sega Genesis'
    platform_names['application/x-genesis-32x-rom'] = 'Sega 32X'
    platform_names['application/x-n64-rom'] = 'Nintendo 64'
    platform_names['application/x-neo-geo-pocket-rom'] = 'Neo Geo Pocket'
    platform_names['application/x-nes-rom'] = 'Nintendo Entertainment System (NES)'
    platform_names['application/x-nintendo-ds-rom'] = 'Nintendo DS'
    platform_names['application/x-pc-engine-rom'] = 'TurboGrafx 16'
    platform_names['application/x-saturn-rom'] = 'Sega Saturn'
    -- Also represents 'Sega Game Gear'
    platform_names['application/x-sms-rom'] = 'Sega Master System'
    platform_names['application/x-wii-rom'] = 'Nintendo Wii'
    platform_names['application/x-wii-wad'] = 'Nintendo Wii'

    -- For disambiguation
    if suffix and
       suffix ~= 'bin' and
       platform_names[mime_type] then

      -- Game Boy / Game Boy Color
      if platform_names[mime_type] == 'Nintendo Game Boy' then
        if suffix == 'gbc' or suffix == 'cgb' then
          return 'Nintendo Game Boy Color'
        end
      end

      -- Sega Master System / Game Gear
      if platform_names[mime_type] == 'Sega Master System' and
         suffix == 'gg' then
        return 'Sega Game Gear'
      end
    end

    return platform_names[mime_type] or 'PC'
end

function get_suffix (url)
    if not url then return nil end
    -- Return a 3-letter suffix
    local ret = url:match('*-%.(...)$')
    if not ret then
      -- Try with a 2-letter suffix (Game Gear...)
      ret = url:match('*-%.(..)$')
    end
    return ret
end

function get_title (title)
    title = string.gsub(title, "_", " ")
    -- FIXME remove dump info flags
    -- http://www.tosecdev.org/tosec-naming-convention#_Toc302254975
    return grl.encode(title)
end
