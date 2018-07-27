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
  tags = { 'music', 'net:plaintext' },
}

netopts = {
  user_agent = "Grilo Source Musicbrainz/0.3.8",
}

MUSICBRAINZ_DEFAULT_QUERY = "http://coverartarchive.org/%s/%s"
MUSICBRAINZ_RELEASES = {
   release = "mb_album_id",
   ["release-group"] = "mb_release_group_id"
}

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

function grl_source_resolve()
  req = grl.get_media_keys()
  if not req then
     grl.callback()
  end

  -- try to get the cover art associated with the mb_album_id
  -- if it does not exist, try the mb_release_group_id one
  -- if none of them exist, return nothing.
  test_thumbnail_url(nil)
end

---------------
-- Utilities --
---------------

function test_thumbnail_url(previous_release)
  local release, key, id
  local url

  release, key = next(MUSICBRAINZ_RELEASES, previous_release)
  id = req[key]
  -- no more url to test, exit
  -- FIXME add more checks on MB ID too
  if not release or not id or #id == 0 then
    grl.callback()
    return
  end

  -- try the next candidate
  url = string.format(MUSICBRAINZ_DEFAULT_QUERY, release, id)
  grl.fetch(url, netopts, test_thumbnail_url_cb, release)
end

function test_thumbnail_url_cb(result, current_release)
  -- url retrieval failed, try the next candidate
  if not result or result == "" then
    test_thumbnail_url(current_release)
    return
  end

  -- valid url found, generate the thumbnail
  media = build_media(current_release)
  grl.callback(media)
end

function build_media(release)
  local id = req[MUSICBRAINZ_RELEASES[release]]
  media = {}

  res = {}
  res[#res + 1] = string.format(MUSICBRAINZ_DEFAULT_QUERY, release, id)
  res[#res + 1] = string.format(MUSICBRAINZ_DEFAULT_QUERY .. '-1200', release, id)
  res[#res + 1] = string.format(MUSICBRAINZ_DEFAULT_QUERY .. '-500', release, id)
  res[#res + 1] = string.format(MUSICBRAINZ_DEFAULT_QUERY .. '-250', release, id)

  media.thumbnail = res

  return media
end
