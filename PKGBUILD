# Maintainer: David Morgan <dmorgan81@gmail.com>

pkgname=dstatus
pkgver=1
pkgrel=1
pkgdesc="A simple status bar"
arch=('i686' 'x86_64')
license=('MIT')
depends=('libx11' 'alsa-lib', 'acpid')
source=(
    dstatus.c
    dstatus.h
    config.h
    config.mk
    Makefile
)
md5sums=(
    'SKIP'
    'SKIP'
    'SKIP'
    'SKIP'
    'SKIP'
)
build() {
    cd $srcdir
    sed -i 's/CPPFLAGS =/CPPFLAGS +=/g' config.mk
    sed -i 's/^CFLAGS = -g/#CFLAGS += -g/g' config.mk
    sed -i 's/^#CFLAGS = -std/CFLAGS += -std/g' config.mk
    sed -i 's/^LDFLAGS = -g/#LDFLAGS += -g/g' config.mk
    sed -i 's/^#LDFLAGS = -s/LDFLAGS += -s/g' config.mk
    sed -i "s/^VERSION =/VERSION = ${pkgver}/" config.mk
    make X11INC=/usr/include/X11 X11LIB=/usr/lib/X11
}

package() {
    cd $srcdir
    make PREFIX=/usr DESTDIR=$pkgdir install
}
