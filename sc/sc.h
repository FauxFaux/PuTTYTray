/* -*-mode: c; indent-tabs-mode: nil; c-basic-offset: 4; -*-  */
/*
 * This file is part of PuTTY SC, a modification of PuTTY
 * supporting smartcard for authentication.
 *
 * PuTTY SC is available at http://www.joebar.ch/puttysc/
 *
 * Copyright (C) 2005-2007 Pascal Buchbinder
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#ifndef PUTTY_SC_H
#define PUTTY_SC_H

static const char rcsid_sc_h[] = "$Id: sc.h,v 1.20 2007/03/21 21:02:26 pbu Exp $";

struct sc_pubkey_blob {
  void *data;
  int   len;
  char *alg;
};

typedef struct sc_lib_st {
    HINSTANCE            hLib;
    CK_FUNCTION_LIST_PTR m_fl;
    unsigned char       *m_KeyID;
    unsigned char       *m_SshPK;
    int                  m_SshPK_len;
    char                *m_SshPk_alg;
    struct RSAKey       *rsakey;
    /* void (*sc_lib_close)(struct sc_lib_st *); */
} sc_lib;

typedef struct sc_cert_list_st {
    CK_ATTRIBUTE cert_attr[2]; /* types: 0=CKA_LABEL 1=CKA_ID */
    struct sc_cert_list_st *next;
} sc_cert_list;

typedef struct sc_pub_list_st {
    CK_ATTRIBUTE pub_attr[4]; /* types: 0=CKA_ID, 1=CKA_MODULUS_BITS, 2=CKA_MODULUS, 3=CKA_PUBLIC_EXPONENT */
    struct sc_pub_list_st *next;
} sc_pub_list;

void              sc_copy2clipboard(const char *data, int len);
void              sc_write_syslog(char *msg);
char             *sc_base64key(char *data, int len);

int               sc_init_library(void *f, int try_write_syslog, sc_lib *sclib, Filename *pkcs11_libfile);
void              sc_free_sclib(sc_lib *sclib);
CK_SESSION_HANDLE sc_get_session(void *f, int try_write_syslog, CK_FUNCTION_LIST_PTR fl,
                                 const char *token_label);

sc_cert_list     *sc_get_cert_list(sc_lib *sclib, CK_SESSION_HANDLE session, char *err_msg);
void              sc_free_cert_list(sc_cert_list *cert_list);
sc_pub_list      *sc_get_pub_list(sc_lib *sclib, CK_SESSION_HANDLE session, char *err_msg);
void              sc_free_pub_list(sc_pub_list *pub_list);

unsigned char    *sc_get_pub(void *f, int try_write_syslog, sc_lib *sclib,
                             const char *token_label, const char *cert_label,
                             char **algorithm, int *blob_len);
struct sc_pubkey_blob *sc_login_pub(void *f, int try_write_syslog, sc_lib *sclib,
                                    const char *token_label, const char *password);
unsigned char    *sc_sig(void *f, int try_write_syslog, sc_lib *sclib,
                         const char *token_label, const char *password_s,
                         char *sigdata, int sigdata_len, int *sigblob_len);

#endif
