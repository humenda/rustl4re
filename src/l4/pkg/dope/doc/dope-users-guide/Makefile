L4DIR ?= ../../../..

GOSH   = $(L4DIR)/tool/gosh/gosh

SRC_TXT = dope-users-guide.txt

all: $(SRC_TXT:.txt=.pdf)

cleanall clean:
	rm -f $(SRC_TXT:.txt=.pdf) *.aux *.log

%.tex: %.txt
	$(GOSH) $< > $(@:.pdf=.tex) 2> /dev/null

%.pdf: %.tex $(INFILES) $(IMGFILES)
	pdflatex $<
	pdflatex $<
	pdflatex $<
