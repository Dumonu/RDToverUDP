WRK_DIR=$(abspath .)
SND_DIR=$(WRK_DIR)/sender
REC_DIR=$(WRK_DIR)/receiver
SHR_DIR=$(WRK_DIR)/shared

SEXE=send
REXE=receive

.PHONY: all debug release clean seshen

all: debug release seshen

debug:
	@ for f in $(notdir $(wildcard $(SHR_DIR)/*)); do \
		if [ ! -f $(SND_DIR)/$${f} ]; then \
			ln -s $(SHR_DIR)/$${f} $(SND_DIR)/$${f}; \
		fi; \
		if [ ! -f $(REC_DIR)/$${f} ]; then \
			ln -s $(SHR_DIR)/$${f} $(REC_DIR)/$${f}; \
		fi; \
	done
	@ $(MAKE) -C $(SND_DIR) debug
	@ $(MAKE) -C $(REC_DIR) debug
	@ cp $(SND_DIR)/$(SEXE).dbg $(WRK_DIR)
	@ cp $(REC_DIR)/$(REXE).dbg $(WRK_DIR)

release:
	@ for f in $(notdir $(wildcard $(SHR_DIR)/*)); do \
		if [ ! -f $(SND_DIR)/$${f} ]; then \
			ln -s $(SHR_DIR)/$${f} $(SND_DIR)/$${f}; \
		fi; \
		if [ ! -f $(REC_DIR)/$${f} ]; then \
			ln -s $(SHR_DIR)/$${f} $(REC_DIR)/$${f}; \
		fi; \
	done
	@ $(MAKE) -C $(SND_DIR) release
	@ $(MAKE) -C $(REC_DIR) release
	@ cp $(SND_DIR)/$(SEXE) $(WRK_DIR)
	@ cp $(REC_DIR)/$(REXE) $(WRK_DIR)

clean:
	@ for f in $(notdir $(wildcard $(SHR_DIR)/*)); do \
		rm -f $(SND_DIR)/$${f}; \
		rm -f $(REC_DIR)/$${f}; \
	done
	@ $(MAKE) -C $(REC_DIR) clean
	@ $(MAKE) -C $(SND_DIR) clean
	rm -f $(SEXE)
	rm -f $(REXE)
	rm -f $(SEXE).dbg
	rm -f $(REXE).dbg