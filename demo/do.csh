#!/bin/csh

rm *.tga
rm *.out.png
foreach n (*.jpg)
    #foreach i ('9' '9a' '9c' '9ac' '9b' '9ab' '8' '8a' '8c' '8ac' '8e' '8ae' '8ec' '8aec')
    #foreach i ('9' '9a' '9c' '9ac' '9b' '9ab')
    foreach i ('C' 'Ca' 'Cc' 'Cac' 'Cb' 'Cab' 'CA' 'CAc' 'CAb')
        ../jakoest -$i $n:r.jpg && \
            convert $n:r.out.tga $n:r.$i.out.png && \
            rm $n:r.out.tga
    end
end
