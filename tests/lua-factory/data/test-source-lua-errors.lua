--[[
 * Copyright (C) 2016 Victor Toso.
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
  id = "test-source-lua-errors",
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

TEST_NOT_CALLBACK_SIMPLE = "test-not-calback-simple"
TEST_NOT_CALLBACK_ASYNC =  "test-not-callback-on-async"
TEST_CALLBACK_ON_FINISHED_OP = "test-callback-after-finished"
TEST_MULTIPLE_FETCH = "multiple-fetch-with-callback"

---------------------------------
-- Handlers of Grilo functions --
---------------------------------
function grl_source_resolve()
  local media = grl.get_media_keys()
  local operation_id = grl.get_options("operation-id" )

  if not media or not media.id or not media.url then
    grl.warning ("test failed due lack of information")
    grl.callback()
    return
  end
  grl.debug ("source-resolve-test: " .. media.id)

  if media.id == TEST_NOT_CALLBACK_SIMPLE then
    test_not_callback_simple (media, operation_id)

  elseif media.id == TEST_NOT_CALLBACK_ASYNC then
    test_not_callback_async (media, operation_id)

  elseif media.id == TEST_CALLBACK_ON_FINISHED_OP then
    test_callback_on_finished_op (media, operation_id)

  else
    grl.warning ("test unknow: " .. media.id)
    grl.callback()
  end
end

function grl_source_search(test_id)
  local url = "http://xml.parser.test/lua-factory/simple.xml"
  local operation_id = grl.get_options("operation-id" )
  grl.debug ("source-search-test: " .. test_id)

  if test_id == TEST_MULTIPLE_FETCH then
    test_multiple_fetch (url, operation_id)
  else
    grl.warning ("test unknow: " .. test_id)
    grl.callback()
  end
end

---------------------------------
-- Handlers of Tests functions --
---------------------------------
function test_multiple_fetch(url, operation_id)
  grl.debug ("calling multiple grl.fetch and only grl.callback() "
             .. "in the last one | operation-id: " .. operation_id)
  grl.debug (url)
  local test_t = { num_op = 4, received = 0 }
  for i = 1, test_t.num_op do
    grl.debug ("operation: " .. i)
    grl.fetch(url, fetch_multiple_url_cb, test_t)
  end
end

function test_not_callback_simple (media, operation_id)
    grl.debug ("not calling grl.callback, operation-id:  " .. operation_id)
end

function test_callback_on_finished_op (media, operation_id)
  grl.debug ("calling grl.callback after operation is over, "
             .. "operation-id: " .. operation_id)
  grl.callback ({title = "grilo-1" }, 0)
  grl.callback ({title = "grilo-2" }, 0)
end

function test_not_callback_async (media, operation_id)
  grl.debug ("calling grl.fetch but not grl.callback, "
             .. "operation-id: " .. operation_id)
  grl.fetch(media.url, fetch_url_cb)
end

---------------------------------
-- Callbacks --------------------
---------------------------------
function fetch_multiple_url_cb(feeds, data)
  if not data then
    grl.warning ("Fail to get userdata")
    return
  end
  data.received = data.received + 1
  grl.debug (string.format("fetch_multiple_url_cb: received %d/%d",
                           data.received, data.num_op))
  local media = { title = "title: " .. data.received }
  local count = data.num_op - data.received
  grl.callback (media, count)
end

function fetch_url_cb(feeds)
  grl.debug ("fetch_url_cb: not calling grl.callback()" )
end
