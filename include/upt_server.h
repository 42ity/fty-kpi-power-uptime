/*  =========================================================================
    upt_server - Actor computing uptime

    Copyright (C) 2014 - 2015 Eaton                                        
                                                                           
    This program is free software; you can redistribute it and/or modify   
    it under the terms of the GNU General Public License as published by   
    the Free Software Foundation; either version 2 of the License, or      
    (at your option) any later version.                                    
                                                                           
    This program is distributed in the hope that it will be useful,        
    but WITHOUT ANY WARRANTY; without even the implied warranty of         
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          
    GNU General Public License for more details.                           
                                                                           
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.            
    =========================================================================
*/

#ifndef UPT_SERVER_H_INCLUDED
#define UPT_SERVER_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif


//  @interface
//  Create new kpi-uptime instance.
//
//      zactor_t *server = zactor_new (upt_server, "uptime");
//
//  Destroy the instance
//      zsock_destroy (&server);
//
//  All upt_server commands are synchronous, so must be followed by zsock_wait
//
//  Set verbose mode
//      zsock_send (server, "VERBOSE");
//      zsock_wait (server);
//
//  Connect to malamute endpoint (it'll interally subscribe on METRIC stream and ups.status.* messages)
//      zsock_sendx (server, "CONNECT", "ipc://@/malamute", NULL);
//      zsock_wait (server);
//
//  Setup directory for storing the state
//      zsock_sendx (server, "CONFIG", "src/", NULL);
//      zsock_wait (server);
//
void upt_server (zsock_t *pipe, void *args);

//  Self test of this class
UPT_EXPORT void
    upt_server_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
