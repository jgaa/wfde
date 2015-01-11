#!/usr/bin/env python3

import os
import ftplib
from ftplib import FTP
from ftplib import FTP_TLS
import server_config
import sys
import hashlib
import stat
import socket
from time import sleep
from test_file import File
from test_file import md5sum
from test_file import md5sumAscii
import test_file

config = {};
config['host'] = "127.0.0.1"
config['port'] = 2121
config['debug-level'] = 0
config['ftp-root'] = os.getcwd() + '/test-tmp/ftproot'
config['workdir'] = os.getcwd() + '/test-tmp/client'
config['server-config'] =  os.getcwd() + '/test-tmp/wfded.conf'


class FtpTest:
    def __init__(self, conf):
        self.host = conf['host']
        self.port = conf['port']
        self.ftp_root = conf['ftp-root']
        self.workdir = conf['workdir']
        self.failcount = 0
        self.debug_level = conf['debug-level']
        self.pasv_mode = False
        self.num_folders_in_root = 7
        self.dl_files_cache = self.workdir + '/dlfiles.cache'
        self.skip_big_files = True
        self.disable_print = False
        self.use_tls = False
        self.use_transfer_tls = True

        for path in (self.ftp_root, self.workdir):
            if not os.path.exists(path):
                print ('Creating path: ' + path)
                os.makedirs(path)

        for sd in ('/home/jgaa', '/upload', '/pub/sub/sub2', '/empty'):
            path = self.ftp_root + sd;
            if not os.path.exists(path):
                print ('Creating path: ' + path)
                os.makedirs(path)

        print('Creating missing test-files for download')
        if os.path.exists(self.dl_files_cache):
            self.dl_files = test_file.LoadFileList(self.dl_files_cache)
            do_update_cache = False
        else:
            self.dl_files = []
            self.dl_files.append(File(self.workdir + '/test1','/pub/test1', \
                self.ftp_root + '/pub/test1', False, 1000, -19))
            self.dl_files.append(File(self.workdir + '/test2','/pub/test2', \
                self.ftp_root + '/pub/test2', False, 1001, 776))
            self.dl_files.append(File(self.workdir + '/test3','/pub/test3', \
                self.ftp_root + '/pub/test3', False, 2000, 2))
            self.dl_files.append(File(self.workdir + '/test4','/pub/test4', \
                self.ftp_root + '/pub/test4', False, 100, 1))
            self.dl_files.append(File(self.workdir + '/test5','/pub/test5', \
                self.ftp_root + '/pub/test5', False, 1, -1))
            self.dl_files.append(File(self.workdir + '/test6','/pub/test6', \
                self.ftp_root + '/pub/test6', False, 999, 123))
            self.dl_files.append(File(self.workdir + '/test7','/pub/test7', \
                self.ftp_root + '/pub/test7', False, 10000, 11))
            self.dl_files.append(File(self.workdir + '/test8','/pub/test8', \
                self.ftp_root + '/pub/test8', False, 100000, 1000))
            self.dl_files.append(File(self.workdir + '/test9','/pub/test9', \
                self.ftp_root + '/pub/test9', False, 1000000, -1))
            self.dl_files.append(File(self.workdir + '/test10','/pub/test10', \
                self.ftp_root + '/pub/test10', False, 1000, 1))
            self.dl_files.append(File(self.workdir + '/test11','/pub/test11', \
                self.ftp_root + '/pub/test11', False, 20000000, 100234))
            self.dl_files.append(File(self.workdir + '/test12','/pub/test12', \
                self.ftp_root + '/pub/test12', False, 100000000, -1019))
            self.dl_files.append(File(self.workdir + '/test_empty','/pub/test_empty', \
                self.ftp_root + '/pub/test_empty', False, 0, 0))
            do_update_cache = True;
            print('This may take a few minutes...')

        for f in self.dl_files:
            f.CreateForDownload()

        if do_update_cache:
            test_file.SaveFileList(self.dl_files_cache, self.dl_files)
            #with open(self.dl_files_cache, 'wb') as pf:
                #pickle.dump(self.dl_files, pf, pickle.HIGHEST_PROTOCOL)

        os.chdir(self.workdir);
        print('Ready to start tests on ftp-root: ' + self.ftp_root)

    def __IsTransferTlsEnabled(self):
        return self.use_tls and self.use_transfer_tls

    def __SetupOneTest(self, user=None, passwd=''):
        if self.use_tls:
            ftp = FTP_TLS()
        else:
            ftp = FTP()
        ftp.connect(self.host, int(self.port))
        ftp.set_debuglevel(self.debug_level)
        ftp.set_pasv(self.pasv_mode)
        if user:
            ftp.login(user, passwd)
            if self.__IsTransferTlsEnabled():
                ftp.prot_p()

        return ftp

    def TestAnonymousLogins(self):
        for name in ('anonymous', 'ftp', 'AnoNymous', 'FTP', 'FtP'):
            self.TestUserLogin(name, '')

    def TestInvalidLogin(self, name, passwd):
        self.__PrintTest('Testing illegal login: ' + name)
        ftp = self.__SetupOneTest()
        try:
            res = ftp.login(name, passwd)
            self.__Assert(res[:3] != '230', res)
        except ftplib.error_perm:
            self.__Assert(True)
        ftp.close()

    def TestInvalidLogins(self):
        self.TestInvalidLogin('guest', '')
        self.TestInvalidLogin('jgaa', '')
        self.TestInvalidLogin('xxYYxx', '')

    def TestUserLogin(self, name, passwd):
        if passwd:
            login_type = 'user'
        else:
             login_type = 'anonymous'
        self.__PrintTest('Testing ' + login_type + ' login: ' + name)
        ftp = self.__SetupOneTest()
        try:
            res = ftp.login(name, passwd)
            self.__Assert(res[:3] == '230', res)
        except ftplib.error_perm:
            self.__Assert(False)
        ftp.close()

    def TestUserLogins(self):
        self.TestUserLogin('jgaa', 'test')

    def __AddLine(self, x):
        self.data += x + '\n'

    def TestList(self, vpath, user, passwd, cmd):
        '''
        Test a listing command.
        Return the listing returned from the server
        Throw on server-side errors
        '''
        self.__PrintTest('[' + user + ']: Testing command ' + cmd + ' on path: ' + vpath)
        ftp = self.__SetupOneTest(user, passwd)
        try:
            if vpath:
                cmd = cmd + ' ' + vpath;
            dir_listing = ''
            self.data = ''
            ftp.retrlines(cmd, self.__AddLine)
            ftp.quit()
            ftp.close()
            return self.data
        except ftplib.error_temp:
            return '  ***EXCEPTION';


    def __Assert(self, expr, expl=''):
        if (expr):
            if not self.disable_print:
                print(' OK')
        else:
            print(' *** FAILED')
            if expl:
                print(expl)
            self.failcount += 1

    # TODO: Verify the results with regex
    def TestLists(self, user='ftp', passwd=''):
        # LIST /
        listing = self.TestList('/', user, passwd, 'LIST')
        self.__Assert(listing.count('\n') == self.num_folders_in_root, listing)

        # LIST -1 (one) /
        listing = self.TestList('/', user, passwd, 'LIST -1')
        self.__Assert(listing.count('\n') == self.num_folders_in_root, listing)

         # LIST -l (el) /
        listing = self.TestList('/', user, passwd, 'LIST -l')
        self.__Assert(listing.count('\n') == self.num_folders_in_root, listing)

        # LIST /empty
        listing = self.TestList('/empty', user, passwd, 'LIST')
        self.__Assert(listing.count('\n') == 2, listing)

        # LIST /home (invalid)
        listing = self.TestList('/home', user, passwd, 'LIST')
        self.__Assert(listing.count('\n') == 0, listing)

        listing = self.TestList('/home/jgaa', user, passwd, 'LIST')
        self.__Assert(listing.count('\n') == 0, listing)

        listing = self.TestList('/invalid', user, passwd, 'LIST')
        self.__Assert(listing.count('\n') == 0, listing)

        listing = self.TestList('/home/jgaa', 'jgaa', 'test', 'LIST')
        self.__Assert(listing.count('\n') == 0, listing)

        # LIST /home (invalid)
        listing = self.TestList('/home', 'jgaa', 'test', 'LIST')
        self.__Assert(listing.count('\n') == 2, listing)

        listing = self.TestList('/pub', user, passwd, 'LIST')
        self.__Assert(listing.count('\n') == 16, listing)

        listing = self.TestList('/pub/..', user, passwd, 'LIST')
        self.__Assert(listing.count('\n') == self.num_folders_in_root, listing)

        listing = self.TestList('/..', user, passwd, 'LIST')
        self.__Assert(listing.count('\n') == 0, listing)

        listing = self.TestList('/../../../../../..', user, passwd, 'LIST')
        self.__Assert(listing.count('\n') == 0, listing)

        listing = self.TestList('/...', user, passwd, 'LIST')
        self.__Assert(listing.count('\n') == 0, listing)

        listing = self.TestList('/pub/a/b/c../../../../../', user, passwd, 'LIST')
        self.__Assert(listing.count('\n') == 0, listing)

    def TestMlsd(self, user, passwd, vpath):
        sys.stdout.write('[' + user + ']: Testing MLST on path: ' + vpath)
        ftp = self.__SetupOneTest(user, passwd)
        try:
            res = ftp.mlsd(vpath)
            lines = 0
            for l in res:
                lines += 1
            ftp.quit()
            ftp.close()
            return lines
        except ftplib.error_temp:
             sys.stdout.write('  ***EXCEPTION')
             return -1

    def TestNlsds(self, user='ftp', passwd=''):
        dirs = (('/', self.num_folders_in_root), \
            ('/home', -1), \
            ('/pub/..', self.num_folders_in_root), \
            ('/pub', 16), \
            ('/pub/a/b/c../../../../../', -1))

        for d in dirs:
            self.__Assert(self.TestMlsd(user, passwd, d[0]) == d[1], d[0])

    def TestMlst(self, user, passwd, vpath):
        self.__PrintTest('[' + user + ']: Testing MLSD on path: ' + vpath)
        ftp = self.__SetupOneTest(user, passwd)
        try:
            cmd = 'MLST'
            if vpath:
                cmd = cmd + ' ' + vpath
            res = ftp.sendcmd(cmd)
            #print(res)
            ftp.quit()
            ftp.close()
            return res[:3] == '250'
        except ftplib.error_temp:
             sys.stdout.write('  ***EXCEPTION')
             return False

    def TestMlsts(self, user='ftp', passwd=''):
        dirs = (('/', True), ('/pub/a/b/c../../../../../', False), \
            ('/home', False), ('/pub/test1', True))
        for d in dirs:
            self.__Assert(self.TestMlst(user, passwd, d[0]) == d[1], d[0])

    def __DigestLine(self, data):
        self.md5.update(data.encode())

    def __DigestBlock(self, data):
        self.md5.update(data)

    def TestDownload(self, file, user, passwd, cmd, bin=True):
        if bin:
            dlt = 'binary'
        else:
            dlt = 'ascii'
        self.__PrintTest('[' + user + ']:' + ' Testing ' + dlt + ' download: ' + file.vpath)
        sys.stdout.flush()
        ftp = self.__SetupOneTest(user, passwd)
        cmd = cmd + ' ' + file.vpath;
        dir_listing = ''
        self.md5 = hashlib.md5()
        if bin:
            ftp.retrbinary(cmd, self.__DigestBlock)
        else:
            #ftp.retrlines(cmd, self.__DigestLine) // Cuts off CRLF
            sck = ftp.transfercmd('TYPE A\r\n' + cmd)
            while True:
                data = sck.recv(1024)
                if not data:
                    break
                self.md5.update(data)
            sck.close()
        ftp.quit()
        ftp.close()

        if bin:
            return file.VerifyMd5(self.md5)
        return file.VerifyMd5Ascii(self.md5)

    def TestUpload(self, file, user, passwd, cmd, bin=True, deleteTarget=True):
        if bin:
            dlt = 'binary'
        else:
            dlt = 'ascii'
        vpath = '/upload' + file.vpath[file.vpath.rfind('/'):]
        diskpath = self.ftp_root + vpath

        if deleteTarget and os.path.exists(diskpath):
            os.remove(diskpath)

        self.__PrintTest('[' + user + ']:' + ' Testing ' + dlt + ' upload: ' + vpath)
        sys.stdout.flush()
        ftp = self.__SetupOneTest(user, passwd)
        cmd = cmd + ' ' + vpath;
        dir_listing = ''
        self.md5 = hashlib.md5()
        try:
            if bin:
                fp = open(file.svrpath, 'rb')
                ftp.storbinary(cmd, fp)
                fp.close()
            else:
                fp = open(file.svrpath, 'rb')
                ftp.storlines(cmd, fp)
                fp.close()
            ftp.quit()
            ftp.close()

            if bin:
                return md5sum(diskpath).hexdigest() == file.md5
            return  md5sumAscii(diskpath).hexdigest() == file.md5ascii
        except ftplib.error_temp:
            return False


    def TestDownloads(self, user='ftp', passwd='', cmd='RETR'):
        for file in self.dl_files:
             if self.skip_big_files and file.size >= 10000000:
                continue
             self.__Assert(self.TestDownload(file, user, passwd, cmd, False))

        for file in self.dl_files:
            if self.skip_big_files and file.size >= 10000000:
                continue
            self.__Assert(self.TestDownload(file, user, passwd, cmd))

    def TestUploads(self, user='jgaa', passwd='test', cmd='STOR'):
        for file in self.dl_files:
             if self.skip_big_files and file.size >= 10000000:
                continue
             self.__Assert(self.TestUpload(file, user, passwd, cmd, False))

        for file in self.dl_files:
            if self.skip_big_files and file.size >= 10000000:
                continue
            self.__Assert(self.TestUpload(file, user, passwd, cmd))

    def TestEmptyInput(self):
        self.__PrintTest('Testing empty command')
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        result = False
        try:
            s.connect((self.host, self.port))
            s.sendall(b'\r\n')
            data = s.recv(1024)
            s.close()
            result = True
        except OSError:
            pass
        except EOFError:
            pass

        self.__Assert(result)

    def CloseDataTransfer(self, user, passwd, cmd, bin, upload, vpath, buffers, closeondone):
        if bin:
            dlt = 'binary'
        else:
            dlt = 'ascii'

        if upload:
            direction = ' upload '
        else:
            direction = ' download '
        self.__PrintTest('[' + user + ']:' + ' Testing ' + dlt + direction + \
            'close of ' + vpath + ' with ' + str(buffers) + ' buffers: ' + \
            'cod ' + str(closeondone))
        sys.stdout.flush()
        ftp = self.__SetupOneTest(user, passwd)
        cmd = cmd + ' ' + vpath;
        rval = False
        try:
            if bin:
                sck = ftp.transfercmd('TYPE I\r\n' + cmd)
            else:
                #ftp.retrlines(cmd, self.__DigestLine) // Cuts off CRLF
                sck = ftp.transfercmd('TYPE A\r\n' + cmd)
                if buffers >= 0:
                    for i in range(buffers):
                        if upload:
                            sck.sendall(('abcdefghijklmnopqrstuvzyxABCDEFGHIJKLMNOPQRSTUVZYX1234567890\r\n' * 16).encode())
                        else:
                            data = sck.recv(1024)
            if closeondone:
                sck.close()
            rval = True
        except:
            pass

        try:
            ftp.quit()
            ftp.close()
        except:
            pass
        return rval

    def CloseDataTransfers(self, user='ftp', passwd=''):
        for closeondone in (True, False):
            for upload in (True, False):
                for bin in (True, False):
                    for buffers in (-1, 0, 1, 2, 3, 5, 7, 10, 16, 32):
                        if upload:
                            cmd = 'STOR'
                            vpath = '/upload/broken'
                        else:
                            cmd = 'RETR'
                            vpath = '/pub/test12'
                        self.__Assert(self.CloseDataTransfer(user, passwd, cmd, \
                            bin, upload, vpath, buffers, closeondone))

    def TestStou(self, file, user, passwd, cmd='STOU'):
        self.__PrintTest('[' + user + ']:' + ' Testing STOU')
        sys.stdout.flush()
        ftp = self.__SetupOneTest(user, passwd)
        ftp.cwd('/upload')
        fp = open(file.svrpath, 'rb')
        res = ftp.storbinary(cmd, fp)
        fp.close()
        if res[:3] != '226':
            sys.stdout.write('Result: ' + res + ' ')
            return False
        uname = res[res.rfind(' ') + 1:]
        sys.stdout.write(' [' + uname + ']')
        diskpath = self.ftp_root + '/upload/' + uname
        result = md5sum(diskpath).hexdigest() == file.md5
        os.remove(diskpath)
        return result

    def TestStous(self, user='ftp', passwd=''):
         for file in self.dl_files:
            if self.skip_big_files and file.size >= 10000000:
                continue
            self.__Assert(self.TestStou(file, user, passwd))

    def TestAppe(self, user, passwd, vpath, bin, numIterations=2, buffers=5):
        if bin:
            dlt = 'binary'
        else:
            dlt = 'ascii'
        self.__PrintTest('[' + user + ']:' + ' Testing APPE ' + dlt  + \
            ' of ' + vpath + ' with ' + str(numIterations) + ' iterations:')
        sys.stdout.flush()
        ftp = self.__SetupOneTest(user, passwd)
        cmd = 'APPE ' + vpath;
        rval = False
        str_buffer = ('abcdefghijklmnopqrstuvzyxABCDEFGHIJKLMNOPQRSTUVZYX1234567890\r\n' * 16)
        buffer = str_buffer.encode()
        asciibuffer = str_buffer.replace('\r\n', '\n').encode()
        md5 = hashlib.md5()
        diskpath = self.ftp_root + vpath
        if os.path.exists(diskpath):
            os.remove(diskpath)

        for it in range(numIterations):
            try:
                if bin:
                    svrb = buffer
                    ftp.sendcmd('TYPE I')
                else:
                    svrb = asciibuffer
                    ftp.sendcmd('TYPE A')

                sck = ftp.transfercmd(cmd)

                for i in range(buffers):
                    md5.update(svrb)
                    sck.sendall(buffer)
                sck.close()
                sleep(1)
                rval = True
            except:
                sys.stdout.write(' *** EXCEPTION')
                break

        try:
            ftp.quit()
            ftp.close()
        except:
            pass

        if rval:
            dcrc = md5sum(diskpath)
            rval = dcrc.hexdigest() == md5.hexdigest()
            if not rval:
                sys.stdout.write(' [CHECKSUM] d=' + dcrc.hexdigest() + ', m=' + md5.hexdigest())

        if os.path.exists(diskpath):
            os.remove(diskpath)

        return rval

    def TestAppes(self, user='ftp', passwd=''):
        path = '/upload/appe_01'
        for bin in (True, False):
            for iterations in (1,2,5):
                self.__Assert(self.TestAppe(user, passwd, path, bin, iterations))

        return

    def TestCd(self, user, passwd, path, realpath):
        self.__PrintTest('[' + user + ']:' + ' Testing CWD: ' + path)
        ftp = self.__SetupOneTest(user, passwd)
        result = False
        try:
            res = ftp.cwd(path)
            if res[0:3] == '250':
                pwd = ftp.sendcmd('PWD')[4:]
                result = pwd == realpath
                if not result:
                    sys.stdout.write('[PWD is: ' + pwd + ']')
            else:
                sys.stdout.write('[CWD failed: ' + res + ']')
        except:
            sys.stdout.write(' ***EXCEPTION')

        ftp.quit()
        ftp.close()
        return result

    def TestCds(self, user='ftp', passwd=''):
        okpaths = (('/', '/'), ('/pub', '/pub'), ('/pub/..', '/'), \
            ('/upload', '/upload'), ('/pub/a/b/c../../..', '/pub'), \
            ('/pub/sub', '/pub/sub'), ('/pub/sub/sub2', '/pub/sub/sub2'))
        badpaths = (('/..', '/'), ('/../../../../..', ''), ('/home', '/home'), \
            ('/home/jgaa', '/home/jgaa'))

        for path in okpaths:
            self.__Assert(self.TestCd(user, passwd, path[0], path[1]))

        for path in badpaths:
            self.__Assert(not self.TestCd(user, passwd, path[0], path[1]))

    def TestCdup(self, user, passwd, path, realpath):
        self.__PrintTest('[' + user + ']:' + ' Testing CDUP from : ' + path)
        ftp = self.__SetupOneTest(user, passwd)
        result = False
        try:
            ftp.cwd(path)
            res = ftp.cwd(path)
            if res[0:3] == '250':
                cdup_res = ftp.sendcmd('CDUP')
                if cdup_res[0:3] == '250':
                    pwd = ftp.sendcmd('PWD')[4:]
                    result = pwd == realpath
        except:
            sys.stdout.write(' ***EXCEPTION')
        ftp.quit()
        ftp.close()
        return result

    def TestCdups(self, user='ftp', passwd=''):
        okpaths = (('/pub', '/'), ('/upload', '/'), ('/pub/sub/sub2', '/pub/sub'))

        for path in okpaths:
            self.__Assert(self.TestCdup(user, passwd, path[0], path[1]))

        self.__Assert(not self.TestCdup(user, passwd, '/', '/'))

    def __Upload(self, ftp, file, vpath):
        fp = open(file.svrpath, 'rb')
        ftp.storbinary('STOR ' + vpath, fp)
        fp.close()

    def TestDele(self, user, passwd, file, vpath='/upload/to-be-deleted'):
        self.__PrintTest('[' + user + ']:' + ' Testing DELE of: ' + vpath)
        ftp = self.__SetupOneTest(user, passwd)
        result = False
        try:
            if file:
                self.__Upload(ftp, file, vpath)
            res = ftp.delete(vpath)
            result = res[0:3] == '250'
        except:
            sys.stdout.write(' ***EXCEPTION')
        ftp.quit()
        ftp.close()
        return result

    def TestDeles(self, user='ftp', passwd=''):
        self.__Assert(self.TestDele(user, passwd, self.dl_files[0]))
        self.__Assert(not self.TestDele(user, passwd, None))
        self.__Assert(not self.TestDele(user, passwd, None, '/pub/test1'))

    def TestCmd(self, user, passwd, cmd, result):
        self.__PrintTest('[' + str(user) + ']:' + ' Testing ' + cmd)
        ftp = self.__SetupOneTest(user, passwd)
        res = False
        try:
            self.cmdret = ftp.sendcmd(cmd)
            res = self.cmdret[0:3] == result
            if not res:
                sys.stdout.write(' [' + self.cmdret + ']')
        except:
            sys.stdout.write(' ***EXCEPTION')
        ftp.quit()
        ftp.close()
        return res

    def TestHelp(self, user='ftp', passwd=''):
        cmds = ('hELp', 'help help', 'helP retr', 'HELP USER')
        for cmd in cmds:
            self.__Assert(self.TestCmd(user, passwd, cmd, '214'))

        self.__Assert(not self.TestCmd(user, passwd, 'help none', '214'))

    def TestNoop(self, user='ftp', passwd=''):
        self.__Assert(self.TestCmd(user, passwd, 'NOOP', '200'))

    def TestSyst(self, user='ftp', passwd=''):
        self.__Assert(self.TestCmd(user, passwd, 'SYST', '215'))

    def TestStat(self, user='ftp', passwd=''):
        self.__Assert(self.TestCmd(user, passwd, 'stat', '211'))

    def TestMdtm(self, user='ftp', passwd=''):
        self.__Assert(self.TestCmd(user, passwd, 'MDTM /pub/test1', '213') \
            and len(self.cmdret) == 18)

    def TestMkd(self, user, passwd, vpath, predel=True):
        self.__PrintTest('[' + user + ']:' + ' Testing MKD ' + vpath)
        ftp = self.__SetupOneTest(user, passwd)
        res = False

        diskpath = self.ftp_root + vpath
        if predel and os.path.exists(diskpath):
            os.rmdir(diskpath)

        try:
            ftp.mkd(vpath)
            res = True
        except:
            sys.stdout.write(' ***EXCEPTION')
        ftp.quit()
        ftp.close()
        return res

    def TestMkds(self, user='ftp', passwd=''):
        paths = ('/upload/mkp1', '/upload/mkp2')
        for vpath in paths:
             self.__Assert(self.TestMkd(user, passwd, vpath))

        self.__Assert(not self.TestMkd(user, passwd, '/home/newdir'))
        self.__Assert(not self.TestMkd(user, passwd, '/pub/newdir'))

        if not os.path.exists(self.ftp_root + '/upload/sub'):
            os.mkdir(self.ftp_root + '/upload/sub')
        self.__Assert(not self.TestMkd(user, passwd, '/upload/sub', False))
        if os.path.exists(self.ftp_root + '/upload/sub'):
            os.rmdir(self.ftp_root + '/upload/sub')

    def TestRmd(self, user, passwd, vpath, precreate=True):
        self.__PrintTest('[' + user + ']:' + ' Testing RMD ' + vpath)
        ftp = self.__SetupOneTest(user, passwd)
        res = False

        diskpath = self.ftp_root + vpath
        if precreate and not os.path.exists(diskpath):
            os.mkdir(diskpath)

        try:
            ftp.rmd(vpath)
            res = True
        except:
            sys.stdout.write(' ***EXCEPTION')
        ftp.quit()
        ftp.close()
        return res

    def TestRmds(self, user='ftp', passwd=''):
        paths = ('/upload/rmdir1', '/upload/rmdir2')
        for vpath in paths:
             self.__Assert(self.TestRmd(user, passwd, vpath))

        self.__Assert(not self.TestRmd(user, passwd, '/home/newdir'))
        self.__Assert(not self.TestRmd(user, passwd, '/pub/sub/sub2'))

    def TestSize(self, user, passwd, file):
        self.__PrintTest('[' + user + ']:' + ' Testing SIZE ' + file.vpath)
        ftp = self.__SetupOneTest(user, passwd)
        ftp.sendcmd('TYPE I')
        res = False
        try:
            len = ftp.size(file.vpath)
            res = len == file.size
        except:
            sys.stdout.write(' ***EXCEPTION')
        ftp.quit()
        ftp.close()
        return res

    def TestSizes(self, user='ftp', passwd=''):
        for file in self.dl_files:
            self.__Assert(self.TestSize(user, passwd, file))

    def TestRename(self, user, passwd, old, new, dir, precreate=True, predelete=True):

        if dir:
            ftype = ' [dir] '
        else:
            ftype = ' [file] '

        self.__PrintTest('[' + user + ']:' + ' Testing RENAME' + ftype + old + ' --> ' + new)
        ftp = self.__SetupOneTest(user, passwd)
        res = False

        old_diskpath = self.ftp_root + old
        new_diskpath = self.ftp_root + new

        if precreate and not os.path.exists(old_diskpath):
            if dir:
                os.mkdir(old_diskpath)
            else:
                open(old_diskpath, 'wb').close()

        if predelete and os.path.exists(new_diskpath):
            if dir:
                os.rmdir(new_diskpath)
            else:
                os.remove(new_diskpath)

        try:
            ftp.rename(old, new)
            res = os.path.exists(new_diskpath)
        except:
            sys.stdout.write(' ***EXCEPTION')
        ftp.quit()
        ftp.close()
        return res

    def TestRenames(self, user='ftp', passwd=''):
        self.__Assert(self.TestRename(user, passwd, '/upload/dog-file', '/upload/cat-file', False))
        self.__Assert(self.TestRename(user, passwd, '/upload/dog-dir', '/upload/cat-dir', True))

        if not os.path.exists(self.ftp_root + '/upload/existing-file'):
            open(self.ftp_root + '/upload/existing-file', 'wb').close()
        if not os.path.exists(self.ftp_root + '/upload/existing-dir'):
            os.mkdir(self.ftp_root + '/upload/existing-dir')

        self.__Assert(not self.TestRename(user, passwd, '/upload/dog-file', '/upload/existing-file', False, True, False))
        self.__Assert(not self.TestRename(user, passwd, '/upload/dog-dir', '/upload/existing-dir', True, True, False))

        self.__Assert(not self.TestRename(user, passwd, '/upload/nonexistant-file', '/upload/cat-file', False, False, False))
        self.__Assert(not self.TestRename(user, passwd, '/upload/nonexistant-dir', '/upload/cat-dir', True, False, False))

        self.__Assert(not self.TestRename(user, passwd, '/pub/sub/dog-file', '/upload/cat-file', False))
        self.__Assert(not self.TestRename(user, passwd, '/pub/sub/dog-dir', '/upload/cat-dir', True))

        self.__Assert(not self.TestRename(user, passwd, '/upload/dog-file', '/pub/sub/cat-file', False))
        self.__Assert(not self.TestRename(user, passwd, '/upload/dog-dir', '/pub/sub/cat-dir', True))

    def TestPassBeforeUser(self):
        self.__Assert(not self.TestCmd(None, None, 'PASS test', '214'))

    def __CopyPartialFile(self, frompath, topath, length):
        bytes_left = length
        if bytes_left > os.path.getsize(frompath):
            return False
        from_f = open(frompath, 'rb')
        to_f = open(topath, 'wb')

        while bytes_left > 0:
            buffer_len = 1024
            if buffer_len > bytes_left:
                buffer_len = bytes_left
            bytes_left -= buffer_len
            buffer = from_f.read(buffer_len)
            to_f.write(buffer)

        from_f.close()
        to_f.close()
        return True

    def __StoreBlock(self, block):
        self.fp.write(block)

    def TestRestDownload(self, user, passwd, file):
        self.__PrintTest('[' + user + ']:' + ' Testing Download + REST ' + \
            str(file.restoffset) + ' of ' + file.vpath + ' [' + str(file.size) + ']')
        sys.stdout.flush()

        # Copy up to offset of the original file
        mypath = self.workdir + file.vpath[file.vpath.rfind('/'):]
        if not self.__CopyPartialFile(file.svrpath, mypath, file.restoffset):
            sys.stdout.write('[Partial Copy]')
            return False

        ftp = self.__SetupOneTest(user, passwd)
        ftp.sendcmd('TYPE I')
        res = False
        try:
            # Resume transfer at offset
            self.fp = open(mypath, 'w+b')
            self.fp.seek(0, 2)
            ftp.retrbinary('RETR ' + file.vpath, self.__StoreBlock, 8192, file.restoffset)
            self.fp.close()
            self.fp = None

            # Check md5sum
            target_md5 = md5sum(mypath).hexdigest()
            res = target_md5 == file.md5
            if not res:
                sys.stdout.write(' [file.md5=' + file.md5 + ' target_md5=' + target_md5 + ']')
                #print('mypath: ' + mypath)
                #print('orig: ' + file.svrpath)
        except:
            sys.stdout.write(' ***EXCEPTION')
        ftp.quit()
        ftp.close()
        return res

    def TestRestUpload(self, user, passwd, file):
        vpath = '/upload' + file.vpath[file.vpath.rfind('/'):]
        self.__PrintTest('[' + user + ']:' + ' Testing Upload + REST ' + \
            str(file.restoffset) + ' of ' + vpath + ' [' + str(file.size) + ']')
        sys.stdout.flush()

        # Copy up to offset of the original file
        mypath = self.ftp_root + vpath
        if not self.__CopyPartialFile(file.svrpath, mypath, file.restoffset):
            sys.stdout.write('[Partial Copy]')
            return False

        ftp = self.__SetupOneTest(user, passwd)
        ftp.sendcmd('TYPE I')
        res = False
        try:
            # Resume transfer at offset
            fp = open(file.svrpath, 'rb')
            # storbinary will call fp.seek()
            ftp.storbinary('STOR ' + vpath, fp, 8192, None, file.restoffset)
            fp.close()

            # Check md5sum
            target_md5 = md5sum(mypath).hexdigest()
            res = target_md5 == file.md5
            if not res:
                sys.stdout.write(' [file.md5=' + file.md5 + ' target_md5=' + target_md5 + ']')
                print('\nmypath: ' + mypath)
                print('orig: ' + file.svrpath)
        except:
            sys.stdout.write(' ***EXCEPTION')
        ftp.quit()
        ftp.close()
        return res


    def TestRests(self, user='ftp', passwd=''):
        for file in self.dl_files:
             if self.skip_big_files and file.size >= 10000000:
                 continue
             self.__Assert(self.TestRestDownload(user, passwd, file))
             self.__Assert(self.TestRestUpload(user, passwd, file))


    def TestAborBeforeData(self, user, passwd, cmd, pre):
        self.__PrintTest('[' + user + ']:' + ' Testing ABOR [before data] ' + cmd + ' ' + pre)
        ftp = self.__SetupOneTest(user, passwd)
        ftp.sendcmd('TYPE I')
        res = False
        try:
            ftp.voidcmd(pre)
            ftp.sendcmd(cmd)
            try:
                ftp.sendcmd('ABOR')
            except ftplib.error_temp:
                pass
            res = True
        except:
            sys.stdout.write(' ***EXCEPTION')
        try:
            ftp.quit()
            ftp.close()
        except:
            pass
        return res

    def __PrintTest(self, string):
        if not self.disable_print:
            sys.stdout.write(string)
            sys.stdout.flush()

    def TestAborDuringTransfer(self, user, passwd, upload):
        self.__PrintTest('[' + user + ']:' + ' Testing ABOR Upload=' + str(upload))
        if upload:
            cmd = 'STOR /upload/aborttest'
        else:
            cmd = 'RETR /pub/test12'

        ftp = self.__SetupOneTest(user, passwd)
        ftp.sendcmd('TYPE I')
        res = False
        try:
            try:
                sck = ftp.transfercmd(cmd)

                if upload:
                    sck.sendall(('1234567890' * 512).encode())
                else:
                    sck.recv(1024*12)
                ftp.sendcmd('ABOR')
            except ftplib.error_temp:
                pass
            res = True
        except:
            sys.stdout.write(' ***EXCEPTION')
        try:
            ftp.quit()
            ftp.close()
        except:
            pass
        return res


    def TestAbors(self, user='ftp', passwd=''):
        orig_pasv = self.pasv_mode
        for pasv in (False, True):
            self.pasv_mode = pasv
            for upload in (True, False):
                self.__Assert(self.TestAborDuringTransfer(user, passwd, upload))
        self.pasv_mode = orig_pasv

    def TestAccessToHomes(self):
        self.__Assert(not self.TestCd('ftp', '', '/home', '/home'));
        self.__Assert(self.TestCd('jgaa', 'test', '/home', '/home'));

    def ReportResult(self):
        if self.failcount:
            print('*** TEST FAILED with ' + str(testcase.failcount) + ' error(s) ***')
        else:
            print('All the tests were successfull!')


if __name__ == '__main__':

    # Run tests
    testcase = FtpTest(config)
    testcase.use_tls = True
    # Uncomment below to enable tests with large files (it's slow)
    #testcase.skip_big_files = False
    server_config.CreateServerConfig(config)
    print('Start the server with config-path to ' + config['server-config'])
    input('Press ENTER when ready')

    for tls in (False, True):
        print('Testing with TLS = ' + str(tls))
        testcase.use_tls = tls

        testcase.TestEmptyInput()
        testcase.TestAnonymousLogins()
        testcase.TestInvalidLogins()
        testcase.TestUserLogins()
        testcase.TestLists()
        testcase.TestNlsds()
        testcase.TestMlsts()
        testcase.TestCds()
        testcase.TestCdups()
        testcase.TestDeles()
        testcase.TestHelp()
        testcase.TestNoop()
        testcase.TestSyst()
        testcase.TestStat()
        testcase.TestMdtm()
        testcase.TestMkds()
        testcase.TestRmds()
        testcase.TestSizes()
        testcase.TestRenames()
        testcase.TestPassBeforeUser()
        testcase.TestAbors()
        testcase.TestAccessToHomes()

        for pm in (False, True):
            print('Testing with passive mode = ' + str(pm))
            testcase.pasv_mode = pm

            testcase.TestAppes()
            testcase.TestDownloads()
            testcase.TestUploads()
            testcase.CloseDataTransfers()
            testcase.TestStous()
            testcase.TestRests()

    testcase.ReportResult()


