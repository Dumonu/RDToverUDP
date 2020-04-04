WRK_DIR=$(abspath .)
SND_DIR=$(WRK_DIR)/sender
REC_DIR=$(WRK_DIR)/receiver

SEXE=send
REXE=receive

.PHONY: all debug release clean

all: debug release

debug:
	@ $(MAKE) -C $(SND_DIR) debug
	@ $(MAKE) -C $(REC_DIR) debug
	@ cp $(SND_DIR)/$(SEXE).dbg $(WRK_DIR)
	@ cp $(REC_DIR)/$(REXE).dbg $(WRK_DIR)

release:
	@ $(MAKE) -C $(SND_DIR) release
	@ $(MAKE) -C $(REC_DIR) release
	@ cp $(SND_DIR)/$(SEXE) $(WRK_DIR)
	@ cp $(REC_DIR)/$(REXE) $(WRK_DIR)

clean:
	@ $(MAKE) -C $(REC_DIR) clean
	@ $(MAKE) -C $(SND_DIR) clean
	rm -f $(SEXE)
	rm -f $(REXE)
	rm -f $(SEXE).dbg
	rm -f $(REXE).dbg