DOPESTDIN = ../../examples/dopestdin/linux/OBJ-x86_586-linux/dopestdin
DOPE      = ../linux/OBJ-x86_586-linux/linuxdope

all:
	@echo "Available targets:"
	@echo "  test - perform widget layout test"
	@echo "  init - make current output the new template"
	@echo "  show - show current and desired output (toggle with space, exit with q)"


test:

	@# start DOpE server
	@$(DOPE) >/dev/null &
	@sleep 1
	
	@# generate test layout
	@$(DOPESTDIN) < layout.dpe
	@sleep 2
	
	@# make DOpE to dump the screen #
	@killall -s USR1 linuxdope
	@sleep 1

	@killall linuxdope

	@# number of changed pixels is not accurate #
	@echo `diff screen.pnm template.pnm | grep ">" | wc -l` pixels changed


init:
	cp screen.pnm template.pnm


show:
	feh screen.pnm template.pnm
