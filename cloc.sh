#!/bin/bash
echo -e "=== by file ==="
cloc --quiet --hide-rate src/ include/ test/ examples/ benchmarks/ --exclude-dir=tl,sole,backward,gsm_enc,base64,ctre,ankerl --exclude-content=LYRA_LYRA_HPP --by-file
echo -e "=== main ==="
cloc --quiet --hide-rate src/ include/ --exclude-dir=tl,sole,backward,gsm_enc,base64,ctre,ankerl --exclude-content=LYRA_LYRA_HPP
echo -e "=== test ==="
cloc --quiet --hide-rate test/ --exclude-dir=tl,sole,backward,gsm_enc,base64,ctre,ankerl --exclude-content=LYRA_LYRA_HPP
echo -e "=== examples ==="
cloc --quiet --hide-rate examples/ --exclude-dir=tl,sole,backward,gsm_enc,base64,ctre,ankerl --exclude-content=LYRA_LYRA_HPP
echo -e "=== benchmarks ==="
cloc --quiet --hide-rate benchmarks/ --exclude-dir=tl,sole,backward,gsm_enc,base64,ctre,ankerl --exclude-content=LYRA_LYRA_HPP
echo -e "=== total ==="
cloc --quiet --hide-rate src/ include/ test/ examples/ benchmarks/ --exclude-dir=tl,sole,backward,gsm_enc,base64,ctre,ankerl --exclude-content=LYRA_LYRA_HPP
