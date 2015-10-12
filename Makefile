all: checkmakefiles
	cd src && $(MAKE)

clean: checkmakefiles
	cd src && $(MAKE) clean

cleanall: checkmakefiles
	cd src && $(MAKE) MODE=release clean
	cd src && $(MAKE) MODE=debug clean

makefiles: makefiles-so

makefiles-so:
	cd src && opp_makemake --make-so -f --deep -o ib_flit_sim

makefiles-lib:
	cd src && opp_makemake --make-lib -f --deep -o ib_flit_sim

makefiles-exe:
	cd src && opp_makemake -f --deep -o ib_flit_sim

checkmakefiles:
	@if [ ! -f src/Makefile ]; then \
		echo; \
	        echo '======================================================================='; \
	        echo 'src/Makefile does not exist. Please use "make makefiles" to generate it!'; \
	        echo '======================================================================='; \
	        echo; \
	        exit 1; \
	fi
