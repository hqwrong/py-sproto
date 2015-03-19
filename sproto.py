import pysproto as core

class Sproto():
    def __init__(self, chunk):
        self.sp = core.new(chunk)
        self.st = {}
        self.proto = {}

    def querytype(self, tagname):
        if not tagname in self.st:
            self.st[tagname] = core.querytype(self.sp, tagname)
        return self.st[tagname]

    def protocal(self, protoname):
        if not protoname in self.proto:
            self.proto[protoname] = core.protocal(self.sp, protoname)
        return self.proto[protoname]

    def encode(self, st, data):
        if isinstance(st, basestring):
            st = self.querytype(st)
        return core.encode(self.sp, st, data);

    def decode(self, st, chunk):
        if isinstance(st, basestring):
            st = self.querytype(st)
        return core.decode(self.sp, st, chunk)

    def pack(self, chunk):
        return core.pack(self.sp, chunk);

    def unpack(self, chunk):
        return core.unpack(self.sp, chunk);
