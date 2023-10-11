/*
                Copyright (C) Dialogic Corporation 1999-2011. All Rights Reserved.

 Name:          eir_main.c

 Description:   Console command line interface to mtr.

 Functions:     main()

 -----  ---------   ---------------------------------------------
 Issue    Date                       Changes
 -----  ---------   ---------------------------------------------
   A     10-Mar-99  - Initial code.
   1     16-Feb-00  - Added option to disable trace.
   2     31-Jul-01  - Changed default module ID to be 0x2d.
   3     10-Aug-01  - Added handling for SEND-ROUTING-INFO-FOR-GPRS
                      and SEND-IMSI.
   4    20-Jan-06   - Include reference to Intel Corporation in file header
   5    13-Dec-06   - Change to use of Dialogic Corporation copyright.
   6    30-Sep-09   - Support for USSD and other MAP services.
   7    11-Jan-11   - Addition of dialog termination mode.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "system.h"
#include "ss7_inc.h"
#include "map_inc.h"
#include "strtonum.h"
#include "eir.h"
#include <unistd.h>



extern int mtr_ent(u8 mtr_mod_id,  u8 mtr_map_id, u8 trace, u8 dlg_term_modem, char* directory_name);

#define CLI_EXIT_REQ            (-1)    /* Option requires immediate exit */
#define CLI_UNRECON_OPTION      (-2)    /* Unrecognised option */
#define CLI_RANGE_ERR           (-3)    /* Option value is out of range */

/*
 * Default values for MTR's command line options:
 */
#define DEFAULT_MODULE_ID        (0x2d)
#define DEFAULT_MAP_ID           (MAP_TASK_ID)

static u8  mtr_mod_id;
static u8  mtr_map_id;
static u8  mtr_trace;
static u8  mtr_dlg_term_mode;

static char *program;

/*
 * Main function for MAP Test Utility (MTR):
 */
int main(argc, argv)
  int argc;
  char *argv[];
{
 

  mtr_mod_id = DEFAULT_MODULE_ID;
  mtr_map_id = DEFAULT_MAP_ID;
  mtr_trace = 1;
  printf("EIR Clone from MTR code \n");
  program = argv[0];
  
   char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("gethostname");
        return 1;
    }
  
  printf("Hostname: %s \n", hostname);
  
  char *directory_name  = (char *) malloc(sizeof(char) * 100);
    // Copy the first argument (not including the name of the program) into the string
     if (argc < 2 || argv[1] == NULL) {
        printf("No argument provided\n");
        strcpy(directory_name, "/app/log/");
        strcat(directory_name,hostname);
        printf("Directory name in main function: %s \n",directory_name);
    } else {
        strcpy(directory_name, argv[1]);
    }
    

 
  
    mtr_ent(mtr_mod_id, mtr_map_id, mtr_trace, mtr_dlg_term_mode,directory_name);

  return(0);
}


