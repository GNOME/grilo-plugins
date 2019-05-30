--[[
 * Copyright (C) 2016 Saiful B. Khan.
 *
 * Contact: Saiful B. Khan <saifulbkhan@gmail.com>
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
  id = "grl-musicbrainz",
  name = "MusicBrainz",
  description = "A plugin for fetching music metadata",
  supported_keys = {
    "title",
    "artist",
    "album",
    "album-disc-number",
    "track-number",
    "genre",
    "publication-date",
    "composer",
    "author",
    "mb-artist-id",
    "mb-album-id",
  },
  supported_media = { "audio" },
  resolve_keys = {
    ["type"] = "audio",
    required = {
   	  "mb-recording-id"
    },
    --Without a 'required' key the plugin fails.
  },
  tags = { 'music', 'net:internet' },
}

netopts = {
  user_agent = "Grilo Source MusicBrainz/0.3.0",
}

------------------
-- Source utils --
------------------

MUSICBRAINZ_BASE = "https://musicbrainz.org/ws/2/"
MUSICBRAINZ_LOOKUP_RECORDING = MUSICBRAINZ_BASE .. "recording/%s?inc=releaseswork-relsrelease-groupsmediumsartiststags"
MUSICBRAINZ_LOOKUP_WORK = MUSICBRAINZ_BASE .. "work/%s?inc=artist-rels"

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

function grl_source_resolve()
  local media = grl.get_media_keys()

  rec_id = media.mb_recording_id
  if rec_id then
    musicbrainz_resolve_by_recording_id(rec_id)
  else
    grl.callback()
    return
  end
end

---------------
-- Utilities --
---------------

function musicbrainz_resolve_by_recording_id(rec_id)
  --[[Resolve recording related keys first as it can provide artist and
      release related metadata if it were requested]]
  local recording_url = string.format (MUSICBRAINZ_LOOKUP_RECORDING, rec_id)
  local debug_str = string.format ("Resolving metadata for recording ID: %s", rec_id)
  grl.debug(debug_str)
  grl.fetch(recording_url, netopts, fetch_recording_cb)
end

--[[These functions append fetched values to the table 'media' which
    is then returned to grl.callback]]
function fetch_recording_cb(results)
  local recording = get_xml_entity(results, 'recording')
  local media = grl.get_media_keys()
  local keys = grl.get_requested_keys()
  local work_id

  if recording then
    if recording.title then
      media.title = recording.title.xml
    end

    if recording['artist-credit'] and
        recording['artist-credit']['name-credit'] and
        recording['artist-credit']['name-credit'].artist then
      local artist = recording['artist-credit']['name-credit'].artist
      if keys.artist then media.artist = artist.name.xml end
      if keys.mb_artist_id then media.mb_artist_id = artist.id end
    end

    if recording['relation-list'] and
        recording['relation-list'].relation then
      work_id = recording['relation-list'].relation.work.id
    end

    if recording['tag-list'] and keys.genre then
      media.genre = {}
      for index, tag in pairs(recording['tag-list']) do
        if tag.name then
          table.insert(media.genre, tostring(tag.name.xml))
        else
          for _ , val in pairs(tag) do
            table.insert(media.genre, tostring(val.name.xml))
          end
        end
      end
    end

    if recording['release-list'] then
      local release = recording['release-list']['release'][1]
      if not release then
        release = recording['release-list']['release']
      end

      if not release then
        grl.debug("Recording not found")
        return
      end

      if keys.album then media.album = release.title.xml end
      if keys.mb_album_id then media.mb_album_id = release.id end

      local release_date = nil
      if release.date then release_date = release.date.xml
      else
        if release['release-group'] and
            release['release-group']['first-release-date'] then
          release_date = release['release-group']['first-release-date'].xml
        end
      end
      if release_date and keys.publication_date then
        local y, m, d = nil
        while not y do
          y, m, d = string.match(release_date, "(%d)-(%d)-(%d)")
          release_date = release_date .. "-01"
        end
        media.publication_date = string.format('%04d-%02d-%02d', y, m, d)
      end

      if release['medium-list'] then
        local medium = release['medium-list'].medium
        if keys.album_disc_number then
          media.album_disc_number = medium.position.xml
        end

        if medium['track-list'] and keys.track_number then
          media.track_number = medium['track-list'].track.position.xml
        end
      end
    end

    --Add recording independent metadata like composer, writer, etc. to songs
    if work_id and (keys.author or keys.composer) then
      song_work_url = string.format(MUSICBRAINZ_LOOKUP_WORK, work_id)
      grl.debug('Fetching work related metadata')
      grl.fetch(song_work_url, netopts, fetch_work_relations_cb, media)
    else
      grl.callback(media)
      return
    end
  end
end

function fetch_work_relations_cb(results, media)
  local work = get_xml_entity(results, 'work')
  if not work or not work['relation-list'] then
    grl.callback(media)
    return
  end

  media.composer = {}
  media.author = {}
  local relation = nil
  if work['relation-list'].relation then
    relation = work['relation-list']['relation'][1]
  end
  if relation and relation.type then add_song_artist(relation, media)
  else
    for _ , val in pairs(relation) do
      add_song_artist(val, media)
    end
  end
  grl.callback(media)
end

function add_song_artist(rel, media)
  local type = rel.type
  if type == "composer" then
    table.insert(media.composer, rel.artist.name.xml)
  --[[For now writer and lyricist are stored in 'author' until seperate key for lyricist exists]]
  elseif type == "writer" or type == "lyricist" then
    table.insert(media.author, rel.artist.name.xml)
  end
end

function get_xml_entity(results, entity_name)
  if not results then return nil end
  if string.find(results, '{') == 1 then
    local error_table = grl.lua.json.string_to_table(results)
    grl.warning('grl-musicbrainz: ' .. error_table.error)
    return
  end

  local results_table = grl.lua.xml.string_to_table(results)
  if not results_table then return nil end

  if results_table.metadata then
    if entity_name == 'recording' and
        results_table.metadata.recording then
      return results_table.metadata.recording
    elseif entity_name == 'release-group' and
        results_table.metadata['release-group'] then
      return results_table.metadata['release-group']
    elseif entity_name == 'work' and
        results_table.metadata.work then
      return results_table.metadata.work
    elseif entity_name == 'recording_list' and
        results_table.metadata['recording-list'].count ~= 0 then
      return results_table.metadata['recording-list'].recording
    end
  end
  return nil
end