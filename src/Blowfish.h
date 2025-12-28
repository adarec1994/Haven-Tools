#pragma once
#include <cstdint>
#include <vector>
#include <string>

class Blowfish {
public:
    Blowfish(const uint8_t* key, size_t keyLen);
    Blowfish(const std::vector<uint8_t>& key);
    Blowfish(const std::string& key);
    
    void encrypt(uint32_t& left, uint32_t& right) const;
    void decrypt(uint32_t& left, uint32_t& right) const;
    
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& data) const;
    
    static constexpr size_t BLOCK_SIZE = 8;
    
private:
    uint32_t F(uint32_t x) const;
    
    uint32_t P[18];
    uint32_t S[4][256];
};