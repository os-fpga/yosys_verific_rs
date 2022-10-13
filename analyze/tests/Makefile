# Copyright (C) 2022 RapidSilicon..

TESTS = Trivium_Module\
		b12\
		b17\
		gpio

define test_tpl =
$(1):
	@set +e; \
	cd $(1); \
	$(ANALYZE_PATH) -f analyze.cmd; \
	if cmp -s "$(CURDIR)/$(1)/port_info.json" "$(CURDIR)/$(1)/gold.json"; then \
		printf "Test %-20s \e[32mPASSED\e[0m @ %s\n" $(1) $(CURDIR); \
		true;\
	else \
		printf "Test %-20s \e[31;1mFAILED\e[0m @ %s\n" $(1) $(CURDIR); \
		false; \
	fi;
endef

all: $(TESTS)

.PHONY: all clean_test $(TESTS)

$(foreach test,$(TESTS),$(eval $(call test_tpl,$(test))))

clean_test:
	@find . -name "port_info.json" | xargs rm -rf

