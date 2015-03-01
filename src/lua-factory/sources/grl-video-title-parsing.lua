--[[
 * Copyright (C) 2014 Grilo Project
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
  id = "grl-video-title-parsing",
  name = "video-title-parsing",
  description = "Video title parsing",
  supported_keys = { "episode-title", 'show', 'publication-date', 'season', 'episode', 'title' },
  supported_media = 'video',
  resolve_keys = {
    ["type"] = "video",
    required = { "title" },
  },
}

blacklisted_words = {
  "720p", "1080p", "x264",
  "ws", "proper",
  "real.repack", "repack",
  "hdtv", "pdtv", "notv",
  "dsr", "DVDRip", "divx", "xvid",
}

parsers = {
  tvshow = {
    "(.-)[sS](%d+)[%s.]*[eE][pP]?(%d+)(.+)",
    "(.-)(%d+)[xX.](%d+)(.+)",
  },
  movies = {
    "(.-)(19%d%d)",
    "(.-)(20%d%d)",
  }
}

function clean_title(title)
  return title:gsub("^[%s%W]*(.-)[%s%W]*$", "%1"):gsub("%.", " ")
end

function clean_title_from_blacklist(title)
  local s = title:lower()
  local last_index

  -- remove movie sufix
  s = s:gsub("(.+)%..-$", "%1")

  -- ignore everything after the first blacklisted word
  last_index = #s
  for i, word in ipairs (blacklisted_words) do
    local index = s:find(word:lower())
    if index and index < last_index then
      last_index = index - 1
    end
  end
  return title:sub(1, last_index)
end

function parse_as_movie(media)
  local title, date
  local str = clean_title_from_blacklist (media.title)
  for i, parser in ipairs(parsers.movies) do
    title, date = str:match(parser)
    if title and date then
      media.title = clean_title(title)
      media.publication_date = date
      return true
    end
  end
  return false
end

function parse_as_episode(media)
  local show, season, episode, title
  for i, parser in ipairs(parsers.tvshow) do
    show, season, episode, title = media.title:match(parser)
    if show and season and episode and tonumber(season) < 50 then
      media.show = clean_title(show)
      media.season = season
      media.episode = episode
      media.episode_title = clean_title(clean_title_from_blacklist(title))
      return true
    end
  end
  return false
end

function grl_source_resolve()
  local req
  local media = {}

  req = grl.get_media_keys()
  if not req or not req.title then
    grl.callback()
    return
  end

  media.title = req.title

  -- It is easier to identify a tv show due information
  -- related to episode and season number
  if parse_as_episode(media) then
    grl.debug(req.title .. " is an EPISODE")
    grl.callback(media, 0)
    return
  end

  if parse_as_movie(media) then
    grl.debug(req.title .. " is a MOVIE")
    grl.callback(media, 0)
    return
  end

  grl.debug("Fail to identify video: " .. req.title)
  grl.callback()
end
