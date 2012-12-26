LEVEL = ..

DIRS = loom_ctl

include $(LEVEL)/Makefile.common

Scripts = loom_instrument.py loom_view_proc.py loom_simple_ctl.py loom_compile.py

install-local::
	$(Verb) for script in $(Scripts) ; do \
		echo Installing $$script ; \
		$(MKDIR) $(DESTDIR)$(PROJ_bindir) ; \
		$(ScriptInstall) $(PROJ_SRC_DIR)/$$script $(DESTDIR)$(PROJ_bindir) ; \
	done

uninstall-local::
	$(Verb) $(RM) -f $(addprefix $(DESTDIR)$(PROJ_bindir)/,$(Scripts))
