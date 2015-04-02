ALL:
	python2 setup.py build --build-lib .
	@ lua -e 'p = require"clib.sprotoparser"; io.write(p.parse(io.open("./test.sproto"):read("*a")))' > test.sp

clean:
	rm -rf build/
	- rm *.sp *.pyc *.so
