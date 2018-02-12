INKSCAPE ?= inkscape
PNGWIDTH ?= 640
PNGOPTS  ?= --export-area-drawing
PDFOPTS  ?= --export-text-to-path

IMAGES := l4-caps-basic l4re-basic io-overview

SVG_PATH ?= ../../../../doc/l4-clipart

vpath %.svg $(SVG_PATH)

PDF_IMAGES := $(addsuffix .pdf,$(IMAGES))
PNG_IMAGES := $(addsuffix .png,$(IMAGES))


all: $(PDF_IMAGES) $(PNG_IMAGES)


%.png: %.svg
	$(INKSCAPE) $(PNGOPTS) --export-width=$(PNGWIDTH) --export-png=$@ -f $<

%.pdf: %.svg
	$(INKSCAPE) $(PDFOPTS) -A $@ -f $<
