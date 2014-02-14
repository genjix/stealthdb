import obelisk

def calculate_bitfield(nonce, ephemkey):
    data = "\x06" + ("%x" % nonce).decode("hex") + ephemkey
    stealth_output_hash = obelisk.Hash(data)[::-1][:4]
    return stealth_output_hash

nonce = int("deadbeef", 16)
ephemkey = "0276044981dc13bdc5e118b63c8715f0d1b00e6c0814d778668fa6b594b2a0ffbd".decode("hex")
print calculate_bitfield(nonce, ephemkey).encode("hex")

