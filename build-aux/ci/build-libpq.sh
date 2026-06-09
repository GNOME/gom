#!/bin/sh

build_libpq_main() {
  : "${CI_PROJECT_DIR:=$(pwd)}"
  : "${POSTGRESQL_VERSION:=14.23}"
  : "${POSTGRESQL_PREFIX:=${CI_PROJECT_DIR}/_postgresql-prefix}"

  archive="postgresql-${POSTGRESQL_VERSION}.tar.bz2"
  url="https://ftp.postgresql.org/pub/source/v${POSTGRESQL_VERSION}/${archive}"
  cache_dir="${CI_PROJECT_DIR}/_postgresql-cache"
  source_dir="${CI_PROJECT_DIR}/_postgresql-src"
  build_dir="${CI_PROJECT_DIR}/_postgresql-build"

  mkdir -p "${cache_dir}"

  if [ ! -f "${cache_dir}/${archive}" ]; then
    if command -v curl >/dev/null 2>&1; then
      curl --fail --location --retry 3 --output "${cache_dir}/${archive}" "${url}"
      curl --fail --location --retry 3 --output "${cache_dir}/${archive}.sha256" "${url}.sha256"
    else
      wget -O "${cache_dir}/${archive}" "${url}"
      wget -O "${cache_dir}/${archive}.sha256" "${url}.sha256"
    fi
  fi

  ( cd "${cache_dir}" && sha256sum -c "${archive}.sha256" )

  rm -rf "${source_dir}" "${build_dir}" "${POSTGRESQL_PREFIX}"
  mkdir -p "${source_dir}" "${build_dir}" "${POSTGRESQL_PREFIX}"
  tar -xf "${cache_dir}/${archive}" -C "${source_dir}" --strip-components=1

  (
    cd "${build_dir}"
    "${source_dir}/configure" \
      --prefix="${POSTGRESQL_PREFIX}" \
      --without-icu \
      --without-openssl \
      --without-readline \
      --without-zlib

    make -C src/interfaces/libpq
    make -C src/bin/pg_config
    make -C src/include install
    make -C src/interfaces/libpq install
    make -C src/bin/pg_config install
  )

  export PATH="${POSTGRESQL_PREFIX}/bin:${PATH}"
  export LD_LIBRARY_PATH="${POSTGRESQL_PREFIX}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
  export PKG_CONFIG_PATH="${POSTGRESQL_PREFIX}/lib/pkgconfig${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"
}

build_libpq_main "$@"
unset -f build_libpq_main
