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

LANG_EN                 = "en"
EURONEWS_URL            = 'https://%s.euronews.com/live'

local langs = {
  arabic = "Arabic",
  de = "German",
  en = "English",
  es = "Spanish; Castilian",
  fr = "French",
  gr = "Greek, Modern (1453-)",
  hu = "Hungarian",
  it = "Italian",
  pt = "Portuguese",
  ru = "Russian",
  tr = "Turkish",
}

local num_callbacks = 0

---------------------------
-- Source initialization --
---------------------------

source = {
  id = "grl-euronews-lua",
  name = "Euronews",
  description = "A source for watching Euronews online",
  supported_keys = { "id", "title", "url" },
  supported_media = 'video',
  icon = 'resource:///org/gnome/grilo/plugins/euronews/euronews.svg',
  tags = { 'news', 'tv', 'net:internet', 'net:plaintext' }
}

------------------
-- Source utils --
------------------

function grl_source_browse(media_id)
  if grl.get_options("skip") > 0 then
    grl.callback()
    return
  end

  for lang in pairs(langs) do
    num_callbacks = num_callbacks + 1
  end

  for lang in pairs(langs) do
    local api_url = get_api_url(lang)
    grl.fetch(api_url, euronews_initial_fetch_cb, lang)
  end
end

------------------------
-- Callback functions --
------------------------

function euronews_initial_fetch_cb(results, lang)
  videoId = results:match('videoId%&quot%;:%&quot%;(.-)%&quot%;')
  if not videoId then
    grl.warning("No videoId for " .. lang)
    callback_done()
    return
  end

  item = {}
  item.primary = string.format("https://www.youtube.com/watch?v=%s", videoId)
  local media = create_media(lang, item)
  if media ~= nil then
    grl.callback(media, -1)
  end

  callback_done()
end

-------------
-- Helpers --
-------------

function callback_done()
  num_callbacks = num_callbacks - 1

  -- finalize operation
  if num_callbacks == 0 then
    grl.callback()
  end
end

function get_api_url(id)
  if not langs[id] then
    grl.warning('Could not find language ' .. id)
    return id
  end

  return string.format(EURONEWS_URL, id == LANG_EN and "www" or id)
end

function get_lang(id)
  if not langs[id] then
    grl.warning('Could not find language ' .. id)
    return id
  end

  return grl.dgettext('iso_639', langs[id])
end

function create_media(lang, item)
  local media = {}

  media.type = "video"
  media.id = lang
  media.title = "Euronews " .. get_lang(lang)
  media.url = item.primary

  return media
end
