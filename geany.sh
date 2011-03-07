#!/bin/sh
geany `for file in $(find src inc -type f); do echo ${file##*/} ${file}; done | sort | awk '{print $2}'` &
