

DEFINES = \
	-DCSP_DEBUG

SOURCES = \
	csp_modem.c \
	cfg.c \
	fec.c \
	libfec/decode_rs_8.c \
	libfec/encode_rs_8.c \
	libfec/ccsds_tab.c \
	randomizer.c

INCLUDES = \
	-Ilibcsp/include/ \
	-Ilibcsp/build/include/ \
	-Isuo/libsuo

LIB_DIRS = \
	-Llibcsp/build/ \
	-Lsuo/libsuo/build/

LIBS = \
	-lcsp \
	-lsuo-io \
	-lsuo-dsp \
	-lpthread \
	-lzmq \
	-lm


csp_modem:
	gcc $(SOURCES) -o csp_modem $(INCLUDES) $(LIB_DIRS) $(LIBS) -std=c99

clean:
	rm csp_modem
