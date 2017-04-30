import socket
import json
import time
import multiprocessing
import os


class cv_network_controller:
    sizeof_uint64 = 8
    localhost = '127.0.0.1'

    def __init__(self):
        self.scanner_proc = multiprocessing.Process(target=self.__scanner)
        self.scanner_mtx = multiprocessing.RLock()
        self.scanner_run = multiprocessing.Value('b', 0)
        self.mac_found_handlers = {}
        self.mac_ip_map = {}

    def add_mac_handler(self, mac_address, handler):
        with self.scanner_mtx:
            if is_local_mac_address(mac_address):
                handler(cv_network_controller.localhost)
                print 'is_local_mac'
            else:
                self.mac_ip_map[mac_address] = handler
            if len(self.mac_ip_map.keys()) == 1:
                self.scanner_run = 1
                self.scanner_proc.run()

    def del_mac_handler(self, mac_address):
        with self.scanner_mtx:
            self.mac_ip_map.pop(mac_address)
            if len(self.mac_ip_map.keys()) == 0 and self.scanner_proc.is_alive():
                    self.scanner_run = 0
                    self.scanner_proc.join()

    def __update_mac_ip_map(self):
        pipe = os.popen('arp')
        out = pipe.readlines()
        out = out[1:]
        self.mac_ip_map = {}
        for line in out:
            line = line[:-1]
            strings = line.split(' ')
            strings = [string for string in strings if string != '']
            self.mac_ip_map[strings[2]] = strings[0]
        pipe.close()

    def __scanner(self):
        while True:
            with self.scanner_mtx:
                if self.scanner_run == 0:
                    return

                time.sleep(10)
                self.__update_mac_ip_map()
                for mac in self.mac_found_handlers:
                    if mac in self.mac_ip_map:
                        self.mac_found_handlers[mac](self.mac_ip_map[mac])
                        self.del_mac_handler(mac)


def is_local_mac_address(target_mac):
    result = False
    pipe = os.popen('ifconfig')
    out = pipe.readlines()
    for line in out:
        line = line[:-1]
        strings = line.split(' ')
        strings = [string for string in strings if string != '']
        if len(strings) > 4 and strings[3] == 'HWaddr' and strings[4] == target_mac:
            result = True
            break
    pipe.close()
    return result


def create_udp_sock(udp_port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((cv_network_controller.localhost, udp_port))
    return sock


def create_tcp_sock(tcp_port):
    sock = socket.socket()
    sock_initialized = False
    while not sock_initialized:
        try:
            sock.bind(('', tcp_port))
        except socket.error:
            print 'Address already in use: try to bind again over 10 seconds'
            time.sleep(10)
        else:
            sock_initialized = True

    return sock


def get_tcp_client(sock):
    sock.listen(1)
    conn, addr = sock.accept()
    return conn


def connect_to_tcp_host(tcp_address, tcp_port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    connected = False
    while not connected:
        try:
            sock.connect((tcp_address, tcp_port))
        except socket.error:
            time.sleep(5)
        else:
            connected = True
    return sock


def async_receive_packet(sock):
    try:
        sock.setblocking(0)
        size_arr = sock.recv(cv_network_controller.sizeof_uint64)
    except socket.error:
        return None
    else:
        if len(size_arr) == 0:
            return None
        size = __bytearr_to_uint(size_arr)
        if size == 0:
            return None
        sock.setblocking(1)
        buf = __recv_all(sock, size)
        return json.loads(buf)


def receive_packet(sock):
    size_arr = sock.recv(cv_network_controller.sizeof_uint64)
    size = __bytearr_to_uint(size_arr)
    if size == 0:
        return None
    buf = __recv_all(sock, size)
    return json.loads(buf)


def send_packet(sock, rest_packet):
    packet = json.dumps(rest_packet, separators=(',', ':'))
    packet_size = __uint_to_bytearr(len(packet))
    sock.sendall(packet_size + packet)


def __bytearr_to_uint(bytearr):
    uinteger = 0
    for i in range(0, cv_network_controller.sizeof_uint64):
        uinteger += ord(bytearr[i]) << (8 * i)
    return uinteger


def __uint_to_bytearr(uinteger):
    bytearr = ''
    for i in range(0, cv_network_controller.sizeof_uint64):
        bytearr += chr((uinteger >> (i * 8)) & 0xff)
    return bytearr


def __recv_all(sock, size):
    buffer = ''
    while len(buffer) < size:
        buffer += sock.recv(size - len(buffer))
    return buffer
