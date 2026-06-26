#ifndef CIPHER_H
#define CIPHER_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

class VFSCipher {
private:
    static const uint8_t sbox[256];
    static const uint8_t rsbox[256];
    static const uint8_t Rcon[11];

    void key_expansion(const uint8_t* key, uint8_t* round_keys);
    void sub_bytes(uint8_t* state);
    void inv_sub_bytes(uint8_t* state);
    void shift_rows(uint8_t* state);
    void inv_shift_rows(uint8_t* state);
    void mix_columns(uint8_t* state);
    void inv_mix_columns(uint8_t* state);
    void add_round_key(uint8_t* state, const uint8_t* round_key);
    
    void cipher_encrypt_block(const uint8_t* in, uint8_t* out, const uint8_t* round_keys);
    void cipher_decrypt_block(const uint8_t* in, uint8_t* out, const uint8_t* round_keys);

public:
    VFSCipher() = default;

    // Encrypts/decrypts buffer using AES-128 CBC mode
    bool encrypt_aes128_cbc(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv, std::vector<uint8_t>& ciphertext);
    bool decrypt_aes128_cbc(const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv, std::vector<uint8_t>& plaintext);
};

#endif // CIPHER_H
