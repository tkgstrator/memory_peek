import contextlib
import socket
import subprocess
import time
import unittest


@contextlib.contextmanager
def connection(scenario="normal"):
    process = subprocess.Popen(["build/fake_server", scenario], stdout=subprocess.PIPE, text=True)
    try:
        port = int(process.stdout.readline())
        with socket.create_connection(("127.0.0.1", port), timeout=3) as sock:
            sock.settimeout(3)
            yield sock
    finally:
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=3)
        process.stdout.close()


def line(sock, request):
    sock.sendall(request.encode("ascii") + b"\r\n")
    data = bytearray()
    while not data.endswith(b"\n"):
        chunk = sock.recv(4096)
        if not chunk:
            break
        data.extend(chunk)
    return bytes(data)


class ServerTests(unittest.TestCase):
    def test_metadata_and_reads_have_exact_shapes(self):
        with connection() as sock:
            self.assertEqual(line(sock, "getVersion"), b"1.0.0\r\n")
            self.assertEqual(line(sock, "getIdentity"), b"memory_peek/1\r\n")
            self.assertEqual(line(sock, "getTitleID"), b"0100C2500FC20000\r\n")
            self.assertEqual(line(sock, "getMainNsoBase"), b"0000000000001000\r\n")
            self.assertEqual(line(sock, "getHeapBase"), b"0000000000008000\r\n")
            self.assertEqual(line(sock, "getBuildID"), b"A0A1A2A3A4A5A6A7\r\n")
            self.assertEqual(line(sock, "isProgramRunning 0x0100C2500FC20000"), b"1\r\n")
            self.assertEqual(line(sock, "isProgramRunning 0x1"), b"0\r\n")
            self.assertEqual(line(sock, "peekMain 0x20 0x4"), b"20212223\r\n")
            self.assertEqual(line(sock, "peekAbsoluteMulti 0x10 0x2 0x20 0x2"), b"10112021\r\n")

    def test_fragmented_request_and_unknown_command(self):
        with connection() as sock:
            sock.sendall(b"get")
            sock.sendall(b"Version\r")
            sock.sendall(b"\n")
            self.assertEqual(sock.recv(16), b"1.0.0\r\n")
            self.assertEqual(line(sock, "poke 0x10 0xFF"), b"ERR INVALID_REQUEST\r\n")
            self.assertEqual(line(sock, "freeze 0x10"), b"ERR INVALID_REQUEST\r\n")

    def test_oversize_line_disconnects_without_processing_following_request(self):
        with connection() as sock:
            sock.sendall(b"A" * 2049 + b"\r\ngetVersion\r\n")
            self.assertEqual(sock.recv(64), b"ERR INVALID_REQUEST\r\n")
            try:
                self.assertEqual(sock.recv(64), b"")
            except ConnectionResetError:
                pass

    def test_idle_client_disconnects(self):
        with connection() as sock:
            time.sleep(1.3)
            self.assertEqual(sock.recv(16), b"")

    def test_maximum_read_is_complete_and_uppercase(self):
        with connection() as sock:
            result = line(sock, "peekAbsolute 0x1000 0x100000")
            self.assertEqual(len(result), 2 * 1048576 + 2)
            self.assertEqual(result[:18], b"000102030405060708")
            self.assertEqual(result[-18:], b"F8F9FAFBFCFDFEFF\r\n")

    def test_stalled_reader_does_not_hold_client_forever(self):
        process = subprocess.Popen(["build/fake_server", "normal"], stdout=subprocess.PIPE, text=True)
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
        try:
            port = int(process.stdout.readline())
            sock.connect(("127.0.0.1", port))
            sock.sendall(b"peekAbsolute 0x1000 0x100000\r\n")
            self.assertEqual(process.wait(timeout=4), 0)
        finally:
            sock.close()
            if process.poll() is None:
                process.kill()
                process.wait(timeout=3)
            process.stdout.close()

    def test_no_game_and_failed_or_switched_read_have_no_partial_hex(self):
        with connection("no_game") as sock:
            self.assertEqual(line(sock, "getTitleID"), b"ERR NO_GAME\r\n")
            self.assertEqual(line(sock, "isProgramRunning 0x1"), b"0\r\n")
        with connection("read_fail") as sock:
            self.assertEqual(line(sock, "peekAbsoluteMulti 0x1000 0x2 0x2000 0x2"), b"ERR READ_FAILED\r\n")
        with connection("switch") as sock:
            self.assertEqual(line(sock, "peekAbsolute 0x1000 0x2"), b"ERR GAME_CHANGED\r\n")

    def test_running_game_without_reader_reports_unavailable(self):
        with connection("unavailable") as sock:
            self.assertEqual(line(sock, "getTitleID"), b"ERR READER_UNAVAILABLE\r\n")
            self.assertEqual(line(sock, "peekAbsolute 0x1000 0x2"), b"ERR READER_UNAVAILABLE\r\n")
            self.assertEqual(line(sock, "isProgramRunning 0x0100C2500FC20000"), b"ERR READER_UNAVAILABLE\r\n")


if __name__ == "__main__":
    unittest.main()
