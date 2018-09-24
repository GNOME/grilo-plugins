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
meson build
ninja -C build
sudo ninja -C build install
```

Enjoy!
