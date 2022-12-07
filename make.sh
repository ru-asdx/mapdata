#!/bin/bash

gcc mapdata22.c -g -o mapdata
strip --strip-all ./mapdata
