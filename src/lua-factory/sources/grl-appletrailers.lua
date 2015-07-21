--[[
 * Copyright (C) 2015 Grilo Project
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
  id = "grl-appletrailers-lua",
  name = "Apple Movie Trailers",
  description = "Apple Trailers",
  supported_media = 'video',
  supported_keys = { 'author', 'publication-date', 'description', 'duration', 'genre', 'id', 'thumbnail', 'title', 'url', 'certificate', 'studio', 'license', 'performer', 'size' },
  config_keys = {
    optional = { 'definition' },
  },
  icon = 'resource:///org/gnome/grilo/plugins/appletrailers/trailers.svg',
  tags = { 'country:us', 'cinema', 'net:internet', 'net:plaintext' },
}

-- Global table to store config data
ldata = {}

-- Global table to store parse results
cached_xml = nil

function grl_source_init(configs)
  ldata.hd = (configs.definition and configs.definition == 'hd')
  return true
end

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

APPLE_TRAILERS_CURRENT_SD = "http://trailers.apple.com/trailers/home/xml/current_480p.xml"
APPLE_TRAILERS_CURRENT_HD = "http://trailers.apple.com/trailers/home/xml/current_720p.xml"

function grl_source_browse()
  local skip = grl.get_options("skip")
  local count = grl.get_options("count")

  -- Make sure to reset the cache when browsing again
  if skip == 0 then
    cached_xml = nil
  end

  if cached_xml then
    parse_results(cached_xml)
  else
    local url = APPLE_TRAILERS_CURRENT_SD
    if ldata.hd then
      url = APPLE_TRAILERS_CURRENT_HD
    end

    grl.debug('Fetching URL: ' .. url .. ' (count: ' .. count .. ' skip: ' .. skip .. ')')
    grl.fetch(url, "fetch_results_cb")
  end
end

---------------
-- Utilities --
---------------

-- simili-XML parsing from
-- http://lua-users.org/wiki/LuaXml
function parseargs(s)
  local arg = {}
  string.gsub(s, "([%-%w]+)=([\"'])(.-)%2", function (w, _, a)
    arg[w] = a
  end)
  return arg
end

function collect(s)
  local stack = {}
  local top = {}
  table.insert(stack, top)
  local ni,c,label,xarg, empty
  local i, j = 1, 1
  while true do
    ni,j,c,label,xarg, empty = string.find(s, "<(%/?)([%w:]+)(.-)(%/?)>", i)
    if not ni then break end
    local text = string.sub(s, i, ni-1)
    if not string.find(text, "^%s*$") then
      table.insert(top, text)
    end
    if empty == "/" then  -- empty element tag
      table.insert(top, {label=label, xarg=parseargs(xarg), empty=1})
    elseif c == "" then   -- start tag
      top = {label=label, xarg=parseargs(xarg)}
      table.insert(stack, top)   -- new level
    else  -- end tag
      local toclose = table.remove(stack)  -- remove top
      top = stack[#stack]
      if #stack < 1 then
        error("nothing to close with "..label)
      end
      if toclose.label ~= label then
        error("trying to close "..toclose.label.." with "..label)
      end
      table.insert(top, toclose)
    end
    i = j+1
  end
  local text = string.sub(s, i)
  if not string.find(text, "^%s*$") then
    table.insert(stack[#stack], text)
  end
  if #stack > 1 then
    error("unclosed "..stack[#stack].label)
  end
  return stack[1]
end

function flatten_array(array)
  local t = {}

  for i, v in ipairs(array) do
    if v.label == 'movieinfo' then
      v.label = v.xarg.id
    end

    if v.xarg and v.xarg.filesize then
      t['filesize'] = v.xarg.filesize
    end

    if v.label then
      if (type(v) == "table") then
        -- t['name'] already exists, append to it
        if t[v.label] then
          table.insert(t[v.label], v[1])
        else
          t[v.label] = flatten_array(v)
        end
      else
        t[v.label] = v
      end
    else
      if (type(v) == "table") then
        table.insert(t, flatten_array(v))
      else
        table.insert(t, v)
      end
    end
  end

  return t
end

function fetch_results_cb(results)
  if not results then
    grl.warning('Failed to fetch XML file')
    grl.callback()
    return
  end

  local array = collect(results)
  cached_xml = flatten_array(array)

  parse_results(cached_xml)
end

function parse_results(results)
  local count = grl.get_options("count")
  local skip = grl.get_options("skip")

  for i, item in pairs(results.records) do
    local media = {}

    media.type = 'video'
    media.id = i
    if item.cast then media.performer = item.cast.name end
    media.genre = item.genre.name
    media.license = item.info.copyright[1]
    media.description = item.info.description[1]
    media.director = item.info.director[1]
    media.publication_date = item.info.releasedate[1]
    media.certificate = item.info.rating[1]
    media.studio = item.info.studio[1]
    media.title = item.info.title[1]
    media.thumbnail = item.poster.xlarge[1]
    media.url = item.preview.large[1]
    media.size = item.preview.filesize

    if skip > 0 then
      skip = skip - 1
    else
      count = count - 1
      grl.callback(media, count)
      if count == 0 then
        return
      end
    end
  end

  if count ~= 0 then
    grl.callback()
  end
end
