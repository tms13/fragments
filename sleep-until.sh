#!/bin/sh

# Assumes GNU date and GNU sleep

set -eu

die() {
    echo "$@" >&2
    exit 1
}

usage() {
    cat <<EOF
Usage:  $0 [-v|--verbose] TIME
or  $0 --version

Sleep until the specified time.

    TIME can be any date/time as accepted by date(1).

Examples:
    $0 3:14
    $0 'next Tuesday'
EOF
}

verbose=false

while [ "${1+y}" ]
do
    case "$1" in
        --version)
            echo "sleep_until version 1.1"
            exit 0
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -v|--verbose)
            verbose=true
            shift
            ;;
        -*)
            die "Unrecognised option: $1"
            ;;
        *)
            test \! "${2+y}" || die "Extra arguments after time"
            end=$(date -d "$1" +%s.%N)
            now=$(date +%s.%N)
            duration=$(dc -e "$end $now -p")
            case "$duration" in
                -*) die "$1 is in the past!";;
            esac
            if $verbose
            then
                printf 'Sleeping for %g seconds until ' $duration
                date -d "$1"
            fi
            exec sleep $duration
            ;;
    esac
done

# If we reach here, we didn't get any non-option argument
die "No time specified"
