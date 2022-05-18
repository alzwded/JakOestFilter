JakOestFilter
=============

Photo filter first developed to be applied to my 2013-14 Austrian Holiday photos

This project lives on http://github.com/alzwded/JakOestFilter/

How it works
============

The `original` filter
---------------------

It applies the following transformations:

1. it grabs the central square
1. it downsamples the photo to 800x800
1. it applies some recolouring rules which amplifies the relative differences between the red, green and blue components (makes warm colours more proeminent)
1. it frames the photo with a round frame thing

The `mosaic` and `mobord` filters
---------------------------------

1. drops color information (convert to grayscale)
1. map a section of the grayscale to a colour
1. mobord does something funky with edge detection or something (I forget)
1. ???
1. profit!

The `faith` & `rgfilter` filters
--------------------------------

These are related.

These try to expand the gamut for a part of the HSV cylinder, while decolouring sections of colours outside that section. It forces colour bleed, basically. The image is also normalized and over-exposed in a non-linear way.

The `rgfilter` variant basically does away with the heuristics that detect the "sort-of used sometimes" colours and always uses a color section or Red-Green.

The `cgadither*` filters
------------------------

These filters throw away 99% of colour, saturation and luminance information, and convert your image to what you would see on an old & basic CGA monitor.

By default, they use the Cyan-Magenta-White-Black pallette, but you can turn the blue channel off to get the alternate pallette.

The `cgadither` algorithm is very basic and REALLY throws away most of the HSL information:

```
   No. Step                         Colour Space
   0. in RGB                        (r[], g[], b[])
   1. RGB -> HSL                    (h[], s[], l[])
   2. dither_m_c                    (isMagenta?, s[], l[])
   3. dither_s                      (isMagenta?, isGray?, l[])
   4. dither_l                      (isMagenta?, isGray?, isWhite?)
   5. output                        (r[], g[], b[])
        isGray?
            isWhite?
                out = FFFFFF
            else
                out = 000000
        else
            isMagenta?
                out = FF00FF
            else
                out = 00FFFF

```

The `cgadither2` algorithm is a bit more advanced, bringing in some triangular noise into the equation. I don't know what colour the noise is since I just call `rand()`, but it's noisy.

Anyway, the most interesting part is that it tries to find a sweet spot (of sorts) between using Black and White/Yellow to conserve luminance, and Cyan/Green, Magenta/Red and White/Yellow to conserve colour. Black is there to even things out saturation-wise.

So it goes step by step and computes dithering between Cyan and Magenta, grays and colours, white and black, and overall dithering between black, colour and white. The output layer then has some simple logic to resolve the various numbers that were crunched.

```
   No. Step                         Colour Space
   0. in RGB                        (r[], g[], b[])
   1. RGB -> HSL                    (h[], s[], l[])
   2. dither_m_c_y                  (M|C|Y, s[], l[])
   3. dither_s                      (M|C|Y, isGray?, l[])
   4. dither_l                      (M|C|Y, isGray?, isWhite?)
   5. dither_w_c_b                  (M|C|Y, isGray?, isWhite?, white|color|black)
   6. _output_layer                 (r[], g[], b[])
        isGray?
            isWhite?
                out = FFFFFF
            else
                out = 000000
        else white|color|black
            is white
                out = FFFFFF
            is black 
                out = 000000
            is color
                is M
                    out = FF00FF
                is Y
                    out = FFFFFF
                is C
                    out = 00FFFF
    opt_alt implies the blue channel is 0
```

The original `cgadither` filter is sort-of a snapshot in time on the way to `cgadither2`, but it yields some nice results by itself. `cgadither2` tries to do a better job to conserve reality, the number-less version is more of a toy. (wait, isn't this whole project a toy?)

The `cgafilterfs` works like `cgafilter2`, but uses Floyd-Steinberg dithering instead of random. `cgafilterfs` has yet another alt color pallette ("extended" range) which tries to use the "bright" color as a color, and not just as white. With "narrow" color range, then it only outputs the BW for low saturation and BCM for high saturation. With "extended" it outputs BW for low saturation and BCMW for high saturation.

The `cgafilterfs2` improves on `cgafilterfs` to get more natural images. This works in luma-chroma space.

Conclusion
==========

Conclusion ca. 2014
-------------------

I like it, I've learnt some stuff.

Conclusion ca. 2020
-------------------

I still like it. I continue to learn stuff :-)

TODO
====

* Upsampling to make stuff look more cartoony.
* raise sharpness / contrast (as a combo, I know what I mean by this)
* better command argument parsing
* more flexible pipelines (oof, we're starting to sound like ImageMagick, aren't we?!) to pick the serializer (jpg vs tga), the enable/disable the aspect ratio correcter, downsampler or frame, etc
* ds800 should be make more generic to allow _just_ downsampling, not necessarily making things square
* fix the "alternate pallette" when dithering colors *for cgadither(1)* (fixed for cgadither2) -- RYG are different to MWC, so the dithering needs to be shifted a bit... c.f. a red parrot that gets R&Y dithering, even though red is part of the pallette
* implement png output; TGA has some stupid max file size limits (although oddly enough, I can still open the image in IrfanView as RAW by skipping over the header; or maybe I should add .raw output as well)

Usage
=====

```sh
usage: jakoest32 <filter> pic1.jpg pic2.jpg pic3.jpg ...
    filter may be: -1  (original)
                   -2  (mosaic)
                   -3  (mobord)
                   -4  (faith)
                   -5  (rgfilter)
                   -6  (cgadither)
                   -6a (cgadither with RYGb pallette)
                   -7  (cgadither2)
                   -7a (cgadither2 with RYGb pallette)
                   -8* (cgaditherfs)
                   -9* (cgaditherfs2)
                   -r  (random filter)
```

The output files will be named `file1.out.jpg`, `file2.out.jpg` etc.

For the `cgadither` filters, the output will be a TARGA lossless 32bit file (`*.out.tga`) and the aspect ratio adjustment and downsampling stages will be skipped (the image will keep its original resolution).

Changelog
=========

v1.6.4
------

* change color biases for `cgaditherfs2` one last time
    - options `9` and `9a` will compute the distance to M-C or R-G respectively
    - options `9c` and `9ac` focus more on teal and less on blue
    - options `9b` and `9bc` focus more on green and less on blue
* just run through all 6 of them (or heck, see [demo/do.csh](./demo/do.csh) and just run all cga ditherers and pick the version that looks the pretties)

v1.6.2/1.6.3
------------

* add `cgaditherfs2` that improves upon v1; it works in luma-chroma and produces more natural images.
* available options are alt color palette and alt blue bias
* accidently bumped the version twice

v1.6.1
------

* fix RYGb palette

v1.6.0
------

* Add `cgaditherfs` filter
    + downsamples images to 2-bit CGA using Floyd-Steinberg dithering
    + it has quite some options to hopefully get you a pretty picture:
        - alt pallette (default is white-cyan-magenta-black; the alt one is red-yellow-green-black)
        - extended vs narrow color range
            * this filter works in HSV space
            * by default, if a pixel is desaturated, the output is black or bright (=white or yellow), if a pixel is devalued, the output is black, else it's one of the other two colors
            * with extended range, if it's neither desaturated nor devalued, then the bright color is considered and "in-between" color
        - alternate color bias
            * by default, for WCMb, it takes `M=(r+b)/2` and `g` and dithers between them; for RYGb, it takes `r` and `C=(g+b)/2` and dithers between them
            * with the alternate color bias, for WCMb, it takes `Y=(r+g)/2` and `b`, and finally and respectively for RYGb, it uses `M=(r+b)/2` and `g`
        + all these options help you tweak the dithering to get you something nice looking, because there are situations where you get some real duds

v1.5.1
------

* improve `cgadither2` by fixing pallette approximation and shadow detection  in `dither_s`

v1.5.0
------

* Add `cgadither` and `cgadither2` filters
    + `cgadither` is a very basic CGA color downsampling & dithering tool which produces pretty "eah" results
    + `cgadither2` is a bit more involved and uses triangle noise to somehow reduce the color resolution by 99% to your basic 4 CGA colors
    + by default, uses the Cyan-Magenta pallette. If the command line option has an `a` appended, it will use the alternate RYG pallette (which makes your eyes bleed)
* Update win32 Makefile to work with whatever the current mingw32 expects; the build is back to 32bits but I can't be bothered to figure MinGW out all over again
* Added TGA output utility (hard-coded to a 32bit lossless image with one plane)

v1.4.0
------

* Switch to OpenMP for multithreading
* Add win32 target using mingw
* Improve faith and rgfilter filters
* Added random filter option

**Errata to the 1.4.0 release's README.md**: no, it does not actually depend on JakWorkers anymore.

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
