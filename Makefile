.PHONY: all regen deps clean
all: deps
	$(MAKE) -C app
deps:
	$(MAKE) -C egalito
clean:
	$(MAKE) -C app clean
