JakOestFilter
=============

Photo filter applied to my 2013-14 Austrian Holiday photos

This project lives on http://github.com/alzwded/JakOestFilter/

This project depends on http://github.com/alzwded/JakWorkers/

The dependency is handled by the makefile (on linux) by cloning the repo using `git` in the `../` directory. If you have obtained this project as a tarball, then you need to get JakWorkers manually and either copy in the `JakWorkers.h` header and `libjw.so` library here (and maybe even disable the make rule) or install git and get an internet connection.

How it works
============

It applies the following transformations:
1. it grabs the central square
1. it downsamples the photo to 800x800
1. it applies some recolouring rules which amplifies the relative differences between the red, green and blue components (makes warm colours more proeminent)
1. it frames the photo with a round frame thing

Conclusion
==========

I like it, I've learnt some stuff.

TODO
====

* Upsampling to make stuff look more cartoony.
* raise sharpness / contrast (as a combo, I know what I mean by this)

Usage
=====

```sh
jakoest file1.jpg file2.jpg ...
```

The output files will be named `file1.out.jpg`, `file2.out.jpg` etc.

Changelog
=========

v1.3.1
------

Added a filter that actually looks a bit like a mosaic -- mobord.

v1.3
----

Added mosaic filter.

v1.2
----

Fix (I hope) buggy rule application where certain colours would burn out. It seems to be solved now, but it might need more testing.

v1.1
----

Less buggy, now actually runs

v1.0
----

Initial buggy release
