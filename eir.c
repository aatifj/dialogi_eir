/*
                Copyright (C) Vodafone Ghana. All Rights Reserved.

 Name:          eir.c

 Description:   EIR Application written by Aatif Jamal CTO VF Ghana
                Simple responder for Check_IMEI message
                This program responds to an incoming dialogue received
                from the MAP module.

                The program receives:
                        MAP-OPEN-IND
                        service indication
                        MAP-DELIMITER-IND

                and it responds with:
                        MAP-OPEN-RSP
                        service response
                        MAP-CLOSE-REQ (or MAP-DELIMITER-REQ)


 Functions:     main

 -----  ---------   ---------------------------------------------
 Issue    Date                       Changes
 -----  ---------   ---------------------------------------------

 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "system.h"
#include "msg.h"
#include "sysgct.h"
#include "map_inc.h"
#include "eir.h"
#include "pack.h"
#include <unistd.h>

/*
 * Prototypes for local functions:
 */
char logline[200];
int MTR_process_map_msg(MSG *m, int found);
int MTR_cfg(u8 _mtr_mod_id, u8 _map_mod_id, u8 _trace_mod_id, u8 _dlg_term_mode);
int MTR_set_default_term_mode(u8 new_term_mode);
int mtr_ent(u8 mtr_id, u8 map_id, u8 trace, u8 dlg_term_mode, char *directory_name);
static int init_resources(void);
void log_to_file(const char *str); // added by Aatif
FILE *eirlogfile;                  // added by aatif
static int counter = 0;            // added by aatif
char directory_n[30];

static dlg_info *get_dialogue_info(u16 dlg_id);
static int MTR_trace_msg(char *prefix, MSG *m);
static int MTR_send_msg(u16 instance, MSG *m);
static int MTR_send_OpenResponse(u16 mtr_map_inst, u16 dlg_id, u8 result);
static int MTR_ForwardSMResponse(u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);

// Added by Aatif

static int MTR_Send_CheckIMEIResponse(u16 mtr_map_inst, u16 dlg_id, u8 invoke_id, int found);
char **loadIMEIBlockList();
int searchIMEIInBlockList(char **imeiblocklist, char imei[20]);
void freeIMEIBlockList(char **imeiblocklist, int size);

// End of Add by Aatif
static int MTR_send_MapClose(u16 mtr_map_inst, u16 dlg_id, u8 method);
static int MTR_send_Abort(u16 mtr_map_inst, u16 dlg_id, u8 reason);
static int MTR_send_Delimit(u16 mtr_map_inst, u16 dlg_id);
static int MTR_get_invoke_id(MSG *m);
static int MTR_get_applic_context(MSG *m, u8 *dst, u16 dstlen);
static int MTR_get_parameter(MSG *m, u16 pname, u8 **found_pptr);
static u16 MTR_get_primitive_type(MSG *m);


int found = 0;
/*
 * Static data:
 */
static dlg_info dialogue_info[MAX_NUM_DLGS]; /* Dialog_info */
static u8 mtr_mod_id;                        /* Module id of this task */
static u8 mtr_map_id;                        /* Module id for all MAP requests */
static u8 mtr_trace;                         /* Controls trace requirements */
static u8 mtr_default_dlg_term_mode;         /* Controls which end terminates a dialog */


#define MTR_ATI_RSP_SIZE (8)
#define MTR_ATI_RSP_NUM_OF_RSP (8)

/*
 * mtr_ent
 *
 * Waits in a continuous loop responding to any received
 * forward SM request with a forward SM response.
 *
 * Never returns.
 */
int mtr_ent(mtr_id, map_id, trace, dlg_term_mode, directory_name)
u8 mtr_id;        /* Module id for this task */
u8 map_id;        /* Module ID for MAP */
u8 trace;         /* Trace requirements */
u8 dlg_term_mode; /* Default termination mode */
char *directory_name;
{
  printf("Directory name selected as %s \n", directory_name);
  strcpy(directory_n, directory_name);
  HDR *h; /* received message */
  MSG *m; /* received message */
  // Added by Aatif for logging to file
  char line[100];                // line of text for filename
  int num = 0;                   // unique number
  time_t t = time(NULL);         // current time
  struct tm tm = *localtime(&t); // broken down time
  struct tm *time_info;
  char date_time_string[20];
  u16 mlen; /* length of received message */
  u8 *pptr; /* pointer to parameter area */
  char pbuffer[150] = {'\0'};
  char match[7] = "500e01";
  sprintf(line, "%s/imeilog/CHKIMEI-%d-%d-%d_%d-%d-%d_%d.txt", directory_name, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, num);
  FILE *f = fopen(line, "w");

  // added for eir logging
  char file_name[100];
  sprintf(line, "%s/eirlog/EIRBIN-%d-%d-%d_%d-%d-%d_%d.txt", directory_name, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, num);

  printf("before first call to eirlogfile fopen \n");
  eirlogfile = fopen(file_name, "w");
  printf("After first call to eirlogfile fopen \n");

  // end of added for eir logging

  // End of Added by Aatif for logging to file

  MTR_cfg(mtr_id, map_id, trace, dlg_term_mode);

  /*
   * Print banner so we know what's running.
   */
  printf("Starting EIR ------ \n");
  memset(logline, '\0', sizeof(logline));
  sprintf(logline, "MTR MAP Test Responder Modified by Aatif Jamal (C) Dialogic Corporation 1999-2015. All Rights Reserved.\n");
  printf("%s", logline);
  log_to_file(logline);

  memset(logline, '\0', sizeof(logline));
  sprintf(logline, "===============================================================================\n\n");
  printf("%s", logline);
  log_to_file(logline);

  memset(logline, '\0', sizeof(logline));
  sprintf(logline, "MTR mod ID - 0x%02x; MAP module Id 0x%x; Termination Mode 0x%x\n", mtr_mod_id, mtr_map_id, dlg_term_mode);
  log_to_file(logline);

  if (mtr_trace == 0)
    memset(logline, '\0', sizeof(logline));
  sprintf(logline, " Tracing disabled.\n\n");
  log_to_file(logline);

  /*
   * Now enter main loop, receiving messages as they
   * become available and processing accordingly.
   */

  int programlock = 0;
  printf("Loading blocked IMEIs list \n ");
  char **imeiblocklist = loadIMEIBlockList();
  if (imeiblocklist != NULL)
  {
    printf("Successfully loaded IMEI Block List:\n");
    int index_i;
    for (index_i = 0; imeiblocklist[index_i] != NULL; index_i++)
    {
      printf("%d ",index_i);
      printf("%s\n", imeiblocklist[index_i]);
    }
 printf("\n After loading IMEI blocklist \n");
    // freeIMEIBlockList(imeiblocklist, imeiblocklistSize);
  }


  while (1)
  {
    /*
     * GCT_receive will attempt to receive messages
     * from the task's message queue and block until
     * a message is ready.
     */
    
    memset(logline, '\0', sizeof(logline));
    sprintf(logline, "********************Entering while loop************ \n");
    log_to_file(logline);
    if ((h = GCT_receive(mtr_mod_id)) != 0)
    {
      programlock = 10;
      m = (MSG *)h;
      memset(logline, '\0', sizeof(logline));
      sprintf(logline, "Received a message from module with length of %d \n", m->len);
      log_to_file(logline);

      if (m != NULL)
      {

        MTR_trace_msg("MTR Rx:", m);
        switch (m->hdr.type)
        {
        case MAP_MSG_DLG_IND:
        case MAP_MSG_SRV_IND:
          // Added by Aatif for logging

          if ((mlen = m->len) > 0)
          {
            if (mlen > MAX_PARAM_LEN)
              mlen = MAX_PARAM_LEN;

            pptr = get_param(m);
            int i = 0;
            while (mlen--)
            {
              pbuffer[i++] = BIN2CH(*pptr / 16);
              pbuffer[i++] = BIN2CH(*pptr % 16);

              pptr++;
            }
            pbuffer[i] = '\n';
          }

          char msisdn[15] = {'\0'};
          char imei[20] = {'\0'};
          char imsi[20] = {'\0'};
          int k = 0;
          char temp;
          if (strncmp(pbuffer, match, 6) == 0)
          {
            time(&t);
            time_info = localtime(&t);
            strftime(date_time_string, sizeof(date_time_string), "%Y.%m.%d.%H.%M.%S", time_info);
            /// pbuffer size comparisons start here
            if (strlen(pbuffer) == 83)
            {
              memcpy(imei, pbuffer + 12, 16);
              for (k = 0; k < 15; k += 2)
              {
                temp = imei[k];
                imei[k] = imei[k + 1];
                imei[k + 1] = temp;
              }
              memcpy(imsi, pbuffer + 36, 16);
              for (k = 0; k < 16; k += 2)
              {
                temp = imsi[k];
                imsi[k] = imsi[k + 1];
                imsi[k + 1] = temp;
              }

              memcpy(msisdn, pbuffer + 56, 14);
              for (k = 0; k < 14; k += 2)
              {
                temp = msisdn[k];
                msisdn[k] = msisdn[k + 1];
                msisdn[k + 1] = temp;
              }
              imei[15] = '\0';
              snprintf(pbuffer, sizeof(pbuffer), "82c:%s,%s,%s,%s\n", date_time_string, imei, imsi, msisdn);
              fprintf(f, pbuffer);
            }

            else if (strlen(pbuffer) == 81)
            {
              memcpy(imei, pbuffer + 12, 16);
              for (k = 0; k < 15; k += 2)
              {
                temp = imei[k];
                imei[k] = imei[k + 1];
                imei[k + 1] = temp;
              }
              memcpy(imsi, pbuffer + 36, 16);
              for (k = 0; k < 16; k += 2)
              {
                temp = imsi[k];
                imsi[k] = imsi[k + 1];
                imsi[k + 1] = temp;
              }

              memcpy(msisdn, pbuffer + 56, 14);
              for (k = 0; k < 14; k += 2)
              {
                temp = msisdn[k];
                msisdn[k] = msisdn[k + 1];
                msisdn[k + 1] = temp;
              }
              imei[15] = '\0';
              snprintf(pbuffer, sizeof(pbuffer), "80c:%s,%s,%s,%s\n", date_time_string, imei, imsi, msisdn);
              fprintf(f, pbuffer);
            }

            else if (strlen(pbuffer) == 79)
            {
              memcpy(imei, pbuffer + 12, 16);
              for (k = 0; k < 15; k += 2)
              {
                temp = imei[k];
                imei[k] = imei[k + 1];
                imei[k + 1] = temp;
              }
              memcpy(imsi, pbuffer + 36, 16);
              for (k = 0; k < 16; k += 2)
              {
                temp = imsi[k];
                imsi[k] = imsi[k + 1];
                imsi[k + 1] = temp;
              }
              memcpy(msisdn, pbuffer + 56, 12);
              for (k = 0; k < 12; k += 2)
              {
                temp = msisdn[k];
                msisdn[k] = msisdn[k + 1];
                msisdn[k + 1] = temp;
              }
              imei[15] = '\0';
              snprintf(pbuffer, sizeof(pbuffer), "79c:%s,%s,%s,%s\n", date_time_string, imei, imsi, msisdn);
              fprintf(f, pbuffer);
            }

            else if (strlen(pbuffer) == 77)
            {
              memcpy(imei, pbuffer + 18, 16);
              for (k = 0; k < 15; k += 2)
              {
                temp = imei[k];
                imei[k] = imei[k + 1];
                imei[k + 1] = temp;
              }
              memcpy(imsi, pbuffer + 42, 16);
              for (k = 0; k < 16; k += 2)
              {
                temp = imsi[k];
                imsi[k] = imsi[k + 1];
                imsi[k + 1] = temp;
              }
              memcpy(msisdn, pbuffer + 62, 12);
              for (k = 0; k < 12; k += 2)
              {
                temp = msisdn[k];
                msisdn[k] = msisdn[k + 1];
                msisdn[k + 1] = temp;
              }
              imei[15] = '\0';
              snprintf(pbuffer, sizeof(pbuffer), "76c:%s,%s,%s,%s\n", date_time_string, imei, imsi, msisdn);
              fprintf(f, pbuffer);
            }

            else if (strlen(pbuffer) == 67)
            {
              memcpy(imei, pbuffer + 12, 16);
              for (k = 0; k < 15; k += 2)
              {
                temp = imei[k];
                imei[k] = imei[k + 1];
                imei[k + 1] = temp;
              }
              memcpy(imsi, pbuffer + 36, 16);
              for (k = 0; k < 16; k += 2)
              {
                temp = imsi[k];
                imsi[k] = imsi[k + 1];
                imsi[k + 1] = temp;
              }
              imei[15] = '\0';
              snprintf(pbuffer, sizeof(pbuffer), "67c:%s,%s,%s,\n", date_time_string, imei, imsi);
              fprintf(f, pbuffer);
            }

            else if (strlen(pbuffer) == 65)
            {
              memcpy(imei, pbuffer + 18, 16);
              for (k = 0; k < 15; k += 2)
              {
                temp = imei[k];
                imei[k] = imei[k + 1];
                imei[k + 1] = temp;
              }
              memcpy(imsi, pbuffer + 42, 16);
              for (k = 0; k < 16; k += 2)
              {
                temp = imsi[k];
                imsi[k] = imsi[k + 1];
                imsi[k + 1] = temp;
              }
              imei[15] = '\0';
              snprintf(pbuffer, sizeof(pbuffer), "65c:%s,%s,%s,\n", date_time_string, imei, imsi);
              fprintf(f, pbuffer);
            }

            else if (strlen(pbuffer) == 61)
            {
              memcpy(imei, pbuffer + 18, 16);
              for (k = 0; k < 15; k += 2)
              {
                temp = imei[k];
                imei[k] = imei[k + 1];
                imei[k + 1] = temp;
              }
              memcpy(imsi, pbuffer + 42, 16);
              for (k = 0; k < 16; k += 2)
              {
                temp = imsi[k];
                imsi[k] = imsi[k + 1];
                imsi[k + 1] = temp;
              }
              snprintf(pbuffer, sizeof(pbuffer), "61c:%s,%s,%s,\n", date_time_string, imei, imsi);
              fprintf(f, pbuffer);
            }

            else if (strlen(pbuffer) == 45)
            {
              memcpy(imei, pbuffer + 12, 16);
              for (k = 0; k < 15; k += 2)
              {
                temp = imei[k];
                imei[k] = imei[k + 1];
                imei[k + 1] = temp;
              }
              imei[15] = '\0';
              snprintf(pbuffer, sizeof(pbuffer), "45c:%s,%s,\n", date_time_string, imei);
              fprintf(f, pbuffer);
            }

            else
            {
              fprintf(f, pbuffer);
              memset(logline, '\0', sizeof(logline));
              sprintf(logline, "Unhandled pbuffer = %s \n", imei);
              log_to_file(logline);
            }
            /// here

            if (imeiblocklist != NULL)
            {
              memset(logline, '\0', sizeof(logline));
              sprintf(logline, "$$$$$$$ IMEI before strcspn = %s \n", imei);
              log_to_file(logline);

              imei[strcspn(imei, "f")] = '\0'; // Remove the newline character at the end
              printf("$$$$$$$ IMEI after strcspn = %s \n", imei);
              memset(logline, '\0', sizeof(logline));
              sprintf(logline, "$$$$$$$ IMEI after strcspn = %s \n", imei);
              log_to_file(logline);

              found = searchIMEIInBlockList(imeiblocklist, imei);
              if (found == 1)
              {
                printf("IMEI found in the blocklist %s.\n", imei);
                memset(logline, '\0', sizeof(logline));
                sprintf(logline, "IMEI found in the blocklist %s.\n", imei);
                log_to_file(logline);
              }
              else
              {
                printf("IMEI not found in the blocklist %s.\n", imei);
                memset(logline, '\0', sizeof(logline));
                sprintf(logline, "IMEI not found in the blocklist %s.\n", imei);
                log_to_file(logline);
              }
            }

            else
            {
              printf("imeiblocklist is null \n");
              memset(logline, '\0', sizeof(logline));
              sprintf(logline, "imeiblocklist is null \n");
              log_to_file(logline);
            }
          }
          memset(pbuffer, '\0', sizeof(pbuffer));
          fseek(f, 0, SEEK_END);
          long size = ftell(f);
          if (size > 5000000)
          { // if file size is greater than 1MB
            fclose(f);
            num++; // increment unique number
            t = time(NULL);
            tm = *localtime(&t); // update current time
            sprintf(line, "%s/imeilog/CHKIMEI-%d-%d-%d_%d-%d-%d_%d.txt", directory_name, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, num);

            f = fopen(line, "w");
          }
          //     else
          //   {
          //   fclose(f);
          //}

          memset(logline, '\0', sizeof(logline));
          sprintf(logline, "found value before MTR_process_map_msg %d .\n", found);
          log_to_file(logline);

          MTR_process_map_msg(m, found);

          // End of Add by Aatif for logging

          break;
        }

        /*
         * Once we have finished processing the message
         * it must be released to the pool of messages.
         */
        relm(h);
      }
    }
    else
    {
      programlock--;
      memset(logline, '\0', sizeof(logline));
      sprintf(logline, "****** NOT RECEIVING ANYTHING = GCT_receive(mtr_mod_id) ==NULL *** \n");
      log_to_file(logline);
      if (programlock < 0)
      {
        memset(logline, '\0', sizeof(logline));
        sprintf(logline, "****** Existing Program *** \n");
        log_to_file(logline);
        exit(0);
      }
    }
  }
  return (0);
}

/*
 * Can be used to configure and initialise mtr
 */
int MTR_cfg(
    u8 _mtr_mod_id,
    u8 _map_mod_id,
    u8 _trace_mod_id,
    u8 _dlg_term_mode)
{
  mtr_mod_id = _mtr_mod_id;
  mtr_map_id = _map_mod_id;
  mtr_trace = _trace_mod_id;
  mtr_default_dlg_term_mode = _dlg_term_mode;

  init_resources();
  return (0);
}

/*
 * Can be used to configure new termination mode
 */
int MTR_set_default_term_mode(
    u8 new_mode)
{
  mtr_default_dlg_term_mode = new_mode;

  return (0);
}

/*
 * Get Dialogue Info
 *
 * Returns pointer to dialogue info or 0 on error.
 */
dlg_info *get_dialogue_info(dlg_id)
u16 dlg_id; /* Dlg ID of the incoming message 0x800a perhaps */
{
  u16 dlg_ref; /* Internal Dlg Ref, 0x000a perhaps */

  if (!(dlg_id & 0x8000))
  {
    if (mtr_trace)
      memset(logline, '\0', sizeof(logline));
    sprintf(logline, "MTR Rx: Bad dialogue id: Outgoing dialogue id, dlg_id == %x\n", dlg_id);
    log_to_file(logline);

    return 0;
  }
  else
  {
    dlg_ref = dlg_id & 0x7FFF;
  }

  if (dlg_ref >= MAX_NUM_DLGS)
  {
    if (mtr_trace)
      memset(logline, '\0', sizeof(logline));
    sprintf(logline, "MTR Rx: Bad dialogue id: Out of range dialogue, dlg_id == %x\n", dlg_id);
    log_to_file(logline);

    return 0;
  }
  return &dialogue_info[dlg_ref];
}

/*
 * MTR_process_map_msg
 *
 * Processes a received MAP primitive message.
 *
 * Always returns zero.
 */
int MTR_process_map_msg(m, found)
MSG *m; /* Received message */
int found;
{
  u16 dlg_id = 0;     /* Dialogue id */
  u16 ptype = 0;      /* Parameter Type */
  u8 *pptr;           /* Parameter Pointer */
  dlg_info *dlg_info; /* State info for dialogue */
  u8 send_abort = 0;  /* Set if abort to be generated */
  int invoke_id = 0;  /* Invoke id of received srv req */
  int length = 0;

  dlg_id = m->hdr.id;
  memset(logline, '\0', sizeof(logline));
  sprintf(logline, "****process_map_msg function: Received Dialogue ID number  = %d and found =%d \n", dlg_id,found);
  log_to_file(logline);

  pptr = get_param(m);
  ptype = *pptr;

  /*
   * Get state information associated with this dialogue
   */
  dlg_info = get_dialogue_info(dlg_id);

  if (dlg_info == 0)
    return 0;

  switch (dlg_info->state)
  {
  case MTR_S_NULL:
    /*
     * Null state.
     */
    switch (m->hdr.type)
    {
    case MAP_MSG_DLG_IND:
      ptype = *pptr;

      switch (ptype)
      {
      case MAPDT_OPEN_IND:
        /*
         * Open indication indicates that a request to open a new
         * dialogue has been received
         */
        if (mtr_trace)
          memset(logline, '\0', sizeof(logline));
        sprintf(logline, "MTR Rx: Received Open Indication\n");
        log_to_file(logline);

        /*
         * Save application context and MAP instance
         * We don't do actually do anything further with it though.
         */
        dlg_info->map_inst = (u16)GCT_get_instance((HDR *)m);

        /*
         * Set the termination mode based on the current default
         */
        dlg_info->term_mode = mtr_default_dlg_term_mode;
        dlg_info->mms = 0;
        dlg_info->ac_len = 0;

        length = MTR_get_applic_context(m, dlg_info->app_context, MTR_MAX_AC_LEN);
        if (length > 0)
          dlg_info->ac_len = (u8)length;

        if (dlg_info->ac_len != 0)
        {
          /*
           * Respond to the OPEN_IND with OPEN_RSP and wait for the
           * service indication
           */
          MTR_send_OpenResponse(dlg_info->map_inst, dlg_id, MAPRS_DLG_ACC);
          dlg_info->state = MTR_S_WAIT_FOR_SRV_PRIM;
        }
        else
        {
          /*
           * We do not have a proper Application Context - abort
           * the dialogue
           */
          send_abort = 1;
        }
        break;

      default:
        /*
         * Unexpected event - Abort the dialogue.
         */
        send_abort = 1;
        break;
      }
      break;

    default:
      /*
       * Unexpected event - Abort the dialogue.
       */
      send_abort = 1;
      break;
    }
    break;

  case MTR_S_WAIT_FOR_SRV_PRIM:
    /*
     * Waiting for service primitive
     */
    switch (m->hdr.type)
    {
    case MAP_MSG_SRV_IND:
      /*
       * Service primitive indication
       */
      ptype = MTR_get_primitive_type(m);

      switch (ptype)
      {

      case MAPST_CHECK_IMEI_IND: // Added on 13th

        if (mtr_trace)
        {
          switch (ptype)
          {

          case MAPST_CHECK_IMEI_IND:
            memset(logline, '\0', sizeof(logline));
            sprintf(logline, "MTR Rx: Received Check IMEI Indication\n");
            log_to_file(logline);

            break;

          default:
            send_abort = 1;
            break;
          }
        }

        /*
         * Recover invoke id. The invoke id is used
         * when sending the Forward short message response.
         */
        invoke_id = MTR_get_invoke_id(m);

        /*
         * If recovery of the invoke id succeeded, save invoke id and
         * primitive type and change state to wait for the delimiter.
         */
        if (invoke_id != -1)
        {
          dlg_info->invoke_id = (u8)invoke_id;
          dlg_info->ptype = ptype;
          dlg_info->mms = 0;

          dlg_info->state = MTR_S_WAIT_DELIMITER;
          break;
        }
        else
        {
          memset(logline, '\0', sizeof(logline));
          sprintf(logline, "MTR RX: No invoke ID included in the message\n");
          log_to_file(logline);
        }
        break;

      default:
        send_abort = 1;
        break;
      }
      break;

    case MAP_MSG_DLG_IND:
      /*
       * Dialogue indication - we were not expecting this!
       */
      ptype = *pptr;

      switch (ptype)
      {
      case MAPDT_NOTICE_IND:
        /*
         * MAP-NOTICE-IND indicates some kind of error. Close the
         * dialogue and idle the state machine.
         */
        if (mtr_trace)
          memset(logline, '\0', sizeof(logline));
        sprintf(logline, "MTR Rx: Received Notice Indication\n");
        log_to_file(logline);

        /*
         * Now send Map Close and go to idle state.
         */
        MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);
        dlg_info->state = MTR_S_NULL;
        send_abort = 0;
        break;

      case MAPDT_CLOSE_IND:
        /*
         * Close indication received.
         */
        if (mtr_trace)
          memset(logline, '\0', sizeof(logline));
        sprintf(logline, "MTR Rx: Received Close Indication\n");
        log_to_file(logline);

        dlg_info->state = MTR_S_NULL;
        send_abort = 0;
        break;

      default:
        /*
         * Unexpected event - Abort the dialogue.
         */
        send_abort = 1;
        break;
      }
      break;

    default:
      /*
       * Unexpected event - Abort the dialogue.
       */
      send_abort = 1;
      break;
    }
    break;

  case MTR_S_WAIT_DELIMITER:
    /*
     * Wait for delimiter.
     */
    switch (m->hdr.type)
    {
    case MAP_MSG_DLG_IND:
      ptype = *pptr;

      switch (ptype)
      {
      case MAPDT_DELIMITER_IND:
        /*
         * Delimiter indication received. Now send the appropriate
         * response depending on the service primitive that was received.
         */
        if (mtr_trace)
          memset(logline, '\0', sizeof(logline));
        sprintf(logline, "MTR Rx: Received delimiter Indication\n");
        log_to_file(logline);

        switch (dlg_info->ptype)
        {

        case MAPST_CHECK_IMEI_IND:

          MTR_Send_CheckIMEIResponse(dlg_info->map_inst, dlg_id, dlg_info->invoke_id, found);
          MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);
          dlg_info->state = MTR_S_NULL;
          send_abort = 0;
          break;

          // End of Add by Aatif
        }
        break;

      default:
        /*
         * Unexpected event - Abort the dialogue
         */
        send_abort = 1;
        break;
      }
      break;

    default:
      /*
       * Unexpected event - Abort the dialogue
       */
      send_abort = 1;
      break;
    }
    break;
  }
  /*
   * If an error or unexpected event has been encountered, send abort and
   * return to the idle state.
   */
  if (send_abort)
  {
    MTR_send_Abort(dlg_info->map_inst, dlg_id, MAPUR_procedure_error);
    dlg_info->state = MTR_S_NULL;
  }
  return (0);
}

/******************************************************************************
 *
 * Functions to send primitive requests to the MAP module
 *
 ******************************************************************************/

/*
 * MTR_send_OpenResponse
 *
 * Sends an open response to MAP
 *
 * Returns zero or -1 on error.
 */
static int MTR_send_OpenResponse(instance, dlg_id, result)
u16 instance; /* Destination instance */
u16 dlg_id;   /* Dialogue id */
u8 result;    /* Result (accepted/rejected) */
{
  MSG *m;             /* Pointer to message to transmit */
  u8 *pptr;           /* Pointer to a parameter */
  dlg_info *dlg_info; /* Pointer to dialogue state information */

  /*
   * Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    memset(logline, '\0', sizeof(logline));
  sprintf(logline, "****MTR_send_OpenResponse: Sending Open Response with dialog ID = %d\n", dlg_id);
  log_to_file(logline);

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_DLG_REQ, dlg_id,
                NO_RESPONSE, (u16)(7 + dlg_info->ac_len))) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;
    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Open response
     * Parameter name   = result_tag
     * Parameter length = 1
     * Parameter value  = result
     * Parameter name   = applic_context_tag
     * parameter length = len
     * parameter data   = applic_context
     * EOC_tag
     */
    pptr = get_param(m);
    pptr[0] = MAPDT_OPEN_RSP;
    pptr[1] = MAPPN_result;
    pptr[2] = 0x01;
    pptr[3] = result;
    pptr[4] = MAPPN_applic_context;
    pptr[5] = (u8)dlg_info->ac_len;
    memcpy((void *)(pptr + 6), (void *)dlg_info->app_context, dlg_info->ac_len);
    pptr[6 + dlg_info->ac_len] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return (0);
}

// Added by Aatif Check IMEI Response

static int MTR_Send_CheckIMEIResponse(instance, dlg_id, invoke_id, found)
u16 instance; /* Destination instance */
u16 dlg_id;   /* Dialogue id */
u8 invoke_id; /* Invoke_id */
{
  MSG *m;             /* Pointer to message to transmit */
  u8 *pptr;           /* Pointer to a parameter */
  dlg_info *dlg_info; /* Pointer to dialogue state information */

  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    memset(logline, '\0', sizeof(logline));
  sprintf(logline, "****MTR_Send_CheckIMEIResponse Tx: Sending check IMEI Response with dialog ID= %d and found value  =%d \n", dlg_id, found);
  log_to_file(logline);

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 8)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = MAPST_CHECK_IMEI_RSP
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_CHECK_IMEI_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = MAPPN_equipment_status;
    pptr[5] = 0x01;
    if (found ==1)
    {
      pptr[6] = 0x01; //blacklist
    
    }
    else
    {
      pptr[6] = 0x00; //whitelist
    }
    pptr[7] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return (0);
}

// End of Check IMEI Response
/*
 * MTR_send_MapClose
 *
 * Sends a Close message to MAP.
 *
 * Always returns zero.
 */
static int MTR_send_MapClose(instance, dlg_id, method)
u16 instance; /* Destination instance */
u16 dlg_id;   /* Dialogue id */
u8 method;    /* Release method */
{
  MSG *m;             /* Pointer to message to transmit */
  u8 *pptr;           /* Pointer to a parameter */
  dlg_info *dlg_info; /* Pointer to dialogue state information */

  /*
   * Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    memset(logline, '\0', sizeof(logline));
  sprintf(logline, "****MTR_send_MapClose Tx: Sending Close Request-  Dlg ID = %d\n\r", dlg_id);
  log_to_file(logline);

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_DLG_REQ, dlg_id, NO_RESPONSE, 5)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Close Request
     * Parameter name   = release method tag
     * Parameter length = 1
     * Parameter value  = release method
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPDT_CLOSE_REQ;
    pptr[1] = MAPPN_release_method;
    pptr[2] = 0x01;
    pptr[3] = method;
    pptr[4] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(dlg_info->map_inst, m);
  }
  return (0);
}

/*
 * MTR_send_Abort
 *
 * Sends an abort message to MAP.
 *
 * Always returns zero.
 */
static int MTR_send_Abort(instance, dlg_id, reason)
u16 instance; /* Destination instance */
u16 dlg_id;   /* Dialogue id */
u8 reason;    /* user reason for abort */
{
  MSG *m;             /* Pointer to message to transmit */
  u8 *pptr;           /* Pointer to a parameter */
  dlg_info *dlg_info; /* Pointer to dialogue state information */

  /*
   * Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    memset(logline, '\0', sizeof(logline));
  sprintf(logline, "****MTR_send_Abort Tx: Sending User Abort Request\n\r %d", dlg_id);
  log_to_file(logline);

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_DLG_REQ, dlg_id, NO_RESPONSE, 5)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Close Request
     * Parameter name   = user reason tag
     * Parameter length = 1
     * Parameter value  = reason
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPDT_U_ABORT_REQ;
    pptr[1] = MAPPN_user_rsn;
    pptr[2] = 0x01;
    pptr[3] = reason;
    pptr[4] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(dlg_info->map_inst, m);
  }
  return (0);
}

/*
 * MTR_send_Delimit
 *
 * Sends a Delimit message to MAP.
 *
 * Always returns zero.
 */
static int MTR_send_Delimit(instance, dlg_id)
u16 instance; /* Destination instance */
u16 dlg_id;   /* Dialogue id */
{
  MSG *m;             /* Pointer to message to transmit */
  u8 *pptr;           /* Pointer to a parameter */
  dlg_info *dlg_info; /* Pointer to dialogue state information */

  /*
   * Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    memset(logline, '\0', sizeof(logline));
  sprintf(logline, "*****MTR_send_Delimit Tx: Sending Delimit \n\r");
  log_to_file(logline);

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_DLG_REQ, dlg_id, NO_RESPONSE, 2)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Delimit Request
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPDT_DELIMITER_REQ;
    pptr[1] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(dlg_info->map_inst, m);
  }
  return (0);
}

/*
 * MTR_send_msg sends a MSG. On failure the
 * message is released and the user notified.
 *
 * Always returns zero.
 */
static int MTR_send_msg(instance, m)
u16 instance; /* Destination instance */
MSG *m;       /* MSG to send */
{
  GCT_set_instance((unsigned int)instance, (HDR *)m);

  MTR_trace_msg("MTR Tx:", m);

  /*
   * Now try to send the message, if we are successful then we do not need to
   * release the message.  If we are unsuccessful then we do need to release it.
   */

  if (GCT_send(m->hdr.dst, (HDR *)m) != 0)
  {
    if (mtr_trace)
      fprintf(stderr, "*** failed to send message ***\n");
    relm((HDR *)m);
  }
  return (0);
}

/******************************************************************************
 *
 * Functions to recover parameters from received MAP format primitives
 *
 ******************************************************************************/

/*
 * MTR_get_invoke_id
 *
 * recovers the invoke id parameter from a parameter array
 *
 * Returns the recovered value or -1 if not found.
 */
static int MTR_get_invoke_id(MSG *m)
{
  int invoke_id = -1; /* Recovered invoke_id */
  u8 *found_pptr;

  if (MTR_get_parameter(m, MAPPN_invoke_id, &found_pptr) == 1)
    invoke_id = (int)*found_pptr;

  return (invoke_id);
}

/*
 * MTR_get_applic_context
 *
 * Recovers the Application Context parameter from a parameter array
 *
 * Returns the length of parameter data recovered (-1 on failure).
 */
static int MTR_get_applic_context(m, dst, dstlen)
MSG *m;     /* Message */
u8 *dst;    /* Start of destination for recovered ac */
u16 dstlen; /* Space available at dst */
{
  int retval = -1; /* Return value */
  int length;
  u8 *found_pptr;

  length = MTR_get_parameter(m, MAPPN_applic_context, &found_pptr);
  if (length > 0 && length <= dstlen)
  {
    memcpy(dst, found_pptr, length);
    retval = length;
  }

  return (retval);
}

/*
 * MTR_get_parameter()
 *
 * Looks for the given parameter in the message data.
 *
 * Returns parameter length
 *         or -1 if parameter was not found
 */
static int MTR_get_parameter(MSG *m, u16 pname, u8 **found_pptr)
{
  u8 *pptr;
  int mlen;
  u16 name; /* Parameter name */
  u16 plen; /* Parameter length */
  u8 *end_pptr;
  u8 code_shift = 0;

  pptr = get_param(m);
  mlen = m->len;

  if (mlen < 1)
    return (-1);

  /*
   * Skip past primitive type octet
   */
  pptr++;
  mlen--;

  end_pptr = pptr + mlen;

  while (pptr < end_pptr)
  {
    name = *pptr++;
    if (name == MAPPN_LONG_PARAM_CODE_EXTENSION)
    {
      /*
       * Process extended parameter name format data
       *
       * Skip first length octet(s)
       */
      pptr++;
      if (code_shift)
        pptr++;

      /*
       * Read 2 octet parameter name
       */
      name = *pptr++ << 8;
      name |= *pptr++;
    }

    /*
     * Read parameter length from 1 or 2 octets
     */
    if (code_shift)
    {
      plen = *pptr++ << 8;
      plen |= *pptr++;
    }
    else
    {
      plen = *pptr++;
    }

    /*
     * Test parameter name for match, end and code_shift
     */
    if (name == 0)
      break;

    if (name == MAPPN_CODE_SHIFT)
      code_shift = *pptr;

    if (name == pname)
    {
      /*
       * Just return the length if the given return data pointer is NULL
       */
      if (found_pptr != NULL)
      {
        *found_pptr = pptr;
        memset(logline, '\0', sizeof(logline));
        sprintf(logline, "pname found \n");
        log_to_file(logline);
      }
      return (plen);
    }

    pptr += plen;
  }
  return (-1);
}

/*
 * MTR_get_primitive_type()
 *
 * Looks for the primitive type of message.
 *
 * Returns primitive type
 *         or MAPST_EXTENDED_SERVICE_TYPE if a valid primitive type was not found
 */
static u16 MTR_get_primitive_type(MSG *m)
{
  u16 ptype = 0;
  int len = 0;
  u8 *found_pptr;
  u8 *pptr;
  int mlen;

  pptr = get_param(m);
  mlen = m->len;

  if (mlen < 1)
    return (-1);

  /*
   * Read first octet as primitive type
   */
  ptype = *pptr;

  if (ptype == MAPST_EXTENDED_SERVICE_TYPE)
  {
    len = MTR_get_parameter(m, MAPPN_SERVICE_TYPE, &found_pptr);
    if (len == 2)
    {
      ptype = *found_pptr++ << 8;
      ptype |= *found_pptr++;
    }
  }
  return (ptype);
}

/*
 * MTR_trace_msg
 *
 * Traces (prints) any message as hexadecimal to the console.
 *
 * Always returns zero.
 */
static int MTR_trace_msg(prefix, m)
char *prefix;
MSG *m; /* received message */
{
  HDR *h;       /* pointer to message header */
  int instance; /* instance of MAP msg received from */
  u16 mlen;     /* length of received message */
  u8 *pptr;     /* pointer to parameter area */

  /*
   * If tracing is disabled then return
   */
  if (mtr_trace == 0)
    return (0);

  h = (HDR *)m;
  instance = GCT_get_instance(h);
  memset(logline, '\0', sizeof(logline));
  sprintf(logline, "%s I%04x M t%04x i%04x f%02x d%02x s%02x", prefix, instance, h->type,
          h->id, h->src, h->dst, h->status);
  log_to_file(logline);

  if ((mlen = m->len) > 0)
  {
    if (mlen > MAX_PARAM_LEN)
      mlen = MAX_PARAM_LEN;
    memset(logline, '\0', sizeof(logline));
    sprintf(logline, " p");
    log_to_file(logline);

    pptr = get_param(m);
    while (mlen--)
    {
      memset(logline, '\0', sizeof(logline));
      sprintf(logline, "%c%c", BIN2CH(*pptr / 16), BIN2CH(*pptr % 16));
      log_to_file(logline);

      pptr++;
    }
  }
  memset(logline, '\0', sizeof(logline));
  sprintf(logline, "\n");
  log_to_file(logline);

  return (0);
}

/*
 * init_resources
 *
 * Initialises all mtr system resources
 * This includes dialogue state information.
 *
 * Always returns zero
 *
 */
static int init_resources()
{
  int i; /* for loop index */

  for (i = 0; i < MAX_NUM_DLGS; i++)
  {
    dialogue_info[i].state = MTR_S_NULL;
    dialogue_info[i].term_mode = mtr_default_dlg_term_mode;
  }
  return (0);
}

void log_to_file(const char *str)
{
  // Create file name in the format "EIRBIN-date-time.number"
  printf("Entering log_file function\n");
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char file_name[100];

  if (eirlogfile == NULL)
  {
    printf("Error opening file!\n");
    counter++;
    // sprintf(file_name, "/app/log/eirlog/EIRBIN-%d-%02d-%02d-%02d-%02d-%02d.%d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, counter);
    sprintf(file_name, "%s/eirlog/EIRBIN-%d-%d-%d_%d-%d-%d_%d.txt", directory_n, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, counter);

    // Open file for writing
    eirlogfile = fopen(file_name, "w");
    log_to_file(str);
  }
  int size = 0;
  // Write string to file and check the size of the file
  printf("In log_to_file function, before printing %s \n", str);
  fprintf(eirlogfile, "%s", str);
  fseek(eirlogfile, 0, SEEK_END);
  size = ftell(eirlogfile);
  // If the file size is greater than 10MB then rotate the file
  printf("In log_to_file function, after printing %s \n", str);

  if (size > 100 * 1024 * 1024)
  {
    fclose(eirlogfile);
    counter++;
    // sprintf(file_name, "/app/log/eirlog/EIRBIN-%d-%02d-%02d-%02d-%02d-%02d.%d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, counter);
    sprintf(file_name, "%s/eirlog/EIRBIN-%d-%d-%d_%d-%d-%d_%d.txt", directory_n, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, counter);

    // Open file for writing
    eirlogfile = fopen(file_name, "w");
    log_to_file(str);
  }

  printf("Exiting log_file function\n");
}

char **loadIMEIBlockList()
{
  FILE *file;
  char **imeiblocklist;
  int imeiblocklistSize = 0;
  int i;

  // Open the text file
  file = fopen("/app/log/blocklist/blocklist.in", "r");
  if (file == NULL)
  {
    printf("Error opening the blocklist file.\n");
    return NULL; // Return NULL to indicate an error
  }

  // Count the number of lines in the file
  char line[20];
  while (fgets(line, sizeof(line), file) != NULL)
  {
    imeiblocklistSize++;
  }

  printf("IMEI Blocklist size loaded = %d",imeiblocklistSize);
  rewind(file);

  // Allocate memory for the imeiblocklist array
  imeiblocklist = (char **)malloc(imeiblocklistSize * sizeof(char *));
  if (imeiblocklist == NULL)
  {
    printf("Error allocating memory.\n");
    fclose(file);
    return NULL; // Return NULL to indicate an error
  }

  // Read each line and load it into imeiblocklist array
  i = 0;
  while (fgets(line, sizeof(line), file) != NULL)
  {
    // Remove the newline character at the end
    printf("Line read before blocklist after newline character removal %s \n", line);

    line[strcspn(line, "\n")] = '\0';
    printf("Line read from blocklist after newline character removal %s \n", line);

    // Allocate memory for the new line
    imeiblocklist[i] = (char *)malloc((strlen(line) + 1) * sizeof(char));
    if (imeiblocklist[i] == NULL)
    {
      printf("Error allocating memory.\n");
      fclose(file);
      free(imeiblocklist);
      return NULL; // Return NULL to indicate an error
    }

    // Copy the line to imeiblocklist array
    strcpy(imeiblocklist[i], line);
    i++;
  }
  fclose(file);

  return imeiblocklist;
}

void freeIMEIBlockList(char **imeiblocklist, int size)
{
  int i;
  if (imeiblocklist == NULL)
  {
    return;
  }

  for (i = 0; i < size; i++)
  {
    free(imeiblocklist[i]);
  }
  free(imeiblocklist);
}

int searchIMEIInBlockList(char **imeiblocklist, char imei[20])
{
  if (imeiblocklist == NULL)
  {
    printf("searchIMEIInBlockList imeiblocklist is null \n");
    return 0;
  }
  int i;

  // for ( i = 0; imeiblocklist[i] != NULL; i++) {
  //       printf("%s\n", imeiblocklist[i]);
  // }

  printf("searchIMEIInBlockList imeiblocklist . IMEI =%s", imei);

  for (i = 0; imeiblocklist[i] != NULL; i++)
  {
    char tempstr1[15], tempstr2[15];
    strncpy(tempstr1,imei,14);
    tempstr1[14] = '\0';

     strncpy(tempstr2,imeiblocklist[i],14);
    tempstr2[14] = '\0';

    if (strcmp(tempstr2, tempstr1) == 0)
    {
      return 1; // imei found in the blocklist
    }
  }

  return 0; // imei not found in the blocklist
}
