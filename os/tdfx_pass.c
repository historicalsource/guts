#include <stdio.h>
#include <stdlib.h>

#include <3dfx.h>
#include <glide.h>
#include <sst1init.h>

/* This should be exported, but isn't yet */
#define SST1INIT_MAX_BOARDS 16

int main (int argc[], char* argv[])
{
  FxU32* sst[SST1INIT_MAX_BOARDS];
  int num_sst;
  int i;

  /* Map all the boards in the system */
  num_sst = 0;
  do {
    sst[num_sst] = sst1InitMapBoard (num_sst);
  } while (sst[num_sst++] != NULL);

  /* Shut them all down */
  for (i = 0; i < num_sst; i += 1)
    sst1InitVgaPassCtrl(sst[i], 1);

  return 0;
}
