--[[
 * Copyright (C) 2014 Victor Toso.
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

---------------------------
-- Source initialization --
---------------------------

source = {
  id = "grl-musicbrainz-coverart",
  name = "Musicbrainz Cover Art",
  description = "a source for coverart",
  supported_keys = { "thumbnail" },
  supported_media = { 'audio', 'video' },
  resolve_keys = {
    ["type"] = "audio",
    required = { "mb-album-id" },
  },
}

MUSICBRAINZ_DEFAULT_QUERY = "http://coverartarchive.org/release/%s/front"

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

function grl_source_resolve()
  local url, req
  local id

  req = grl.get_media_keys()
  id = req.mb_album_id
  -- FIXME add more checks on MB ID too
  if not req or not id or #id == 0 then
    grl.callback()
    return
  end

  -- Prepare artist and title strings to the url
  media = {}

  res = {}
  res[#res + 1] = string.format(MUSICBRAINZ_DEFAULT_QUERY, id)
  res[#res + 1] = string.format(MUSICBRAINZ_DEFAULT_QUERY .. '-500', id)
  res[#res + 1] = string.format(MUSICBRAINZ_DEFAULT_QUERY .. '-250', id)

  media.thumbnail = res
  grl.callback(media, 0)
end
