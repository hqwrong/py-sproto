#!/usr/bin/python2

from sproto import Sproto

with open("./test.sp") as f:
    chunk = f.read()


sp = Sproto(chunk)
spbin = sp.encode("AddressBook", {"person": [{"name": "Alice", "id": 10000}]})
spbin_p = sp.pack(spbin)
print(sp.decode("AddressBook", sp.unpack(spbin_p)))


req, resp = sp.protocal("call")

print("foo request:", sp.decode(req, sp.encode(req, {"name":"whq", "email":"give@money.com"})))
print("foo response:", sp.decode(resp, sp.encode(resp, {"ok": True})))
