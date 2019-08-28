Thanks for using Grilo!

# What is Grilo?

Grilo is a framework for browsing and searching media content from various
sources using a single API.

# What is grilo-plugins?

It is a collection of plugins for Grilo implementing Grilo's API for various
multimedia content providers.

# Where can I find more?

We have a [Wiki page](https://wiki.gnome.org/Projects/Grilo)

You can subscribe to our [maling list](http://mail.gnome.org/mailman/listinfo/grilo-list)

You can join us on the IRC:
#grilo on GIMPNet

# How do I start?

If you are asking this then you should probably be reading Grilo's README
file first :)

# Building from git

Make sure you have [Grilo](https://gitlab.gnome.org/GNOME/grilo) installed first! If you do then:

```
git clone https://gitlab.gnome.org/thiago/grilo-plugins.git
cd grilo-plugins
meson . build
ninja -C build
sudo ninja -C build install
```

# Plugins and Dependence List

## bookmarks
 * gio
 * libxml
 * gom

## chromaprint
 * gstreamer

## dleyna
 * gio
 * gio_unix
 * libsoup

## dmap
 * libdmapsharing
 * libxml

## filesystem
 * grilo_pls

## flickr
 * grilo_net
 * libxml
 * oauth

## freebox
 * grilo_pls
 * avahi_client
 * avahi_glib
 * avahi_gobject

## gravatar
 * nos

## jamendo
* grilo_net
* libxml

## local-metadata
 * gio
 * libmediaart

## lua-factory
 * lua
 * libarchive
 * grilo_net
 * json_glib
 * libxml

## magnatune
 * sqlite3
 * grilo_net

## metadata-store
 * sqlite3

## opensubtitles
 * gio
 * libsoup

## optical-media
 * totem_plparser

## podcasts
 * grilo_net
 * libxml
 * sqlite3
 * totem_plparser

## raitv
 * grilo_net
 * libxml

## shoutcast
 * grilo_net
 * libxml

## thetvdb
 * grilo_net
 * libxml
 * libarchive
 * gom

## tmdb
 * json_glib
 * libsoup
 * grilo_net

## tracker
 * tracker_sparql

## vimeo
 * grilo_net
 * libxml
 * totem_plparser

## youtube
 * grilo_net
 * libxml
 * libgdata
 * totem_plparser

# Dependence Links/Instructions

## Avahi

[avahi_client/avahi_glib/avahi_gobject](https://github.com/lathiat/avahi)

Service Discovery for Linux using mDNS/DNS-SD -- compatible with Bonjour

* ArchLinux

```
sudo pacman -S avahi
```

## Glib2

[gio/gio_unix/gthread](https://wiki.gnome.org/Projects/GLib0)

Low level core library

* ArchLinux

```
sudo pacman -S glib2
```

## Goa

[goa](https://wiki.gnome.org/Projects/GnomeOnlineAccounts)

Single sign-on framework for GNOME

* ArchLinux

```
sudo pacman -S gnome-online-accounts
```

## Gom

[gom](https://wiki.gnome.org/Projects/Gom)

A GObject to SQLite object mapper

* ArchLinux

```
sudo pacman -S gom
```

## Grilo

[grilo_pls/grilo_net](https://wiki.gnome.org/Projects/Grilo)

Framework that provides access to various sources of multimedia content

* ArchLinux

```
sudo pacman -S grilo
```

## Gstreamer

[gstreamer](https://gstreamer.freedesktop.org/)

GStreamer open-source multimedia framework core library

* ArchLinux

```
sudo pacman -S gstreamer
```

## Json Glib

[json_glib](https://wiki.gnome.org/Projects/JsonGlib)

JSON library built on GLib

* ArchLinux

```
sudo pacman -S json-glib
```

## Lib Archive

[libarchive](http://libarchive.org/)

Multi-format archive and compression library

* ArchLinux

```
sudo pacman -S libarchive
```

## Lib Map Sharing

[libdmapsharing](http://www.flyn.org/projects/libdmapsharing/index.html)

A library that implements the DMAP family of protocols

* ArchLinux

```
sudo pacman -S libdmapsharing
```

## Lib Gdata

[libgdata](https://wiki.gnome.org/Projects/libgdata)

GLib-based library for accessing online service APIs using the GData protocol

* ArchLinux

```
sudo pacman -S libgdata
```

## Lib Media Art

[libmediaart](https://git.gnome.org/browse/libmediaart)

Library tasked with managing, extracting and handling media art caches

* ArchLinux

```
sudo pacman -S libmediaart
```

## Lib Soup

[libsoup](https://wiki.gnome.org/Projects/libsoup)

HTTP client/server library for GNOME

* ArchLinux

```
sudo pacman -S libsoup
```

## LibXML 2.0

[libxml](http://www.xmlsoft.org/)

XML parsing library, version 2

* ArchLinux

```
sudo pacman -S libxml2
```

## Lib Oauth

[liboauth](https://github.com/x42/liboauth)

C library implementing OAuth Core RFC 5849

* ArchLinux

```
sudo pacman -S liboauth
```

## Sqlite

[sqlite3](http://www.sqlite.org/)

A C library that implements an SQL database engine

* ArchLinux

```
sudo pacman -S sqlite
```

## Totem Pl Parser

[totem-plparser](https://gitlab.gnome.org/GNOME/totem-pl-parser)

Simple GObject-based library to parse and save a host of playlist formats

* ArchLinux

```
sudo pacman -S totem-plparser
```

## Tracker

[tracker_sparql](https://wiki.gnome.org/Projects/Tracker)

Desktop-neutral user information store, search tool and indexer

* ArchLinux

```
sudo pacman -S tracker
```

## Lua

[lua](http://www.lua.org/)

Powerful lightweight programming language designed for extending applications

* ArchLinux

```
sudo pacman -S lua
```

Enjoy!
