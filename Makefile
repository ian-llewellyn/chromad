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
	cp chromad-logrotate.d-config /etc/logrotate.d/chromad

uninstall-config:
	rm -fr /etc/chromad
	rm -f /etc/logrotate.d/chromad

install-log:
	mkdir /var/log/chromad

uninstall-log:
	rm -fr /var/log/chromad

install: chromad install-config install-log
	cp chromad /usr/local/bin/

uninstall: uninstall-config uninstall-log
	rm -f /usr/local/bin/chromad

