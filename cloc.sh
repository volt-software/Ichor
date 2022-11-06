#!/bin/bash
cloc src/ include/ test/ examples/ benchmarks/ --exclude-dir=etcd,etcd_example,tl,sole
echo ""
cloc src/ include/ test/ examples/ benchmarks/ --exclude-dir=etcd,etcd_example,tl,sole --by-file
