--[[
 * Copyright (C) 2014 Bastien Nocera
 *
 * Contact: Bastien Nocera <hadess@hadess.net>
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

EURONEWS_URL               = 'http://euronews.hexaglobe.com/json/'

---------------------------
-- Source initialization --
---------------------------

source = {
  id = "grl-euronews-lua",
  name = "Euronews",
  description = "A source for watching Euronews online",
  supported_keys = { "id", "title", "url" },
  supported_media = 'video',
  tags = { 'news', 'tv' }
}

------------------
-- Source utils --
------------------

function grl_source_browse(media_id)
  if grl.get_options("skip") > 0 then
    grl.callback()
  else
    grl.fetch(EURONEWS_URL, "euronews_fetch_cb")
  end
end

------------------------
-- Callback functions --
------------------------

-- return all the media found
function euronews_fetch_cb(results)
  local json = {}

  json = grl.lua.json.string_to_table(results)
  if not json or json.stat == "fail" or not json.primary then
    grl.callback()
    return
  end

  for index, item in pairs(json.primary) do
    local media = create_media(index, item)
    if media ~= nil then
      grl.callback(media, -1)
    end
  end

  grl.callback()
end

-------------
-- Helpers --
-------------

function get_lang(id)
  local langs = {}
  langs.ru = "Russian"
  langs.hu = "Hungarian"
  langs.de = "German"
  langs.fa = "Persian"
  langs.ua = "Ukrainian"
  langs.ar = "Arabic"
  langs.es = "Spanish; Castilian"
  langs.gr = "Greek, Modern (1453-)"
  langs.tr = "Turkish"
  langs.pe = "Persian" -- Duplicate?
  langs.en = "English"
  langs.it = "Italian"
  langs.fr = "French"
  langs.pt = "Portuguese"

  if not langs[id] then
    grl.warning('Could not find language ' .. id)
    return id
  end

  return grl.dgettext('iso_639', langs[id])
end

function create_media(lang, item)
  local media = {}

  if item.rtmp_flash["750"].name == 'UNAVAILABLE' then
    return nil
  end

  media.type = "video"
  media.id = lang
  media.title = "Euronews " .. get_lang(lang)
  media.url = item.rtmp_flash["750"].server ..
              item.rtmp_flash["750"].name ..
              " playpath=" .. item.rtmp_flash["750"].name ..
              " swfVfy=1 " ..
              "swfUrl=http://euronews.com/media/player_live_1_14.swf "..
              "live=1"

  return media
end
