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

RADIOFRANCE_URL               = 'http://app2.radiofrance.fr/rfdirect/config/Radio.js'
FRANCEBLEU_URL                = 'http://app2.radiofrance.fr/rfdirect/config/FranceBleu.js'

---------------------------
-- Source initialization --
---------------------------

source = {
  id = "grl-radiofrance-lua",
  name = "Radio France",
  description = "A source for browsing Radio France radio stations",
  supported_keys = { "id", "thumbnail", "title", "url", "mime-type" },
  icon = 'http://www.radiofrance.fr/sites/all/themes/custom/rftheme/logo.png',
  supported_media = 'audio',
  tags = { 'radio', 'country:fr' }
}

------------------
-- Source utils --
------------------

function grl_source_browse(media_id)
  if grl.get_options("skip") > 0 then
    grl.callback()
  else
    grl.fetch(RADIOFRANCE_URL, "radiofrance_fetch_cb")
  end
end

------------------------
-- Callback functions --
------------------------

-- return all the media found
function radiofrance_fetch_cb(playlist)
  if parse_playlist(playlist, false) then
    grl.fetch(FRANCEBLEU_URL, "francebleu_fetch_cb")
  end
end

function francebleu_fetch_cb(playlist)
  parse_playlist(playlist, true)
end

-------------
-- Helpers --
-------------

function parse_playlist(playlist, francebleu)
  local match1_prefix, match2
  if francebleu then
    match1_prefix = '_frequence'
    match2 = '{(.-logo_region.-)}'
  else
    match1_prefix = '_radio'
    match2 = '{(.-#rfdirect.-)}'
  end

  if not playlist then
    grl.callback()
    return false
  end

  local items = playlist:match('Flux = {.-' .. match1_prefix .. ' : {(.*)}.-}')
  for item in items:gmatch(match2) do
    local media = create_media(item, francebleu)
    if media then
      grl.callback(media, -1)
    end
  end

  if francebleu then
    grl.callback()
  end

  return true
end

function get_thumbnail(id)
  local images = {}
  images['FranceInter'] = 'http://www.franceinter.fr/sites/all/themes/franceinter/logo.png'
  images['FranceInfo'] = 'http://www.franceinfo.fr/sites/all/themes/franceinfo/logo.png'
  images['FranceCulture'] = 'http://www.franceculture.fr/sites/all/themes/franceculture/images/logo.png'
  images['FranceMusique'] = 'http://www.francemusique.fr/sites/all/themes/custom/france_musique/logo.png'
  images['Fip'] = 'http://www.fipradio.fr/sites/all/themes/fip2/images/logo_121x121.png'
  images['LeMouv'] = 'http://www.lemouv.fr/sites/all/themes/mouv/images/logo_119x119.png'
  images['FranceBleu'] = 'http://www.francebleu.fr/sites/all/themes/francebleu/logo.png'

  return images[id]
end

function create_media(item, francebleu)
  local media = {}

  if francebleu then
    media.url = item:match("mp3_direct : '(http://.-)'")
  else
    media.url = item:match("hifi :'(.-)'")
  end
  if not media.url or media.url == '' then
    return nil
  end

  media.type = "audio"
  media.mime = "audio/mpeg"
  media.id = item:match("id : '(.-)',")
  media.title = item:match("nom : '(.-)',")
  media.title = media.title:gsub("\\'", "'")
  if francebleu then
    media.thumbnail = get_thumbnail('FranceBleu')
  else
    media.thumbnail = get_thumbnail(media.id)
  end

  return media
end
