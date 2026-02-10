CPP = formatter.cpp
TARFILES = $(CPP) Makefile README.md
HOMEPAGE = https://github.com/T3sT3ro/easy-stream-formatter

# Version from git tags (fallback to 0.0.0 if no tags)
VER_CURRENT = $(shell git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || echo "0.0.0")
VER_STR = v$(VER_CURRENT)

.PHONY: build install clean distclean dist bump-patch bump-minor bump-major publish

build: $(CPP)
	sed 's/@SVERSION/$(VER_STR)/; s/@VER/$(VER_CURRENT)/' $(CPP) | \
	sed 's#@HOMEPAGE#$(HOMEPAGE)#' | \
	g++ -xc++ -std=c++20 -O3 -o formatter -

install: build
	sudo cp -u formatter /usr/local/bin/

clean:
	rm -rf formatter

distclean: clean
	rm -rf dist/

dist: $(TARFILES)
	@mkdir -p dist
	tar -czf dist/formatter-$(VER_CURRENT).tar.gz --transform 's,^,formatter-$(VER_CURRENT)/,' $(TARFILES)

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

publish: build dist
	@echo "Built and packaged $(VER_STR) -> dist/formatter-$(VER_CURRENT).tar.gz"
	@echo "Push tag with: git push origin $(VER_STR)"
