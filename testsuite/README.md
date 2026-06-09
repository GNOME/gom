# PostgreSQL Test Setup

`libgom` ships a PostgreSQL conformance test target in `testsuite/test-gom-pgsql.c`.
It is gated behind the Meson option `-Dpgsql-tests=true`.

This document assumes:

- Fedora 44 host
- `postgresql-server` is installed
- the cluster has already been initialized with `initdb`
- the PostgreSQL service is running locally

## Expected test database

The Meson test wiring defaults to this URI:

```text
postgresql:///libgom?user=<current-login-user>
```

That means the local PostgreSQL cluster should have:

- a role named after your current Unix login user
- a database named `libgom` owned by that role
- local socket authentication that allows the current Unix user to connect as that PostgreSQL role

If you want a different database name or URI, override it at configure time with:

```sh
meson setup build \
  -Dpgsql-tests=true \
  -Dpgsql-tests-dbname=libgom \
  -Dpgsql-tests-uri='postgresql:///libgom?user=youruser'
```

## Fedora 44 setup

1. Make sure the PostgreSQL server is running:

```sh
sudo systemctl enable --now postgresql
```

2. Create a PostgreSQL role for your current Unix login user:

```sh
sudo -u postgres createuser --login --createdb "$USER"
```

If you already created the role, skip this step.

3. Create the test database owned by that role:

```sh
sudo -u postgres createdb -O "$USER" libgom
```

If the database already exists, skip this step.

4. Verify local socket access works:

```sh
psql "postgresql:///libgom?user=$USER" -X -v ON_ERROR_STOP=1 -qAtc 'SELECT current_user, current_database()'
```

You should see your login user and `libgom`.

5. Run the PGSQL conformance test:

```sh
meson test build test-gom-pgsql
```

## Tablespace notes

The PGSQL test harness also exposes `GOM_PGSQL_TEST_TABLESPACE` for checking an
expected default tablespace. Leave it unset unless you have explicitly configured
the test database to use a non-default tablespace.

If you do want to check a custom tablespace, configure the Meson option:

```sh
meson setup build -Dpgsql-tests=true -Dpgsql-tests-tablespace=your_tablespace
```

## Troubleshooting

- If `psql "postgresql:///libgom?user=$USER"` fails, the role usually does not exist yet or peer authentication is not allowing that login.
- If the test skips immediately, make sure `-Dpgsql-tests=true` was enabled when configuring the build.
- If you changed the database name, update `-Dpgsql-tests-dbname` or `-Dpgsql-tests-uri` accordingly.
