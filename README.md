# grilo-plugins
Thanks for using Grilo!

## What is Grilo?
Grilo is a framework for browsing and searching media content from various
sources using a single API.

## What is grilo-plugins?
It is a collection of plugins for Grilo implementing Grilo's API for various
multimedia content providers.

## Where can I find more?
We have a wiki page at: <https://wiki.gnome.org/Projects/Grilo>

You can discuss in the support forum: <https://discourse.gnome.org/c/platform/5>

You can join us on the IRC: #grilo on GIMPNet

## How do I start?
If you are asking this then you should probably be reading Grilo's README
file first :)

## Building from git
Make sure you have Grilo installed first! If you do then:

```bash
$ git clone https://gitlab.gnome.org/GNOME/grilo-plugins.git
$ cd grilo-plugins
$ meson . build
$ ninja -C build
$ sudo ninja -C build install
```

## License

grilo-plugins is available under the [GNU Lesser General Public License v2.1 or later](https://spdx.org/licenses/LGPL-2.1-or-later.html).
Check the sources themselves for individual copyrights and licenses.

Enjoy!
