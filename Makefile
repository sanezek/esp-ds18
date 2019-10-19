# Simple makefile for simple example
PROGRAM=simple
EXTRA_COMPONENTS = extras/onewire extras/ds18b20 extras/mbedtls extras/httpd


EXTRA_CFLAGS=-DLWIP_HTTPD_CGI=1 -DLWIP_HTTPD_SSI=1 -I./fsdata

#Enable debugging
#EXTRA_CFLAGS+=-DLWIP_DEBUG=1 -DHTTPD_DEBUG=LWIP_DBG_ON
include ../../common.mk

html:
	@echo "Generating fsdata.."
	cd fsdata && ./makefsdata
