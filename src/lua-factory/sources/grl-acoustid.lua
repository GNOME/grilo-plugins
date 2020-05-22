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
  supported_keys = { "title", "album", "album-artist", "artist", "mb-recording-id", "mb-album-id", "mb-artist-id", "mb-release-group-id", "mb-release-id", "album-disc-number", "publication-date", "track-number", "creation-date" },
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
ACOUSTID_ROOT_URL = "https://api.acoustid.org/v2/lookup?client=%s&meta=compress+recordings+releasegroups+releases+sources+tracks"
ACOUSTID_LOOKUP_FINGERPRINT = ACOUSTID_ROOT_URL .. "&duration=%d&fingerprint=%s"

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

  url = string.format (ACOUSTID_LOOKUP_FINGERPRINT, acoustid.api_key, media.duration,
                       media.chromaprint)
  grl.fetch (url, netopts, lookup_cb_resolve)
end

-- Query is a Method of acoustid's webservice to perform a lookup operation
-- See: https://acoustid.org/webservice
-- by fingerprint: duration=duration&fingerprint=fingerprint
function grl_source_query (query)
  local url
  duration, fingerprint = query:match("duration=(%d+)&fingerprint=(%w+)")
  if duration and fingerprint then
    url = string.format(ACOUSTID_ROOT_URL .. "&" .. query, acoustid.api_key)
    grl.fetch (url, netopts, lookup_cb_query)
  else
    grl.callback ()
    return
  end
end

---------------
-- Utilities --
---------------

function get_count(results)
  local count = 0
  local recordings_found = {}

  if results and #results > 0 then
    for _,result in ipairs(results) do
      if result.recordings and #result.recordings > 0 then
        for _,recording in ipairs(result.recordings) do
          if recording.releasegroups ~= nil and
	      not recordings_found[recording.id] then
	    recordings_found[recording.id] = true
            count = count + #recording.releasegroups
          end
        end
      end
    end
  end

  return count
end

function lookup_cb_resolve (feed)
  local sources = 0
  local record, releasegroup
  if not feed then
    grl.callback()
    return
  end

  local json = grl.lua.json.string_to_table (feed)
  if not json or json.status ~= "ok" or
     not json.results or #json.results <= 0 or
     not json.results[1].recordings or #json.results[1].recordings <= 0 then
    grl.callback()
  end

  for _, recording in ipairs(json.results[1].recordings) do
    if recording.sources > sources then
      sources = recording.sources
      if #recording.releasegroups > 0 then
        record = recording
        releasegroup = recording.releasegroups[1]
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

function lookup_cb_query (feed)
  local count
  local recordings_found = {}
  if not feed then
    grl.callback()
    return
  end

  local json = grl.lua.json.string_to_table (feed)
  if not json or json.status ~= "ok" or
     not json.results or #json.results <= 0 then
    grl.callback()
    return
  end

  count = grl.get_options("count")
  if not count or count <= 0 then
    count = get_count(json.results)
  end
  for _,result in ipairs(json.results) do
    if result.recordings and
      #result.recordings > 0 then
      for _, recording in ipairs(result.recordings) do
        if recording.releasegroups and
            #recording.releasegroups > 0 and
	    not recordings_found[recording.id] then
	  recordings_found[recording.id] = true
          for _, releasegroup in ipairs(recording.releasegroups) do
            count = count - 1
            media = build_media (recording, releasegroup)
            grl.callback (media, count)
            if count == 0 then
              return
            end
          end
        end
      end
    end
  end
end

function build_media(record, releasegroup)
  local media = {}
  local keys = grl.get_requested_keys ()
  local album, release, artist
  local creation_date = nil

  media.title = keys.title and record.title or nil
  media.mb_recording_id = keys.mb_recording_id and record.id or nil

  album = releasegroup
  media.album = keys.album and album.title or nil
  media.mb_album_id = keys.mb_album_id and album.id or nil
  media.mb_release_group_id = keys.mb_release_group_id and album.id or nil

  if album.artists and keys.album_artist then
    media.album_artist = {}
    for _, artist in ipairs(album.artists) do
      table.insert(media.album_artist, artist.name)
    end
  end

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
    media.mb_release_id = (keys.mb_release_id or keys.mb_album_id) and release.id or nil

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
