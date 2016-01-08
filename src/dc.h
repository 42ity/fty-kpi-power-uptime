/*  =========================================================================
    dc - DC information

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

#ifndef DC_H_INCLUDED
#define DC_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _dc_t dc_t;

//  @interface
//  Create a new dc
UPT_EXPORT dc_t *
    dc_new (void);

//  Destroy the dc
UPT_EXPORT void
    dc_destroy (dc_t **self_p);

//  Return if dc is offline
bool
    dc_is_offline (dc_t *self);

//  Set UPS as as offline
void
    dc_set_offline (dc_t *self, char* ups);

// Set UPS as online
void
    dc_set_online (dc_t *self, char* ups);

// Compute uptime, return result in total/offline pointers
void
    dc_uptime (dc_t *self, uint64_t* total, uint64_t* offline);

//  Print properties of object
UPT_EXPORT void
    dc_print (dc_t *self);

//  Self test of this class
UPT_EXPORT void
    dc_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
