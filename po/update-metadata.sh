#!/bin/sh

BASEDIR=".." # root of translatable sources
PROJECT="plasma_applet_org.kde.store.nowdock.panel" # project name
PROJECTPATH="../nowdockpanel" # project path
BUGADDR="https://github.com/psifidotos/nowdock-panel/" # MSGID-Bugs
DEFAULTLAYOUT="../layout-templates/org.kde.store.nowdock.defaultPanel"
EMPTYLAYOUT="../layout-templates/org.kde.store.nowdock.emptyPanel"
WDIR="`pwd`" # working dir

intltool-merge --quiet --desktop-style . ../metadata.desktop.template "${PROJECTPATH}"/metadata.desktop
intltool-merge --quiet --desktop-style . ../defaultLayout.metadata.desktop.template "${DEFAULTLAYOUT}"/metadata.desktop
intltool-merge --quiet --desktop-style . ../emptyLayout.metadata.desktop.template "${EMPTYLAYOUT}"/metadata.desktop

echo "metadata.desktop files were updated..."
