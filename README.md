JakOestFilter
=============

Photo filter applied to my 2013-14 Austrian Holiday photos

This project lives on http://github.com/alzwded/JakOestFilter/

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
