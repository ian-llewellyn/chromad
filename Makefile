CC = cc
CHROMA_DAEMON_BIN = chromad
CHROMA_DAEMON_SRC = chromad.c

all: chromad

chromad:
	${CC} ${CHROMA_DAEMON_SRC} -o ${CHROMA_DAEMON_BIN} -lm

clean:
	rm -f ${CHROMA_DAEMON_BIN}

install-service: install
	cp chromad-init.d-script /etc/init.d/chromad
	chkconfig --add chromad

uninstall-service:
	chkconfig --del chromad
	rm -f /etc/init.d/chromad

install-config:
	mkdir /etc/chromad
	cp alert-chroma.conf /etc/chromad/
	cp chroma-channels.conf /etc/chromad/

install-log:
	mkdir /var/log/chromad

install: chromad install-config install-log
	cp chromad /usr/local/bin/

uninstall:
	rm -fr /etc/chromad
	rm -fr /var/log/chromad
	rm -f /usr/local/bin/chromad

