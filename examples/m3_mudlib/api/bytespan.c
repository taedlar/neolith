#pragma save_binary

string embedded_null_octal() {
  return "ab\0cd";
}

string embedded_null_hex() {
  return "ab\x00yz";
}
