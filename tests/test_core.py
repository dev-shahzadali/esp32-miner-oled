import copy
from fractions import Fraction
import hashlib
import json
import math
import os
import random
import struct
import subprocess
import unittest

DIFF1=65535 << 208
GENESIS='01000000'+'00'*32+'3ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a'+'29ab5f49ffff001d1dac2b7c'
def double_sha(data):
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()

class CoreTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.proc=subprocess.Popen([os.environ['MINER_CORE_EXE']],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)

    @classmethod
    def tearDownClass(cls):
        cls.proc.stdin.close()
        cls.proc.wait(timeout=5)
        cls.proc.stdout.close()

    def rpc(self,op,**args):
        self.proc.stdin.write(json.dumps(dict(op=op,**args))+'\n')
        self.proc.stdin.flush()
        return json.loads(self.proc.stdout.readline())

    def test_sha_vectors(self):
        for data in [b'',b'abc',b'a'*55,b'a'*56,b'a'*63,b'a'*64,b'a'*65,b'a'*1000]:
            with self.subTest(length=len(data)):
                self.assertEqual(self.rpc('sha',hex=data.hex())['hash'],hashlib.sha256(data).hexdigest())
                self.assertEqual(self.rpc('sha2',hex=data.hex())['hash'],double_sha(data).hex())

    def test_genesis_header(self):
        expected='000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f'
        self.assertEqual(bytes.fromhex(self.rpc('sha2',hex=GENESIS)['hash'])[::-1].hex(),expected)
        self.assertEqual(self.rpc('fast',hex=GENESIS)['hash'],double_sha(bytes.fromhex(GENESIS)).hex())

    def test_optimized_random_headers(self):
        rng=random.Random(507)
        for i in range(256):
            header=rng.randbytes(80)
            self.assertEqual(self.rpc('fast',hex=header.hex())['hash'],double_sha(header).hex(),i)

    def test_nonce_extremes(self):
        for n in [0,1,0x12345678,0x7fffffff,0xfffffffe,0xffffffff]:
            header=bytes.fromhex(GENESIS)[:76]+struct.pack('<I',n)
            self.assertEqual(self.rpc('fast',hex=header.hex())['hash'],double_sha(header).hex())

    def test_difficulty_targets_exact(self):
        rng=random.Random(8)
        values=[1.,2.,.5,.0001,.00015,math.ldexp(1.,-64),math.ldexp(1.,256),1e12]
        values += [math.ldexp(rng.uniform(1,2),rng.randrange(-30,180)) for _ in range(100)]
        for d in values:
            expected=min((1<<256)-1,int(Fraction(DIFF1)/Fraction(d)))
            actual=self.rpc('target',decimal=repr(d))
            self.assertTrue(actual['ok'],d)
            self.assertEqual(int.from_bytes(bytes.fromhex(actual['target']),'little'),expected,d)

    def test_invalid_difficulty(self):
        for d in [0,-1,1e-100,1e100]:
            self.assertFalse(self.rpc('target',decimal=repr(d))['ok'])

    def test_compact_target(self):
        for c in [0x1d00ffff,0x207fffff,0x0300ffff,0x0200ffff,0x01010000,0x220000ff]:
            e=c>>24;m=c&0x7fffff;expected=m<<(8*(e-3)) if e>3 else m>>(8*(3-e))
            result=self.rpc('compact',bits=c)
            self.assertTrue(result['ok'])
            self.assertEqual(int.from_bytes(bytes.fromhex(result['target']),'little'),expected)
        for c in [0,0x1d80ffff,0x23000001,0x22000100,0x21010000]:
            self.assertFalse(self.rpc('compact',bits=c)['ok'])

    def test_compare_full_256_bits(self):
        target=DIFF1
        for value in [target-1,target,target+1,1,(1<<256)-1]:
            result=self.rpc('compare',hash=value.to_bytes(32,'little').hex(),target=target.to_bytes(32,'little').hex())
            self.assertEqual(result['meets'],value<=target)
            self.assertAlmostEqual(float(result['difficulty'])/(DIFF1/value),1,places=13)
        # A high byte dominates even when a low byte is smaller.
        self.assertFalse(self.rpc('compare',hash=(256).to_bytes(32,'little').hex(),target=(255).to_bytes(32,'little').hex())['meets'])

    @staticmethod
    def job():
        return ['fixture-job',''.join(f'{i:02x}' for i in range(32)),'010203','aabbcc',['11'*32,'22'*32],'20000000','1d00ffff','65000001',True]

    def test_coinbase_merkle_header(self):
        p=self.job();ex1='01020304';ex2='0000000000000001'
        root=double_sha(bytes.fromhex(p[2]+ex1+ex2+p[3]))
        for branch in p[4]:root=double_sha(root+bytes.fromhex(branch))
        prev=bytes.fromhex(p[1]);prev=b''.join(prev[i:i+4][::-1] for i in range(0,32,4))
        expected=struct.pack('<I',int(p[5],16))+prev+root+struct.pack('<III',int(p[7],16),int(p[6],16),0)
        result=self.rpc('header',params=p,ex1=ex1,ex2=ex2)
        self.assertTrue(result['ok']);self.assertEqual(result['header'],expected.hex())

    def test_coinbase_beyond_upstream_buffer(self):
        p=self.job();p[2]='ab'*1200;p[3]='cd'*1200;p[4]=[]
        result=self.rpc('header',params=p,ex1='1234',ex2='00')
        self.assertTrue(result['ok'])
        self.assertEqual(bytes.fromhex(result['header'])[36:68],double_sha(bytes.fromhex(p[2]+'1234'+'00'+p[3])))

    def test_malformed_jobs(self):
        p=self.job()
        bad=[None,{},p[:-1],p+[1]]
        for index,value in [(0,''),(1,'00'),(2,'zz'),(3,'a'),(4,['00']), (4,['11'*32]*33),(5,'1'),(6,5),(7,None),(8,'true')]:
            n=copy.deepcopy(p);n[index]=value;bad.append(n)
        for params in bad:self.assertFalse(self.rpc('notify',params=params)['ok'],params)

    def test_subscription_bounds(self):
        for size in [1,2,4,8,16]:
            result=self.rpc('subscribe',result=[[['mining.notify','session']],'00112233',size])
            self.assertTrue(result['ok']);self.assertEqual(result['size'],size)
        for value in [None,[],[[],'zz',8],[[],'00',0],[[],'00',17],[[],'00',-1],[[],'00','8']]:
            self.assertFalse(self.rpc('subscribe',result=value)['ok'])

    def test_share_submission(self):
        for nonce in [0,1,0x12345678,0xffffffff]:
            result=self.rpc('submit',id=9,worker='public-fixture.worker',job='job"quoted',time='65000001',ex2='0000000000000001',nonce=nonce)
            payload=json.loads(result['payload'])
            self.assertEqual(payload,{'id':9,'method':'mining.submit','params':['public-fixture.worker','job"quoted','0000000000000001','65000001',f'{nonce:08x}']})
            self.assertTrue(result['payload'].endswith('\n'))

    def test_base58_checksum(self):
        # Published consensus test addresses, never payout defaults.
        for address in ['1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa','3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy','1111111111111111111114oLvT2']:
            self.assertTrue(self.rpc('address',address=address)['ok'])
            self.assertFalse(self.rpc('address',address=address[:-1]+'b')['ok'])
        for address in ['', 'not-a-wallet','mipcBbFg9gMiCh81Kj8tqqdgoZub1ZJRfn','1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfN0']:
            self.assertFalse(self.rpc('address',address=address)['ok'])

    def test_segwit_checksum(self):
        valid=['BC1QW508D6QEJXTDG4Y5R3ZARVARY0C5XW7KV8F3T4','bc1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqzk5jj0']
        for address in valid:
            self.assertTrue(self.rpc('address',address=address)['ok'],address)
            self.assertFalse(self.rpc('address',address=address[:-1]+'q')['ok'])
        for address in ['bc1QW508D6QEJXTDG4Y5R3ZARVARY0C5XW7KV8F3T4','tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx','bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kygt080']:
            self.assertFalse(self.rpc('address',address=address)['ok'])

    def test_retry_backoff(self):
        result=self.rpc('backoff')
        self.assertEqual(result['delays'],[1,2,4,8,16,32,60,60,60,60]);self.assertEqual(result['reset'],1)

    def test_difficulty_wire_precision(self):
        for decimal in ['0.00015','1e-4','1.2345678901234567','1000000000000']:
            raw='{"id":null,"method":"mining.set_difficulty","params":['+decimal+']}'
            actual=self.rpc('difficultyMessage',raw=raw)
            expected=min((1<<256)-1,int(Fraction(DIFF1)/Fraction(float(decimal))))
            self.assertTrue(actual['ok']);self.assertEqual(int.from_bytes(bytes.fromhex(actual['target']),'little'),expected)
        for raw in ['{"params":["1"]}','{"params":[1,2]}','{"params":[1],"params":[2]}','{"params":[null]}']:
            self.assertFalse(self.rpc('difficultyMessage',raw=raw)['ok'])

    def test_response_acceptance_is_boolean(self):
        for message,success in [({'result':True,'error':None},True),({'result':False,'error':None},False),({'result':True,'error':[23,'low difficulty',None]},False),({'result':'true'},False),({'error':None},False)]:
            self.assertEqual(self.rpc('response',message=message)['success'],success)

    def test_connection_deadlines_and_wrap(self):
        for now,ready,expired in [(19000,False,False),(20001,False,True),(20001,True,False),(90001,True,True)]:
            self.assertEqual(self.rpc('timeout',now=now,connectedAt=0,lastRx=0,lastJob=0,ready=ready)['expired'],expired)
        start=(1<<32)-10000
        self.assertFalse(self.rpc('timeout',now=5000,connectedAt=start,lastRx=start,lastJob=start,ready=False)['expired'])
        self.assertTrue(self.rpc('timeout',now=15000,connectedAt=start,lastRx=start,lastJob=start,ready=False)['expired'])

if __name__=='__main__':unittest.main()
