Title: Sample: SQLite Encryption Setup

This sample focuses on opening an encrypted SQLite repository. It does not
define entities or run queries; the only important part is passing encryption
key bytes through [class@Gom.DriverOptions] before opening the driver.

The key bytes are not stored in the database URI. The SQLite backend applies
them when a connection is opened, and [ctor@Gom.Repository.new] fails if the
database cannot be read with that key.

If the key comes from a user password or passphrase, derive the database key
with a password KDF such as Argon2id, scrypt, or PBKDF2-HMAC-SHA256. Do not use
the raw password bytes directly as the SQLite encryption key. Keep any KDF salt
and parameters somewhere that can be read before opening the encrypted database,
for example in a sidecar metadata file.

## Setup Flow

```c
g_autoptr(GBytes) key = NULL;
g_autoptr(GomDriverOptions) options = NULL;
g_autoptr(GomDriver) driver = NULL;
g_autoptr(GomRepository) repository = NULL;
g_autoptr(GError) error = NULL;

/* Load or derive the raw database key bytes before opening the driver.
 *
 * For a user password, this should be the output of a KDF, not the password
 * itself.
 */
key = load_or_derive_encryption_key (...);

options = gom_driver_options_new ();
gom_driver_options_set_encryption_key (options, key);

driver = gom_driver_open_with_options ("file:///tmp/encrypted.db", options, &error);
if (driver == NULL)
  {
    /* The URI could not be parsed, the SQLite driver is unavailable, or the
     * backend could not create the driver.
     */
    return;
  }

repository = dex_await_object (gom_repository_new (driver, registry, NULL), &error);
if (repository == NULL)
  {
    /* This is where an invalid encryption key is normally reported. The SQLite
     * connection is opened for repository initialization, the key is applied,
     * and the backend verifies that the database can be read.
     */
    if (g_error_matches (error, GOM_ERROR, GOM_ERROR_INVALID_ENCRYPTION_KEY))
      {
        /* Prompt for a different password or load a different key. */
      }

    return;
  }

/* Use the repository normally. */
```

## Invalid Keys

To handle a bad key, treat repository creation as the operation that verifies
the encrypted database:

```c
repository = dex_await_object (gom_repository_new (driver, registry, NULL), &error);
if (repository == NULL)
  {
    if (g_error_matches (error, GOM_ERROR, GOM_ERROR_INVALID_ENCRYPTION_KEY))
      {
        /* Prompt for a different password or load a different key. */
      }
    else
      {
        /* Report the failure to the user. */
      }

    /* Do not continue with this repository instance. */
    g_clear_error (&error);
  }
```

When trying another key, create a new [class@Gom.DriverOptions], open a new
[class@Gom.Driver], and call [ctor@Gom.Repository.new] again.

## Notes

- The SQLite backend must be built and available. `file:` URIs are the usual
  SQLite URI filename form; `sqlite` is also registered as a libgom backend
  scheme, but the URI is still passed through to SQLite for opening.
- [method@Gom.DriverOptions.set_encryption_key] keeps the key as bytes, not as
  URI text.
- Wrong-key handling belongs on repository open. The driver object can be
  created before the SQLite connection is verified.
- `GBytes` can wrap secure memory with [ctor@GLib.Bytes.new_with_free_func].
  Applications with stricter memory-hygiene requirements can allocate derived
  keys from a secure allocator and use its clearing free function for the
  `GBytes` passed to libgom.
