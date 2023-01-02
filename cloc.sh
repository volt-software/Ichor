#!/bin/bash
cloc src/ include/ test/ examples/ benchmarks/ --exclude-dir=etcd,etcd_example,tl,sole,backward --exclude-content=LYRA_LYRA_HPP
echo ""
cloc src/ include/ test/ examples/ benchmarks/ --exclude-dir=etcd,etcd_example,tl,sole,backward --exclude-content=LYRA_LYRA_HPP --by-file
