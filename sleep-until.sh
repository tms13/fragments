#!/bin/sh

# Sleep until the specified time

# Accepts any date/time format accepted by 'date'

# Assumes GNU date and GNU sleep

die() {
    echo "$@" >&2
    exit 1
}

usage() {
    echo "Usage: $0 TIME"
}


test $# = 1 || die $(usage)

case "$1" in
    --version)
        echo "sleep_until version 1.0"
        exit 0
        ;;
    --help)
        usage
        exit 0
        ;;
    -*)
        die "Unrecognised option: $1"
        ;;
    *)
        end=$(date -d "$1" +%s.%N)
        now=$(date +%s.%N)
        test ${end%.*} -gt ${now%.*} || die "$1 is in the past!"
        exec sleep $(echo $end $now - p | dc )
        ;;
esac
