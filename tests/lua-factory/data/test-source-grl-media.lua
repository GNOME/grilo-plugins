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
  id = "test-source-grl-media",
  name = "Fake Source",
  description = "a source to test GrlMedia",
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
  -- to a GrlMedia with grl.lua.json.string_to_table().
  local req = grl.get_media_keys()
  if not req or not req.url or #req.url == 0 then
    grl.warning("resolve was called without metadata-key url")
    grl.callback()
    return
  end
  grl.fetch(req.url, fetch_url_cb)
end

function fetch_url_cb(feed)
  if not feed or #feed == 0 then
    grl.warning("failed to load json")
    grl.callback()
    return
  end

  local media = grl.lua.json.string_to_table(feed)
  if not media then
    grl.warning ("fail to make media from json")
    grl.callback()
    return
  end

  -- related keys need to have an index
  if media["related-keys"] then
    for _, rel_key in pairs(media["related-keys"]) do
      media[#media + 1] = rel_key
    end
    media["related-keys"] = nil
  end

  grl.callback(media, 0)
end
