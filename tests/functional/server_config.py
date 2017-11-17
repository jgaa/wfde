

# Create wfded's configuration-data and write it to disk
def CreateServerConfig(config):
    template = '''
Server {
    Name Server
    Hosts {
        FanClub {
            Protocols {
                ftp {
                    Name FTP
                    Interfaces {
                        tcp-local {
                            Name tcp-local
                            Ip "{{IP}}"
                            Port "{{PORT}}"
                            TlsCert "{{CERT}}"
                        }
                    }
                }
            }
            Paths {
                root {
                    Name "/"
                    Path "{{FTPROOT}}"
                    Perms "CAN_READ,CAN_LIST,CAN_ENTER,IS_RECURSIVE"
                }
                bin {
                    Name bin
                    Path "/usr/bin"
                    Perms "CAN_READ,CAN_LIST,CAN_ENTER,IS_RECURSIVE"
                }
                home {
                    Name home
                    Path "{{FTPROOT}}/home"
                    Perms "IS_RECURSIVE"
                }
                upload {
                    Name upload
                    Path "{{FTPROOT}}/upload"
                    Perms "IS_RECURSIVE,CAN_LIST,CAN_ENTER,CAN_READ,CAN_WRITE,CAN_CREATE_FILE,CAN_DELETE_FILE,CAN_CREATE_DIR,CAN_DELETE_DIR,CAN_RENAME"
                }
            }

            Users {
                ; Anonymous users are users without password
                anonymous

                ; Aliases has a value that identifies the real user-account
                ftp anonymous

                ; A real user
                jgaa {
                    Passwd "test"

                    HomePath "/home"

                    Paths {
                        home {
                            Name "home"
                            Path "{{FTPROOT}}/home/jgaa"
                            Perms "IS_RECURSIVE,CAN_LIST,CAN_ENTER,CAN_READ,CAN_WRITE,CAN_CREATE_FILE,CAN_DELETE_FILE,CAN_CREATE_DIR,CAN_DELETE_DIR,CAN_RENAME"
                        }
                    }
                }
            }
        }
    }
}
'''

    conf_data = template.replace('{{IP}}', config['host']).replace('{{PORT}}', str(config['port'])).replace('{{FTPROOT}}', config['ftp-root'].replace('\\','/')).replace('{{CERT}}', config['server-cert'])
    f = open(config['server-config'], 'w');
    f.write(conf_data)
    f.close()
