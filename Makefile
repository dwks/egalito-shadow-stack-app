.PHONY: all test deps clean
all: deps test
	$(MAKE) -C app
deps:
	$(MAKE) -C egalito USE_KEYSTONE=1
test:
	$(MAKE) -C test
clean:
	$(MAKE) -C app clean
	$(MAKE) -C test clean
