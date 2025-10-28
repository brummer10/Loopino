
include libxputty/Build/Makefile.base

NOGOAL := mod install all features

PASS := features 

SUBDIR := Loopino

.PHONY: $(SUBDIR) libxputty  recurse 

$(MAKECMDGOALS) recurse: $(SUBDIR)

check-and-reinit-submodules :
ifeq (,$(filter $(NOGOAL),$(MAKECMDGOALS)))
ifeq (,$(findstring clean,$(MAKECMDGOALS)))
	@if git submodule status 2>/dev/null | egrep -q '^[-]|^[+]' ; then \
		echo "$(red)INFO: Need to reinitialize git submodules$(reset)"; \
		git submodule update --init; \
		echo "$(blue)Done$(reset)"; \
	else echo "$(blue)Submodule up to date$(reset)"; \
	fi
endif
endif

libxputty: check-and-reinit-submodules
ifeq (,$(filter $(NOGOAL),$(MAKECMDGOALS)))
ifeq (,$(wildcard ./libxputty/xputty/resources/dir.png))
	@cp ./Loopino/Resources/*.png ./libxputty/xputty/resources/
endif
	@exec $(MAKE) --no-print-directory -j 1 -C $@ $(filter-out jack,$(MAKECMDGOALS))
endif

$(SUBDIR): libxputty
ifeq (,$(filter $(PASS),$(MAKECMDGOALS)))
	@exec $(MAKE) --no-print-directory -j 1 -C $@ $(MAKECMDGOALS)
endif

clean:
	@rm -f ./libxputty/xputty/resources/dir.png
	@rm -f ./libxputty/xputty/resources/exit_.png
	@rm -f ./libxputty/xputty/resources/load_.png
	@rm -f ./libxputty/xputty/resources/save_.png
	@rm -f ./libxputty/xputty/resources/play.png
	@rm -f ./libxputty/xputty/resources/clip_.png
	@rm -f ./libxputty/xputty/resources/quit.png
	@rm -f ./libxputty/xputty/resources/loop.png
	@rm -f ./libxputty/xputty/resources/okay.png
	@rm -f ./libxputty/xputty/resources/plus.png
	@rm -f ./libxputty/xputty/resources/loopino.png


features:
