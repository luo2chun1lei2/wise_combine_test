#include "integrity/sha256.hpp"
#include <iostream>
int main() {
  if (wise::integrity::sha256_hex("") != "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") return 1;
  if (wise::integrity::sha256_hex("abc") != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") return 1;
  std::cout << "sha256: all tests passed\n";
}
