#include <bootutil/sign_key.h>
#include <mcuboot_config/mcuboot_config.h>

#if defined(MCUBOOT_SIGN_ED25519)
extern const unsigned char ed25519_pub_key[];
extern const unsigned int ed25519_pub_key_len;
const struct bootutil_key bootutil_keys[] = {
  {
    .key = ed25519_pub_key,
    .len = &ed25519_pub_key_len,
  },
};
const int bootutil_key_cnt = 1;
#endif /* MCUBOOT_SIGN_ED25519 */

#if defined(MCUBOOT_ENCRYPT_X25519)
extern const unsigned char enc_priv_key[];
extern const unsigned int enc_priv_key_len;
const struct bootutil_key bootutil_enc_key = {
    .key = enc_priv_key,
    .len = &enc_priv_key_len,
};
#endif
