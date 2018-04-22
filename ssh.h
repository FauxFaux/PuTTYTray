#include <string.h>

#include "puttymem.h"
#include "network.h"
#include "int64.h"

struct ssh_channel;

extern void sshfwd_close(struct ssh_channel *c);
extern int sshfwd_write(struct ssh_channel *c, char *, int);
extern void sshfwd_unthrottle(struct ssh_channel *c, int bufsize);

/*
 * Useful thing.
 */
#ifndef lenof
#define lenof(x) ((sizeof((x))) / (sizeof(*(x))))
#endif

#define SSH_CIPHER_IDEA 1
#define SSH_CIPHER_DES 2
#define SSH_CIPHER_3DES 3
#define SSH_CIPHER_BLOWFISH 6

#ifdef MSCRYPTOAPI
#define APIEXTRA 8
#else
#define APIEXTRA 0
#endif

#ifndef BIGNUM_INTERNAL
typedef void *Bignum;
#endif

struct RSAKey {
  int bits;
  int bytes;
#ifdef MSCRYPTOAPI
  unsigned long exponent;
  unsigned char *modulus;
#else
  Bignum modulus;
  Bignum exponent;
  Bignum private_exponent;
  Bignum p;
  Bignum q;
  Bignum iqmp;
#endif
  char *comment;
};

struct dss_key {
  Bignum p, q, g, y, x;
};

int makekey(unsigned char *data,
            struct RSAKey *result,
            unsigned char **keystr,
            int order);
int makeprivate(unsigned char *data, struct RSAKey *result);
void rsaencrypt(unsigned char *data, int length, struct RSAKey *key);
Bignum rsadecrypt(Bignum input, struct RSAKey *key);
void rsasign(unsigned char *data, int length, struct RSAKey *key);
void rsasanitise(struct RSAKey *key);
int rsastr_len(struct RSAKey *key);
void rsastr_fmt(char *str, struct RSAKey *key);
void rsa_fingerprint(char *str, int len, struct RSAKey *key);
int rsa_verify(struct RSAKey *key);
unsigned char *rsa_public_blob(struct RSAKey *key, int *len);
int rsa_public_blob_len(void *data);
void freersakey(struct RSAKey *key);

typedef unsigned int word32;
typedef unsigned int uint32;

unsigned long crc32(const void *s, size_t len);
unsigned long crc32_update(unsigned long crc_input, const void *s, size_t len);

/* SSH CRC compensation attack detector */
int detect_attack(unsigned char *buf, uint32 len, unsigned char *IV);

typedef struct {
  uint32 h[4];
} MD5_Core_State;

struct MD5Context {
#ifdef MSCRYPTOAPI
  unsigned long hHash;
#else
  MD5_Core_State core;
  unsigned char block[64];
  int blkused;
  uint32 lenhi, lenlo;
#endif
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context,
               unsigned char const *buf,
               unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);

typedef struct {
  uint32 h[5];
  unsigned char block[64];
  int blkused;
  uint32 lenhi, lenlo;
} SHA_State;
void SHA_Init(SHA_State *s);
void SHA_Bytes(SHA_State *s, void *p, int len);
void SHA_Final(SHA_State *s, unsigned char *output);
void SHA_Simple(void *p, int len, unsigned char *output);

void hmac_sha1_simple(
    void *key, int keylen, void *data, int datalen, unsigned char *output);

typedef struct {
  uint64 h[8];
  unsigned char block[128];
  int blkused;
  uint32 len[4];
} SHA512_State;
void SHA512_Init(SHA512_State *s);
void SHA512_Bytes(SHA512_State *s, const void *p, int len);
void SHA512_Final(SHA512_State *s, unsigned char *output);
void SHA512_Simple(const void *p, int len, unsigned char *output);

struct ssh_cipher {
  void (*sesskey)(unsigned char *key); /* for ssh 1 */
  void (*encrypt)(unsigned char *blk, int len);
  void (*decrypt)(unsigned char *blk, int len);
  int blksize;
};

struct ssh2_cipher {
  void (*setcsiv)(unsigned char *key);  /* for ssh 2 */
  void (*setcskey)(unsigned char *key); /* for ssh 2 */
  void (*setsciv)(unsigned char *key);  /* for ssh 2 */
  void (*setsckey)(unsigned char *key); /* for ssh 2 */
  void (*encrypt)(unsigned char *blk, int len);
  void (*decrypt)(unsigned char *blk, int len);
  char *name;
  int blksize;
  int keylen;
};

struct ssh2_ciphers {
  int nciphers;
  const struct ssh2_cipher *const *list;
};

struct ssh_mac {
  void (*setcskey)(unsigned char *key);
  void (*setsckey)(unsigned char *key);
  void (*generate)(unsigned char *blk, int len, unsigned long seq);
  int (*verify)(unsigned char *blk, int len, unsigned long seq);
  char *name;
  int len;
};

struct ssh_kex {
  /*
   * Plugging in another KEX algorithm requires structural chaos,
   * so it's hard to abstract them into nice little structures
   * like this. Hence, for the moment, this is just a
   * placeholder. I claim justification in the fact that OpenSSH
   * does this too :-)
   */
  char *name;
};

struct ssh_signkey {
  void *(*newkey)(char *data, int len);
  void (*freekey)(void *key);
  char *(*fmtkey)(void *key);
  unsigned char *(*public_blob)(void *key, int *len);
  unsigned char *(*private_blob)(void *key, int *len);
  void *(*createkey)(unsigned char *pub_blob,
                     int pub_len,
                     unsigned char *priv_blob,
                     int priv_len);
  void *(*openssh_createkey)(unsigned char **blob, int *len);
  int (*openssh_fmtkey)(void *key, unsigned char *blob, int len);
  char *(*fingerprint)(void *key);
  int (*verifysig)(void *key, char *sig, int siglen, char *data, int datalen);
  unsigned char *(*sign)(void *key, char *data, int datalen, int *siglen);
  char *name;
  char *keytype; /* for host key cache */
};

struct ssh_compress {
  char *name;
  void (*compress_init)(void);
  int (*compress)(unsigned char *block,
                  int len,
                  unsigned char **outblock,
                  int *outlen);
  void (*decompress_init)(void);
  int (*decompress)(unsigned char *block,
                    int len,
                    unsigned char **outblock,
                    int *outlen);
  int (*disable_compression)(void);
};

struct ssh2_userkey {
  const struct ssh_signkey *alg; /* the key algorithm */
  void *data;                    /* the key data */
  char *comment;                 /* the key comment */
};

extern const struct ssh_cipher ssh_3des;
extern const struct ssh_cipher ssh_des;
extern const struct ssh_cipher ssh_blowfish_ssh1;
extern const struct ssh2_ciphers ssh2_3des;
extern const struct ssh2_ciphers ssh2_des;
extern const struct ssh2_ciphers ssh2_aes;
extern const struct ssh2_ciphers ssh2_blowfish;
extern const struct ssh_kex ssh_diffiehellman;
extern const struct ssh_kex ssh_diffiehellman_gex;
extern const struct ssh_signkey ssh_dss;
extern const struct ssh_signkey ssh_rsa;
extern const struct ssh_mac ssh_md5;
extern const struct ssh_mac ssh_sha1;
extern const struct ssh_mac ssh_sha1_buggy;

/*
 * PuTTY version number formatted as an SSH version string.
 */
extern char sshver[];

/*
 * Gross hack: pscp will try to start SFTP but fall back to scp1 if
 * that fails. This variable is the means by which scp.c can reach
 * into the SSH code and find out which one it got.
 */
extern int ssh_fallback_cmd;

#ifndef MSCRYPTOAPI
void SHATransform(word32 *digest, word32 *data);
#endif

int random_byte(void);
void random_add_noise(void *noise, int length);
void random_add_heavynoise(void *noise, int length);

void logevent(char *);
void *new_sock_channel(
    Socket s); // allocates and register a new channel for port forwarding
void ssh_send_port_open(void *channel, char *hostname, int port, char *org);

Bignum copybn(Bignum b);
Bignum bn_power_2(int n);
void bn_restore_invariant(Bignum b);
Bignum bignum_from_long(unsigned long n);
void freebn(Bignum b);
Bignum modpow(Bignum base, Bignum exp, Bignum mod);
Bignum modmul(Bignum a, Bignum b, Bignum mod);
void decbn(Bignum n);
extern Bignum Zero, One;
Bignum bignum_from_bytes(unsigned char *data, int nbytes);
int ssh1_read_bignum(unsigned char *data, Bignum *result);
int bignum_bitcount(Bignum bn);
int ssh1_bignum_length(Bignum bn);
int ssh2_bignum_length(Bignum bn);
int bignum_byte(Bignum bn, int i);
int bignum_bit(Bignum bn, int i);
void bignum_set_bit(Bignum bn, int i, int value);
int ssh1_write_bignum(void *data, Bignum bn);
Bignum biggcd(Bignum a, Bignum b);
unsigned short bignum_mod_short(Bignum number, unsigned short modulus);
Bignum bignum_add_long(Bignum number, unsigned long addend);
Bignum bigmul(Bignum a, Bignum b);
Bignum bigmuladd(Bignum a, Bignum b, Bignum addend);
Bignum bigdiv(Bignum a, Bignum b);
Bignum bigmod(Bignum a, Bignum b);
Bignum modinv(Bignum number, Bignum modulus);
Bignum bignum_bitmask(Bignum number);
Bignum bignum_rshift(Bignum number, int shift);
int bignum_cmp(Bignum a, Bignum b);
char *bignum_decimal(Bignum x);

void dh_setup_group1(void);
void dh_setup_group(Bignum pval, Bignum gval);
void dh_cleanup(void);
Bignum dh_create_e(int nbits);
Bignum dh_find_K(Bignum f);

int loadrsakey(char *filename, struct RSAKey *key, char *passphrase);
int rsakey_encrypted(char *filename, char **comment);
int rsakey_pubblob(char *filename, void **blob, int *bloblen);

int saversakey(char *filename, struct RSAKey *key, char *passphrase);

void base64_encode_atom(unsigned char *data, int n, char *out);

/* ssh2_load_userkey can return this as an error */
extern struct ssh2_userkey ssh2_wrong_passphrase;
#define SSH2_WRONG_PASSPHRASE (&ssh2_wrong_passphrase)

int ssh2_userkey_encrypted(char *filename, char **comment);
struct ssh2_userkey *ssh2_load_userkey(char *filename, char *passphrase);
char *ssh2_userkey_loadpub(char *filename, char **algorithm, int *pub_blob_len);
int ssh2_save_userkey(char *filename,
                      struct ssh2_userkey *key,
                      char *passphrase);

enum
{
  SSH_KEYTYPE_UNOPENABLE,
  SSH_KEYTYPE_UNKNOWN,
  SSH_KEYTYPE_SSH1,
  SSH_KEYTYPE_SSH2,
  SSH_KEYTYPE_OPENSSH,
  SSH_KEYTYPE_SSHCOM
};
int key_type(char *filename);
char *key_type_to_str(int type);

int import_possible(int type);
int import_target_type(int type);
int import_encrypted(char *filename, int type, char **comment);
int import_ssh1(char *filename, int type, struct RSAKey *key, char *passphrase);
struct ssh2_userkey *import_ssh2(char *filename, int type, char *passphrase);
int export_ssh1(char *filename, int type, struct RSAKey *key, char *passphrase);
int export_ssh2(char *filename,
                int type,
                struct ssh2_userkey *key,
                char *passphrase);

void des3_decrypt_pubkey(unsigned char *key, unsigned char *blk, int len);
void des3_encrypt_pubkey(unsigned char *key, unsigned char *blk, int len);
void des3_decrypt_pubkey_ossh(unsigned char *key,
                              unsigned char *iv,
                              unsigned char *blk,
                              int len);
void des3_encrypt_pubkey_ossh(unsigned char *key,
                              unsigned char *iv,
                              unsigned char *blk,
                              int len);
void aes256_encrypt_pubkey(unsigned char *key, unsigned char *blk, int len);
void aes256_decrypt_pubkey(unsigned char *key, unsigned char *blk, int len);

/*
 * For progress updates in the key generation utility.
 */
#define PROGFN_INITIALISE 1
#define PROGFN_LIN_PHASE 2
#define PROGFN_EXP_PHASE 3
#define PROGFN_PHASE_EXTENT 4
#define PROGFN_READY 5
#define PROGFN_PROGRESS 6
typedef void (*progfn_t)(void *param, int action, int phase, int progress);

int rsa_generate(struct RSAKey *key, int bits, progfn_t pfn, void *pfnparam);
int dsa_generate(struct dss_key *key, int bits, progfn_t pfn, void *pfnparam);
Bignum primegen(int bits,
                int modulus,
                int residue,
                Bignum factor,
                int phase,
                progfn_t pfn,
                void *pfnparam);

/*
 * zlib compression.
 */
void zlib_compress_init(void);
void zlib_decompress_init(void);
int zlib_compress_block(unsigned char *block,
                        int len,
                        unsigned char **outblock,
                        int *outlen);
int zlib_decompress_block(unsigned char *block,
                          int len,
                          unsigned char **outblock,
                          int *outlen);

/*
 * SSH1 agent messages.
 */
#define SSH1_AGENTC_REQUEST_RSA_IDENTITIES 1
#define SSH1_AGENT_RSA_IDENTITIES_ANSWER 2
#define SSH1_AGENTC_RSA_CHALLENGE 3
#define SSH1_AGENT_RSA_RESPONSE 4
#define SSH1_AGENTC_ADD_RSA_IDENTITY 7
#define SSH1_AGENTC_REMOVE_RSA_IDENTITY 8
#define SSH1_AGENTC_REMOVE_ALL_RSA_IDENTITIES 9 /* openssh private? */

/*
 * Messages common to SSH1 and OpenSSH's SSH2.
 */
#define SSH_AGENT_FAILURE 5
#define SSH_AGENT_SUCCESS 6

/*
 * OpenSSH's SSH2 agent messages.
 */
#define SSH2_AGENTC_REQUEST_IDENTITIES 11
#define SSH2_AGENT_IDENTITIES_ANSWER 12
#define SSH2_AGENTC_SIGN_REQUEST 13
#define SSH2_AGENT_SIGN_RESPONSE 14
#define SSH2_AGENTC_ADD_IDENTITY 17
#define SSH2_AGENTC_REMOVE_IDENTITY 18
#define SSH2_AGENTC_REMOVE_ALL_IDENTITIES 19

/*
 * Need this to warn about support for the original SSH2 keyfile
 * format.
 */
void old_keyfile_warning(void);
