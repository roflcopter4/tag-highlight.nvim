#
# If your `make` is actually BSD make, you're not running Windows.
#

.MAIN: all
.PHONY: all

.if 1

all:
	@echo BSD make eh? >&2
	sh ./build.sh

.endif
