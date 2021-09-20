.PHONY: all test deps clean
all: deps test
	$(MAKE) -C app
deps:
	$(MAKE) -C egalito
test:
	$(MAKE) -C test
clean:
	$(MAKE) -C app clean
	$(MAKE) -C test clean
