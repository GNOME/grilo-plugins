#!/bin/sh

unset GRL_PLUGIN_PATH

plugin_dir=src

echo "Scanning directory '$plugin_dir' for plugins...."
PLUGINS=`find $plugin_dir -type d | grep .libs | sort -r`
for p in $PLUGINS; do
  pname=`expr $p : '.*/\(.*\)/.*'`
  echo "  Found plugin:" $pname
  GRL_PLUGIN_PATH=$GRL_PLUGIN_PATH:$PWD/$p
done

export GRL_PLUGIN_PATH

echo ""
echo "Include this script as source for your current shell or just copy and paste"
echo "the followind environment variable definition:"
echo ""

echo "------- 8< ------ 8< ------- 8< -------- 8< ------- 8< ------- 8< ------- 8< -------"
echo export GRL_PLUGIN_PATH=$GRL_PLUGIN_PATH
echo "------- 8< ------ 8< ------- 8< -------- 8< ------- 8< ------- 8< ------- 8< -------"
