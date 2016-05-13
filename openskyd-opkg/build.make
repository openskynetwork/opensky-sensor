#!/usr/bin/make -f

VERSION=1.0.7
DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

help:
	@echo Targets:
	@echo ' build:   extract and build daemon'
	@echo ' prepare: prepare package'
	@echo ' package: build package'
	@echo ' all:     do all'

all: build prepare config package

build:
	rm -rf openskyd-$(VERSION)
	tar xzf openskyd-$(VERSION).tar.gz
	cd openskyd-$(VERSION) && ./configure --host=arm-angstrom-linux-gnueabi \
		--prefix=/usr --sysconfdir=/etc --libdir=/usr/lib/openskyd \
		--enable-silent-rules \
		--enable-input=network --disable-standalone --disable-talkback \
		--without-systemd --without-pacman --without-check \
		CFLAGS="-O3 -g -Wall -mtune=native"
	make -C openskyd-$(VERSION)

prepare:
	rm -rf pkg
	mkdir pkg
	make -C openskyd-$(VERSION) DESTDIR=$(DIR)/pkg install
	cp -r CONTROL pkg
	mkdir -p pkg/lib/systemd/system/
	cp openskyd.service pkg/lib/systemd/system/

config:
	cp openskyd.conf pkg/etc/openskyd.conf

package:
	sh opkg-build -O pkg

clean:
	-rm -rf pkg openskyd-$(VERSION) openskyd-net_$(VERSION)_armv7a-vfp-neon.opk

