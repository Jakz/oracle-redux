# Support bidirectional original-save conversion

Status: Accepted

Oracle Redux imports and explicitly exports cartridge-compatible Original Save Images for both campaigns while using a Redux Save Envelope during ordinary play. Import validates the campaign verification string, checksums, slots, and redundant copies; conversion preserves unknown bytes so they can survive a round trip. Redux-only settings and Gameplay Extension state remain outside the cartridge representation. Export updates the original checksums and redundant copies and creates a recovery backup before replacing an existing save.
