# crypt()

## NAME
**crypt** - encrypt a password string using portable MD5 or SHA256 hashing

## SYNOPSIS
~~~cxx
string crypt( string str, string seed );
~~~

## DESCRIPTION

Cryptographically hashes a password string **str** using a secure algorithm determined by the **seed** format. The result is a hashed password suitable for storage and validation.

### Portable Multi-Algorithm Support (Neolith Extension)

Neolith implements `crypt()` using OpenSSL-based portable hashing, supporting **two algorithms**:

#### MD5 Format: `$1$<salt>$<hash>` (Backward Compatibility)

For compatibility with passwords saved in older LPMud drivers, the function detects and validates MD5-hashed passwords:

- **Triggered when**: seed begins with `$1$` (e.g., `"$1$oldsalt$AbCdEfGhIjKlMnOpQrStUv"`)
- **Hash algorithm**: OpenSSL EVP_md5() for cryptographic compatibility
- **Encoding**: Base64-style (crypt alphabet): `./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz`
- **Example**: 
  ```lpc
  crypt("password", "$1$oldsalt$") 
    → "$1$oldsalt$AbCdEfGhIjKlMnOpQrStUv"
  ```

#### SHA256 Format: `$5$<salt>$<hash>` (Recommended Default)

For new passwords, Neolith uses the more secure SHA256 algorithm:

- **Triggered when**: seed begins with `$5$` or is plain text / empty / < 2 characters
- **Hash algorithm**: OpenSSL EVP_sha256() for security
- **Encoding**: Hexadecimal (32 bytes → 64 characters)
- **Example**:
  ```lpc
  crypt("password", "ab")          → "$5$ab$a1b2c3d4e5f6..."
  crypt("password", "$5$newsalt$") → "$5$newsalt$a1b2c3d4..."
  crypt("password", "")            → "$5$randomXX$a1b2c3d4..."
  ```

### Algorithm Selection Logic

The algorithm is automatically determined from the **seed** parameter:

| Seed Format | Algorithm | Use Case |
|---|---|---|
| `$1$<salt>$` | MD5 | Validating old driver passwords |
| `$5$<salt>$` | SHA256 | Validating/creating new passwords |
| Plain text (2+ chars) | SHA256 | Quick hashing with provided salt |
| `""` or `0` | SHA256 | Auto-generate random salt |

### Parameters

- **str** - The password string to hash. Can be any string, including empty string.
- **seed** - The salt and algorithm specification:
  - `$1$<salt>$` - Use MD5 with specified salt (backward compat)
  - `$5$<salt>$` - Use SHA256 with specified salt (recommended)
  - Plain text - Use SHA256 with this salt (plain text)
  - Empty string or 0 - Use SHA256 with auto-generated random salt

### Return Value

Returns a hashed password string in one of these formats:

- `$1$<salt>$<22-char-hash>` - MD5 format (validation of old passwords)
- `$5$<salt>$<64-char-hash>` - SHA256 format (new passwords)
- `$0$<salt>$<16-char-hash>` - Fallback format (systems without OpenSSL)

The salt is always included in the result, making the hash reusable as a seed for validation.

## EXAMPLES

### Creating a New Password (SHA256)

```lpc
string password_hash = crypt("user_password_input", "");
// Returns: $5$xxxxxxxx$a1b2c3d4e5f6g7h8...
save_password(user, password_hash);
```

### Validating an Old MD5 Password

```lpc
// Stored password from old driver
string stored_hash = user->query_password();  // Might be: $1$salt$AbCdEf...

// User enters password
string entered = read_line("Password: ");

// Validate
if (crypt(entered, stored_hash) == stored_hash) {
    write("Password correct!");
    // Optionally migrate to SHA256:
    user->set_password(crypt(entered, ""));
}
```

### Validating a SHA256 Password

```lpc
string stored_hash = user->query_password();  // Format: $5$...

if (crypt(entered_password, stored_hash) == stored_hash) {
    write("Password correct!");
}
```

### Checking Password Algorithm Type

```lpc
string hash = user->query_password();

switch(hash[0..1]) {
  case "$1": write("Old MD5 password - should migrate"); break;
  case "$5": write("Modern SHA256 password"); break;
  case "$0": write("Fallback hash (no OpenSSL)"); break;
}
```

### Batch Migration of Old Passwords

```lpc
void migrate_password(object user, string password) {
    // Only called after manual password verification
    string new_hash = crypt(password, "");  // SHA256
    user->set_password(new_hash);
    write("Password upgraded to secure format.");
}
```

## COMPATIBILITY NOTES

### Backward Compatibility

- The function maintains full compatibility with MD5-hashed passwords from older LPMud drivers
- Existing `crypt()` calls using plain salt continue to work unchanged
- No changes needed to existing password validation code

### Neolith Extension

This implementation extends the standard POSIX `crypt()` function with:
- **Portable MD5/SHA256 support** across all platforms (Linux, Windows, macOS)
- **Algorithm detection** from salt format
- **No platform dependencies** on system `libcrypt` or DES implementation
- **OpenSSL-based hashing** for consistent, cryptographically sound results across systems

### Migration Path for Old Drivers

If upgrading from an old LPMud driver:
1. Existing MD5 passwords (`$1$...`) continue to validate without modification
2. Create a password-change request mechanism in your mudlib
3. When users change passwords, store new hashes as SHA256 (`$5$...`)
4. Over time, automatically migrate users to SHA256 when they log in

## SECURITY RECOMMENDATIONS

- **For new passwords**: Always use plain text seed or empty string to trigger SHA256 with auto-generated random salt
- **For migration**: Prompt users to change passwords during software upgrade to migrate from MD5 to SHA256
- **Never reuse salts**: Let the system auto-generate unique random salts
- **Avoid MD5 for new systems**: Only use MD5 format when validating legacy passwords
