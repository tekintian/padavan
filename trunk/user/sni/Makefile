include ../../.config
include ../../rules.mk

CFLAGS	+= -I../../include -I../../linux-4.4.x/include -I../../linux-4.4.x/user_headers/include -I../../build/iptables-1.8.7/include

OBJS = libxt_sni.o

all: $(OBJS)
	$(CC) -shared -o libxt_sni.so $(OBJS) $(LDFLAGS)

romfs:
	$(INSTALL_DIR) $(ROMFSDIR)/usr/lib/xtables
	$(INSTALL_BIN) libxt_sni.so $(ROMFSDIR)/usr/lib/xtables/

clean:
	rm -f *.o *.so