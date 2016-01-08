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
//  Create a new upt_server
UPT_EXPORT upt_server_t *
    upt_server_new (void);

//  Destroy the upt_server
UPT_EXPORT void
    upt_server_destroy (upt_server_t **self_p);

//  Set server verbose
UPT_EXPORT void
    upt_server_verbose (upt_server_t *self);

//  Print properties of object
UPT_EXPORT void
    upt_server_print (upt_server_t *self);

//  Uptime server as an actor
void upt_server (zsock_t *pipe, void *args);

//  Self test of this class
UPT_EXPORT void
    upt_server_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
