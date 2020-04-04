WRK_DIR=$(abspath .)
SND_DIR=$(WRK_DIR)/sender
REC_DIR=$(WRK_DIR)/receiver

SEXE=send
REXE=receive

all: debug release

debug: $(SEXE).dbg $(REXE).dbg

release: $(SEXE) $(REXE)

$(SEXE):
	@ $(MAKE) -C $(SND_DIR) release
	cp $(SND_DIR)/$(SEXE) $(WRK_DIR)

$(REXE):
	@ $(MAKE) -C $(REC_DIR) release
	cp $(REC_DIR)/$(REXE) $(WRK_DIR)

$(SEXE).dbg:
	@ $(MAKE) -C $(SND_DIR) debug
	cp $(SND_DIR)/$(SEXE).dbg $(WRK_DIR)

$(REXE).dbg:
	@ $(MAKE) -C $(REC_DIR) debug
	cp $(REC_DIR)/$(REXE).dbg $(WRK_DIR)

clean:
	@ $(MAKE) -C $(REC_DIR) clean
	@ $(MAKE) -C $(SND_DIR) clean
	rm -f $(SEXE)
	rm -f $(REXE)
	rm -f $(SEXE).dbg
	rm -f $(REXE).dbg