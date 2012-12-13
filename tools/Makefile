LEVEL = ..

include $(LEVEL)/Makefile.common

Scripts = loom_instrument.py

install-local::
	$(Verb) for script in $(Scripts) ; do \
		echo Installing $$script ; \
		$(MKDIR) $(DESTDIR)$(PROJ_bindir) ; \
		if [ "$$script" = "loom_utils.py" ]; then \
			$(DataInstall) $(PROJ_SRC_DIR)/$$script $(DESTDIR)$(PROJ_bindir) ; \
		else \
			$(ScriptInstall) $(PROJ_SRC_DIR)/$$script $(DESTDIR)$(PROJ_bindir) ; \
		fi; \
	done

uninstall-local::
	$(Verb) $(RM) -f $(addprefix $(DESTDIR)$(PROJ_bindir)/,$(Scripts))
