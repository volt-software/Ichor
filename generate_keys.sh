#!/bin/bash

openssl req -nodes -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 99999
