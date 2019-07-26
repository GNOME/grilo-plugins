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
ACOUSTID_ROOT_URL = "https://api.acoustid.org/v2/lookup?client=%s&meta=compress+recordings+releasegroups+releases+sources+tracks"

---------------------------------
-- Handlers of Grilo functions --
---------------------------------
function grl_source_init (configs)
    acoustid.api_key = configs.api_key
    return true
end

function grl_source_resolve (media, options, callback)
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
  grl.fetch (url, netopts, lookup_cb)
end

-- Query is a Method of acoustid's webservice to perform a lookup operation
-- See: https://acoustid.org/webservice
-- by fingerprint: meta=compress+recordings+releasegroups+releases+sources+tracks&duration=duration&fingerprint=fingerprint
-- by trackid: meta=compress+recordings+releasegroups+releases+sources+tracks&acoustid=trackid
function grl_source_query (query)
  if check_input(query) then
    local url = string.format(ACOUSTID_ROOT_URL .. "&" .. query, acoustid.api_key)
    grl.fetch (url, netopts, lookup_cb_query)
  end
end

---------------
-- Utilities --
---------------

function check_input(query)
  if not query then
    return false
  end
  local duration = ""
  local fingerprint = ""
  local j = 0
  for i = 10, string.len(query) do
    chars = string.sub(query, i, i)
    if chars ~= '&' then
      duration = duration .. string.sub(query, i, i)
    else
      j = i + 13
      break
    end
  end

  for i = j, string.len(query) do
    fingerprint = fingerprint .. string.sub(query, i, i)
  end

  if duration and string.len(duration) > 0 and
    fingerprint and string.len(fingerprint) > 0 then
    return true
  else
    return false
  end
end

function get_count(results)
  local count = 0

  if results and #results > 0 then
    for _,result in ipairs(results) do
      if result.recordings and #result.recordings > 0 then
        for _,recording in ipairs(result.recordings) do
          count = count + #recording.releasegroups
        end
      end
    end
  end

  return count
end

function lookup_cb_query (feed)
  local count = 0
  if not feed then
    grl.callback()
    return
  end

  local json = grl.lua.json.string_to_table (feed)
  if not json or json.status ~= "ok" then
    grl.callback()
  end

  if not json.results or #json.results == 0 then
    grl.callback()
    return
  end

  count = get_count(json.results)
  for i,result in ipairs(json.results) do
    if result.recordings and
      #result.recordings > 0 then
      for _, recording in ipairs(result.recordings) do
        if recording.releasegroups and
          #recording.releasegroups > 0 then
          for _, releasegroup in ipairs(recording.releasegroups) do
            count = count - 1
            media = build_media_query (recording, releasegroup)
            grl.callback (media, count)
          end
        end
      end
    end
  end
  grl.callback ()

end


function lookup_cb (feed)
  if not feed then
    grl.callback()
    return
  end

  local json = grl.lua.json.string_to_table (feed)
  if not json or json.status ~= "ok" then
    grl.callback()
  end

  media = build_media (json.results)
  grl.callback (media)
end


function build_media(results)
  local media = grl.get_media_keys ()
  local keys = grl.get_requested_keys ()
  local record, album, artist
  local release_group_id
  local sources = 0
  local creation_date = nil

  if results and #results > 0 and
      results[1].recordings and
      #results[1].recordings > 0 then
    for _, recording in ipairs(results[1].recordings) do
      if recording.sources > sources then
        sources = recording.sources
        record = recording
      end
    end

    media.title = keys.title and record.title or nil
    media.mb_recording_id = keys.mb_recording_id and record.id or nil
  end

  if record and
      record.releasegroups and
      #record.releasegroups > 0 then

    album = record.releasegroups[1]
    media.album = keys.album and album.title or nil
    release_group_id = keys.mb_album_id and album.id or nil
    media.mb_album_id = release_group_id
    media.mb_release_group_id = release_group_id
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
    media.mb_release_id = keys.mb_album_id and release.id or nil

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

function build_media_query(record, releasegroup)
  local media = {}
  local keys = grl.get_requested_keys ()
  local album, release

  if record then
    media.title = keys.title and record.title or nil
    media.mb_recording_id = keys.mb_recording_id and record.id or nil
  end

  if releasegroup then
    album = releasegroup
    media.album = keys.album and album.title or nil
    media.mb_release_group_id = keys.mb_release_group_id and album.id or nil
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
    release = album.releases[1]
    media.mb_release_id = keys.mb_release_id and release.id or nil

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
