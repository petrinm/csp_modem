

SOURCES = \
	csp_modem.c \
	cfg.c \
	fec.c \
	libfec/decode_rs_8.c \
	libfec/encode_rs_8.c \
	libfec/ccsds_tab.c \
	randomizer.c

LIBCSP=libcsp

csp_modem:
	gcc $(SOURCES) -o csp_modem -L$(LIBCSP)/build/ -lcsp -lpthread -lzmq  -lm -I$(LIBCSP)/include/ -I$(LIBCSP)/build/include/ -DCSP_DEBUG -std=c99
