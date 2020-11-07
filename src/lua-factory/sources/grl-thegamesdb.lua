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
  supported_keys = { "description", "thumbnail", "external-url", "rating", "publication-date", "genre", "developer", "publisher", "coop", "players"},
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

THEGAMESDB_BASE_API_URL = "http://legacy.thegamesdb.net/api/"

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
  grl.fetch(url, netopts, fetch_results_cb)
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
  -- If there's only a single match, just return it
  if results_table.Data.Game.id then
    return results_table.Data.Game.id.xml
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
      if game.Genres.genre.xml then
        table.insert(media.genre, game.Genres.genre.xml)
      else
        for _, genre in pairs(game.Genres.genre) do
          table.insert(media.genre, genre.xml)
        end
      end
    end

    if game.Rating then
      -- from /10 to /5
      media.rating = tonumber(game.Rating.xml) / 2
    end

    if game.Developer then
      media.developer = game.Developer.xml
    end

    if game.Publisher then
      media.publisher = game.Publisher.xml
    end

    if game.Players then
      media.players = tostring(game.Players.xml)
    end

    if game['Co-op'] then
      if game['Co-op'].xml == 'Yes' then
        media.coop = true
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
    platform_names['application/x-atari-lynx-rom'] = 'Atari Lynx'
    platform_names['application/x-dreamcast-rom'] = 'Sega Dreamcast'
    platform_names['application/x-fds-disk'] = 'Famicom Disk System'
    platform_names['application/x-gameboy-rom'] = 'Nintendo Game Boy'
    platform_names['application/x-gameboy-color-rom'] = 'Nintendo Game Boy Color'
    platform_names['application/x-gamecube-rom'] = 'Nintendo GameCube'
    platform_names['application/x-gba-rom'] = 'Nintendo Game Boy Advance'
    platform_names['application/x-genesis-rom'] = 'Sega Genesis'
    platform_names['application/x-genesis-32x-rom'] = 'Sega 32X'
    platform_names['application/x-n64-rom'] = 'Nintendo 64'
    platform_names['application/x-neo-geo-pocket-rom'] = 'Neo Geo Pocket'
    platform_names['application/x-neo-geo-pocket-color-rom'] = 'Neo Geo Pocket Color'
    platform_names['application/x-nes-rom'] = 'Nintendo Entertainment System (NES)'
    platform_names['application/x-nintendo-ds-rom'] = 'Nintendo DS'
    platform_names['application/x-pc-engine-rom'] = 'TurboGrafx 16'
    platform_names['application/x-saturn-rom'] = 'Sega Saturn'
    platform_names['application/x-sega-cd-rom'] = 'Sega CD'
    platform_names['application/x-sg1000-rom'] = 'SEGA SG-1000'
    platform_names['application/x-sega-pico-rom'] = 'Sega Pico'
    -- Also represents 'Sega Game Gear' through magic
    platform_names['application/x-sms-rom'] = 'Sega Master System'
    platform_names['application/x-gamegear-rom'] = 'Sega Game Gear'
    platform_names['application/x-virtual-boy-rom'] = 'Nintendo Virtual Boy'
    platform_names['application/x-wii-rom'] = 'Nintendo Wii'
    platform_names['application/x-wii-wad'] = 'Nintendo Wii'
    platform_names['application/x-wonderswan-rom'] = 'WonderSwan'
    platform_names['application/x-wonderswan-color-rom'] = 'WonderSwan Color'

    -- CD image file types that can't be identified via magic, but
    -- should still be differentiated. Usually they are represented
    -- via application/octet-stream + application/x-cue combination
    platform_names['application/x-pc-engine-cd-rom'] = 'TurboGrafx CD'
    platform_names['application/x-playstation-rom'] = 'Sony Playstation'

    -- For disambiguation
    if suffix and
       suffix ~= 'bin' and
       platform_names[mime_type] then

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
