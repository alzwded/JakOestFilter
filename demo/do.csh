#!/bin/csh

rm *.tga
foreach n (*.jpg)
    foreach i ('9' '9a' '9c' '9ac' '9b' '9ab' '8' '8a' '8c' '8ac' '8e' '8ae' '8ec' '8aec')
    #foreach i ('9' '9a' '9c' '9ac')
        ../jakoest -$i $n:r.jpg && mv $n:r.out.tga $n:r.$i.tga
    end
end
