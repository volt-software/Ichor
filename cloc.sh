#!/bin/bash
cloc src/ include/ test/ examples/ benchmarks/ --exclude-dir=etcd,etcd_example,tl
echo ""
cloc src/ include/ test/ examples/ benchmarks/ --exclude-dir=etcd,etcd_example,tl --by-file
