all: build/libGom.a build/libMock.a build/test-gom run-tests

libGom_a_OBJS :=
libGom_a_OBJS += gom/gom-adapter.o
libGom_a_OBJS += gom/gom-collection.o
libGom_a_OBJS += gom/gom-enumerable.o
libGom_a_OBJS += gom/gom-enumerable-array.o
libGom_a_OBJS += gom/gom-property-set.o
libGom_a_OBJS += gom/gom-resource.o
libGom_a_OBJS += gom/gom-adapter-sqlite.o

libMock_a_OBJS :=
libMock_a_OBJS += mock/mock-gender.o
libMock_a_OBJS += mock/mock-occupation.o
libMock_a_OBJS += mock/mock-person.o

test_gom_OBJS :=
test_gom_OBJS += tests/test-gom.o

CFLAGS :=
CFLAGS += -I.
CFLAGS += -fPIC
CFLAGS += -Wall
CFLAGS += -Werror
CFLAGS += -Wold-style-definition
CFLAGS += -Wdeclaration-after-statement
CFLAGS += -Wredundant-decls
CFLAGS += -Wmissing-noreturn
CFLAGS += -Wcast-align
CFLAGS += -Wwrite-strings
CFLAGS += -Winline
CFLAGS += -Wformat-nonliteral
CFLAGS += -Wformat-security
CFLAGS += -Wswitch-enum
CFLAGS += -Wswitch-default
CFLAGS += -Winit-self
CFLAGS += -Wmissing-include-dirs
CFLAGS += -Wundef
CFLAGS += -Waggregate-return
CFLAGS += -Wmissing-format-attribute
CFLAGS += -Wnested-externs
CFLAGS += -Wshadow
CFLAGS += $(shell pkg-config --cflags gobject-2.0)

LDFLAGS :=
LDFLAGS += -lsqlite3
LDFLAGS += $(shell pkg-config --libs gobject-2.0)

%.o: %.c %.h Makefile
	@echo "  [CC]   $@"
	@$(CC) $(CFLAGS) -c -g -o $@.tmp $*.c
	@mv $@.tmp $@

tests/test-gom.o: tests/test-gom.c Makefile
	@echo "  [CC]   $@"
	@$(CC) $(CFLAGS) -c -g -o $@.tmp tests/test-gom.c
	@mv $@.tmp $@

build/libGom.a: $(libGom_a_OBJS) Makefile
	@mkdir -p build
	@echo "  [LD]   $@"
	@$(CC) $(LDFLAGS) -shared -fPIC -o $@ $(libGom_a_OBJS)

build/libMock.a: $(libMock_a_OBJS) Makefile
	@mkdir -p build
	@echo "  [LD]   $@"
	@$(CC) $(LDFLAGS) -shared -fPIC -o $@ $(libMock_a_OBJS)

build/test-gom: $(test_gom_OBJS) $(libGom_a_OBJS) $(libMock_a_OBJS) Makefile
	@mkdir -p build
	@echo "  [LD]   $@"
	@$(CC) -g -o $@ $(LDFLAGS) $(test_gom_OBJS) $(libGom_a_OBJS) $(libMock_a_OBJS)

run-tests: build/test-gom
	@echo "  [TEST] test-gom"
	@./build/test-gom

clean:
	rm -rf build gom/*.o mock/*.o tests/*.o
