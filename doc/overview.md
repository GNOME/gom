Title: Overview

# Overview

Gom is a [GNOME](https://www.gnome.org/) library that provides binding of
`GObjects` to and from a SQLite database.

## pkg-config name

To build a program that uses Gom, you can use the following command to get
the cflags and libraries necessary to compile and link.

```sh
gcc hello.c `pkg-config --cflags --libs gom-1.0` -o hello
```
