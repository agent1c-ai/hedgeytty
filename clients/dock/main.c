/* htdock / hedgeytty-dock — composable desktop dock for HedgeyTTY. */
#include "dock.h"

#include <stdlib.h>

int main(void) {
  Dock *d = dock_new();
  int rc;

  if (!d)
    return 1;

  dock_load(d);
  if (!dock_acquire_lock(d)) {
    dock_free(d);
    return 1;
  }

  rc = dock_run(d);
  dock_free(d);
  return rc;
}
