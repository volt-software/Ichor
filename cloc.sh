#!/bin/bash
cloc src/ include/ test/ examples/ benchmarks/ --exclude-dir=tl,sole,backward,gsm_enc,base64,ctre --exclude-content=LYRA_LYRA_HPP --by-file
echo ""
cloc src/ include/ test/ examples/ benchmarks/ --exclude-dir=tl,sole,backward,gsm_enc,base64,ctre --exclude-content=LYRA_LYRA_HPP
