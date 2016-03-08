--[[
 * Copyright (C) 2015 Victor Toso.
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
  id = "test-source-xml-parser",
  name = "Fake Source for XML Parser",
  description = "a source to test XML parser function",
  supported_keys = { "title" },
  supported_media = "all",
  resolve_keys = {
    ["type"] = "all",
    required = { "url" } ,
  },
  tags = { 'test', 'net:plaintext' },
}

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

function grl_source_resolve()
  -- This source expects an url which will be fetched and converted
  -- to a table using grl.lua.xml.string_to_table().
  local req = grl.get_media_keys()
  if not req or not req.url or #req.url ~= 2 then
    grl.warning("resolve was called without metadata-key url")
    grl.callback()
    return
  end

  grl.fetch(req.url, fetch_url_cb)
end

-- feeds[1] is the xml to test
-- feeds[2] is a lua table with this xml, to compare
function fetch_url_cb(feeds)
  if not feeds or #feeds ~= 2 then
    grl.warning("failed to load xml")
    grl.callback()
    return
  end

  local xml = grl.lua.xml.string_to_table(feeds[1])
  local ref = load(feeds[2])()
  if not xml or not ref then
    grl.warning ("xml parser failed")
    grl.callback()
    return
  end

  if not test_table_contains(xml, ref) or
     not test_table_contains(ref, xml) then
    grl.warning("xml parser failed, results are not the same\n" ..
                "reference table of test:\n" .. grl.lua.inspect(ref) .. "\n" ..
                "table from xml parser:\n" .. grl.lua.inspect(xml))
    grl.callback()
    return
  end

  local media = { id = "success" }
  grl.callback(media, 0)
end

function test_table_contains(t, e)
  if type(t) ~= "table" or
     type(e) ~= "table" then
     return false
  end

  -- This is xml: keys are always strings
  for key, value in pairs(t) do
    if not e[key] then
      grl.debug ("table does not have key: " .. key)
      return false
    end

    if type(value) == "string" then
      if t[key] ~= e[key] then
        grl.debug ("values differ '" .. t[key] .. "' and '" .. e[key] .. "'")
        return false
      end
    elseif type(value) == "table" then
      if not test_table_contains(t[key], e[key]) or
         not test_table_contains(e[key], t[key]) then
         return false
      end
    else
      grl.warning("test not handling type: " .. type(value))
      return false
    end
  end
  return true
end
