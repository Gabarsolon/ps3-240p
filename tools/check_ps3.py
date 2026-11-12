import urllib.request, re, ftplib
try:
    url = 'http://192.168.0.169/xmb.ps3(/setting/display/tvSystem)'
    res = urllib.request.urlopen(url, timeout=5).read().decode('latin-1')
    m = re.search(r'xregistry\([^)]+\)\s*=>\s*([^<]+)', res)
    print('Current tvSystem:', m.group(1).strip() if m else 'ERR', '(Raw:', res[:200], ')')
except Exception as e:
    print('HTTP err:', e)

try:
    ftp = ftplib.FTP('192.168.0.169', encoding='latin-1', timeout=5)
    ftp.login()
    buf = bytearray()
    ftp.retrbinary('RETR /dev_hdd0/tmp/ps3_240p.log', lambda b: buf.extend(b))
    ftp.quit()
    print('=== ps3_240p.log ===')
    print(bytes(buf).decode('latin-1')[-1000:])
except Exception as e:
    print('FTP err:', e)
