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

local stations = { 'franceinter', 'franceinfo', 'franceculture', 'francemusique', 'fipradio', 'lemouv' }

---------------------------
-- Source initialization --
---------------------------

source = {
  id = "grl-radiofrance-lua",
  name = "Radio France",
  description = "A source for browsing Radio France radio stations",
  supported_keys = { "id", "thumbnail", "title", "url", "mime-type" },
  icon = 'resource:///org/gnome/grilo/plugins/radiofrance/radiofrance.png',
  supported_media = 'audio',
  tags = { 'radio', 'country:fr', 'net:internet', 'net:plaintext' }
}

------------------
-- Source utils --
------------------

function grl_source_browse(media_id)
  if grl.get_options("skip") > 0 then
    grl.callback()
  else
    local urls = {}
    for index, item in pairs(stations) do
      local url = 'http://www.' .. item .. '.fr/player'
      table.insert(urls, url)
    end
    grl.fetch(urls, radiofrance_now_fetch_cb)
  end
end

------------------------
-- Callback functions --
------------------------

-- return all the media found
function radiofrance_now_fetch_cb(results)
  for index, result in pairs(results) do
    local media = create_media(stations[index], result)
    grl.callback(media, -1)
  end

  grl.callback()
end

-------------
-- Helpers --
-------------

function get_thumbnail(id)
  local images = {}
  images['franceinter'] = 'http://www.franceinter.fr/sites/all/themes/franceinter/logo.png'
  images['franceinfo'] = 'http://www.franceinfo.fr/sites/all/themes/custom/france_info/logo.png'
  images['franceculture'] = 'http://www.franceculture.fr/sites/all/themes/franceculture/images/logo.png'
  images['francemusique'] = 'http://www.francemusique.fr/sites/all/themes/custom/france_musique/logo.png'
  images['fipradio'] = 'http://www.fipradio.fr/sites/all/themes/custom/fip/logo.png'
  images['lemouv'] = 'http://www.lemouv.fr/sites/all/themes/mouv/images/logo_119x119.png'

  return images[id]
end

function get_title(id)
  local names = {}
  names['franceinter'] = 'France Inter'
  names['franceinfo'] = 'France Info'
  names['franceculture'] = 'France Culture'
  names['francemusique'] = 'France Musique'
  names['fipradio'] = 'Fip Radio'
  names['lemouv'] = "Le Mouv'"

  return names[id]
end

function create_media(id, result)
  local media = {}

  media.type = "audio"
  media.mime_type = "audio/mpeg"
  media.id = id
  if media.id == 'fipradio' then
    media.id = 'fip'
  end

  media.url = result:match("urlLive:'(http.-%mp3)")
  if not media.url then
    media.url = result:match('player" href="(http.-%.mp3)')
  end
  if not media.url then
    media.url = result:match('data%-url%-live="(http.-%.mp3)')
  end

  media.title = get_title(id)
  media.thumbnail = get_thumbnail(id)

  -- FIXME Add metadata about the currently playing tracks
  -- Available in 'http://www.' .. item .. '.fr/api/now&full=true'
  return media
end
