import os
import json
import struct
import subprocess
import random
import unittest

WALLET = bytes.fromhex('0014'+'11'*20)
OTHER = bytes.fromhex('0014'+'22'*20)

def compact(n):
    if n < 253: return bytes([n])
    if n <= 65535: return b'\xfd'+struct.pack('<H',n)
    if n <= 0xffffffff: return b'\xfe'+struct.pack('<I',n)
    return b'\xff'+struct.pack('<Q',n)

def transaction(outputs, script=b'\x03\x01\x02\x03'):
    prefix=struct.pack('<I',2)+b'\x01'+b'\0'*32+b'\xff'*4+compact(len(script))+script+b'\xff'*4
    body=compact(len(outputs))+b''.join(struct.pack('<Q',value)+compact(len(pk))+pk for value,pk in outputs)
    return prefix+body+b'\0'*4

class PayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.proc=subprocess.Popen([os.environ['MINER_CORE_EXE']],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
    @classmethod
    def tearDownClass(cls):
        cls.proc.stdin.close();cls.proc.wait(timeout=5);cls.proc.stdout.close()
    def rpc(self,op,**args):
        self.proc.stdin.write(json.dumps(dict(op=op,**args))+'\n');self.proc.stdin.flush()
        return json.loads(self.proc.stdout.readline())
    def check(self,tx,expected=WALLET):
        return self.rpc('payout',tx=tx.hex(),script=expected.hex())

    def test_standard_address_scripts(self):
        genesis='1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa'
        self.assertEqual(self.rpc('addressScript',address=genesis)['script'],
                         '76a91462e907b15cbf27d5425399ebf6f0fb50ebb88f1888ac')
        p2sh='3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy'
        alphabet='123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz'
        number=0
        for c in p2sh:number=number*58+alphabet.index(c)
        payload=number.to_bytes(25,'big')[1:21]
        self.assertEqual(self.rpc('addressScript',address=p2sh)['script'],'a914'+payload.hex()+'87')
        self.assertEqual(self.rpc('addressScript',address='BC1QW508D6QEJXTDG4Y5R3ZARVARY0C5XW7KV8F3T4')['script'],
                         '0014751e76e8199196d454941c45d1b3a323f1433bd6')
        self.assertEqual(self.rpc('addressScript',address='bc1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqzk5jj0')['script'],
                         '512079be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798')
        self.assertFalse(self.rpc('addressScript',address=genesis+'\0other')['ok'])

    def test_all_reward_to_wallet(self):
        result=self.check(transaction([(312500000,WALLET),(0,b'\x6a\x24\xaa\x21\xa9\xed'+b'\0'*32)]))
        self.assertTrue(result['valid']);self.assertTrue(result['verified'])
        self.assertEqual(result['total'],312500000);self.assertEqual(result['paid'],312500000)

    def test_redirect_and_any_fee_blocked(self):
        for outputs in [[(312500000,OTHER)],[(312499999,WALLET),(1,OTHER)],
                        [(307812500,WALLET),(4687500,OTHER)],[(312500000,b'\x6a')]]:
            result=self.check(transaction(outputs))
            self.assertTrue(result['valid']);self.assertFalse(result['verified'])

    def test_wallet_bytes_in_input_do_not_pass(self):
        result=self.check(transaction([(312500000,OTHER)],script=WALLET))
        self.assertFalse(result['verified'])

    def test_multiple_outputs_same_wallet(self):
        result=self.check(transaction([(100,WALLET),(200,WALLET),(0,OTHER)]))
        self.assertTrue(result['verified']);self.assertEqual(result['paid'],300)

    def test_empty_and_zero_reward_blocked(self):
        for outputs in [[],[(0,WALLET)]]:
            self.assertFalse(self.check(transaction(outputs))['verified'])

    def test_truncation_and_trailing_data(self):
        tx=transaction([(312500000,WALLET)])
        for length in range(len(tx)):
            self.assertFalse(self.check(tx[:length])['valid'],length)
        self.assertFalse(self.check(tx+b'\0')['valid'])

    def test_noncoinbase_and_noncanonical_compact(self):
        tx=transaction([(312500000,WALLET)])
        for bad in [tx[:5]+b'\x01'+tx[6:],tx[:37]+b'\0'*4+tx[41:],
                    tx[:4]+b'\xfd\x01\0'+tx[5:],tx[:4]+b'\0\x01'+tx[5:],
                    transaction([(1,WALLET)],script=b'\x01'),
                    transaction([(1,WALLET)],script=b'\0'*101)]:
            self.assertFalse(self.check(bad)['valid'])

    def test_amount_overflow_and_output_bounds(self):
        for outputs in [[((1<<64)-1,WALLET)],[(2100000000000000,WALLET),(1,WALLET)],
                        [(1,WALLET)]*129]:
            self.assertFalse(self.check(transaction(outputs))['valid'])

    def test_extended_script_lengths(self):
        tx=transaction([(100,WALLET),(0,b'\x6a'+b'\0'*300)])
        self.assertTrue(self.check(tx)['verified'])

    def test_mutation_invariants(self):
        rng=random.Random(19);tx=transaction([(312500000,WALLET),(0,b'\x6a')])
        for _ in range(100):
            n=bytearray(tx);index=rng.randrange(len(n));n[index]^=rng.randrange(1,256)
            result=self.check(n)
            if result['verified']:
                self.assertTrue(result['valid']);self.assertGreater(result['total'],0)
                self.assertEqual(result['total'],result['paid'])

if __name__=='__main__':unittest.main()
