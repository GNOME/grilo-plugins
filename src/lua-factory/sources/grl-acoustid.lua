--[[
 * Copyright (C) 2016 Grilo Project
 *
 * Contact: Victor Toso <me@victortoso.com>
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
  id = "grl-acoustid",
  name = "Acoustid",
  description = "a source that provides audio identification",
  supported_keys = { "title", "album", "artist", "mb-recording-id", "mb-album-id", "mb-artist-id", "mb-release-group-id", "mb-release-id", "album-disc-number", "publication-date", "track-number", "creation-date" },
  supported_media = { 'audio' },
  config_keys = {
    required = { "api-key" },
  },
  resolve_keys = {
    ["type"] = "audio",
    required = { "duration", "chromaprint" }
  },
  tags = { 'music', 'net:internet' },
}

netopts = {
  user_agent = "Grilo Source AcoustID/0.3.0",
}

------------------
-- Source utils --
------------------
acoustid = {}

-- https://acoustid.org/webservice#lookup
ACOUSTID_LOOKUP = "https://api.acoustid.org/v2/lookup?client=%s&meta=compress+recordings+releasegroups+releases+sources+tracks&duration=%d&fingerprint=%s"

---------------------------------
-- Handlers of Grilo functions --
---------------------------------
function grl_source_init (configs)
  acoustid.api_key = configs.api_key
  return true
end

function grl_source_resolve ()
  local url
  local media = grl.get_media_keys()

  if not media or
     not media.duration or
     not media.chromaprint or
     #media.chromaprint == 0 then
    grl.callback ()
    return
  end

  url = string.format (ACOUSTID_LOOKUP, acoustid.api_key, media.duration,
                       media.chromaprint)
  grl.fetch (url, netopts, lookup_cb_resolve)
end

---------------
-- Utilities --
---------------

function lookup_cb_resolve (feed)
  local sources = 0
  local record, releasegroup
  if not feed then
    grl.callback()
    return
  end

  local json = grl.lua.json.string_to_table (feed)
  if not json or json.status ~= "ok" then
    grl.callback()
  end

  if json.results and #json.results > 0 and
     json.results[1].recordings and
     #json.results[1].recordings > 0 then
    for _, recording in ipairs(json.results[1].recordings) do
      if recording.sources > sources then
        sources = recording.sources
        record = recording
        if #recording.releasegroups > 0 then
          releasegroup = recording.releasegroups[1]
        end
      end
    end
  end

  if record and releasegroup then
    media = build_media (record, releasegroup)
    grl.callback (media)
  else
    grl.callback ()
  end
end


function build_media(record, releasegroup)
  local media = {}
  local keys = grl.get_requested_keys ()
  local album, release, artist
  local release_group_id
  local creation_date = nil

  media.title = keys.title and record.title or nil
  media.mb_recording_id = keys.mb_recording_id and record.id or nil

  album = releasegroup
  media.album = keys.album and album.title or nil
  release_group_id = releasegroup.id or nil
  media.mb_album_id = release_group_id
  media.mb_release_group_id = release_group_id

  -- FIXME: related-keys on lua sources are in the TODO list
  -- https://bugzilla.gnome.org/show_bug.cgi?id=756203
  -- and for that reason we are only returning first of all metadata
  if record and record.artists and #record.artists > 0 then
    artist = record.artists[1]
    media.artist = keys.artist and artist.name or nil
    media.mb_artist_id = keys.mb_artist_id and artist.id or nil
  end

  if album and album.releases and #album.releases > 0 then
    if keys.creation_date then
      for _, release in ipairs(album.releases) do
        if release.date then
          local month = release.date.month or 1
          local day = release.date.day or 1
          local year= release.date.year
        if not creation_date or
           year < creation_date.year or
           (year == creation_date.year and
           month < creation_date.month) or
           (year == creation_date.year and
           month == creation_date.month and
           day < creation_date.day) then
            creation_date = {day=day, month=month, year=year}
          end
        end
      end

      if creation_date then
        media.creation_date = string.format('%04d-%02d-%02d', creation_date.year,
                                            creation_date.month, creation_date.day)
      end
    end

    release = album.releases[1]
    media.mb_release_id = release.id or nil

    if release.date then
      local date = release.date
      local month = date.month or 1
      local day = date.day or 1
      date = string.format('%04d-%02d-%02d', date.year, month, day)
      media.publication_date = keys.publication_date and date or nil
    end

    if release.mediums and #release.mediums > 0 then
      medium = release.mediums[1]
      media.album_disc_number = keys.album_disc_number and medium.position or nil
      if medium.tracks and #medium.tracks > 0 then
        media.track_number = keys.track_number and medium.tracks[1].position or nil
      end
    end
  end

  return media
end
