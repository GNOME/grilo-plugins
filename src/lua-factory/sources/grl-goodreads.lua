--[[
 * Copyright (C) 2016 Grilo Project
 *
 * Contact: Thiago Mendes <thiago@posteo.net>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
--]]

---------------------------
-- Source initialization --
---------------------------

source = {
  id = "grl-goodreads",
  name = "Goodreads",
  description = "Goodreads is the world's largest site for readers and book recommendations",
  supported_keys = { "author", "id", "title", "thumbnail", "description", "url",
                     "publication", "rating",
                     "gr-author-id", "gr-small-image-url", "gr-country-code",
                     "gr-publisher", "gr-pages"},
  -- config_keys = {
    -- required = { "api-key" },
  -- },
  resolve_keys = {
    ["type"] = "none",
    required = { "isbn" }
  },
  tags = { 'books', 'net:internet', 'net:plaintext' }
}

netopts = {
  user_agent = "Grilo Source Goodreads Plugin/0.3.8",
}

-------------------
-- API RESOURCES --
-------------------
-- api max itens
API_ITENS_PER_PAGE = 20

-- api urls
api_key = "bI47FWrmOlgtub2BlieRw"
api_main_url = "https://www.goodreads.com/search/index.xml"
api_more_info = "https://www.goodreads.com/book/show"

---------------------------------
-- Handlers of Grilo functions --
---------------------------------

function grl_source_resolve ()
  local req = grl.get_media_keys()

  if not req or not req.isbn then
    grl.callback ()
    return
  end

  local url = string.format("%s?key=%s&q=%s", api_main_url, api_key,
                            req.isbn)
  grl.fetch (url, netopts, fetch_goodreads_book_id_cb)
end

---------------
-- Utilities --
---------------

function fetch_goodreads_book_id_cb (result)
  if not result then
    grl.warning ("We are getting an empty response from server")
    grl.callback ()
    return
  end

  local xml = grl.lua.xml.string_to_table(result)

  if not xml
      or not xml.GoodreadsResponse.Request
      or not xml.GoodreadsResponse.search then
    grl.warning ("Error on transform received data a lua table")
    grl.callback ()
  end

  xml = xml.GoodreadsResponse

  if xml.search.total_results and xml.search.total_results <= 0 then
    grl.warning ("No data found for this query")
    grl.callback ()
    return
  end

  local work = nil
  local book = {}

  if xml.search and xml.search.results and xml.search.results.work then
    work = xml.search.results.work
  end

  if work then
    if work.average_rating then
      book.rating = tonumber(work.average_rating.xml)
    end
    if work.original_publication_year or
        work.original_publication_month or
        work.original_publication_day then
        local year = work.original_publication_year and
          work.original_publication_year.xml or ""
        local month = work.original_publication_month and
          work.original_publication_month.xml or ""
        local day = work.original_publication_year and
          work.original_publication_day.xml or ""

        book.publication_date = string.format("%s-%s-%s", year, month, day)
    end
    if work.best_book.id then
      book.id = tonumber(work.best_book.id.xml)
    end
    if work.best_book.title then
      book.title = work.best_book.title.xml
    end
    if work.best_book.author then
      if work.best_book.author.id then
          book.gr_author_id = tonumber(work.best_book.author.id.xml)
      end
      if work.best_book.author.name then
          book.author = work.best_book.author.name.xml
      end
    end
    if work.best_book.image_url then
      book.thumbnail = work.best_book.image_url.xml
    end
    if work.best_book.small_image_url then
      book.gr_small_image_url = work.best_book.small_image_url.xml
    end
    local url = string.format("%s/%s.xml?key=%s", api_more_info, book.id,
                              api_key)
	grl.fetch (url, netopts, fetch_book_info_cb, book)
  end
end

function fetch_book_info_cb (result, book)
  if not result then
    grl.warning ("We are getting an empty response from server")
    grl.callback ()
    return
  end

  local xml = grl.lua.xml.string_to_table(result)

  grl.warning(grl.lua.inspect(xml))
  if not xml then
    grl.warning ("Error on transform received data a lua table")
    grl.callback ()
  end

  xml = xml.GoodreadsResponse

  if not xml.book then
    grl.warning ("No data found for this query")
    grl.callback ()
    return
  end

  local binfo = xml.book

  if binfo then
    if binfo.country_code then
      book.gr_country_code = binfo.country_code.xml
    end
    if binfo.publisher then
      book.gr_publisher = binfo.publisher.xml
    end
    if binfo.description then
      book.description = binfo.description.xml
    end
    if binfo.num_pages then
      book.gr_pages = tonumber(binfo.num_pages.xml)
    end
    if binfo.url then
      book.url = binfo.url.xml
    end

    grl.callback (book)
  end

  grl.callback ()
end
