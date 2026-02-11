CPP = formatter.cpp
TARFILES = $(CPP) Makefile README.md .gitignore tests/
HOMEPAGE = https://github.com/T3sT3ro/easy-stream-formatter

# Version from git tags (fallback to 0.0.0 if no tags)
VER_CURRENT = $(shell git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || echo "0.0.0")
VER_STR = v$(VER_CURRENT)

# Platform detection
OS := $(shell uname -s | tr '[:upper:]' '[:lower:]')
ARCH := $(shell uname -m)
BINARY_NAME = formatter-$(VER_CURRENT)-$(OS)-$(ARCH)

.PHONY: build install clean distclean dist release bump-patch bump-minor bump-major test

build: $(CPP)
	sed 's/@SVERSION/$(VER_STR)/; s/@VER/$(VER_CURRENT)/' $(CPP) | \
	sed 's#@HOMEPAGE#$(HOMEPAGE)#' | \
	g++ -xc++ -std=c++20 -O3 -static-libgcc -static-libstdc++ -o formatter -

install: build
	sudo cp -u formatter /usr/local/bin/

clean:
	rm -rf formatter

distclean: clean
	rm -rf dist/

dist: build $(TARFILES)
	@mkdir -p dist
	cp formatter dist/$(BINARY_NAME)
	tar -czf dist/formatter-$(VER_CURRENT)-src.tar.gz --transform 's,^,formatter-$(VER_CURRENT)/,' $(TARFILES)
	@echo "Created:"
	@echo "  dist/$(BINARY_NAME)           (binary)"
	@echo "  dist/formatter-$(VER_CURRENT)-src.tar.gz  (source)"

# Version bumping via git tags
bump-patch:
	@NEXT=$$(echo $(VER_CURRENT) | awk -F. '{print $$1"."$$2"."$$3+1}') && \
	git tag -a "v$$NEXT" -m "Release v$$NEXT" && \
	echo "Tagged: $(VER_STR) -> v$$NEXT"

bump-minor:
	@NEXT=$$(echo $(VER_CURRENT) | awk -F. '{print $$1"."$$2+1".0"}') && \
	git tag -a "v$$NEXT" -m "Release v$$NEXT" && \
	echo "Tagged: $(VER_STR) -> v$$NEXT"

bump-major:
	@NEXT=$$(echo $(VER_CURRENT) | awk -F. '{print $$1+1".0.0"}') && \
	git tag -a "v$$NEXT" -m "Release v$$NEXT" && \
	echo "Tagged: $(VER_STR) -> v$$NEXT"

release: dist
	@echo ""
	@echo "Release $(VER_STR) ready. Upload dist/* to GitHub Releases."
	@echo "Push tag: git push origin $(VER_STR)"

test: build
	@chmod +x tests/run_tests.sh
	@cd tests && ./run_tests.sh ../formatter
