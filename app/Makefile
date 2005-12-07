
all: update

update: debug product

debug:
	(cd wlib/gtklib; make debug2;)
	(cd help; make pngs; make;)
	(cd lib/params; make;)
	(cd lib; make;)
	(cd bin; make debug;)

product:
	(cd wlib/gtklib; make product2;)
	(cd help; make pngs; make;)
	(cd lib/params; make;)
	(cd lib; make;)
	(cd bin; make product;)

ins:
	(./install.sh )

clean:
	(cd wlib/gtklib; make clean;)
	(cd help; make clean;)
	(cd lib/params; make clean ;)
	(cd lib; make clean;)
	(cd bin; make clean;)
	rm -f cscope.out *.tar.gz
	rm -fr install
	find . -name \*~ -exec rm {} \; 
	rm -fr bin/Debug
	rm -fr bin/Release
	rm -fr wlib/mswlib/Debug
	rm -fr wlib/mswlib/Release
	rm -fr mswrel
	find . -name cscope.out -exec rm {} \; 
