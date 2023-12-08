# Etcd Example

This example showcases Ichor's Etcd v2 API implementation. This example relies on boost.BEAST, but any HttpClient implementation will allow the Etcd interface to work.

Etcd since v3.4 has disabled the v2 API by default, to enable it, use `--enable-v2=true` when starting the server, or add `enable-v2: true` in the etcd server config file.
