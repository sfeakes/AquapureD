#include <stdio.h>
#include <stdlib.h>

#include "ap_config.h"
#include "utils.h"

void write_cache(struct apdata *ar_prms) {
  FILE *fp;

  fp = fopen(ar_prms->cache_file, "w");
  if (fp == NULL) {
    logMessage(LOG_ERR, "Open file failed '%s'\n", ar_prms->cache_file);
    return;
  }

  fprintf(fp, "%d\n", ar_prms->Percent);
  fprintf(fp, "%d\n", ar_prms->PPM);
  fclose(fp);
}

void read_cache(struct apdata *ar_prms) {
  FILE *fp;
  int i;

  fp = fopen(ar_prms->cache_file, "r");
  if (fp == NULL) {
    logMessage(LOG_ERR, "Open file failed '%s'\n", ar_prms->cache_file);
    return;
  }

  fscanf (fp, "%d", &i);
  logMessage(LOG_DEBUG, "Read Percent '%d' from cache\n", i);
  if (i > -1 && i< 102)
    ar_prms->Percent = i;

  fscanf (fp, "%d", &i);
  logMessage(LOG_DEBUG, "Read PPM '%d' from cache\n", i);
  if (i > -1 && i< 5000)
    ar_prms->PPM = i;

  fclose (fp);
}