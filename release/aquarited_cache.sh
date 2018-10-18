#!/bin/bash

CACHE_LIVE="/var/cache/aquarite/aquarite.cache"
CACHE_PERSISTENT="/var/cache/aquarite/aquarite.cache.persistant"
LOG="/tmp/aquarited_cache.log"

log()                                                                                                                              
{
  echo `date "+%b %d %Y %H:%M:%S"` - ${0} - $* >> $LOG                                                                             
}

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root"
   log "This script must be run as root" 
   exit 1
fi

if `diff "$CACHE_LIVE" "$CACHE_PERSISTENT" >/dev/null` ; then
  log "Persistent is same as cache" 
  exit 0
fi

if [ -s "$CACHE_LIVE" ]; then
  # We have a live cash, so make it persistent
  #echo "Making persistent"
  log "Run: cp $CACHE_LIVE $CACHE_PERSISTENT"
  mount / -o remount,rw
  cp "$CACHE_LIVE" "$CACHE_PERSISTENT"
  mount / -o remount,ro
else
  # No live cache (mush be after boot), make one if persistent exists
  if [ -s "$CACHE_PERSISTENT" ]; then
    log "Run: cat $CACHE_PERSISTENT >> $CACHE_LIVE"
    mount / -o remount,rw
    cat "$CACHE_PERSISTENT" >> "$CACHE_LIVE"
    mount / -o remount,ro
  else
   log "No files, not doing anything"
  fi
  #echo "Empty cache"
fi