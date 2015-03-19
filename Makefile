ALL:
	python2 setup.py build --build-lib .
	lua -e 'p = require"sproto.sprotoparser"; io.write(p.parse(io.open("./test.sproto"):read("*a")))' > test.sp
