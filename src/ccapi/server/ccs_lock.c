/*
 * $Header$
 *
 * Copyright 2006 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "ccs_common.h"

struct ccs_lock_d {
    cc_uint32 type;
    cc_uint32 pending;
    cc_int32  invalid_object_err;
    ccs_pipe_t client_pipe;
    ccs_pipe_t reply_pipe;
};

struct ccs_lock_d ccs_lock_initializer = { 0, 1, 1, NULL, NULL };

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_new (ccs_lock_t *out_lock,
                       cc_uint32   in_type,
                       cc_int32    in_invalid_object_err,
                       ccs_pipe_t  in_client_pipe,
                       ccs_pipe_t  in_reply_pipe)
{
    cc_int32 err = ccNoError;
    ccs_lock_t lock = NULL;
    
    if (!out_lock                       ) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_client_pipe)) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_reply_pipe) ) { err = cci_check_error (ccErrBadParam); }
    
    if (in_type != cc_lock_read && 
        in_type != cc_lock_write &&
        in_type != cc_lock_upgrade &&
        in_type != cc_lock_downgrade) { 
        err = cci_check_error (ccErrBadLockType); 
    }
    
    if (!err) {
        lock = malloc (sizeof (*lock));
        if (lock) { 
            *lock = ccs_lock_initializer;
        } else {
            err = cci_check_error (ccErrNoMem); 
        }
    }
    
    if (!err) {
        err = ccs_pipe_copy (&lock->client_pipe, in_client_pipe);
    }
    
    if (!err) {
        err = ccs_pipe_copy (&lock->reply_pipe, in_reply_pipe);
    }
    
    if (!err) {
        lock->type = in_type;
        lock->invalid_object_err = in_invalid_object_err;
        
        *out_lock = lock;
        lock = NULL;
    }
    
    ccs_lock_release (lock);
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_release (ccs_lock_t io_lock)
{
    cc_int32 err = ccNoError;
    
    if (!io_lock) { err = cci_check_error (ccErrBadParam); }
    
    if (!err && io_lock->pending) {
        err = ccs_server_send_reply (io_lock->reply_pipe, 
                                     io_lock->invalid_object_err, NULL);
    }
    
    if (!err) {
        ccs_pipe_release (io_lock->client_pipe);
        ccs_pipe_release (io_lock->reply_pipe);
        free (io_lock);
    }
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_grant_lock (ccs_lock_t io_lock)
{
    cc_int32 err = ccNoError;
    
    if (!io_lock) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        if (io_lock->pending) {
            err = ccs_server_send_reply (io_lock->reply_pipe, err, NULL);
            
            if (!err) {
                ccs_pipe_release (io_lock->reply_pipe);
                io_lock->pending = 0;
                io_lock->reply_pipe = NULL;
            }
        } else {
            cci_debug_printf ("WARNING %s() called on non-pending lock!", 
                              __FUNCTION__);
        }
    }
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

cc_uint32 ccs_lock_is_pending (ccs_lock_t  in_lock,
                               cc_uint32  *out_pending)
{
    cc_int32 err = ccNoError;
    
    if (!in_lock    ) { err = cci_check_error (ccErrBadParam); }
    if (!out_pending) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        *out_pending = in_lock->pending;
    }
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_type (ccs_lock_t  in_lock,
                        cc_uint32  *out_lock_type)
{
    cc_int32 err = ccNoError;
    
    if (!in_lock      ) { err = cci_check_error (ccErrBadParam); }
    if (!out_lock_type) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        *out_lock_type = in_lock->type;
    }
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_is_read_lock (ccs_lock_t  in_lock,
                                cc_uint32  *out_is_read_lock)
{
    cc_int32 err = ccNoError;
    
    if (!in_lock         ) { err = cci_check_error (ccErrBadParam); }
    if (!out_is_read_lock) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        *out_is_read_lock = (in_lock->type == cc_lock_read || 
                             in_lock->type == cc_lock_downgrade);
    }
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_is_write_lock (ccs_lock_t  in_lock,
                                 cc_uint32  *out_is_write_lock)
{
    cc_int32 err = ccNoError;
    
    if (!in_lock          ) { err = cci_check_error (ccErrBadParam); }
    if (!out_is_write_lock) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        *out_is_write_lock = (in_lock->type == cc_lock_write || 
                              in_lock->type == cc_lock_upgrade);
    }
    
    return cci_check_error (err);    
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_is_for_client (ccs_lock_t     in_lock,
                                 ccs_pipe_t     in_client_pipe,
                                 cc_uint32     *out_is_for_client)
{
    cc_int32 err = ccNoError;
    
    if (!in_lock                        ) { err = cci_check_error (ccErrBadParam); }
    if (!out_is_for_client              ) { err = cci_check_error (ccErrBadParam); }
    if (!ccs_pipe_valid (in_client_pipe)) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        err = ccs_pipe_compare (in_lock->client_pipe, in_client_pipe, 
                                out_is_for_client);
    }
    
    return cci_check_error (err);    
}


/* ------------------------------------------------------------------------ */

cc_int32 ccs_lock_client (ccs_lock_t  in_lock,
                          ccs_pipe_t *out_client_pipe)
{
    cc_int32 err = ccNoError;
    
    if (!in_lock        ) { err = cci_check_error (ccErrBadParam); }
    if (!out_client_pipe) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        *out_client_pipe = in_lock->client_pipe;
    }
    
    return cci_check_error (err);    
}