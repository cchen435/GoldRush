#include "papi.h"
#include <stdio.h>

int main()
{
int EventSet = PAPI_NULL;
int native = 0x0;
char error_str[PAPI_MAX_STR_LEN];
        
/* Initialize the PAPI library */
int retval = PAPI_library_init(PAPI_VER_CURRENT);
 
if (retval != PAPI_VER_CURRENT ) {
  fprintf(stderr,"PAPI library version mismatch! %d\n", retval);
  exit(1);
}

if ((retval = PAPI_create_eventset(&EventSet)) != PAPI_OK) {
     fprintf(stderr, "PAPI error %d: %s\n",retval, 		PAPI_strerror(retval));
     exit(1);
}     

return 0;
}
