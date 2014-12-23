
import os
import hashlib
from functools import partial
import pickle

def CreateFile(path, block, numBlocks):
    ''' Create a file conxitting of numBlocks x block.
        Returns the md5sum of the data '''
    md5 = hashlib.md5()
    f = open(path, 'wb')
    for n in range(numBlocks):
        data = block
        f.write(data)
        md5.update(data)
    f.close()
    return md5

def md5sum(filename):
    with open(filename, mode='rb') as f:
        d = hashlib.md5()
        for buf in iter(partial(f.read, 128), b''):
            d.update(buf)
    return d

def md5sumAscii(filename):
    with open(filename, mode='r') as f:
        d = hashlib.md5()
        for line in f:
            d.update(line.replace('r\n', '\n').replace('\n', '\r\n').encode())
    return d


class File:
    def __init__(self, mypath, vpath, svrpath, block, numBlocks, restoffset):
        self.mypath = mypath
        self.vpath = vpath
        self.svrpath = svrpath
        self.restoffset = restoffset
        if (block):
            self.block = block
        else:
            self.block = 'abcdefghijklmnopqrstuvzyxABCDEFGHIJKLMNOPQRSTUVZYX1234567890\n'.encode()
        self.num_blockes = numBlocks

    def CreateForUpload(self):
        self.md5 = CreateFile(self.mypath, self.block, self.num_blockes).hexdigest()

    def CreateForDownload(self):
        if not os.path.exists(self.svrpath):
            self.md5 = CreateFile(self.svrpath, self.block, self.num_blockes).hexdigest()
            self.md5ascii = md5sumAscii(self.svrpath).hexdigest()
            self.size = os.stat(self.svrpath).st_size
            if self.restoffset < 0:
                self.restoffset = self.size + self.restoffset


    def VerifyUpload(self):
        if md5sum(self.self.svrpath).hexdigest() != self.md5:
            print('File ' + self.svrpath + ' is not equal to ' + self.mypath)
            return False
        return True

    def VerifyMd5(self, md5):
        hd = md5.hexdigest()
        res = hd == self.md5
        if not res:
            print('self.md5=' + str(self.md5) + ', md5=' + str(hd))
        return res

    def VerifyMd5Ascii(self, md5):
        hd = md5.hexdigest()
        res = hd == self.md5ascii
        if not res:
            print('self.md5ascii=' + str(self.md5) + ', md5=' + str(hd))
        return res

def SaveFileList(path, files):
    with open(path, 'wb') as pf:
        pickle.dump(files, pf, pickle.HIGHEST_PROTOCOL)

def LoadFileList(path):
    with open(path, 'rb') as pf:
        return pickle.load(pf)
