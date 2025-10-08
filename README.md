[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://gitlab.xfce.org/apps/orage/-/blob/master/COPYING)

# Orage

Orage is a time-managing application for the [Xfce](https://www.xfce.org) desktop environment.

Orage aims to be a fast and easy to use graphical calendar. It uses portable ical format and
includes common calendar features like repeating appointments and multiple alarming possibilities.
Orage does not have group calendar features, but can only be used for single user. It takes a list
of files for ical files that should be imported. Contents of those files are read and converted into
Orage, but those files are left untouched.

----

### Homepage

[Orage documentation](https://docs.xfce.org/apps/orage/start)

### Changelog

See [NEWS](https://gitlab.xfce.org/apps/orage/-/blob/master/NEWS) for details on changes and fixes made in the current release.

### Required Packages

Orage depends on the following packages:

* [GLib](https://wiki.gnome.org/Projects/GLib) >= 2.74.0
* [GTK](https://www.gtk.org) >= 3.24.0
* [Libical](https://github.com/libical/libical) >= 3.0.0

### Optional Packages

Orage optionally depends on the following packages:

* [Libnotify](https://gitlab.gnome.org/GNOME/libnotify) >= 0.7.0
* For audible reminder any audio command which play wav and ogg files (sox, play, etc...)

### Source Code Repository

[Orage source code](https://gitlab.xfce.org/apps/orage)

### Download a Release Tarball

[Orage archive](https://archive.xfce.org/src/apps/orage)
    or
[Orage tags](https://gitlab.xfce.org/apps/orage/-/tags)

### Installation

From source code repository:

    % cd orage
    % ./autogen.sh
    % make
    % make install

From release tarball:

    % tar xf orage-<version>.tar.bz2
    % cd orage-<version>
    % ./configure
    % make
    % make install

### Reporting Bugs

Visit the [reporting bugs](https://docs.xfce.org/apps/orage/bugs) page to view currently open bug reports and instructions on reporting new bugs or submitting bugfixes.
