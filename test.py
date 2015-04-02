#!/usr/bin/python2

from sproto import Sproto

with open("./test.sp") as f:
    chunk = f.read()

sp = Sproto(chunk)
spbin = sp.encode("AddressBook", 
                  {"person": [
                      {"name": "Alice", "id": 10000},
                      {"name": "Bob", "id":200, "email":"hotmail"},
                  ],
                   "names" :["Alice", "Bob", "vv"],
               })

print(sp.decode("AddressBook", sp.unpack(sp.pack(spbin))))

req, resp,tag = sp.protocal("call")

print("foo request:", sp.decode(req, sp.encode(req, {"name":"whq", "email":"give@money.com"})))
print("foo response:", sp.decode(resp, sp.encode(resp, {"ok": True, "reason":"GoodJob"})))
