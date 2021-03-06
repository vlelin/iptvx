/*

   Copyright 2018   Jan Kammerath

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

*/
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <microhttpd.h>
#include <glib.h>
#include <json-c/json.h>
#include "db.h"
#include "record.h"
#include "channel.h"
#include "util.h"

/* indicator to stay alive */
volatile sig_atomic_t iptvx_daemon_alive;

/* define the port to run the daemon on */
int iptvx_daemon_server_port;

/* defines recording tolerance in minutes */
int iptvx_daemon_record_tolerance;

/* sets the percentage of epg content loaded */
int iptvx_daemon_epg_status;

/* represents the working dir */
GString* record_dir;

/* path where the app files are in */
GString* app_dir;

/* path where the data files are in */
GString* data_dir;

/* string with epg json data */
GString* iptvx_daemon_epg_json;

/* garray with all epg data */
GArray* iptvx_daemon_epg_data;

/* a list with all recordings */
GArray* recordlist;

/*
   Sets the application directory for the overlay
   @param       appdir        path where the app files are in
*/
void iptvx_daemon_set_app_dir(GString* appdir){
   app_dir = appdir;
}

/*
   Sets the data directory where all application data is in
   @param       datadir        path where the data files are in
*/
void iptvx_daemon_set_data_dir(GString* datadir){
   data_dir = datadir;
}

/*
   Sets the percentage amount of loaded epg content
   @param      percentage     int with percentage loaded (0-100)
*/
void iptvx_daemon_set_epg_status(int percentage){
   iptvx_daemon_epg_status = percentage;
}

/*
   Sets the recording tolerance in minutes
   @param      tolerance      the tolerance in minutes
*/
void iptvx_daemon_set_record_tolerance(int tolerance){
   iptvx_daemon_record_tolerance = tolerance;
}

/*
   Sets the directory to store data in
   @param      dirname     full path of directory to work in
*/
void iptvx_daemon_set_dir(char* dirname){
   record_dir = g_string_new(dirname);
}

/*
   Sets the epg data for the daemon to return
   @param      epg_data          epg data as json string
*/
void iptvx_daemon_set_epg_data(GArray* epg_data){
   iptvx_daemon_epg_data = epg_data;
}

/*
   Sets the epg data for the daemon to return
   @param      epg_data          epg data as json string
*/
void iptvx_daemon_set_epg_json(GString* epg_data){
   iptvx_daemon_epg_json = epg_data;
}

/*
   Sets the port number for the daemons http server
   @param         port_number       int with port number to listen on
*/
void iptvx_daemon_set_server_port(int port_number){
   iptvx_daemon_server_port = port_number;
}

/*
   Kills/ stops the current daemon
*/
void iptvx_daemon_kill(){
   /* set to false and main loop will end itself */
   iptvx_daemon_alive = false;
}

/*
   Returns the list with all recordings as json
   @return        list with all recordings as json
*/
GString* iptvx_daemon_get_recordlist_json(){
   GString* result;

   json_object* j_rec_array = json_object_new_array();

   if(recordlist != NULL){
      int c = 0;
      for(c = 0; c < recordlist->len; c++){
         /* fetch the recording from the array */
         recording* rec = &g_array_index(recordlist,recording,c);

         /* create json object for this recording */
         json_object* j_rec = json_object_new_object();
         json_object_object_add(j_rec,"channel",
               json_object_new_string(rec->channel->str));
         json_object_object_add(j_rec,"title",
               json_object_new_string(rec->title->str));
         json_object_object_add(j_rec,"filename",
               json_object_new_string(rec->filename->str));
         json_object_object_add(j_rec,"start",
               json_object_new_int(rec->start));
         json_object_object_add(j_rec,"stop",
               json_object_new_int(rec->stop));
         json_object_object_add(j_rec,"status",
               json_object_new_int(rec->status));
         json_object_object_add(j_rec,"seconds_recorded",
               json_object_new_int(rec->seconds_recorded));
         json_object_object_add(j_rec,"filesize",
               json_object_new_int(rec->filesize));

         /* add to json array */
         json_object_array_add(j_rec_array,j_rec);
      }
   }

   /* finally pass j_object to result string */
   result = g_string_new(json_object_to_json_string(j_rec_array));

   return result;
}

/*
   Returns a json string with the channel list
*/
GString* iptvx_daemon_get_channel_list_json(){
   json_object* j_chan_array = json_object_new_array();

   if(iptvx_daemon_epg_data != NULL){
      int c = 0;
      for(c = 0; c < iptvx_daemon_epg_data->len; c++){
         channel* chan = &g_array_index(iptvx_daemon_epg_data,channel,c);
         
         json_object* j_chan = json_object_new_object();
         json_object_object_add(j_chan,"default",
            json_object_new_boolean(chan->isDefault));
         json_object_object_add(j_chan,"name",
            json_object_new_string(chan->name->str));
         json_object_object_add(j_chan,"url",
            json_object_new_string(chan->url->str));

         /* add the channel to the result json array */
         json_object_array_add(j_chan_array,j_chan);
      }    
   }
   /* finally pass j_object to result string */
   char* json_result_buf = (char*)json_object_to_json_string(j_chan_array);
   GString* result = g_string_new(json_result_buf);

   /* free the json object */
   json_object_put(j_chan_array);

   return result;
}

/*
   Returns the url for a given channel
   @param         channel_name      name of the channel
   @return                          url of the channel
*/
GString* iptvx_daemon_get_url(GString* channel_name){
   GString* result = g_string_new("");

   int c = 0;
   for(c = 0; c < iptvx_daemon_epg_data->len; c++){
      channel* chan = &g_array_index(iptvx_daemon_epg_data,channel,c);
      if(g_strcmp0(chan->name->str,channel_name->str)==0){
         /* this is the channel we're looking for */
         result = chan->url;
      }
   } 

   return result;
}

/*
   Generates a filename for a scheduled recording
   @param         rec      the recording object
   @return                 the string with the filename
*/
GString* iptvx_daemon_get_filename(recording rec){
   GString* result;

   /* generate the date for the filename */
   time_t starttime_struct = rec.start;
   char startdate_text[50];
   strftime(startdate_text, 50, "%Y-%m-%d %H:%M:%S", 
               gmtime(&(starttime_struct)));

   /* generate the filename */
   char filename[265];
   sprintf(filename,"%s/%s %s - %s.ts",record_dir->str,
            startdate_text,rec.channel->str,rec.title->str);

   /* push filename into result */
   result = g_string_new(filename);

   return result;
}

/*
   Gets the title for a specific recording by checking the epg data
   @param               rec      recording to get title for
   @return                       a string with the title of the show
*/
GString* iptvx_daemon_get_recording_title(recording* rec){
   /* the default title, if no programme can be found, is 'Live' */
   GString* result = g_string_new("Live");

   if(iptvx_daemon_epg_data != NULL){
      int c = 0;
      long max_prog_len = 0;
      for(c = 0; c < iptvx_daemon_epg_data->len; c++){
         channel* chan = &g_array_index(iptvx_daemon_epg_data,channel,c);

         /* check if this is the recording's channel */
         if(g_strcmp0(chan->name->str,rec->channel->str) == 0){
            /* Check which programme matches the recording schedule.
               If there are multiple programmes during the recording 
               period than the programme with the longest duration during
               the recording period will represent the recording */
            if(chan->programmeList != NULL){
               int p = 0;
               for(p = 0; p < chan->programmeList->len; p++){
                  programme* prog = &g_array_index(chan->programmeList,programme,p);
                  
                  /* calculate the duration of the programme */
                  long prog_len = prog->stop - prog->start;

                  if(prog->start >= rec->start && prog->stop <= rec->stop && prog_len > max_prog_len){
                     /* this programme runs during the recording */
                     result = g_string_new(prog->title->str);

                     /* increase the max len */
                     max_prog_len = prog_len;
                  }
               }
            }
         }
      }
   }

   return result;
}

/*
   Creates a new recording struct from given data
   @param      channel     name of channel to record
   @param      start       timestamp of start time
   @param      stop        timestamp of stop time
   @return                 the recording struct with the data   
*/
recording iptvx_daemon_create_recording(char* channel, long start, long stop){
   recording result;

   /* set the start time */
   result.start = start;
   if(result.start < time(NULL)){
      /* recording should have started already, give it 10 secs */
      result.start = time(NULL)+10;
   }

   /* set the stop time for the recording */
   result.stop = stop;
   if(result.stop < time(NULL)){
      /* this is gone already, so take the next 5 minutes */
      result.stop = time(NULL)+300;
   }   

   result.channel = g_string_new(channel);
   result.status = 0;
   result.seconds_recorded = 0;
   result.filesize = 0;
   result.tolerance = iptvx_daemon_record_tolerance*60;

   /* generate the title for this recording */
   result.title = iptvx_daemon_get_recording_title(&result);

   /* generate the filename for this recording */
   result.filename = iptvx_daemon_get_filename(result);

   return result;
}

/*
   Adds a new recording to the list if it does not already exist
   @param         rec         the new recording to add to the list
*/
void iptvx_daemon_add_recording(recording rec){
   bool exists = false;

   int c;
   for(c = 0; c<recordlist->len;c++){
      recording* exist_rec = &g_array_index(recordlist,recording,c);

      /* check if this is the same or equal */
      if(g_strcmp0(exist_rec->channel->str,rec.channel->str)==0 
         && exist_rec->start == rec.start
         && exist_rec->stop == rec.stop){
         /* set the marker, we have it already */
         exists = true;
      }
   }

   if(!exists){
      /* this is a new one, so add it */
      g_array_append_val(recordlist,rec);

      /* sync the recording list with the db */
      iptvx_db_update_recording(recordlist);
   }
}

/*
   Gets the current status of the daemon and 
   its operations as json string
   @return        json string with status info
*/
GString* iptvx_daemon_get_status_json(){
   json_object* j_status = json_object_new_object();
   json_object_object_add(j_status,"epg_loaded",json_object_new_int(iptvx_daemon_epg_status));
   return g_string_new(json_object_to_json_string(j_status));
}

/*
   Removes a recording from the list and from the disk, if exists
   @param         number            id of the recording in the list
*/
void iptvx_daemon_remove_recording(long number){
   /* make sure it is not out of bounds */
   if(number >= 0 && number < recordlist->len){
      /* get the recording to check it's status */
      recording* exist_rec = &g_array_index(recordlist,recording,number);

      if(exist_rec->status == 1){
         /* this is in progress, so we 
            need to stop the recording now */
         iptvx_record_cancel(exist_rec);
      }if(exist_rec->status == 2){
         /* this is already finished and just 
            sitting on the disk where we will
            remove it from now */
         util_delete_file(exist_rec->filename->str);
      }

      /* remove recording from database */
      iptvx_db_remove_recording(exist_rec);

      /* finally also remove it from the list */
      g_array_remove_index(recordlist,number);
   }
}

/*
   Provides the proper result for the url
   @param      request_url    url requested from the client
   @param      connection     handle for the current connection
   @return                    string with JSON contents to show
*/
GString* iptvx_daemon_get_response(char* request_url, struct MHD_Connection* connection){
   GString* result = g_string_new("{}");

   /* check for requested data */
   if(g_strcmp0(request_url,"/")==0){
      /* current epg status is requested */
      result = iptvx_daemon_get_status_json();
   }if(g_strcmp0(request_url,"/list.json")==0){
      /* channel list is requested */
      result = iptvx_daemon_get_channel_list_json();
   }if(g_strcmp0(request_url,"/epg.json")==0){
      /* full epg in json is requested */
      if(iptvx_daemon_epg_json != NULL){
         result = iptvx_daemon_epg_json;
      }
   }if(g_strcmp0(request_url,"/record.json")==0){
      /* recorder information requested */
      const char* requestedAction = MHD_lookup_connection_value 
                     (connection, MHD_GET_ARGUMENT_KIND, "action");

      /* check for any actions requested */
      if(g_strcmp0(requestedAction,"add")==0){
         /* requested to add new recording */
         const char* rec_channel = MHD_lookup_connection_value 
                  (connection, MHD_GET_ARGUMENT_KIND, "channel");
         const char* rec_start = MHD_lookup_connection_value 
                  (connection, MHD_GET_ARGUMENT_KIND, "start");
         const char* rec_stop = MHD_lookup_connection_value 
                  (connection, MHD_GET_ARGUMENT_KIND, "stop");

         /* esnure values exist and are not null */
         if(rec_channel != NULL && rec_start != NULL && rec_stop != NULL){
            /* create new recording struct */
            recording new_rec = iptvx_daemon_create_recording((char*)rec_channel,
                                                atol(rec_start), atol(rec_stop));

            /* append new recording to list */
            iptvx_daemon_add_recording(new_rec);
         }
      }if(g_strcmp0(requestedAction,"remove")==0){
         /* requested to remove recording */
         const char* rec_item = MHD_lookup_connection_value 
                  (connection, MHD_GET_ARGUMENT_KIND, "item");

         /* check for the correct item number */
         if(rec_item != NULL){
            /* remove the item from the list */
            iptvx_daemon_remove_recording(atol(rec_item));
         }
      }

      /* output list with recordings */
      result = iptvx_daemon_get_recordlist_json();
   }

   return result;
}

/*
   Handles http requests to this daemon
*/
static int iptvx_daemon_handle_request(void * cls, struct MHD_Connection * connection,
                                       const char * url, const char * method,
                                       const char * version, const char * upload_data,
                                       size_t * upload_data_size, void ** ptr) {
   static int dummy;
   struct MHD_Response * response;
   int ret;

   /* define the response */
   char* content_type = "application/json";
   GString* content = iptvx_daemon_get_response((char*)url,connection);

   /* check if content app file data */
   if(g_str_has_prefix((char*)url,"/app/") == true){
      /* get the file from the local disk and return it */
      GString* s_url = g_string_new((char*)url);
      GString* requested_file = util_substr(s_url,5,0);

      /* prepend the full path of the file */
      g_string_prepend(requested_file,"/");
      g_string_prepend(requested_file,app_dir->str);
      
      /* check if the file exists */
      if(util_file_exists(requested_file->str)){
         /* get the file content as result */
         content = file_get_contents(requested_file);
         GString* req_content_type = util_file_get_mime_type(requested_file);

         /* assign to content type char ptr, but clear its mem first */
         content_type = req_content_type->str;
      }

      /* free all these temporary string */
      g_string_free(s_url,true);
      g_string_free(requested_file,true);
   }

   /* check if local logo files are requested and return them */
   if(g_str_has_prefix((char*)url,"/logo/") == true){
      GString* s_url = g_string_new((char*)url);

      /* prepend the full path of the file */
      g_string_prepend(s_url,data_dir->str);

      /* check if the file exists */
      if(util_file_exists(s_url->str)){
         /* get the file content as result */
         content = file_get_contents(s_url);
         GString* req_content_type = util_file_get_mime_type(s_url);

         /* assign to content type char ptr, but clear its mem first */
         content_type = req_content_type->str;
      }

      /* free all these temporary string */
      g_string_free(s_url,true);
   }

   /* create the response for the query */
   response = MHD_create_response_from_buffer(content->len, (void*)content->str,
                                             MHD_RESPMEM_PERSISTENT);

   /* set the response content type to json */
   MHD_add_response_header(response, "Content-Type", content_type);

   /* queue the response */
   ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
   MHD_destroy_response(response);

   return ret;
}

/*
   Checks if any scheduled recording is bound
   to be started and initialises the recording process
*/
void iptvx_daemon_check_recording(){
   /* get the current timestamp */
   long now = (long)time(NULL);

   /* go through the list of scheduled recordings */
   int c = 0;
   for(c = 0; c < recordlist->len; c++){
      recording* rec = &g_array_index(recordlist,recording,c);
   
      /* check if recording not active 
         and schedule for now */
      long actual_start = (rec->start-(rec->tolerance));
      long actual_stop = (rec->stop+(rec->tolerance));

      if(actual_start <= now && actual_stop >= now && rec->status == 0){
         /* it might be that the daemon was just fired up 
            and EPG data is not yet, ready and we need to 
            wait for it to come up */
         while(iptvx_daemon_epg_data == NULL){
            sleep(1);
         }

         /* add the url of the channel to the recording */
         rec->url = iptvx_daemon_get_url(rec->channel);

         /* supposed to start now, has not yet finished and still scheduled */
         iptvx_record_start(rec);
      }
   }
}

/*
   Initialises the local record list
*/
void iptvx_daemon_init_recordlist(){
   /* initialise recording list by 
      retrieving it from the database */
   recordlist = iptvx_db_get_recording_list();

   /* get the timestamp for now */
   long t_now = time(NULL);

   /* go through all recordings and update 
      them with the information that is not
      present in the database */
   int r = 0;
   for(r = 0; r < recordlist->len; r++){
      /* get the recording struct from the list */
      recording* rec = &g_array_index(recordlist,recording,r);

      /* set the values independent of the status */
      rec->tolerance = iptvx_daemon_record_tolerance*60;
      rec->filename = iptvx_daemon_get_filename(*rec);
      
      /* default values to be set below */
      rec->seconds_recorded = 0;
      rec->filesize = 0;

      /* scheduled status default */   
      rec->status = 0;   

      /* Check the current status.
         This will only check for recordings to be 
         scheduled, finished or failed. If they are
         supposed to be active, then the daemon will
         pick them up right away and amend their status
         anyhow. So there is no need to check whether
         a recording that just came out of the database
         is actually now active.
       */
      if(t_now > rec->stop){
         /* this recording is already finished or failed */
         if(util_file_exists(rec->filename->str)){
            /* set recording status to finished */
            rec->status = 2;

            /* file exists, get it's details */
            rec->filesize = util_get_filesize(rec->filename->str);

            /* we assume the recording went successfull and 
               contains all the seconds required including
               the tolerance. */
            rec->seconds_recorded = (rec->stop-rec->start)+(rec->tolerance*2);
         }else{
            /* file does not exist, so this is a failed recording */
            rec->status = 9; /* marked as failed */
         }
      }
   }
}

/*
   Runs the daemon loop, checks for signals
   and performs the necessary operations.
*/
void iptvx_daemon_run(){
   /* indicate the daemon to stay alive */
   iptvx_daemon_alive = true;

   /* initialise epg status to zero */
   iptvx_daemon_epg_status = 0;

   /* initialise recording list */
   iptvx_daemon_init_recordlist();

   /* attach sigterm handler to kill daemon */
   struct sigaction action;
   memset(&action, 0, sizeof(struct sigaction));
   action.sa_handler = iptvx_daemon_kill;
   sigaction(SIGTERM, &action, NULL);

   /* create the http lib */
   struct MHD_Daemon * d;

   /* start the http daemon and listen */
   d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
             iptvx_daemon_server_port, NULL, NULL,
             &iptvx_daemon_handle_request,
             "", MHD_OPTION_END);

   /* monitor scheduled actions until a signal 
      is caught to stop or interrupt */
   while(iptvx_daemon_alive == true){
      /* start recording if necessary */
      iptvx_daemon_check_recording();

      /* wait */
      sleep(5);
   }

   /* stop the daemon when finished */
   MHD_stop_daemon(d);
}