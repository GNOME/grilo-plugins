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
  id = "grl-gamefaqs",
  name = "gamefaqs",
  description = "GameFAQs.com",
  supported_keys = { "description", "thumbnail", "external-url", "rating", "publication-date", "original-title" },
  resolve_keys = {
    ["type"] = "none",
    required = { "title" },
  },
  tags = { 'games', 'net:internet', 'net:plaintext' }
}

netopts = {
  user_agent = "Grilo Source GameFAQs/0.3.0",
}

------------------
-- Source utils --
------------------

GAMEFAQS_DEFAULT_QUERY = "http://www.gamefaqs.com/search/index.html?game=%s"

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

function get_platform_code(mime_type, suffix)
    local platform_codes = {}

    -- mime-types are upstream in shared-mime-info
    -- codes are from:
    -- http://www.gamefaqs.com/search?game=
    platform_codes['application/vnd.nintendo.snes.rom'] = 63
    platform_codes['application/x-amiga-disk-format'] = 39
    platform_codes['application/x-atari-2600-rom'] = 6
    platform_codes['application/x-atari-5200-rom'] = 20
    platform_codes['application/x-atari-7800-rom'] = 51
    platform_codes['application/x-dc-rom'] = 67
    -- Also represents Game Boy Color (57)
    platform_codes['application/x-gameboy-rom'] = 59
    platform_codes['application/x-gamecube-rom'] = 99
    platform_codes['application/x-gba-rom'] = 91
    -- Also represents 32X (74)
    platform_codes['application/x-genesis-rom'] = 54
    platform_codes['application/x-n64-rom'] = 84
    platform_codes['application/x-neo-geo-pocket-rom'] = 89
    platform_codes['application/x-nes-rom'] = 41
    platform_codes['application/x-nintendo-ds-rom'] = 108
    platform_codes['application/x-pc-engine-rom'] = 53
    platform_codes['application/x-saturn-rom'] = 76
    -- Also represents Game Gear (62)
    platform_codes['application/x-sms-rom'] = 49
    platform_codes['application/x-wii-rom'] = 114
    platform_codes['application/x-wii-wad'] = 114

    -- For disambiguation
    if suffix and
       suffix ~= 'bin' and
       platform_codes[mime_type] then

      -- Game Boy / Game Boy Color
      if platform_codes[mime_type] == 59 then
        if suffix == 'gbc' or suffix == 'cgb' then
          return 57
        end
      end

      -- Genesis / 32X
      if platform_codes[mime_type] == 74 then
        if suffix == '32x' or
           suffix == 'mdx' then
          return 74
        end
      end

      -- Sega Master System / Game Gear
      if platform_codes[mime_type] == 62 and
         suffix == 'gg' then
        return 62
      end
    end

    return platform_codes[mime_type]
end

function get_suffix (url)
    if not url then return nil end
    -- Return a 3-letter suffix
    return url:match('.-%.(...)$')
end

function get_title (title)
    title = string.gsub(title, "_", " ")
    -- FIXME remove dump info flags
    -- http://www.tosecdev.org/tosec-naming-convention#_Toc302254975
    return grl.encode(title)
end

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
  url = string.format(GAMEFAQS_DEFAULT_QUERY, title)
  local suffix = get_suffix (req.url)
  if req.mime_type then
    local platform_code = get_platform_code(req.mime_type, suffix)
    if platform_code then
      url = url .. "&platform=" .. platform_code
    end
  end
  grl.fetch(url, fetch_results_cb, netopts)
end

---------------
-- Utilities --
---------------

function fetch_results_cb(results)
  if results then
    local item = results:match('<td class="rtitle">(.-)</td>')

    -- Process the first item in the results
    if item then
      local link = item:match('href="(.-)"')
      local urls = {}

      -- If only thumbnails were requested, get just that
      local keys = grl.get_requested_keys();

      if keys.thumbnail then
        local url = "http://www.gamefaqs.com" .. link .. '/images'
        table.insert(urls, url)
      end

      -- Needs to be synced with the supported_keys
      if keys.description or
         keys.rating or
         keys.publication_date or
         keys.original_title then
        local url = "http://www.gamefaqs.com" .. link
        table.insert(urls, url)
      end

      grl.fetch(urls, fetch_pages_cb)
    end
  end
end

function fetch_pages_cb(results)
  local media = {}
  media.thumbnail = {}

  for index, result in pairs(results) do
    -- Different code paths depending on whether that's the
    -- images page (first) or the main game page
    if result:match('/images" /><input class="hidden"') then
      local section = result:match('<div class="img boxshot">(.-)</div><div class="head">')
      if section then
        for i in section:gmatch('<a href=".- src="(.-)"') do
          local url = string.gsub(i, "_thumb", "_front")
          table.insert(media.thumbnail, url)
        end
      end
    else
      media.description = result:match('<div class="desc">(.-)</div>')
      media.external_url = result:match('name="path" value="(.-)"')
      local rating, rating_dec = result:match('/stats#rate">(%d+)%.(%d+)')
      media.rating = rating + rating_dec / 100.0
      media.publication_date = result:match('Release:</b> <a .-">(%d+).-</a>')
      -- FIXME not really the original title, but an AKA
      media.original_title = result:match('Also Known As:</b> <i>(.-)</i>')
    end
  end

  grl.debug (grl.lua.inspect(media))

  grl.callback(media, 0)
end
