JakOestFilter
=============

Photo filter first developed to be applied to my 2013-14 Austrian Holiday photos

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
jakoest <filter> file1.jpg file2.jpg ...
filters:    -1  original
            -2  mosaic
            -3  mobord
            -4  faith
            -5  rgfilter
            -r  random filter
```

The output files will be named `file1.out.jpg`, `file2.out.jpg` etc.

Changelog
=========

v1.4.0
------

* Switch to OpenMP for multithreading
* Add win32 target using mingw
* Improve faith and rgfilter filters
* Added random filter option

v1.3.4
------

Faith filter now searches for least represented colours.

Added a new rgfilter inspired by the earliest 2-colour Technicolor technology.


v1.3.3
------

Overhaul the faith filter. Fix problems (colours were coming out of left field in some scenarios) and tweak parameters to fulfil my vision.

v1.3.2
------

Added a filter than renders the photo in a bichrome color space and dodges the photo a lot.

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
