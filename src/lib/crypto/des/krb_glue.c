/*
 * $Source$
 * $Author$
 *
 * Copyright 1985, 1986, 1987, 1988, 1990 by the Massachusetts Institute
 * of Technology.
 *
 * These routines perform encryption and decryption using the DES
 * private key algorithm, or else a subset of it -- fewer inner loops.
 * (AUTH_DES_ITER defaults to 16, may be less.)
 *
 * Under U.S. law, this software may not be exported outside the US
 * without license from the U.S. Commerce department.
 *
 * These routines form the library interface to the DES facilities.
 *
 * Originally written 8/85 by Steve Miller, MIT Project Athena.
 *
 * For copying and distribution information, please see the file
 * <krb5/copyright.h>.
 *
 */

/*
 * These routines were extracted out of enc_dec.c because they will
 * drag in the kerberos library, if someone references mit_des_cbc_encrypt,
 * even no kerberos routines are called
 */

#if !defined(lint) && !defined(SABER)
static char rcsid_enc_dec_c[] =
"$Id$";
#endif	/* !lint & !SABER */

#include <krb5/copyright.h>

#include <krb5/krb5.h>
#include <krb5/ext-proto.h>
#include <krb5/crc-32.h>

#include "des_int.h"

#ifdef DEBUG
#include <stdio.h>

extern int mit_des_debug;
#endif

/*
	encrypts "size" bytes at "in", storing result in "out".
	"eblock" points to an encrypt block which has been initialized
	by process_key().

	"out" must be preallocated by the caller to contain sufficient
	storage to hold the output; the macro krb5_encrypt_size() can
	be used to compute this size.
	
	returns: errors
*/
static krb5_error_code
mit_des_encrypt_f(DECLARG(krb5_const_pointer, in),
		  DECLARG(krb5_pointer, out),
		  DECLARG(const size_t, size),
		  DECLARG(krb5_encrypt_block *, key),
		  DECLARG(krb5_pointer, ivec))
OLDDECLARG(krb5_const_pointer, in)
OLDDECLARG(krb5_pointer, out)
OLDDECLARG(const size_t, size)
OLDDECLARG(krb5_encrypt_block *, key)
OLDDECLARG(krb5_pointer, ivec)
{
    krb5_octet	*iv;
    
    if ( ivec == 0 )
	iv = key->key->contents;
    else
	iv = (krb5_octet *)ivec;

    /* XXX should check that key sched is valid here? */
    return (mit_des_cbc_encrypt((krb5_octet *)in, 
			    (krb5_octet *)out,
			    size, 
			    (struct mit_des_ks_struct *)key->priv, 
			    iv,
			    MIT_DES_ENCRYPT));
}    


/*

	decrypts "size" bytes at "in", storing result in "out".
	"eblock" points to an encrypt block which has been initialized
	by process_key().

	"out" must be preallocated by the caller to contain sufficient
	storage to hold the output; this is guaranteed to be no more than
	the input size.

	returns: errors

 */
static krb5_error_code
mit_des_decrypt_f(DECLARG(krb5_const_pointer, in),
		  DECLARG(krb5_pointer, out),
		  DECLARG(const size_t, size),
		  DECLARG(krb5_encrypt_block *, key),
		  DECLARG(krb5_pointer, ivec))
OLDDECLARG(krb5_const_pointer, in)
OLDDECLARG(krb5_pointer, out)
OLDDECLARG(const size_t, size)
OLDDECLARG(krb5_encrypt_block *, key)
OLDDECLARG(krb5_pointer, ivec)
{
    krb5_octet	*iv;

    if ( ivec == 0 )
	iv = key->key->contents;
    else
	iv = (krb5_octet *)ivec;

    /* XXX should check that key sched is valid here? */
    return (mit_des_cbc_encrypt ((krb5_octet *)in, 
			     (krb5_octet *)out, 
			     size, 
			     (struct mit_des_ks_struct *)key->priv, 
			     iv,
			     MIT_DES_DECRYPT));
}    

krb5_error_code mit_des_encrypt_func(DECLARG(krb5_const_pointer, in),
				     DECLARG(krb5_pointer, out),
				     DECLARG(const size_t, size),
				     DECLARG(krb5_encrypt_block *, key),
				     DECLARG(krb5_pointer, ivec))
OLDDECLARG(krb5_const_pointer, in)
OLDDECLARG(krb5_pointer, out)
OLDDECLARG(const size_t, size)
OLDDECLARG(krb5_encrypt_block *, key)
OLDDECLARG(krb5_pointer, ivec)
{
    krb5_checksum cksum;
    krb5_octet 	contents[CRC32_CKSUM_LENGTH];
    char 	*p, *endinput;
    int sumsize;
    krb5_error_code retval;

/*    if ( size < sizeof(mit_des_cblock) )
	return KRB5_BAD_MSIZE; */

    /* caller passes data size, and saves room for the padding. */
    /* we need to put the cksum in the end of the padding area */
    sumsize =  krb5_roundup(size+CRC32_CKSUM_LENGTH, sizeof(mit_des_cblock));

    p = (char *)in + sumsize - CRC32_CKSUM_LENGTH;
    endinput = (char *)in + size;
    memset(endinput, 0, sumsize - size);
    cksum.contents = contents; 

    if (retval = (*krb5_cksumarray[CKSUMTYPE_CRC32]->
                  sum_func)((krb5_pointer) in,
                            sumsize,
                            (krb5_pointer)key->key->contents,
                            sizeof(mit_des_cblock),
                            &cksum)) 
	return retval;
    
    memcpy(p, (char *)contents, CRC32_CKSUM_LENGTH);
 
    return (mit_des_encrypt_f(in, out, sumsize, key, ivec));
}

krb5_error_code mit_des_decrypt_func(DECLARG(krb5_const_pointer, in),
				     DECLARG(krb5_pointer, out),
				     DECLARG(const size_t, size),
				     DECLARG(krb5_encrypt_block *, key),
				     DECLARG(krb5_pointer, ivec))
OLDDECLARG(krb5_const_pointer, in)
OLDDECLARG(krb5_pointer, out)
OLDDECLARG(const size_t, size)
OLDDECLARG(krb5_encrypt_block *, key)
OLDDECLARG(krb5_pointer, ivec)
{
    krb5_checksum cksum;
    krb5_octet 	contents_prd[CRC32_CKSUM_LENGTH];
    krb5_octet  contents_get[CRC32_CKSUM_LENGTH];
    char 	*p;
    krb5_error_code   retval;

    if ( size < sizeof(mit_des_cblock) )
	return KRB5_BAD_MSIZE;

    if (retval = mit_des_decrypt_f(in, out, size, key, ivec))
	return retval;

    cksum.contents = contents_prd;
    p = (char *)out + size - CRC32_CKSUM_LENGTH;
    memcpy((char *)contents_get, p, CRC32_CKSUM_LENGTH);
    memset(p, 0, CRC32_CKSUM_LENGTH);

    if (retval = (*krb5_cksumarray[CKSUMTYPE_CRC32]->
                  sum_func)(out,
                            size,
                            (krb5_pointer)key->key->contents,
                            sizeof(mit_des_cblock),
                            &cksum)) 
	return retval;

    if (memcmp((char *)contents_get, (char *)contents_prd, CRC32_CKSUM_LENGTH) )
        return KRB5KRB_AP_ERR_BAD_INTEGRITY;

    return 0;
}

